#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "util.h"

extern enum lb_input_mode {
	INPUT_SELECT,
	INPUT_DRAW
} input_mode;

enum lb_draw_mode {
	DRAW_REALTIME,
	DRAW_STAGGERED,
	DRAW_CONCURRENT
};

struct lb_stroke {
	struct bezier_point* vertices;
	float global_start_time;
	float global_duration;
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

void lb_strokes_handleMouseMove(vec2 point, float time);
void lb_strokes_handleMouseDown(vec2 point, float time);
void lb_strokes_handleMouseUp();
bool lb_strokes_isDrawing();
