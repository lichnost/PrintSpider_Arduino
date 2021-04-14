#ifndef PRINTSPIDER_BUFFER_FILLER_H
#define PRINTSPIDER_BUFFER_FILLER_H

#include <stdint.h>

//For debugging: the amount of buffer memory the printspider_buffer_filler_fn function actually used
extern int printspider_mem_words_used;

enum printspider_buffer_filler_waveform_type_en {
	PRINTSPIDER_WAVEFORM_COLOR_A=0,		//old, duplicates lines on 2nd color cart
	PRINTSPIDER_WAVEFORM_COLOR_B,			//works on 2nd color cart
	PRINTSPIDER_WAVEFORM_BLACK_A,			//old, works on bw cart
	PRINTSPIDER_WAVEFORM_BLACK_B,			//new bw cart
};

//Select one of the above waveforms to be used.
void printspider_select_waveform(enum printspider_buffer_filler_waveform_type_en waveform_id);

/*
Meant as a callback from the I2S buffer filling code. Fills a buffer with the data from the queue passed as the argument,
or a 'blank' pattern if the buffer is empty.
*/
void printspider_buffer_filler_fn(void *buf, int len, void *arg);

#endif