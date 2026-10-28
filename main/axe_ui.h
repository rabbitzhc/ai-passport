// Axe King screens. Call axe_ui_start and axe_ui_on_key while holding the LVGL lock.
// The LVGL timer callback already runs under that lock and must not take it again.
#pragma once

#include <stdbool.h>
#include <stdint.h>

void axe_ui_start(uint32_t rng_seed, bool battery_ready);
void axe_ui_on_key(int button, int event);
int axe_ui_phase(void);
