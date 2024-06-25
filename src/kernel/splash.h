#pragma once

#include "defs.h"

#include <sys/gfx.h>

#include <common/boot_info.h>

#define SPLASH_WIDTH 400
#define SPLASH_HEIGHT 500
#define SPLASH_SHADOW_OFFSET 1

#define SPLASH_NAME_SCALE 3
#define SPLASH_NAME_OFFSET -150

#define SPLASH_VERSION_SCALE 2
#define SPLASH_VERSION_OFFSET (SPLASH_NAME_OFFSET + 50)

#define SPLASH_MESSAGE_SCALE 2
#define SPLASH_MESSAGE_OFFSET (100)

#define SPLASH_LICENCE "Licensed under GPLv3"
#define SPLASH_LICENCE2 "www.gnu.org/licenses/gpl-3.0.html"
#define SPLASH_LICENCE_SCALE 1
#define SPLASH_LICENCE_OFFSET (175)

void splash_init(gop_buffer_t* gopBuffer, psf_t* screenFont);

void splash_cleanup(void);

void splash_print(const char* string, pixel_t color);
