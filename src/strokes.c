#include "strokes.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include "gl.h"
#include "util.h"

#define VERTICES_CAPACITY 2048
#define MAX_STROKE_VERTICES 1024

static struct {
	vec2 vertices[VERTICES_CAPACITY];
	float vertices_timing[VERTICES_CAPACITY];
	size_t vertices_len;
	
	struct lb_stroke strokes[64];
	uint8_t strokes_len;
} data;

// Timeline
enum lb_draw_mode lb_strokes_drawMode = DRAW_REALTIME;
bool lb_strokes_playing = false;
float lb_strokes_timelineDuration = 10.0f;
float lb_strokes_timelinePosition = 0;
bool lb_strokes_draggingPlayhead = false;

static bool drawing = false;
static uint16_t drawing_stroke_idx;
static float draw_start_time;

float lb_strokes_setTimelinePosition(float pos) {
	pos = (pos < 0.0f ? 0.0f : pos);
	pos = (pos > lb_strokes_timelineDuration ? lb_strokes_timelineDuration : pos);
	return lb_strokes_timelinePosition = pos;
}

void lb_strokes_updateTimeline(float dt) {
	if((!drawing && !lb_strokes_playing) || lb_strokes_draggingPlayhead) return;
	lb_strokes_timelinePosition += dt;

	if(lb_strokes_timelinePosition > lb_strokes_timelineDuration) {
		lb_strokes_timelinePosition = 0;
	}
}

struct lb_stroke* lb_strokes_getSelectedStroke() {
	if(data.strokes_len == 0) return NULL;
	return &data.strokes[data.strokes_len-1];
}

// Drawing
static struct {
	GLuint vao;
	GLuint vbo;
} gl_lines;

static struct shaderProgram linesShader;
#include "../build/assets/shaders/lines.frag.c"
#include "../build/assets/shaders/lines.vert.c"

void lb_strokes_init() {
	
	// Line buf
	{
		const char* uniformNames[1] = { "projection" };
		buildProgram(
			loadShader(GL_VERTEX_SHADER, (char*)src_assets_shaders_lines_vert, (int*)&src_assets_shaders_lines_vert_len),
			loadShader(GL_FRAGMENT_SHADER, (char*)src_assets_shaders_lines_frag, (int*)&src_assets_shaders_lines_frag_len),
			uniformNames, 1, &linesShader);
		
		glGenVertexArrays(1, &gl_lines.vao);
		glBindVertexArray(gl_lines.vao);
		glCheckError();
		
		glGenBuffers(1, &gl_lines.vbo);
		glCheckError();
		
		size_t vertexStride = 0;
		vertexStride += sizeof(GLfloat) * 2;
		
		glBindBuffer(GL_ARRAY_BUFFER, gl_lines.vbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizei) sizeof(vec2) * MAX_STROKE_VERTICES, NULL, GL_DYNAMIC_DRAW);
		glCheckError();
		
		// Enable vertex attributes
		void* attrib_offset = 0;
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vertexStride, attrib_offset);
		attrib_offset += sizeof(GLfloat) * 2;
		glEnableVertexAttribArray(0);
		glCheckError();
	}
}

void lb_strokes_render() {
	
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_PROGRAM_POINT_SIZE);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	
	glUseProgram(linesShader.program);
	glUniformMatrix4fv(linesShader.uniforms[0], 1, GL_FALSE, (const GLfloat*) screen_ortho);
	
	glBindVertexArray(gl_lines.vao);
	
	glBindBuffer(GL_ARRAY_BUFFER, gl_lines.vbo);
	
	for(size_t i = 0; i < data.strokes_len; i++) {
		if(!data.strokes[i].vertices_len) continue;
		if(data.strokes[i].global_start_time > lb_strokes_timelinePosition) continue;
		
		// Find the maximum vertex to draw
		size_t v;
		for(v = 0; v < data.strokes[i].vertices_len; v++) {
			if(data.strokes[i].global_start_time + data.strokes[i].vertices_timing[v] > lb_strokes_timelinePosition) {
				break;
			}
		}
		
		if(v < 2) continue;

		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec2)*v, data.strokes[i].vertices);
		glCheckError();
		glDrawArrays(GL_LINE_STRIP, 0, v);
	}
}

bool lb_strokes_isDrawing() {
	return drawing;
}

void lb_strokes_start(vec2 point, float time) {
	assert(!drawing);
	drawing = true;
	drawing_stroke_idx = data.strokes_len;
	draw_start_time = time;

	data.strokes[data.strokes_len] = (struct lb_stroke) {
		.vertices = &data.vertices[data.vertices_len],
		.vertices_timing = &data.vertices_timing[data.vertices_len],
		.global_start_time = lb_strokes_timelinePosition,
		.global_duration = 0,
		.playback = PLAYBACK_REALTIME,
		.vertices_len = 0,
	};
	
	data.strokes_len++;

	lb_strokes_addVertex(point, time);
}

void lb_strokes_addVertex(vec2 point, float time) {
	assert(drawing);
	
	struct lb_stroke* stroke = &data.strokes[drawing_stroke_idx];
	assert(stroke->vertices_len < MAX_STROKE_VERTICES);
	assert(data.vertices_len < VERTICES_CAPACITY);
	
	stroke->vertices[stroke->vertices_len] = point;
	stroke->vertices_timing[stroke->vertices_len] = time - draw_start_time;
	stroke->global_duration = time - draw_start_time;

	stroke->vertices_len++;
	data.vertices_len++;
}

void lb_strokes_end() {
	drawing = false;

	struct lb_stroke* stroke = &data.strokes[drawing_stroke_idx];

	// Delete if nothing was drawn
	if(stroke->vertices_len < 2) {
		lb_strokes_timelinePosition = stroke->global_start_time;
		data.strokes_len--;

		return;
	}
}