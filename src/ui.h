#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif

EXTERN_C void lb_ui_init(
	void(*glInit)(const unsigned char*, const int, const int, unsigned int*),
	void(*glPrepFrameState)(int, int, int, int),
	void(*glUploadData)(uint32_t, const void*, uint32_t, const void*),
	void(*glDrawElement)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, const void*));

EXTERN_C void lb_ui_render(int32_t windowWidth, int32_t windowHeight, int32_t framebufferWidth, int32_t framebufferHeight, double dt);
EXTERN_C void lb_ui_destroy(void(*glDestroy)());
EXTERN_C bool lb_ui_isDrawingCursor();

EXTERN_C void lb_ui_windowFocusCallback(bool focused);
EXTERN_C void lb_ui_scrollCallback(double x, double y);
EXTERN_C void lb_ui_cursorPosCallback(double x, double y);
EXTERN_C void lb_ui_mouseButtonCallback(int button, int action, int mods);
EXTERN_C void lb_ui_charCallback(unsigned int codePoint);
EXTERN_C void lb_ui_keyCallback(int key, int scancode, int action, int mods);