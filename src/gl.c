#include "gl.h"

#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>

GLenum glCheckError() {
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
	}
	
	return out;
}

GLubyte* loadPNG(const char* name, uint32_t* out_width, uint32_t* out_height, bool* out_has_alpha) {
	
	png_structp png_ptr;
    png_infop info_ptr;
    unsigned int sig_read = 0;
    int color_type, interlace_type;
    FILE *fp;
 
    if ((fp = fopen(name, "rb")) == NULL)
        return NULL;

    /* Create and initialize the png_struct
     * with the desired error handler
     * functions.  If you want to use the
     * default stderr and longjump method,
     * you can supply NULL for the last
     * three parameters.  We also supply the
     * the compiler header file version, so
     * that we know if the application
     * was compiled with a compatible version
     * of the library.  REQUIRED
     */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        fclose(fp);
        return NULL;
    }
 
    /* Allocate/initialize the memory
     * for image information.  REQUIRED. */
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fclose(fp);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }
 
    /* Set error handling if you are
     * using the setjmp/longjmp method
     * (this is the normal method of
     * doing things with libpng).
     * REQUIRED unless you  set up
     * your own error handlers in
     * the png_create_read_struct()
     * earlier.
     */
    if (setjmp(png_jmpbuf(png_ptr))) {
        /* Free all of the memory associated
         * with the png_ptr and info_ptr */
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        /* If we get here, we had a
         * problem reading the file */
        return NULL;
    }
 
    /* Set up the output control if
     * you are using standard C streams */
    png_init_io(png_ptr, fp);
 
    /* If we have already
     * read some of the signature */
    png_set_sig_bytes(png_ptr, sig_read);
 
    /*
     * If you have enough memory to read
     * in the entire image at once, and
     * you need to specify only
     * transforms that can be controlled
     * with one of the PNG_TRANSFORM_*
     * bits (this presently excludes
     * dithering, filling, setting
     * background, and doing gamma
     * adjustment), then you can read the
     * entire image (including pixels)
     * into the info structure with this
     * call
     *
     * PNG_TRANSFORM_STRIP_16 |
     * PNG_TRANSFORM_PACKING  forces 8 bit
     * PNG_TRANSFORM_EXPAND forces to
     *  expand a palette into RGB
     */
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING, NULL);
 	
    int bit_depth;
    png_get_IHDR(png_ptr, info_ptr, out_width, out_height, &bit_depth, &color_type,
                 &interlace_type, NULL, NULL);
    *out_has_alpha = (color_type == PNG_COLOR_TYPE_RGBA);
    printf("COLOR TYPE: %d / %d %d %d\n", color_type, PNG_COLOR_TYPE_GRAY, PNG_COLOR_TYPE_RGBA, PNG_COLOR_TYPE_GRAY_ALPHA);
    printf("BIT DEPTH: %d\n", bit_depth);
    
 	uint32_t height = *out_height;
    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    GLubyte* out_data = malloc(row_bytes * height);
 
    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
 
    for (int i = 0; i < height; i++) {
        // note that png is ordered top to
        // bottom, but OpenGL expect it bottom to top
        // so the order or swapped
        memcpy(out_data+(row_bytes * (height-1-i)), row_pointers[i], row_bytes);
    }
 
    /* Clean up after the read,
     * and free any memory allocated */
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
 
    /* Close the file */
    fclose(fp);
 
 	return out_data;
}

/*
void writePNG(const char* filename) {
	FILE* file = fopen(filename, "wb");
	if(!file) {
		fprintf(stderr, "Could not open export file %s\n", filename);
		return;
	}
	
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)user_error_ptr, user_error_fn, user_warning_fn);
	if(!png_ptr)
		return
	(ERROR);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if
	(!info_ptr)
	{
	png_destroy_write_struct(&png_ptr,
	(png_infopp)NULL);
	return
	(ERROR);
	}
}
*/