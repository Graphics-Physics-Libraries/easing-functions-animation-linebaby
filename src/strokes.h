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

enum lb_animate_method {
	ANIMATE_NONE = 0,
	ANIMATE_DRAW,
	ANIMATE_FADE
};

struct lb_stroke_transition {
	enum lb_animate_method method;
	float duration;
	bool draw_reverse;
};

struct lb_stroke {
	struct bezier_point* vertices;
	float global_start_time;
	float full_duration;
	float scale;
	
	struct lb_stroke_transition enter;
	struct lb_stroke_transition exit;
	
	uint16_t vertices_len;
};

extern color32 lb_clear_color;
extern enum lb_draw_mode lb_strokes_drawMode;
extern bool lb_strokes_playing;
extern float lb_strokes_timelineDuration;
extern float lb_strokes_timelinePosition;
extern bool lb_strokes_draggingPlayhead;
extern struct lb_stroke* lb_strokes_selected;

float lb_strokes_setTimelinePosition(float pos);
void lb_strokes_updateTimeline(float dt);

void lb_strokes_init();
void lb_strokes_render();

void lb_strokes_handleKeyDown(int key, int scancode, int mods);
void lb_strokes_handleKeyUp(int key, int scancode, int mods);
void lb_strokes_handleKeyRepeat(int key, int scancode, int mods);
void lb_strokes_handleMouseMove(vec2 point, float time);
void lb_strokes_handleMouseDown(vec2 point, float time);
void lb_strokes_handleMouseUp();
bool lb_strokes_isDrawing();
