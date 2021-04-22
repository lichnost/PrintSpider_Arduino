/*
Copyright 2021 Pavel Semenov

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <stdio.h>

#include "Arduino.h"
#include "printspider.h"

#define WAVEFORM_LEN 600

// GPIO numbers for the lines that are connected (via level converters) to the
// printer cartridge.
#define PIN_NUM_CART_D1 0
#define PIN_NUM_CART_D2 1
#define PIN_NUM_CART_D3 2
#define PIN_NUM_CART_CSYNC 3
#define PIN_NUM_CART_S1 4
#define PIN_NUM_CART_S2 5
#define PIN_NUM_CART_S3 6
#define PIN_NUM_CART_S4 7
#define PIN_NUM_CART_S5 8
#define PIN_NUM_CART_DCLK 9
#define PIN_NUM_CART_F3 10
#define PIN_NUM_CART_F5 11

// Output settings.
static uint8_t gpio_bus[] = {
    PIN_NUM_CART_D1,     // 0
    PIN_NUM_CART_D2,     // 1
    PIN_NUM_CART_D3,     // 2
    PIN_NUM_CART_CSYNC,  // 3
    PIN_NUM_CART_S2,     // 4
    PIN_NUM_CART_S4,     // 5
    PIN_NUM_CART_S1,     // 6
    PIN_NUM_CART_S5,     // 7
    PIN_NUM_CART_DCLK,   // 8
    PIN_NUM_CART_S3,     // 9
    PIN_NUM_CART_F3,     // 10
    PIN_NUM_CART_F5      // 11
};

/**
 * Each array row is the row of pixels from image offsetted from each other by
 * PRINTSPIDER_COLOR_ROW_OFFSET. Number of elementsof each row is
 * PRINTSPIDER_COLOR_NOZZLES_IN_ROW. Each row consist of only one color channel.
 * Each pixel element is one byte value.
 */
typedef struct color_image_part_t {
    // Cayan color channel. First image row.
    const uint8_t *cayan_row;
    // Magenta color channel. This row should be taken from image by offsetting
    // y position from first row position by PRINTSPIDER_COLOR_ROW_OFFSET.
    const uint8_t *magenta_row;
    // Yellow color channel. This row should be taken from image by offsetting y
    // position from second row position by PRINTSPIDER_COLOR_ROW_OFFSET.
    const uint8_t *yellow_row;
} color_image_part_t;

/**
 * Each array row is the row of pixels from image offsetted from each other by
 * PRINTSPIDER_BLACK_ROW_OFFSET. Number of elementsof each row is
 * PRINTSPIDER_BLACK_NOZZLES_IN_ROW. Pixel element is one byte grayscale value.
 */
typedef struct black_image_part_t {
    // First image row.
    const uint8_t *first_row;
    // Second image row. This row should be taken from image by offsetting y
    // position from first row position by PRINTSPIDER_BLACK_ROW_OFFSET.
    const uint8_t *second_row;
} black_image_part_t;

static printspider_waveform_desc_t selected_waveform;

void digitalWriteFast(uint8_t pin, uint8_t x) {
    if (pin / 8) {  // pin >= 8
        PORTB ^= (-x ^ PORTB) & (1 << (pin % 8));
    } else {
        PORTD ^= (-x ^ PORTD) & (1 << (pin % 8));
    }
}

void out_to_pins(uint16_t *buffer, int len) {
    for (int i = 0; i < len; i++) {
        for (int p = 0; p < sizeof(gpio_bus); p++) {
            digitalWrite(gpio_bus[p], buffer[i] >> p & 1);
        }
    }
    // delayMicroseconds(10);
}

/**
 * Send nozzle data to output pins.
 */
void send_nozdata_out(uint8_t *nozdata) {
    static uint16_t waveform_buffer[WAVEFORM_LEN];
    memset(waveform_buffer, 0, sizeof(uint16_t) * WAVEFORM_LEN);
    int generated_len =
        printspider_generate_waveform(waveform_buffer, selected_waveform.data,
                                      nozdata, selected_waveform.len);
    out_to_pins(waveform_buffer, generated_len);
}

/**
 * Sends image data to cartridge.
 * @param color_image_part image data part.
 */
void send_image_row_color(color_image_part_t *color_image_part) {
    uint8_t nozdata[PRINTSPIDER_NOZDATA_SZ];
    memset(nozdata, 0, sizeof(nozdata));
    for (int c = 0; c < 3; c++) {
        for (int y = 0; y < PRINTSPIDER_COLOR_NOZZLES_IN_ROW; y++) {
            uint8_t v;
            if (c == 0) {
                v = color_image_part->cayan_row[y];
            } else if (c == 1) {
                v = color_image_part->magenta_row[y];
            } else if (c == 2) {
                v = color_image_part->yellow_row[y];
            }
            // Note the v returned is 0 for black, 255 for the color. We need to
            // invert that here as we're printing on white.
            v = 255 - v;
            // Random-dither. The chance of the nozzle firing is equal to
            // (v/256).
            if (v > (rand() & 255)) {
                // Note: The actual nozzles for the color cart start around y=14
                printspider_set_nozzle_color(
                    nozdata, y + PRINTSPIDER_COLOR_VERTICAL_OFFSET, c);
            }
        }
    }
    // Send nozzle data.
    send_nozdata_out(nozdata);
}

/**
 * Sends black image data to cartridge.
 * @param black_image_part image data array.
 */
