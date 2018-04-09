#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "util.h"
#include "easing.h"

extern enum lb_input_mode {
	INPUT_SELECT,
	INPUT_DRAW,
	INPUT_ARTBOARD
} input_mode;

enum lb_animate_method {
	ANIMATE_NONE = 0,
	ANIMATE_DRAW,
	ANIMATE_FADE
};

struct lb_2point_beizer {
	vec2 a;
	vec2 h1;
	vec2 h2;
	vec2 b;
};

struct lb_stroke_transition {
	enum EasingMethod easing_method;
	enum lb_animate_method animate_method;
	float duration;
	bool draw_reverse;
};

struct bezier_point {
	vec2 anchor;
	vec2 handles[2];
};

struct lb_stroke {
	struct bezier_point* vertices;
	float global_start_time;
	float full_duration;
	float scale;
	colorf color;
	
	struct lb_2point_beizer thickness_curve;
	struct lb_stroke_transition enter;
	struct lb_stroke_transition exit;
	
	
	uint16_t vertices_len;
	
};

extern color32 lb_clear_color;
extern bool lb_strokes_playing;
extern float lb_strokes_timelineDuration;
extern float lb_strokes_timelinePosition;
extern bool lb_strokes_draggingPlayhead;
extern struct lb_stroke* lb_strokes_selected;
extern bool lb_strokes_artboard_set;
extern int lb_strokes_artboard_set_idx;
extern vec2 lb_strokes_artboard[2];

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
