#pragma once

#include <GL/glew.h>
#include <stdbool.h>
#include <stdint.h>

#define LB_MAX_UNIFORMS 32

struct shaderProgram {
	GLuint program;
	uint8_t numUniforms;
	GLint uniforms[LB_MAX_UNIFORMS];
};

GLuint loadShader(const GLenum type, const char* source, const int* sourceLength);
struct shaderProgram* buildProgram(GLuint vertexShader, GLuint fragmentShader, const char** uniformNames, uint8_t numUniforms, struct shaderProgram* out);

GLenum glCheckError();

GLubyte* loadPNG(const char* name, uint32_t* out_width, uint32_t* out_height, bool* out_has_alpha);