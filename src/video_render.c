/*****************************************************************************
 * video_renderer.c: network communication and rendering for the Mensadisplay vlc-plugin 
 *****************************************************************************
 * Copyright (c) 2014 Malte Kießling 
 *
 * Authors: Malte Kießling <mkalte666@stratum0.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "video_render.h"

#define WIDTH 480
#define HEIGHT 70

#define CMD_PIXEL 0
#define CMD_BLIT 1

struct msgBlit {
	uint8_t cmd;
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
	uint8_t data[WIDTH * HEIGHT];
} __attribute__ ((packed));

//Stolen from: http://rosettacode.org/wiki/Bilinear_interpolation
float lerp(float s, float e, float t){return s+(e-s)*t;}

float blerp(float c00, float c10, float c01, float c11, float tx, float ty){
	    return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
}

//Blit was stolen from the sidescroller-blarh and modified


struct msgBlit *screen = NULL;
void *context;
void *requester;

int Blackvalue = 16;
bool shouldCalcBlackvalue = false;
bool isInverted = false;

static int counter;

void setupRenderer(char* url, bool dynamic, int value, bool inverted) {
	screen = (struct msgBlit*) malloc(sizeof(struct msgBlit));
	screen->x = 0;
	screen->y = 0;
	screen->w = WIDTH;
	screen->h = HEIGHT;
	screen->cmd = CMD_BLIT;
	void *context = zmq_ctx_new ();
	requester = zmq_socket (context, ZMQ_REQ);
	zmq_connect(requester, url);
	Blackvalue = value;
	shouldCalcBlackvalue = dynamic;
	if (shouldCalcBlackvalue == true) {
		printf("Dynamic averge calculation is active!\n");
	}
	printf("Setting up Mensadisplay renderer complete. Using %i as blackvalue (basevalue if dynamic mode is active)\n", Blackvalue);
	isInverted = inverted;
}
	
void destroyRenderer() {
	free(screen);
	zmq_close(requester);
	zmq_ctx_destroy(context);
}

void* blit(uint8_t*pixels, int w, int h) {
	//First, in pixels we have an rgba array. we want greyscale. So make greyscale
	//in avPixels the image will be stored
	char* avPixels = (char*)malloc(sizeof(char[w*h]));
	//float av = 0;
	for(int i = 0; i < w*h*4; i+=4) {
		/* REC 709 */
//		avPixels[i/4] = ((18*pixels[i]+183*pixels[i+1]+54*pixels[i+2])/255);
		avPixels[i/4] = ((29*pixels[i]+150*pixels[i+1]+77*pixels[i+2])/256);
	}

	//The resolution of the Mensadisplay is WIDTH*HEIGHT, but the size of pixels is not - cause random.
	//used in case the value for black is calculated dynamicly
	//we'll use bilinear interpolation
	int x = 0;
	int y = 0;
	for (; y < HEIGHT; x++) {
		if(x > WIDTH) {
			x = 0;
			y++;
		}
		float gx = x/(float)(WIDTH) * (w-1);
		float gy = y/(float)(HEIGHT) * (h-1);
		int gxi = (int)gx;
		int gyi = (int)gy;
		uint8_t c00 = avPixels[gxi+gyi*w];
		uint8_t c10 = avPixels[gxi+1+gyi*w];
		uint8_t c01 = avPixels[gxi+(gyi+1)*w];
		uint8_t c11 = avPixels[gxi+1+(gyi+1)*w];
		
		uint8_t result = (uint8_t)blerp((float)c00, (float)c10, (float)c01, (float)c11, gx-gxi, gy-gyi);
		if (result > Blackvalue)
			screen->data[x+y*WIDTH] = (result - Blackvalue) * 255 / (255 - Blackvalue);
		else
			screen->data[x+y*WIDTH] = 0;
	}
	
	//Send the data to the display and then a empty package

	zmq_msg_t msg;
	zmq_msg_init_size(&msg, sizeof(struct msgBlit));

	memcpy(zmq_msg_data(&msg), (struct msgBlit*)(screen), sizeof(struct msgBlit));
	zmq_msg_send(&msg, requester, ZMQ_SNDMORE);
	zmq_msg_init_size(&msg, 0);
	zmq_msg_send(&msg, requester, 0);
	zmq_recv(requester, NULL, 0, 0);

	//Free all the memory!
	free(avPixels);
	return NULL;
}

