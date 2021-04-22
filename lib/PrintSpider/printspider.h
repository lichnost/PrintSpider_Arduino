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

#ifndef PRINTSPIDER_H
#define PRINTSPIDER_H

#include <stdint.h>

/*
These are routines that manage the waveforms for firing all the nozzles of the cartridge once,
to essentially fire a 'row' of ink drops. You use this by allocating an uint8_t-sized buffer of
PRINTSPIDER_NOZDATA_SZ to contain the nozzle data. Clear it by setting all elements to 0, then 
use printspider_fire_nozzle_color or printspider_fire_nozzle_mono to enable firing a nozzle. 
Finally, feed the nozzle data to printspider_generate_waveform to generate the actual waveform that
needs to be sent to the cartridge.
*/

//Size of the nozzle data, in bytes
#define PRINTSPIDER_NOZDATA_SZ (14*3)

//Colors, for printspider_fire_nozzle_color
#define PRINTSPIDER_COLOR_C 0
#define PRINTSPIDER_COLOR_M 1
#define PRINTSPIDER_COLOR_Y 2

//In the color cartridge, there are three rows, one for each color. They're next to eachother, so we need to take care
//to grab the bits of image that actually are in the position of the nozzles.
#define PRINTSPIDER_COLOR_ROW_OFFSET 16

//The actual nozzles for the color cart start around y=14
#define PRINTSPIDER_COLOR_VERTICAL_OFFSET 14

//In the mono cartridge, there are two rows of nozzles, slightly offset (in the X direction) from the other.
#define PRINTSPIDER_BLACK_ROW_OFFSET 10

//Number of nozzles in each row of color cartridge
#define PRINTSPIDER_COLOR_NOZZLES_IN_ROW 84

//Number of nozzles in each row of mono cartridge
#define PRINTSPIDER_BLACK_NOZZLES_IN_ROW 168

enum printspider_waveform_type_en {
	PRINTSPIDER_WAVEFORM_COLOR_A = 0,		//old, duplicates lines on 2nd color cart
	PRINTSPIDER_WAVEFORM_COLOR_B = 1,			//works on 2nd color cart
	PRINTSPIDER_WAVEFORM_BLACK_A = 2,			//old, works on bw cart
	PRINTSPIDER_WAVEFORM_BLACK_B = 3			//new bw cart
};

/*
Structure to store the waveform data, so you can easily add others if needed.
*/
typedef struct printspider_waveform_desc_t {
	uint16_t *data;
	int len;
} printspider_waveform_desc_t;

/**
 * Get waveform descriptor by type.
 */
printspider_waveform_desc_t printspider_get_waveform(enum printspider_waveform_type_en type);

/*
In the nozzle data array `l`, this enables the `p`'th nozzle from the top of the row of nozzles with color 
`color` to fire.
*/
void printspider_set_nozzle_color(uint8_t *l, int p, int color);

/*
In the nozzle data array `l` , this function sets the enable bit for the `p`'th nozzle from the top 
of the inkjet nozzles in row `row`. The black cartridge has two rows, the 2nd one is slightly offset in the X 
direction and interleaved with the 1st (offset by half a nozzle). Note that the 2 first and last nozzles 
of each 168-nozzle row are not connected (giving a total of 324 nozzles in the combined two rows).
*/
void printspider_set_nozzle_black(uint8_t *l, int p, int row);

/*
Use the nozzle data in `nozdata` combined with the waveform template `tp` which has a length of `l` 16-bit 
elements, generate the waveform to send to the printer cartridge and put it in the buffer `w`. Returns the 
amount of elements used in buffer `w`. This return value will always be the same given a certain template; 
make sure `w` is sized accordingly.

Note that in the ESP32 implementation, this writes w in a fashion usable for sending through the parallel
I2S peripheral; if you use this on another controller, you may need to change the C code (specifically
the write_signals function).
*/
int printspider_generate_waveform(uint16_t *w, const uint16_t *tp, const uint8_t *nozdata, int l);

#endif