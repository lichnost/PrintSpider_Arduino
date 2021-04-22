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

// This contains the routines to convert nozzle data to drive waveforms.
#include "printspider.h"

#include <stdio.h>
#include <string.h>

// To decode or change these waveforms, please use tools/waveform_editor.html
static uint16_t waveform_tpl_color_a[] = {
    0x0,    0xc001, 0xc101, 0xc142, 0x42,   0x4001, 0x4101, 0x4112, 0x12,
    0x4001, 0x4101, 0x4302, 0x602,  0x4401, 0x4501, 0x4522, 0x422,  0x402,
    0x4000, 0x4000, 0x4080, 0x880,  0x800,  0x800,  0x800,  0x800,  0x800};
static uint16_t waveform_tpl_color_b[] = {
    0x0,    0xc000, 0xc101, 0xc141, 0x42,   0x2,    0x4101, 0x4111, 0x12,
    0x4002, 0x4101, 0x4301, 0x602,  0x4402, 0x4501, 0x4521, 0x422,  0x402,
    0x4000, 0x4000, 0x4080, 0x880,  0x800,  0x800,  0x800,  0x800,  0x800};
static uint16_t waveform_tpl_black_a[] = {
    0x0,    0xc001, 0xc101, 0xc142, 0x42,  0xc001, 0xc101, 0xc112, 0x12,
    0x4001, 0x4101, 0x4302, 0x602,  0x401, 0x501,  0x522,  0x422,  0x402,
    0x4000, 0x4000, 0x4080, 0x880,  0x800, 0x800,  0x800,  0x800,  0x800};
static uint16_t waveform_tpl_black_b[] = {
    0x0,    0xc000, 0xc101, 0xc141, 0x42,  0x2,   0xc101, 0xc111, 0x12,
    0x4002, 0x4101, 0x4301, 0x602,  0x402, 0x501, 0x521,  0x422,  0x402,
    0x4002, 0x4000, 0x4080, 0x880,  0x800, 0x800, 0x800,  0x800,  0x800};

const printspider_waveform_desc_t printspider_waveforms[] = {
    {waveform_tpl_color_a, sizeof(waveform_tpl_color_a) / 2},
    {waveform_tpl_color_b, sizeof(waveform_tpl_color_b) / 2},
    {waveform_tpl_black_a, sizeof(waveform_tpl_black_a) / 2},
    {waveform_tpl_black_b, sizeof(waveform_tpl_black_b) / 2},
};

/**
 * Get waveform descriptor by type.
 */
printspider_waveform_desc_t printspider_get_waveform(
    enum printspider_waveform_type_en type) {
    return printspider_waveforms[type];
}

void printspider_set_nozzle_color(uint8_t *l, int p, int color) {
    // Byte order for the three colors. Note that these arrays are
    // just shifted versions of eachother.
    const int bo[3][14] = {{8, 13, 4, 9, 0, 5, 10, 1, 6, 11, 2, 7, 12, 3},
                           {11, 2, 7, 12, 3, 8, 13, 4, 9, 0, 5, 10, 1, 6},
                           {0, 5, 10, 1, 6, 11, 2, 7, 12, 3, 8, 13, 4, 9}};
    if (p > (8 * 14) || p < 0) return;
    int byteno = bo[color][p % 14];
    int bitno = p / 14;
    l[byteno + (14 * color)] |= (1 << bitno);
}

// Nozzle order data for the black cartridge.
typedef struct {
    int c;
    int bit;
    int order;
} bw_nozinfo_t;

const bw_nozinfo_t ni[] = {
    {2, 0, 1}, {2, 1, 1}, {1, 0, 1}, {1, 1, 1}, {0, 0, 1}, {0, 1, 1},
    {2, 4, 1}, {2, 5, 1}, {1, 4, 1}, {1, 5, 1}, {0, 4, 1}, {0, 5, 1},
    {2, 2, 0}, {2, 3, 0}, {1, 2, 0}, {1, 3, 0}, {0, 2, 0}, {0, 3, 0},
    {2, 6, 0}, {2, 7, 0}, {1, 6, 0}, {1, 7, 0}, {0, 6, 0}, {0, 7, 0},
};

// In a set of bits representing the bits being shifted out to the cartridge,
// this function sets the enable bit for the p'th nozzle from the top of the
// inkjet nozzles. The black cartridge has two rows, the 2nd one is slightly
// offset in the X direction and interleaved with the 1st (offset by half a
// nozzle). Note that the 2 first and last nozzles of each 168-nozzle row are
// not connected (giving a total of 324 nozzles in the combined two rows).
void printspider_set_nozzle_black(uint8_t *l, int p, int row) {
    if (row) p += 168;
    int j = p / 14;
    int k = 13 - (p % 14);

    const int bo[2][14] = {
        {4, 12, 10, 2, 8, 0, 6, 13, 7, 1, 9, 3, 11, 5},
        {13, 7, 1, 9, 3, 11, 5, 4, 12, 10, 2, 8, 0, 6},
    };

    l[ni[j].c * 14 + bo[ni[j].order][k]] |= (1 << ni[j].bit);
}

