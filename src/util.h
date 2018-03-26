#pragma once

#include <stdint.h>

// -- Globals --
extern int32_t windowWidth, windowHeight;
extern int32_t framebufferWidth, framebufferHeight;

extern float screen_ortho[4][4];
extern float* update_screen_ortho();

// -- Types --
typedef struct vec2 {
	float x, y;
} vec2;

typedef union color32 {
	struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};
	uint32_t u;
} color32;
