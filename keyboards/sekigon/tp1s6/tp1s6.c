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

#include "tp1s6.h"

#include "i2c_master.h"
#include "pointing_device.h"
#include "debug.h"

#include "mtch6102.h"
#include "iqs5xx.h"
#include "host.h"

#define TP_TYPE_MTCH6102 0
#define TP_TYPE_IQS5XX 1
#define TP_TYPE_IQS5XX_BOOTLOADER 2

#ifndef TP_TYPE
#define TP_TYPE TP_TYPE_MTCH6102
#endif

static void dummy_func(uint8_t btn){};
void (*pointing_device_set_button)(uint8_t btn) = dummy_func;
void (*pointing_device_clear_button)(uint8_t btn) = dummy_func;

bool mouse_send_flag = false;
void pointing_device_task(void) {
    if (mouse_send_flag) {
        pointing_device_send();
        mouse_send_flag = false;
    }
}

void keyboard_pre_init_kb() {
    i2c_init();

    keyboard_pre_init_user();
}

void keyboard_post_init_kb() {
    debug_enable = true;
    debug_mouse  = true;
    debug_matrix = true;

    setPinOutput(F4);
    writePinHigh(F4);

    wait_ms(200);

#if TP_TYPE == TP_TYPE_MTCH6102
    init_mtch6102();
    pointing_device_set_button = pointing_device_set_button_mtch6102;
    pointing_device_clear_button = pointing_device_clear_button_mtch6102;
#elif TP_TYPE == TP_TYPE_IQS5XX
    check_iqs5xx();
    init_iqs5xx();
    pointing_device_set_button = pointing_device_set_button_iqs5xx;
    pointing_device_clear_button = pointing_device_clear_button_iqs5xx;
#endif

    keyboard_post_init_user();
}

#if TP_TYPE == TP_TYPE_MTCH6102

void matrix_scan_kb() {
    static int      cnt = 0;
    static uint16_t last_read_time;
    mtch6102_data_t mtch6102_data;
    report_mouse_t  mouse_rep = {0};
    bool            is_valid  = false;

    // read mtch6102 data every 15ms
    if (timer_elapsed(last_read_time) > 15) {
        last_read_time = timer_read();
        is_valid       = read_mtch6102(&mtch6102_data);
    }

    if (is_valid) {
        bool send_flag = process_mtch6102(&mtch6102_data, &mouse_rep);

        if (send_flag) {
            mouse_send_flag = true;
            pointing_device_set_report(mouse_rep);
        }

        if (++cnt % 10 == 0) {
            if (debug_mouse) {
                dprintf("0x%02X 0x%02X %d %d\n", mtch6102_data.status, mtch6102_data.gesture, mtch6102_data.x, mtch6102_data.y);
            }
        }
    }

    matrix_scan_user();
}

#elif TP_TYPE == TP_TYPE_IQS5XX
void matrix_scan_kb() {
    static int cnt = 0;
    iqs5xx_data_t iqs5xx_data;
    report_mouse_t mouse_rep = {0};
    bool is_valid = false;

    is_valid = read_iqs5xx(&iqs5xx_data);

    if (is_valid) {
        static iqs5xx_processed_data_t iqs5xx_processed_data;
        static iqs5xx_gesture_data_t iqs5xx_gesture_data;
        bool send_flag = process_iqs5xx(&iqs5xx_data, &iqs5xx_processed_data, &mouse_rep, &iqs5xx_gesture_data);

        switch (iqs5xx_gesture_data.two.gesture_state) {
            case GESTURE_SWIPE_U:
                mouse_rep.v = -2;
                send_flag = true;
                break;
            case GESTURE_SWIPE_D:
                mouse_rep.v = 2;
                send_flag = true;
                break;
            case GESTURE_SWIPE_R:
                mouse_rep.h = 2;
                send_flag = true;
                break;
            case GESTURE_SWIPE_L:
                mouse_rep.h = -2;
                send_flag = true;
                break;
            case GESTURE_PINCH_IN:
                tap_code16(LCTL(KC_EQL));
                break;
            case GESTURE_PINCH_OUT:
                tap_code16(LCTL(KC_MINS));
                break;
            default:
                break;
        }

        if (send_flag) {
            mouse_send_flag = true;
            pointing_device_set_report(mouse_rep);
        }

        if (++cnt % 10 == 0) {
            if (debug_mouse) {
                dprintf("%2d %2d %3d %3d %3d %3d %6u %4d %4d\n",
                iqs5xx_gesture_data.two.gesture_state,
                iqs5xx_data.finger_cnt,
                iqs5xx_data.fingers[0].current.x, iqs5xx_data.fingers[0].current.y,
                iqs5xx_data.fingers[1].current.x, iqs5xx_data.fingers[1].current.y,
                iqs5xx_gesture_data.two.dist_sq, iqs5xx_gesture_data.two.dot, iqs5xx_gesture_data.two.dot_rel);
            }
        }
    }

    matrix_scan_user();
}
#endif
