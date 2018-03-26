#pragma once

/* --- Must be included in this order --- */
#include <GL/glew.h>
#include <GLFW/glfw3.h>
/* -------------------------------------- */

void lb_init();
void lb_update(double time, double dt);
void lb_render();
void lb_destroy();

void handleCallback_key(GLFWwindow* window, int key, int scancode, int action, int mods);
void handleCallback_char(GLFWwindow* window, unsigned int codepoint);
void handleCallback_cursorPos(GLFWwindow* window, double x, double y);
void handleCallback_mouseButton(GLFWwindow* window, int button, int action, int mods);
void handleCallback_scroll(GLFWwindow* window, double x, double y);
void handleCallback_focus(GLFWwindow* window, int focused);