// Function to set a value in the output buffer.
static inline __attribute__((always_inline)) void set_signals(uint16_t *buf,
                                                              int pos,
                                                              uint16_t val) {
    buf[pos] = val;
}

// The masks for each bit in an 16-bit word of template data
#define TP_BIT_TOGGLE1 (1 << 0)
#define TP_BIT_TOGGLE2 (1 << 1)
#define TP_S2 (1 << 4)
#define TP_S4 (1 << 5)
#define TP_S1 (1 << 6)
#define TP_S5 (1 << 7)
#define TP_DCLK (1 << 8)
#define TP_S3 (1 << 9)
#define TP_F3 (1 << 10)
#define TP_F5 (1 << 11)
#define TP_CSYNC_LAST (1 << 14)
#define TP_CSYNC_NORM (1 << 15)

#define OUT_D1 (1 << 0)
#define OUT_D2 (1 << 1)
#define OUT_D3 (1 << 2)
#define OUT_CSYNC (1 << 3)
#define OUT_S2 (1 << 4)
#define OUT_S4 (1 << 5)
#define OUT_S1 (1 << 6)
#define OUT_S5 (1 << 7)
#define OUT_DCLK (1 << 8)
#define OUT_S3 (1 << 9)
#define OUT_F3 (1 << 10)
#define OUT_F5 (1 << 11)

// This takes a pointer to a buffer of words to send out to the cartridge after
// eachother. It then uses a template (tp) with length l to generate the base
// signals to control the inkjet. It then uses the bitstreams in nozdata to add
// in the image data into the buffer. It finally returns the length written.
int printspider_generate_waveform(uint16_t *w, const uint16_t *tp, const uint8_t *nozdata, int l) {
    const uint8_t *d1data = &nozdata[0];
    const uint8_t *d2data = &nozdata[14];
    const uint8_t *d3data = &nozdata[28];
    int p = 0;

    // See if we actually need to output anything
    int is_empty = 1;
    for (int i = 0; i < PRINTSPIDER_NOZDATA_SZ; i++) {
        if (nozdata[i] != 0) {
            is_empty = 0;
            break;
        }
    }

    // Generate the 14 data packets
    for (int j = 0; j < 14; j++) {
        uint8_t bit = 0;
        int ov = 0;
        // We need to mask out some bits in the template, because their data is
        // generated here.
        uint16_t mask = 0xffff;
        mask &= ~(OUT_CSYNC | OUT_D1 | OUT_D2 | OUT_D3);  // Data and csync
        // mask out power lines if not needed
        int power = (d1data[j] | d2data[j] | d3data[j]);
        if ((power & 0x0f) == 0x00) mask &= ~OUT_F3;
        if ((power & 0xf0) == 0x00) mask &= ~OUT_F5;
        // Depending on which packet we're generating, we pick a different csync
        // line.
        uint16_t csync_sel = 0;
        if (j == 13) {  // last one
            csync_sel = TP_CSYNC_LAST;
        } else {
            csync_sel = TP_CSYNC_NORM;
        }

        for (int i = 0; i < l; i++) {
            if (is_empty) {
                // no need to output anything
                set_signals(w, p++, 0);
                continue;
            }
            uint16_t v = tp[i];
            // Increase bitno if data toggle lines toggle
            if ((v & (TP_BIT_TOGGLE1 | TP_BIT_TOGGLE2)) != ov) {
                if (!bit) {
                    bit = 1;  // first bit
                } else {
                    bit <<= 1;  // next bit
                }
                ov = v & (TP_BIT_TOGGLE1 | TP_BIT_TOGGLE2);
            }
            int en = v & (TP_BIT_TOGGLE1 |
                          TP_BIT_TOGGLE2);  // only send out bit if data toggle
                                            // lines are non-zero

            v &= mask;  // We got what we needed from the template. Mask off all
                        // generated bits.

            // Set bit values for [d1, d2, d3] streams
            if (en) {
                if (!(d1data[j] & bit)) v |= OUT_D1;
                if (!(d2data[j] & bit)) v |= OUT_D2;
                if (!(d3data[j] & bit)) v |= OUT_D3;
            }
            // Select correct value for seg ena
            if (v & csync_sel) v |= OUT_CSYNC;
            set_signals(w, p++, v);
        }
        p += 8;  // idle time after data
    }
    // Always return an even amount of words
    return (p + 1) & ~1;
}
