#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "util.h"

enum lb_draw_mode {
	DRAW_REALTIME,
	DRAW_STAGGERED,
	DRAW_CONCURRENT
};

enum lb_playback_mode {
	PLAYBACK_REALTIME = 0,
	PLAYBACK_LINEAR
};

struct lb_stroke {
	vec2* vertices;
	float* vertices_timing;
	float global_start_time;
	float global_duration;
	enum lb_playback_mode playback;
	uint16_t vertices_len;
};

extern enum lb_draw_mode lb_strokes_drawMode;
extern bool lb_strokes_playing;
extern float lb_strokes_timelineDuration;
extern float lb_strokes_timelinePosition;
extern bool lb_strokes_draggingPlayhead;

float lb_strokes_setTimelinePosition(float pos);
void lb_strokes_updateTimeline(float dt);
struct lb_stroke* lb_strokes_getSelectedStroke();

void lb_strokes_init();
void lb_strokes_render();

void lb_strokes_start(vec2 point, float time);
void lb_strokes_addVertex(vec2 point, float time);
void lb_strokes_end();
bool lb_strokes_isDrawing();
