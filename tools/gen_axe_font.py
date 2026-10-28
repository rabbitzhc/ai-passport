#!/usr/bin/env python3
"""Rasterize the Axe King UI subset into LVGL 9.5 fmt_txt fonts.

The converter in lv_font_conv 1.5.3 can emit bitmaps this LVGL build does not
decode. This script reads FreeType directly, packs 4 bpp PLAIN glyphs the way
lv_font_get_bitmap_fmt_txt walks them (continuous nibbles, high nibble first,
stride 0), and checks the packed bytes against the source coverage.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import freetype

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FONT = Path("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc")
SOURCES = (
    ROOT / "main" / "axe_logic.h",
    ROOT / "main" / "axe_logic.c",
    ROOT / "main" / "axe_ui.c",
)
STRING_RE = re.compile(r'"((?:\\.|[^"\\])*)"')
NEGATIVE = 0x9F98


def c_string(literal: str) -> str:
    out: list[str] = []
    i = 0
    while i < len(literal):
        ch = literal[i]
        if ch != "\\":
            out.append(ch)
            i += 1
            continue
        i += 1
        esc = literal[i]
        i += 1
        mapping = {"n": "\n", "r": "\r", "t": "\t", "\\": "\\", '"': '"', "'": "'"}
        if esc in mapping:
            out.append(mapping[esc])
        elif esc == "x":
            out.append(chr(int(literal[i:i + 2], 16)))
            i += 2
        else:
            raise ValueError(f"unsupported escape \\{esc}")
    return "".join(out)


def codepoints_from_sources() -> set[int]:
    found: set[int] = set()
    for path in SOURCES:
        text = path.read_text(encoding="utf-8")
        for match in STRING_RE.finditer(text):
            for ch in c_string(match.group(1)):
                if ch in "\n\r\t":
                    continue
                found.add(ord(ch))
    found.update(range(0x20, 0x7F))
    if NEGATIVE in found:
        raise SystemExit("the negative coverage code point is used by UI text")
    return found


def render(face: freetype.Face, codepoint: int, px: int) -> dict:
    if face.get_char_index(codepoint) == 0:
        raise SystemExit(f"source font has no U+{codepoint:04X}")
    face.set_pixel_sizes(0, px)
    face.load_char(codepoint, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL)
    slot = face.glyph
    bitmap = slot.bitmap
    width = int(bitmap.width)
    rows = int(bitmap.rows)
    pitch = int(bitmap.pitch)
    raw = bytes(bitmap.buffer)
    pixels: list[int] = []
    if width and rows:
        if pitch < 0:
            pitch = -pitch
            raw = b"".join(raw[y * pitch:(y + 1) * pitch] for y in range(rows - 1, -1, -1))
        for y in range(rows):
            pixels.extend(raw[y * pitch:y * pitch + width])
        if len(pixels) != width * rows:
            raise SystemExit(f"short bitmap for U+{codepoint:04X}")
    nibbles = [(value * 15 + 127) // 255 for value in pixels]
    packed = bytearray()
    for index in range(0, len(nibbles), 2):
        low = nibbles[index + 1] if index + 1 < len(nibbles) else 0
        packed.append((nibbles[index] << 4) | low)
    unpacked: list[int] = []
    for byte in packed:
        unpacked.append(byte >> 4)
        unpacked.append(byte & 0x0F)
    if unpacked[: len(nibbles)] != nibbles:
        raise SystemExit(f"nibble pack mismatch for U+{codepoint:04X}")
    advance = int(round(slot.advance.x / 4.0))
    ofs_x = int(slot.bitmap_left)
    ofs_y = int(slot.bitmap_top) - rows
    if not -128 <= ofs_x <= 127 or not -128 <= ofs_y <= 127:
        raise SystemExit(f"glyph offset does not fit int8 for U+{codepoint:04X}")
    if width > 255 or rows > 255 or advance > 4095:
        raise SystemExit(f"glyph metrics do not fit the fmt_txt descriptor for U+{codepoint:04X}")
    return {
        "cp": codepoint,
        "adv": advance,
        "box_w": width,
        "box_h": rows,
        "ofs_x": ofs_x,
        "ofs_y": ofs_y,
        "packed": bytes(packed),
    }


def emit(path: Path, symbol: str, px: int, glyphs: list[dict], source: str, license_name: str) -> None:
    bitmap = bytearray()
    dsc_lines = [
        "    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},"
    ]
    ordered = sorted(glyphs, key=lambda item: item["cp"])
    ascii_glyphs = [item for item in ordered if 0x20 <= item["cp"] <= 0x7E]
    extra = [item for item in ordered if item["cp"] > 0x7E]
    if [item["cp"] for item in ascii_glyphs] != list(range(0x20, 0x7F)):
        raise SystemExit("ASCII subset is not contiguous")
    sequence = ascii_glyphs + extra
    for item in sequence:
        item["index"] = len(bitmap)
        bitmap.extend(item["packed"])
        dsc_lines.append(
            "    {.bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d, .ofs_x = %d, .ofs_y = %d},"
            % (item["index"], item["adv"], item["box_w"], item["box_h"], item["ofs_x"], item["ofs_y"])
        )
    ascent = 0
    descent = 0
    for item in sequence:
        ascent = max(ascent, item["ofs_y"] + item["box_h"])
        if item["ofs_y"] < 0:
            descent = max(descent, -item["ofs_y"])
    line_height = max(ascent + descent, 1)
    if len(bitmap) >= (1 << 20):
        raise SystemExit("bitmap index no longer fits the 20-bit fmt_txt field")

    hex_lines = []
    for offset in range(0, len(bitmap), 16):
        chunk = ", ".join("0x%02X" % byte for byte in bitmap[offset:offset + 16])
        hex_lines.append(f"    {chunk},")

    if extra:
        start = extra[0]["cp"]
        offsets = [item["cp"] - start for item in extra]
        if offsets != sorted(offsets):
            raise SystemExit("sparse cmap is not sorted")
        span = extra[-1]["cp"] - start + 1
        if span > 65535:
            raise SystemExit("sparse cmap span does not fit uint16")
        offset_lines = []
        for item, rel in zip(extra, offsets):
            offset_lines.append(f"    0x{rel:04X}, /* U+{item['cp']:04X} */")
        sparse = "\n".join(
            [
                "static const uint16_t unicode_list_1[] = {",
                *offset_lines,
                "};",
                "",
            ]
        )
        sparse_cmap = (
            "    {\n"
            f"        .range_start = 0x{start:04X}, .range_length = {span}, .glyph_id_start = 96,\n"
            "        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, "
            f".list_length = {len(extra)}, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY\n"
            "    },"
        )
        cmap_num = 2
    else:
        sparse = ""
        sparse_cmap = ""
        cmap_num = 1

    text = f"""/* Generated by tools/gen_axe_font.py. Do not edit by hand.
 * Source face: {source}
 * Source license: {license_name}. The face file is not committed; this subset
 * is the redistributed bitmap.
 * LVGL 9.5 fmt_txt, {px}px, 4 bpp, PLAIN, stride 0, no kerning, no compression.
 * AXE_FONT_BYTES {len(bitmap)}
 * AXE_FONT_GLYPHS {len(sequence)}
 * AXE_FONT_LINE {line_height} {descent}
 */
