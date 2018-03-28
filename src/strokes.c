#include "strokes.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
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




static GLuint mask_texture;
static GLuint brush_texture;

#define RADIAL_GRADIENT_SIZE 64

void upload_texture() {

	static uint8_t pix[RADIAL_GRADIENT_SIZE][RADIAL_GRADIENT_SIZE];
	const uint8_t midpoint = RADIAL_GRADIENT_SIZE / 2;
	const float scale = 2.5f;

	for(uint8_t y = 0; y < RADIAL_GRADIENT_SIZE; y++) {
		for(uint8_t x = 0; x < RADIAL_GRADIENT_SIZE; x++) {
			double a = sqrt(pow(midpoint - x, 2) + pow(midpoint - y, 2));

			a = (a - midpoint) / (a - RADIAL_GRADIENT_SIZE) * scale;
			
			if(a > 1) a = 1;
			else if(a < 0) a = 0;

			pix[y][x] = a * 255;
		}
	}

	glGenTextures(1, &mask_texture);
	glBindTexture(GL_TEXTURE_2D, mask_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, RADIAL_GRADIENT_SIZE, RADIAL_GRADIENT_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, pix);

	uint32_t brush_width, brush_height;
	bool brush_alpha;
	GLubyte* brush_pix;
	if(!loadPNG("src/assets/images/pencil.png", &brush_width, &brush_height, &brush_alpha, &brush_pix)) {
		assert(false);
		return;
	}
	
	glGenTextures(1, &brush_texture);
	glBindTexture(GL_TEXTURE_2D, brush_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, brush_width, brush_height, 0, GL_RED, GL_UNSIGNED_BYTE, brush_pix);

	free(brush_pix);
}

static GLuint plane_vao;
static GLuint plane_vbo;

void upload_plane() {
	static float vertices[] = {
	//  Position   // Texcoords
		-0.5f,  0.5f, 0.0f, 0.0f, // Top-left
		 0.5f,  0.5f, 1.0f, 0.0f, // Top-right
		 0.5f, -0.5f, 1.0f, 1.0f, // Bottom-right
		-0.5f,  0.5f, 0.0f, 0.0f, // Top-left
		 0.5f, -0.5f, 1.0f, 1.0f, // Bottom-right
		-0.5f, -0.5f, 0.0f, 1.0f  // Bottom-left
	};

	glGenVertexArrays(1, &plane_vao);
	glBindVertexArray(plane_vao);
	glCheckError();

	glGenBuffers(1, &plane_vbo);
	glCheckError();

	size_t vertex_stride = 0;
	vertex_stride += sizeof(GLfloat) * 2; // XY
	vertex_stride += sizeof(GLfloat) * 2; // UV

	glBindBuffer(GL_ARRAY_BUFFER, plane_vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizei) sizeof(GLfloat) * 4 * 6, vertices, GL_STATIC_DRAW);
	glCheckError();

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vertex_stride, (char*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vertex_stride, (char*)8);
	glEnableVertexAttribArray(1);
	glCheckError();
}

void draw_plane() {
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glUniform1i(linesShader.uniforms[3], 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mask_texture);
	
	glUniform1i(linesShader.uniforms[4], 1);
	glActiveTexture(GL_TEXTURE0+1);
	glBindTexture(GL_TEXTURE_2D, brush_texture);

	glBindVertexArray(plane_vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}


void lb_strokes_init() {
	
	// Line buf
	{
		const char* uniformNames[6] = { "projection", "translation", "scale", "maskTex", "brushTex", "rotation"};
		buildProgram(
			loadShader(GL_VERTEX_SHADER, (char*)src_assets_shaders_lines_vert, (int*)&src_assets_shaders_lines_vert_len),
			loadShader(GL_FRAGMENT_SHADER, (char*)src_assets_shaders_lines_frag, (int*)&src_assets_shaders_lines_frag_len),
			uniformNames, 6, &linesShader);
		
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

	upload_plane();
	upload_texture();
}

void lb_strokes_render() {

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	
	glUseProgram(linesShader.program);
	glUniformMatrix4fv(linesShader.uniforms[0], 1, GL_FALSE, (const GLfloat*) screen_ortho);
	
	glBindVertexArray(gl_lines.vao);
	
	glBindBuffer(GL_ARRAY_BUFFER, gl_lines.vbo);
	
	glUniform2f(linesShader.uniforms[2], 5.0f, 5.0f);

	for(size_t i = 0; i < data.strokes_len; i++) {
		if(!data.strokes[i].vertices_len) continue;
		if(data.strokes[i].global_start_time > lb_strokes_timelinePosition) continue;
		
		// Find the maximum vertex to draw
		size_t v;
		const float max_duration = data.strokes[i].vertices_timing[data.strokes[i].vertices_len-1];
		const float scale = data.strokes[i].global_duration / max_duration;
		switch(data.strokes[i].playback) {
			case PLAYBACK_REALTIME:
				for(v = 0; v < data.strokes[i].vertices_len; v++) {
					if(data.strokes[i].global_start_time + data.strokes[i].vertices_timing[v] * scale > lb_strokes_timelinePosition) {
						break;
					}
				}

				break;
			case PLAYBACK_LINEAR: {
					const float sec_per_vertex = data.strokes[i].global_duration / data.strokes[i].vertices_len;
					for(v = 0; v < data.strokes[i].vertices_len; v++) {
						if(data.strokes[i].global_start_time + sec_per_vertex*v > lb_strokes_timelinePosition) {
							break;
						}
					}
				}

				break;
		}
		
		if(v < 2) continue;

		/*
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec2)*v, data.strokes[i].vertices);
		glCheckError();
		glDrawArrays(GL_LINE_STRIP, 0, v);
		*/

		static const float brush_dist = 2.0f;
		for(int j = 0; j < v-2; j++) {
			const vec2* v1 = &data.strokes[i].vertices[j];
			const vec2* v2 = &data.strokes[i].vertices[j+1];
			const float dist = vec2_dist(v1, v2);
			const float dist_ratio = brush_dist / dist;
			glUniform1f(linesShader.uniforms[5], (float)j);
			glUniform2f(linesShader.uniforms[1], (1-dist_ratio)*v1->x + dist_ratio*v2->x, (1-dist_ratio)*v1->y + dist_ratio*v2->y);
			draw_plane();
		}


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






