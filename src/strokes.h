#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "util.h"
#include "easing.h"

extern enum lb_input_mode {
	INPUT_SELECT,
	INPUT_DRAW,
	INPUT_ARTBOARD,
	INPUT_TRIM,
} input_mode;

extern enum lb_drag_mode {
	DRAG_NONE,
	DRAG_ANCHOR,
	DRAG_HANDLE,
	DRAG_STROKE,
	DRAG_PAN,
} drag_mode;

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
	enum lb_animate_method animate_method;
	enum EasingMethod easing_method;
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
	float jitter;
	colorf color;
	
	struct lb_stroke_transition enter;
	struct lb_stroke_transition exit;
	
	uint16_t vertices_len;
};

enum lb_export_type {
	EXPORT_SPRITESHEET = 0,
	EXPORT_IMAGE_SEQUENCE,
};

struct lb_export_options {
	enum lb_export_type type;
	
	union {
		struct {
			bool include_css;
			bool retina_2x;
		} spritesheet;
		
		struct {
			bool retina_2x;
		} image_sequence;
	};
};

extern color32 lb_clear_color;
extern bool lb_strokes_playing;
extern float lb_strokes_timelineDuration;
extern float lb_strokes_timelinePosition;
extern bool lb_strokes_draggingPlayhead;
extern struct lb_stroke* lb_strokes_selected;
extern bool lb_strokes_artboard_set;
extern int lb_strokes_artboard_set_idx;
extern bool lb_strokes_export_range_set;
extern int lb_strokes_export_range_set_idx;
extern float lb_strokes_export_range_begin;
extern float lb_strokes_export_range_duration;
extern float lb_strokes_export_fps;
extern vec2 lb_strokes_pan;
extern vec2 lb_strokes_artboard[2];

float lb_strokes_setTimelinePosition(float pos);
void lb_strokes_updateTimeline(float dt);

void lb_strokes_init();
void lb_strokes_render_app();
void lb_strokes_render_export(const char* outdir, const float fps, struct lb_export_options options);

void lb_strokes_handleKeyDown(int key, int scancode, int mods);
void lb_strokes_handleKeyUp(int key, int scancode, int mods);
void lb_strokes_handleKeyRepeat(int key, int scancode, int mods);
void lb_strokes_handleMouseMove(vec2 point, float time);
void lb_strokes_handleMouseDown(int button, vec2 point, float time);
void lb_strokes_handleMouseUp(int button);
void lb_strokes_handleScroll(vec2 dist);

void lb_strokes_save(const char* filename);
void lb_strokes_open(const char* filename);