<p align="right">
  <a href="axe-king.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Axe King Mini-Game

The firmware built from this tree is an offline mini-game. `app_main` draws that game and does not open the baseline hardware-test menu. Demo sources stay in `main/` for the existing host tests, and they are not linked into the application. The screens are a separate night-arena layout built from LVGL primitives. They do not use `ui_pixel` panels, the demo mascot, or the test-menu shell.

## Play

The panel is 240 by 320. The product specification names the three keys UP, DOWN, and OK. This game treats UP as volume-plus and DOWN as volume-minus. That is a control choice; it does not change `bsp_pins.h`.

1. Cover. A large original portrait, the title, and a continue hint. Any key press starts a round.
2. Setup, five seconds. A red king token, slightly larger than each minion, and ten green minions stand inside one fixed circle. The king cluster and the minion cluster are placed at new positions every round. The king spins in place. A translucent red circle with the same radius flashes around the king. The status line is on the left and the countdown is on the right.
3. Aim. Minions hide and the king stays put. Volume-plus rotates a clockwise arrow; volume-minus rotates it the other way. Holding a volume key keeps rotating. OK locks the direction. A distance marker then stretches and shrinks along the arrow. OK again locks the current distance.
4. Resolve. Minions return to their setup positions, the king moves to the locked landing point, the same-radius circle is drawn there, and minions whose centers lie inside that circle turn red.
5. Result. The portrait expression follows the hit count: 0 with hands on the head, 1-3 crying, 4-6 blank, 7-9 sunglasses, and 10 with teeth showing. One comment is chosen from the band that matches the count. OK returns to the cover for another round.

Wi-Fi and Bluetooth are not initialized. The game does not write NVS. A merged image flashed from `0x0` can still reset the NVS region; see the [flashing policy](engineering/firmware-layout.md#flashing-and-stored-data).

The battery indicator is at the bottom-right so it does not cover the countdown. `bsp_battery_soc()` of `-1` draws an empty indicator and `--` instead of a percentage. On the cover, the backlight drops from 80 percent to 8 percent after 30 seconds without input and returns on the next key.

## Font

Montserrat 14 and 20 have no CJK glyphs. The game links `assets/fonts/axe_king_font_16.c` and `assets/fonts/axe_king_font_24.c`, a 4 bpp LVGL 9.5 subset with plain bitmaps and stride 0. `tools/gen_axe_font.py` rasterizes WenQuanYi Micro Hei (Apache-2.0 or GPL-3.0 with the font embedding exception). The source face is not committed. Regenerate with:

```bash
python3 tools/gen_axe_font.py --font /usr/share/fonts/truetype/wqy/wqy-microhei.ttc
```

`tests/test_axe_glyphs.py` requires every game string literal, including ASCII, to be present in both sizes, and requires U+9F98 to stay absent. Each label selects one of these fonts explicitly. A successful host check does not prove on-device rendering.

## Logic tests

`tests/test_axe_logic.c` covers circle containment (including the boundary), the clockwise-from-up jump, distance oscillation, score bands, comment prefixes, and round placement. It does not include ESP-IDF or LVGL headers.
