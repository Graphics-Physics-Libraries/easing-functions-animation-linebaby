#include "gl.h"

#include <assert.h>
#include <stdio.h>

static GLenum glCheckError() {
	GLenum errorCode;
	while((errorCode = glGetError()) != GL_NO_ERROR) {
		const char* error;
		switch (errorCode) {
			case GL_INVALID_ENUM:					error = "INVALID_ENUM"; break;
			case GL_INVALID_VALUE:					error = "INVALID_VALUE"; break;
			case GL_INVALID_OPERATION:				error = "INVALID_OPERATION"; break;
			case GL_STACK_OVERFLOW:					error = "STACK_OVERFLOW"; break;
			case GL_STACK_UNDERFLOW:				error = "STACK_UNDERFLOW"; break;
			case GL_OUT_OF_MEMORY:					error = "OUT_OF_MEMORY"; break;
			case GL_INVALID_FRAMEBUFFER_OPERATION:	error = "INVALID_FRAMEBUFFER_OPERATION"; break;
			default:								error = "UNKNOWN ERROR"; break;
		}
		
		fprintf(stderr, "OpenGL Error: %s\n", error);
		assert(errorCode == GL_NO_ERROR);
		return errorCode;
	}
	
	return GL_NO_ERROR;
}

GLuint loadShader(const GLenum type, const char* source, const int* sourceLength) {
	const GLchar* shaderSource = source;
	assert(shaderSource);
	assert(sourceLength);
	
	GLuint shader = glCreateShader(type);
	glCheckError();
	
	glShaderSource(shader, 1, (const GLchar**) &shaderSource, sourceLength);
	glCheckError();
	
	glCompileShader(shader);
	glCheckError();
	
	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	
	if(compiled != GL_TRUE) {
		GLsizei msgLength;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &msgLength);
		GLchar msg[msgLength + 1];
		glGetShaderInfoLog(shader, msgLength, &msgLength, msg);
		fprintf(stderr, "Shader compilation error: %s", msg);
		return 0;
	}
	
	GLint outType;
	glGetShaderiv(shader, GL_SHADER_TYPE, &outType);
	
	return shader;
}

struct shaderProgram* buildProgram(GLuint vertexShader, GLuint fragmentShader, const char** uniformNames, uint8_t numUniforms, struct shaderProgram* out) {
	assert(uniformNames);
	assert(numUniforms <= LB_MAX_UNIFORMS);
	
	GLuint program = glCreateProgram();
	glCheckError();
	
	glAttachShader(program, vertexShader);
	glCheckError();
	
	glAttachShader(program, fragmentShader);
	glCheckError();
	
	glLinkProgram(program);
	glCheckError();
	
	GLint linked;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if(!linked) {
		GLsizei msgLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &msgLength);
		GLchar msg[msgLength + 1];
		glGetProgramInfoLog(program, msgLength, &msgLength, msg);
		fprintf(stderr, "Shader link error: %s", msg);
		
		glDeleteProgram(program);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		
		return NULL;
	}
	
	// Cleanup after successful link
	glDetachShader(program, vertexShader);
	glDetachShader(program, fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	
	out->program = program;
	out->numUniforms = numUniforms;
	for(uint8_t i = 0; i < numUniforms; i++) {
		GLint uniformLoc = glGetUniformLocation(program, uniformNames[i]);
		out->uniforms[i] = uniformLoc;
		fprintf(stderr, "%s uniform set to loc %d at index %d\n", uniformNames[i], uniformLoc, i);
	}
	
	return out;
}

