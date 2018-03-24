#include <stdio.h>

/* --- Must be included in this order --- */
#include <GL/glew.h>
#include <GLFW/glfw3.h>
/* -------------------------------------- */

#include "app.h"

static GLFWwindow* window = NULL;

static void handleGLFWError(int error, const char* description) {
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, char** argv) {
	
	glfwSetErrorCallback(handleGLFWError);
	if(!glfwInit()) {
		return -1;
	}
	
	// OpenGL Context Version
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	
	glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);
	//glfwWindowHint(GLFW_SAMPLES, 2);
	
	#ifdef DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	#endif
	
	window = glfwCreateWindow(640, 480, "Linebaby", NULL, NULL);
	if (!window) {
		fprintf(stderr, "Could not initialize GLFW window.\n");
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	
	glfwSetKeyCallback(window, handleCallback_key);
	glfwSetCharCallback(window, handleCallback_char);
	glfwSetCursorPosCallback(window, handleCallback_cursorPos);
	glfwSetMouseButtonCallback(window, handleCallback_mouseButton);
	glfwSetScrollCallback(window, handleCallback_scroll);
	glfwSetWindowFocusCallback(window, handleCallback_focus);
	
	GLenum glewError = glewInit();
	if(glewError != GLEW_OK) {
		fprintf(stderr, "Could not initialize GLEW: %s\n", glewGetErrorString(glewError));
		return -1;
	}
	
	lb_init();
	
	glfwSwapInterval(1); // VSYNC
	
	while(!glfwWindowShouldClose(window)) {
		glfwGetWindowSize(window, &windowWidth, &windowHeight);
		glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
		
		lb_update(1.0f/60.0f);
		lb_render();
		
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	
	lb_destroy();
	
	glfwTerminate();
	return 0;
}