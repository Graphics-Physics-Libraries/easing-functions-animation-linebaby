#include "util.h"

#include <math.h>

int32_t windowWidth, windowHeight;
int32_t framebufferWidth, framebufferHeight;

float screen_ortho[4][4] = {
	{ 2.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, -2.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, -1.0f, 0.0f },
	{-1.0f, 1.0f, 0.0f, 1.0f },
};

float* update_screen_ortho() {
	screen_ortho[0][0] = 2.0f / (float)windowWidth;
	screen_ortho[1][1] = -2.0f / (float)windowHeight;
	return (float*)screen_ortho;
}

float vec2_dist(const vec2* a, const vec2* b) {
	return sqrt(pow(b->x - a->x, 2) + pow(b->y - a->y, 2));
}