#ifndef RENDER_HEADER
#define RENDER_HEADER

#include <assert.h>
#include <stdio.h>
#include <zmq.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>

void setupRenderer(char*, bool, int);
void destroyRenderer();
void* blit(uint8_t*, int, int);

#endif
