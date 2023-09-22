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
#include <string.h>

#include "pointing_device.h"
#include "virtser.h"
#include "iqs5xx.h"
#include "i2c_master.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {{{KC_NO}}};

#define SERIAL_BUFFER_LEN 256

#define READ_CMD 0x01
#define CRC_CMD 0x03
#define WRITE_CMD 0x04

#define BL_SUCCESS 0x00
#define ERROR_BL_RECEIVE 0x01
#define ERROR_BL_ACTIVATION 0x02

enum {
    SLIP_END     = 0xC0,
    SLIP_ESC     = 0xDB,
    SLIP_ESC_END = 0xDC,
    SLIP_ESC_ESC = 0xDD,
};

bool bl_enabled = false;

void exe_bl_command(uint8_t const *cmd, uint8_t len) {
    if (cmd[1] != len) {
        dprintf("Invalid command len:%d\n", len);
    }

    if (!bl_enabled) {
        dprintf("bl not enabled\n");
        virtser_send(ERROR_BL_ACTIVATION);
        return;
    }

    uint16_t addr = (cmd[2] << 8) | cmd[3];
    uint8_t  read_block[FIRM_BLOCK_SIZE];

    // dprintf("cmd:%d, len:%d, rcmd:0x%X\n", cmd[0], cmd[1], addr);

    switch (cmd[0]) {
        case READ_CMD:
            // dprintf("rcmd:0x%X\n", addr);
            read_firmware_block_iqs5xx(addr, read_block);
            virtser_send(BL_SUCCESS);
            for (int idx = 0; idx < FIRM_BLOCK_SIZE; idx++) {
                virtser_send(read_block[idx]);
            }

            break;

        case CRC_CMD:
            virtser_send(crc_check_iqs5xx());
            break;

        case WRITE_CMD:
            // dprintf("wcmd:0x%X\n", addr);
            write_firmware_block_iqs5xx(addr, &cmd[2]);
            wait_ms(7);
            virtser_send(BL_SUCCESS);
            break;

        default:
            break;
    }
}

void virtser_recv(uint8_t b) {
    static uint8_t  buf[SERIAL_BUFFER_LEN];  // serial buffer
    static uint16_t widx     = 0;            // write index
    static bool     escaped  = false;        // escape flag
    static bool     overflow = false;        // overflow flag

    bool receive_complete = false;

    dprintf("%02X ", b);

    // process SLIP
    if (b == SLIP_END) {
        // dprintf("\nDetect END signal\n");
        if (overflow) {
            // reset receive buffer
            overflow         = false;
            receive_complete = false;
            widx             = 0;
            escaped          = false;
            memset(buf, 0, sizeof(buf));
        } else {
            receive_complete = true;
        }
    } else if (b == SLIP_ESC) {
        escaped = true;
    } else if (widx < sizeof(buf)) {
        if (escaped) {
            if (b == SLIP_ESC_END) {
                buf[widx] = SLIP_END;
            } else if (b == SLIP_ESC_ESC) {
                buf[widx] = SLIP_ESC;
            } else {
                buf[widx] = b;
            }
            escaped = false;
        } else {
            buf[widx] = b;
        }

        widx++;
        if (widx > sizeof(buf)) {
            dprintf("Buffer overflow\n");
            overflow = true;
            widx     = 0;
        }
    }

    if (receive_complete) {
        exe_bl_command(buf, widx);

        receive_complete = false;
        widx             = 0;
        escaped          = false;
        overflow         = false;
        memset(buf, 0, sizeof(buf));
    }
}

void matrix_setup(void) {
    i2c_init();

    // power off
    setPinOutput(F4);
    writePinLow(F4);
    wait_us(100);
    // power on
    writePinHigh(F4);
    wait_us(500);

    read_bootloader_version_iqs5xx();

    for (int cnt = 0; cnt < 100; cnt++) {
        uint16_t ver = read_bootloader_version_iqs5xx();
        if (ver == 0x200) {
            bl_enabled = true;
            break;
        }

        wake_bootloader_iqs5xx();
    }
}
