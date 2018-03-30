#include "util.h"

#include <math.h>
#include <stddef.h>
#include <assert.h>
#include <float.h>

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

float vec2_dist(const vec2 a, const vec2 b) {
	return sqrt(pow(b.x - a.x, 2) + pow(b.y - a.y, 2));
}

float vec2_len(const vec2 a) {
	return sqrt(a.x*a.x +a.y*a.y);
}

vec2 vec2_add(const vec2 a, const vec2 b) {
	return (vec2){a.x + b.x, a.y + b.y};
}

vec2 vec2_sub(const vec2 a, const vec2 b) {
	return (vec2){a.x - b.x, a.y - b.y};
}

vec2 bezier_cubic(const struct bezier_point* a, const struct bezier_point* b, const float t) {
	float t2 = t*t;
	float t3 = t2*t;
	float mt = 1-t;
	float mt2 = mt*mt;
	float mt3 = mt2*mt;
	return (vec2){
		.x = a->anchor.x * mt3  +  3 * a->handles[1].x * mt2 * t  +  3 * b->handles[0].x * mt * t2  +  b->anchor.x * t3,
		.y = a->anchor.y * mt3  +  3 * a->handles[1].y * mt2 * t  +  3 * b->handles[0].y * mt * t2  +  b->anchor.y * t3
	};
}

float bezier_estimate_length(const struct bezier_point* a, const struct bezier_point* b) {
	return (float) ceil(vec2_len(vec2_sub(a->handles[1], a->anchor)) + vec2_len(vec2_sub(b->handles[0], a->handles[1])) + vec2_len(vec2_sub(b->anchor, b->handles[0])));
}

uint16_t hyperbola_min_segments(const float length) {
	static const uint16_t min = 10;
	float segments = length / 30.0f;
	return (uint16_t) ceil(sqrt(segments*segments*0.6 + min*min));
}

static float bezier_distance_cache[BEZIER_DISTANCE_CACHE_SIZE];
static float bezier_distance_total;

float bezier_distance_update_cache(const struct bezier_point* a, const struct bezier_point* b) {
	
	size_t i = 0;
	bezier_distance_total = 0.0f;

	float t1 = 0.0f;
	vec2 p1 = bezier_cubic(a, b, t1);
	float t2;
	vec2 p2;

	for(i = 0; i < BEZIER_DISTANCE_CACHE_SIZE; i++) {
		t2 = ((float)i+1) / ((float)BEZIER_DISTANCE_CACHE_SIZE); // x distances requires x+1 discrete points
		p2 = bezier_cubic(a, b, t2);

		bezier_distance_cache[i] = vec2_dist(p1, p2);
		bezier_distance_total += bezier_distance_cache[i];

		p1 = p2;
		t1 = t2;
	}

	return bezier_distance_total;
}

float bezier_distance_closest_t(float dist_t) {
	if(dist_t <= 0.0f || dist_t >= 1.0f) return dist_t;
	
	float dist_length = bezier_distance_total * dist_t;
	
	float dist_accum = 0.0f;
	size_t i;
	for(i = 0; i < BEZIER_DISTANCE_CACHE_SIZE; i++) {
		dist_accum += bezier_distance_cache[i];
		if(dist_accum >= dist_length) {
			break;
		}
	}

	assert(i < BEZIER_DISTANCE_CACHE_SIZE); // should have found the segment before reaching the end

	// requested distance is within distance [i] (between (t) points [i] and [i+1])
	// simple linear interpolation between t1 and t2 mapped to distance
	float t1 = (float)i / (float)BEZIER_DISTANCE_CACHE_SIZE;
	float t2 = ((float)i+1) / (float)BEZIER_DISTANCE_CACHE_SIZE;
	float prev_dist = dist_accum - bezier_distance_cache[i];
	return t1 + (t2-t1)*((dist_length - prev_dist) / (dist_accum - prev_dist));
}

// Finds the closest point on the curve to the supplied point
vec2 bezier_closest_point(const struct bezier_point* a, const struct bezier_point* b, uint16_t resolution, uint16_t iterations, vec2 point) {
	
	uint16_t closest_idx;
	vec2 points_on_curve[resolution];
	float distances[resolution];
	float start_t = 0;
	float end_t = 1;
	
	for(uint16_t i = 0; i < iterations; i++) {
		
		for(uint16_t r = 0; r < resolution; r++) {
			points_on_curve[r] = bezier_cubic(a, b, map(r, 0, resolution, start_t, end_t));
			distances[r] = vec2_dist(points_on_curve[r], point);
		}
		
		float smallest = FLT_MAX;
		for(uint16_t r = 0; r < resolution; r++) {
			if(smallest > distances[r]) {
				smallest = distances[r];
				closest_idx = r;
			}
		}
		
		float midpoint_t = map(closest_idx, 0, resolution, start_t, end_t);
		float spread = end_t - start_t;
		start_t = midpoint_t - spread/3.0f;
		if(start_t < 0) start_t = 0;
		end_t = midpoint_t + spread/3.0f;
		if(end_t > 1) end_t = 1;
	}
	
	return points_on_curve[closest_idx];
}

float map(float value, float istart, float istop, float ostart, float ostop) {
	return ostart + (ostop - ostart) * ((value - istart) / (istop - istart));
}