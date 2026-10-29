#!/usr/bin/env python3
"""Check the Axe King UI strings against the generated LVGL subset."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCES = (
    ROOT / "main" / "axe_logic.h",
    ROOT / "main" / "axe_logic.c",
    ROOT / "main" / "axe_ui.c",
)
FONTS = (
    ROOT / "assets" / "fonts" / "axe_king_font_16.c",
    ROOT / "assets" / "fonts" / "axe_king_font_24.c",
)
STRING_RE = re.compile(r'"((?:\\.|[^"\\])*)"')
NEGATIVE = 0x9F98


def c_string(literal: str) -> str:
    out: list[str] = []
    index = 0
    while index < len(literal):
        if literal[index] != "\\":
            out.append(literal[index])
            index += 1
            continue
        index += 1
        esc = literal[index]
        index += 1
        mapping = {"n": "\n", "r": "\r", "t": "\t", "\\": "\\", '"': '"', "'": "'"}
        if esc not in mapping:
            raise AssertionError(f"unsupported escape \\{esc}")
        out.append(mapping[esc])
    return "".join(out)


def ui_codepoints() -> set[int]:
    found: set[int] = set()
    for path in SOURCES:
        for match in STRING_RE.finditer(path.read_text(encoding="utf-8")):
            for char in c_string(match.group(1)):
                if char not in "\n\r\t":
                    found.add(ord(char))
    return found


def font_codepoints(path: Path) -> set[int]:
    text = path.read_text(encoding="utf-8")
    tiny = re.search(
        r"\.range_start = 32, \.range_length = 95, \.glyph_id_start = 1,",
        text,
    )
    if not tiny:
        raise AssertionError(f"{path.name}: missing ASCII cmap")
    covered = set(range(32, 127))
    sparse = re.search(
        r"\.range_start = 0x([0-9A-Fa-f]+), \.range_length = (\d+), \.glyph_id_start = 96,",
        text,
    )
    listing = re.search(r"static const uint16_t unicode_list_1\[\] = \{(.*?)\};", text, re.S)
    if not sparse or not listing:
        raise AssertionError(f"{path.name}: missing sparse cmap")
    start = int(sparse.group(1), 16)
    offsets = [int(item, 16) for item in re.findall(r"0x([0-9A-Fa-f]{4})", listing.group(1))]
    if offsets != sorted(offsets):
        raise AssertionError(f"{path.name}: sparse cmap is not sorted")
    span = int(sparse.group(2))
    if not offsets or offsets[-1] >= span:
        raise AssertionError(f"{path.name}: sparse range does not contain its last glyph")
    covered.update(start + offset for offset in offsets)
    declared = int(re.search(r"AXE_FONT_GLYPHS (\d+)", text).group(1))
    if declared != 95 + len(offsets):
        raise AssertionError(f"{path.name}: glyph count does not match the cmap")
    bitmap = re.search(r"glyph_bitmap\[\] = \{(.*?)\};", text, re.S)
    byte_count = len(re.findall(r"0x[0-9A-Fa-f]{2}", bitmap.group(1)))
    marked = int(re.search(r"AXE_FONT_BYTES (\d+)", text).group(1))
    if byte_count != marked:
        raise AssertionError(f"{path.name}: bitmap byte count {byte_count} != {marked}")
    return covered


class AxeGlyphCoverageTest(unittest.TestCase):
    def test_ui_strings_are_covered_and_the_negative_case_is_not(self) -> None:
        required = ui_codepoints()
        self.assertIn(ord("飞"), required)
        self.assertIn(ord("请"), required)
        self.assertIn(ord("，"), required)
        self.assertNotIn(NEGATIVE, required)
        fonts = [font_codepoints(path) for path in FONTS]
        self.assertEqual(fonts[0], fonts[1])
        missing = sorted(required - fonts[0])
        self.assertEqual(missing, [], msg=" ".join(f"U+{cp:04X}" for cp in missing))
        self.assertTrue(set(range(0x20, 0x7F)) <= fonts[0])
        self.assertNotIn(NEGATIVE, fonts[0])
        inventory = {
            int(line.strip()[2:], 16)
            for line in (ROOT / "assets" / "fonts" / "axe_king_inventory.txt").read_text(encoding="utf-8").splitlines()
            if line.strip()
        }
        self.assertEqual(inventory, fonts[0])
        self.assertTrue(required <= inventory)


if __name__ == "__main__":
    unittest.main()