#include "lvgl.h"

#ifndef LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_CONST
#endif

static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {{
{chr(10).join(hex_lines)}
}};

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {{
{chr(10).join(dsc_lines)}
}};

{sparse}static const lv_font_fmt_txt_cmap_t cmaps[] = {{
    {{
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }},
{sparse_cmap}
}};

static const lv_font_fmt_txt_dsc_t font_dsc = {{
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = {cmap_num},
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = LV_FONT_FMT_TXT_PLAIN,
    .stride = 0,
}};

static void {symbol}_release(const lv_font_t *font, lv_font_glyph_dsc_t *glyph) {{
    (void)font;
    (void)glyph;
}}

const lv_font_t {symbol} = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .release_glyph = {symbol}_release,
    .line_height = {line_height},
    .base_line = {descent},
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -1,
    .underline_thickness = 1,
    .dsc = &font_dsc,
    .fallback = NULL,
    .user_data = NULL,
}};
"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    print(f"wrote {path} ({len(bitmap)} bitmap bytes, line {line_height}/{descent})")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, default=DEFAULT_FONT)
    parser.add_argument("--license", default="Apache-2.0 OR GPL-3.0-with-font-exception")
    parser.add_argument("--sizes", default="16,24")
    args = parser.parse_args()
    if not args.font.is_file():
        print(f"source font not found: {args.font}", file=sys.stderr)
        return 1
    needed = codepoints_from_sources()
    face = freetype.Face(str(args.font))
    for size in (int(item) for item in args.sizes.split(",") if item):
        glyphs = [render(face, codepoint, size) for codepoint in sorted(needed)]
        emit(
            ROOT / "assets" / "fonts" / f"axe_king_font_{size}.c",
            f"axe_king_font_{size}",
            size,
            glyphs,
            args.font.name,
            args.license,
        )
    inventory = ROOT / "assets" / "fonts" / "axe_king_inventory.txt"
    inventory.write_text(
        "\n".join(f"U+{codepoint:04X}" for codepoint in sorted(needed)) + "\n",
        encoding="utf-8",
    )
    print(f"wrote {inventory} ({len(needed)} code points)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
