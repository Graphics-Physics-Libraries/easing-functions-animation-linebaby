#include "app.h"

#include <stdint.h>
#include <string.h>
#include "gl.h"
#include "ui.h"

int32_t windowWidth, windowHeight;
int32_t framebufferWidth, framebufferHeight;

static float ortho_projection[4][4] = {
	{ 2.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, -2.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, -1.0f, 0.0f },
	{-1.0f, 1.0f, 0.0f, 1.0f },
};

static float* updateOrthoProjection() {
	ortho_projection[0][0] = 2.0f / (float)windowWidth;
	ortho_projection[1][1] = -2.0f / (float)windowHeight;
	return ortho_projection;
}

// --- UI ---
static struct {
	struct shaderProgram shader;
	GLuint vaoHandle;
	GLuint vboHandle;
	GLuint elementsHandle;
	GLuint fontTexture;
} ui_glState;

#include "../build/assets/shaders/ui.frag.c"
#include "../build/assets/shaders/ui.vert.c"

static void ui_initGL(const unsigned char* fontPixels, const int fontWidth, const int fontHeight, unsigned int* fontTextureOut) {
	
	const char** uniformNames[2] = { "projection", "tex" };
	buildProgram(
		loadShader(GL_VERTEX_SHADER, (char*)src_assets_shaders_ui_vert, (int*)&src_assets_shaders_ui_vert_len),
		loadShader(GL_FRAGMENT_SHADER, (char*)src_assets_shaders_ui_frag, (int*)&src_assets_shaders_ui_frag_len),
		uniformNames, 2, &ui_glState.shader);
	
	glGenBuffers(1, &ui_glState.vboHandle);
	glGenBuffers(1, &ui_glState.elementsHandle);
	
	glGenVertexArrays(1, &ui_glState.vaoHandle);
	glBindVertexArray(ui_glState.vaoHandle);
	glBindBuffer(GL_ARRAY_BUFFER, ui_glState.vboHandle);
	
	glEnableVertexAttribArray(0); // position
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4 + sizeof(unsigned int), 0);
	glEnableVertexAttribArray(1); // texCoord
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4 + sizeof(unsigned int), (void*)(sizeof(float)*2));
	glEnableVertexAttribArray(2); // color
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(float)*4 + sizeof(unsigned int), (void*)(sizeof(float)*4));
	
	// Upload texture to graphics system
	glGenTextures(1, &ui_glState.fontTexture);
	*fontTextureOut = ui_glState.fontTexture;
	glBindTexture(GL_TEXTURE_2D, ui_glState.fontTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fontWidth, fontHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, fontPixels);
}

static void ui_destroyGL() {
	if(ui_glState.vaoHandle) glDeleteVertexArrays(1, &ui_glState.vaoHandle);
	if(ui_glState.vboHandle) glDeleteBuffers(1, &ui_glState.vboHandle);
	if(ui_glState.elementsHandle) glDeleteBuffers(1, &ui_glState.elementsHandle);
	ui_glState.vaoHandle = ui_glState.vboHandle = ui_glState.elementsHandle = 0;

	if(ui_glState.shader.program) glDeleteProgram(ui_glState.shader.program);
	ui_glState.shader.program = 0;

	if(ui_glState.fontTexture) {
		glDeleteTextures(1, &ui_glState.fontTexture);
		ui_glState.fontTexture = 0;
	}
}

static void ui_uploadGLData(uint32_t bufSize, const void* bufData, uint32_t elementsSize, const void* elementsData) {
	glBindBuffer(GL_ARRAY_BUFFER, ui_glState.vboHandle);
	glBufferData(GL_ARRAY_BUFFER, bufSize, bufData, GL_STREAM_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ui_glState.elementsHandle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, elementsSize, elementsData, GL_STREAM_DRAW);
}

static void ui_drawGLElement(uint32_t texID, int32_t scissorX, int32_t scissorY, int32_t scissorWidth, int32_t scissorHeight, int32_t numElements, uint32_t elementIdxTypeSize, const void* bufferOffset) {
	glBindTexture(GL_TEXTURE_2D, texID);
	glScissor(scissorX, scissorY, scissorWidth, scissorHeight);
	glDrawElements(GL_TRIANGLES, numElements, elementIdxTypeSize == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, bufferOffset);
}

static void ui_prepGLState(int windowWidth, int windowHeight, int fbWidth, int fbHeight) {
	
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	glViewport(0, 0, (GLsizei)fbWidth, (GLsizei)fbHeight);
	
	
	glUseProgram(ui_glState.shader.program);
	glUniformMatrix4fv(ui_glState.shader.uniforms[0], 1, GL_FALSE, ortho_projection);
	glUniform1i(ui_glState.shader.uniforms[1], 0);
	glBindVertexArray(ui_glState.vaoHandle);
	glBindSampler(0, 0); // Rely on combined texture/sampler state.
}

void lb_init() {
	lb_ui_init(ui_initGL, ui_prepGLState, ui_uploadGLData, ui_drawGLElement);
}

void lb_update(double t) {
	
}

void lb_render() {
	glDisable(GL_SCISSOR_TEST);
	glClear(GL_COLOR_BUFFER_BIT);
	updateOrthoProjection();
	lb_ui_render(windowWidth, windowHeight, framebufferWidth, framebufferHeight, 1.0f/60.0f);
}

void lb_destroy() {
	lb_ui_destroy(ui_destroyGL);
}


// --- INPUT ---
void handleCallback_key(GLFWwindow* window, int key, int scancode, int action, int mods) {
	lb_ui_keyCallback(key, scancode, action, mods);
}
void handleCallback_char(GLFWwindow* window, unsigned int codepoint) {
	lb_ui_charCallback(codepoint);
}
void handleCallback_cursorPos(GLFWwindow* window, double x, double y) {
	lb_ui_cursorPosCallback(x, y);
}
void handleCallback_mouseButton(GLFWwindow* window, int button, int action, int mods) {
	lb_ui_mouseButtonCallback(button, action, mods);
}
void handleCallback_scroll(GLFWwindow* window, double x, double y) {
	lb_ui_scrollCallback(x, y);
}
void handleCallback_focus(GLFWwindow* window, int focused) {
	lb_ui_windowFocusCallback(focused);
}
