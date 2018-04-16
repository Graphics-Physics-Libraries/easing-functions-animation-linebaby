#pragma once

#include <stdint.h>

// -- Globals --
extern int32_t windowWidth, windowHeight;
extern int32_t framebufferWidth, framebufferHeight;

// -- Types --
typedef struct vec2 {
	float x, y;
} vec2;

typedef float vec4[4];
typedef vec4 mat4[4];

extern float screen_ortho[4][4];
extern float crop_ortho[4][4];
void update_ortho(mat4 dest, float left, float right, float bottom, float top, float nearVal, float farVal);

float* get_custom_ortho(int width, int height);

float vec2_dist(const vec2 a, const vec2 b);
float vec2_len(const vec2 a);
vec2 vec2_add(const vec2 a, const vec2 b);
vec2 vec2_sub(const vec2 a, const vec2 b);

float map(float value, float istart, float istop, float ostart, float ostop);

typedef union color32 {
	struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};
	uint32_t u;
} color32;

typedef struct colorf {
	float r;
	float g;
	float b;
	float a;
} colorf;

vec2 bezier_cubic(const vec2 a, const vec2 h1, const vec2 h2, const vec2 b, const float t);
float bezier_estimate_length(const vec2 a, const vec2 h1, const vec2 h2, const vec2 b);
uint16_t hyperbola_min_segments(const float length);

#define BEZIER_DISTANCE_CACHE_SIZE 512
float bezier_distance_update_cache(const vec2 a, const vec2 h1, const vec2 h2, const vec2 b);
float bezier_distance_closest_t(float dist_t);
vec2 bezier_closest_point(const vec2 a, const vec2 h1, const vec2 h2, const vec2 b, uint16_t resolution, uint16_t iterations, vec2 point);

