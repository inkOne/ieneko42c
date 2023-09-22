/* Copyright 2021 sekigon-gonnoc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include QMK_KEYBOARD_H
#include "mtch6102.h"
#include "iqs5xx.h"
#include "pointing_device.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    {{KC_BTN1, KC_BTN2, KC_BTN3, KC_BTN4, KC_BTN5, MO(1)}},
    {{_______, _______, _______, _______, _______, _______}},
};

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case KC_BTN1 ... KC_BTN5:
            if (record->event.pressed) {
                pointing_device_set_button(1 << (keycode - KC_BTN1));
            } else {
                pointing_device_clear_button(1 << (keycode - KC_BTN1));
            }
            return false;
            break;
        default:
            break;
    }
    return true;
}
void matrix_scan_user() {
    // Change cursor movement to scroll movement if layer is 1
    if (layer_state_is(1)) {
        report_mouse_t mouse_rep = pointing_device_get_report();
        if (mouse_rep.x != 0) {
            mouse_rep.h = mouse_rep.x > 0 ? 1 : -1;
            mouse_rep.x = 0;
        }

        if (mouse_rep.y != 0) {
            mouse_rep.v = mouse_rep.y > 0 ? -1 : 1;
            mouse_rep.y = 0;
        }

        pointing_device_set_report(mouse_rep);
    }
}
