/*
 * Blink
 * Turns on an LED on for one second,
 * then off for one second, repeatedly.
 */
#include "Arduino.h"

#include "Arduino_FreeRTOS.h"
#include "queue.h"

#include "printspider_genwaveform.h"
#include "printspider_buffer_filler.h"

//Queue for nozzle data
QueueHandle_t nozdata_queue;

static bool image_color = true;

//Selecting printsider waveform.
static int select_waveform() {
    if (image_color) {
      printf("Setting waveform PRINTSPIDER_WAVEFORM_COLOR_B");
    	printspider_select_waveform(PRINTSPIDER_WAVEFORM_COLOR_B);
    } else {
      printf("Setting waveform PRINTSPIDER_WAVEFORM_BLACK_B");
      printspider_select_waveform(PRINTSPIDER_WAVEFORM_BLACK_B);
    }
    return 1;
}

void send_image_row_color(int pos) {
	uint8_t nozdata[PRINTSPIDER_NOZDATA_SZ];
	memset(nozdata, 0, PRINTSPIDER_NOZDATA_SZ);
	for (int c=0; c<3; c++) {
		for (int y=0; y<PRINTSPIDER_COLOR_NOZZLES_IN_ROW; y++) {
			uint8_t v=image_get_pixel(pos-c*PRINTSPIDER_COLOR_ROW_OFFSET, y*2, c);
			//Note the v returned is 0 for black, 255 for the color. We need to invert that here as we're printing on
			//white.
			v=255-v;
			//Random-dither. The chance of the nozzle firing is equal to (v/256).
			if (v>(rand()&255)) {
				//Note: The actual nozzles for the color cart start around y=14
				printspider_fire_nozzle_color(nozdata, y+PRINTSPIDER_COLOR_VERTICAL_OFFSET, c);
			}
		}
	}
	//Send nozzle data to queue so ISR can pick up on it.
	xQueueSend(nozdata_queue, nozdata, portMAX_DELAY);
}

void send_image_row_black(int pos) {
	uint8_t nozdata[PRINTSPIDER_NOZDATA_SZ];
	memset(nozdata, 0, PRINTSPIDER_NOZDATA_SZ);
	for (int row=0; row<2; row++) {
		for (int y=0; y<PRINTSPIDER_BLACK_NOZZLES_IN_ROW; y++) {
			//We take anything but white in any color channel of the image to mean we want black there.
			if (image_get_pixel(pos+row*PRINTSPIDER_BLACK_ROW_OFFSET, y, 0)!=0xff ||
				image_get_pixel(pos+row*PRINTSPIDER_BLACK_ROW_OFFSET, y, 1)!=0xff ||
				image_get_pixel(pos+row*PRINTSPIDER_BLACK_ROW_OFFSET, y, 2)!=0xff) {
				//Random-dither 50%, as firing all nozzles is a bit hard on the power supply.
				if (rand()&1) {
					printspider_fire_nozzle_black(nozdata, y, row);
				}
			}
		}
	}
	//Send nozzle data to queue so ISR can pick up on it.
	xQueueSend(nozdata_queue, nozdata, portMAX_DELAY);
}

void setup()
{
  // Set up serial port and wait until connected
  Serial.begin(9600);
  while(!Serial && !Serial.available()){}

  //Create nozzle data queue
	nozdata_queue=xQueueCreate(1, PRINTSPIDER_NOZDATA_SZ);

  select_waveform();
}

void loop()
{
  if (image_color) {
	  send_image_row_color(0);
  } else {
	  send_image_row_black(0);
  }
}