void send_image_row_black(black_image_part_t *black_image_part) {
    uint8_t nozdata[PRINTSPIDER_NOZDATA_SZ];
    memset(nozdata, 0, sizeof(nozdata));
    for (int row = 0; row < 2; row++) {
        for (int y = 0; y < PRINTSPIDER_BLACK_NOZZLES_IN_ROW; y++) {
            // We take anything but white in any color channel of the image to
            // mean we want black there.
            uint8_t color;
            if (row == 0) {
                color = black_image_part->first_row[y];
            } else if (row == 1) {
                color = black_image_part->second_row[y];
            }
            if (color != 0xff) {
                // Random-dither 50%, as firing all nozzles is a bit hard on the
                // power supply.
                if (rand() & 1) {
                    printspider_set_nozzle_black(nozdata, y, row);
                }
            }
        }
    }
    // Send nozzle data.
    send_nozdata_out(nozdata);
}

void setup_gpio() {
    for (int i = 0; i < 12; i++) {
        int pin = gpio_bus[i];
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
}

// #define PRINT_COLOR

#ifdef PRINT_COLOR
static const uint8_t cayan_row_value[] = {
    255, 252, 249, 246, 243, 240, 237, 234, 231, 228, 225, 222, 219, 216,
    213, 210, 207, 204, 201, 198, 195, 192, 189, 186, 183, 180, 177, 174,
    171, 168, 165, 162, 159, 156, 153, 150, 147, 144, 141, 138, 135, 132,
    129, 126, 123, 120, 117, 114, 111, 108, 105, 102, 99,  96,  93,  90,
    87,  84,  81,  78,  75,  72,  69,  66,  63,  60,  57,  54,  51,  48,
    45,  42,  39,  36,  33,  30,  27,  24,  21,  18,  15,  12,  9,   6};
static const uint8_t magenta_row_value[] = {
    1,   4,   7,   10,  13,  16,  19,  22,  25,  28,  31,  34,  37,  40,
    43,  46,  49,  52,  55,  58,  61,  64,  67,  70,  73,  76,  79,  82,
    85,  88,  91,  94,  97,  100, 103, 106, 109, 112, 115, 118, 121, 124,
    127, 130, 133, 136, 139, 142, 145, 148, 151, 154, 157, 160, 163, 166,
    169, 172, 175, 178, 181, 184, 187, 190, 193, 196, 199, 202, 205, 208,
    211, 214, 217, 220, 223, 226, 229, 232, 235, 238, 241, 244, 247, 250};
static const uint8_t yellow_row_value[] = {
    255, 252, 249, 246, 243, 240, 237, 234, 231, 228, 225, 222, 219, 216,
    213, 210, 207, 204, 201, 198, 195, 192, 189, 186, 183, 180, 177, 174,
    171, 168, 165, 162, 159, 156, 153, 150, 147, 144, 141, 138, 135, 132,
    129, 126, 123, 120, 117, 114, 111, 108, 105, 102, 99,  96,  93,  90,
    87,  84,  81,  78,  75,  72,  69,  66,  63,  60,  57,  54,  51,  48,
    45,  42,  39,  36,  33,  30,  27,  24,  21,  18,  15,  12,  9,   6};
#else
static const uint8_t first_row_value[] = {
    1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,
    29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
    43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
    57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,
    71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,
    85,  87,  89,  91,  93,  95,  97,  99,  101, 103, 105, 107, 109, 111,
    113, 115, 117, 119, 121, 123, 125, 127, 129, 131, 133, 135, 137, 139,
    141, 143, 145, 147, 149, 151, 153, 155, 157, 159, 161, 163, 165, 167,
    169, 171, 173, 175, 177, 179, 181, 183, 185, 187, 189, 191, 193, 195,
    197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221, 223,
    225, 227, 229, 231, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251};
static const uint8_t second_row_value[] = {
    1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,
    29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
    43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
    57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,
    71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,
    85,  87,  89,  91,  93,  95,  97,  99,  101, 103, 105, 107, 109, 111,
    113, 115, 117, 119, 121, 123, 125, 127, 129, 131, 133, 135, 137, 139,
    141, 143, 145, 147, 149, 151, 153, 155, 157, 159, 161, 163, 165, 167,
    169, 171, 173, 175, 177, 179, 181, 183, 185, 187, 189, 191, 193, 195,
    197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221, 223,
    225, 227, 229, 231, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251};
#endif

// Selecting printsider waveform.
static void select_waveform() {
#ifdef PRINT_COLOR
    selected_waveform = printspider_get_waveform(PRINTSPIDER_WAVEFORM_COLOR_B);
#else
    selected_waveform = printspider_get_waveform(PRINTSPIDER_WAVEFORM_BLACK_B);
#endif
}

void print() {
#ifdef PRINT_COLOR
    color_image_part_t color_part;
    color_image_part_t *color_part_adr = &color_part;
    color_part_adr->cayan_row = cayan_row_value;
    color_part_adr->magenta_row = magenta_row_value;
    color_part_adr->yellow_row = yellow_row_value;
    send_image_row_color(color_part_adr);
#else
    black_image_part_t black_part;
    black_image_part_t *black_part_adr = &black_part;
    black_part_adr->first_row = first_row_value;
    black_part_adr->second_row = second_row_value;
    send_image_row_black(black_part_adr);
#endif
}

void setup() {
    setup_gpio();
    select_waveform();

    print();
}

void loop() {
    // Printing executes only once at start in setup method.
}