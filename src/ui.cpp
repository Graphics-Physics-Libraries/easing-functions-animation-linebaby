#include "ui.h"

#include <inttypes.h>
#include <imgui/imgui.h>

// EXTERN_C {
// 	#include "../render/gl.h"
// 	#include "../engine.h"
// }

static bool windowFocused;
static double lastMouseX, lastMouseY;
static float scrollAccumulator;

static void(*glPrepFrameStateFunc)(int, int, int, int);
static void(*glUploadDataFunc)(uint32_t, const void*, uint32_t, const void*);
static void(*glDrawElementFunc)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, const void*);

void lb_ui_windowFocusCallback(bool focused) {
	windowFocused = focused;
}

void lb_ui_scrollCallback(double x, double y) {
	scrollAccumulator += y;
}

void lb_ui_cursorPosCallback(double x, double y) {
	lastMouseX = x;
	lastMouseY = y;
}

void lb_ui_mouseButtonCallback(int button, int action, int mods) {
	if(button < 0 || button > 3) {
		return;
	}
	
	ImGuiIO& io = ImGui::GetIO();
	io.MouseDown[button] = (action == 1);
	
	//TODO: Make resilient to sub-frame clicks
	// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
	// io.MouseDown[i] = g_MouseJustPressed[i] || glfwGetMouseButton(g_Window, i) != 0;
	// g_MouseJustPressed[i] = false;
}

void lb_ui_charCallback(unsigned int codePoint) {
	ImGuiIO& io = ImGui::GetIO();
	if (codePoint > 0 && codePoint < 0x10000) {
		io.AddInputCharacter((unsigned short)codePoint);
	}
}

void lb_ui_keyCallback(int key, int scancode, int action, int mods) {
	ImGuiIO& io = ImGui::GetIO();
	if (action == 1) io.KeysDown[key] = true;
	if (action == 0) io.KeysDown[key] = false;
}


static void renderImGuiDrawLists(ImDrawData* data) {
	ImGuiIO& io = ImGui::GetIO();
	
	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
	int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
	if (fb_width == 0 || fb_height == 0)
		return;
	
	data->ScaleClipRects(io.DisplayFramebufferScale);
	
	glPrepFrameStateFunc(io.DisplaySize.x, io.DisplaySize.y, fb_width, fb_height);
	
	for(int n = 0; n < data->CmdListsCount; n++) {
		const ImDrawList* cmd_list = data->CmdLists[n];
		const ImDrawIdx* idx_buffer_offset = 0;
		
		glUploadDataFunc(cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (void*)cmd_list->VtxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (void*)cmd_list->IdxBuffer.Data);
		
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback) {
				pcmd->UserCallback(cmd_list, pcmd);
			} else {
				glDrawElementFunc(
					(intptr_t)pcmd->TextureId,
					(int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y),
					pcmd->ElemCount, sizeof(ImDrawIdx), idx_buffer_offset);
			}
			idx_buffer_offset += pcmd->ElemCount;
		}
	}
}

static struct {
	bool showDemoPanel = false;
} guiState;

EXTERN_C void lb_ui_init(
	void(*glInit)(const unsigned char*, const int, const int, unsigned int*),
	void(*glPrepFrameState)(int, int, int, int),
	void(*glUploadData)(uint32_t, const void*, uint32_t, const void*),
	void(*glDrawElement)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, const void*))
{
	
	glPrepFrameStateFunc = glPrepFrameState;
	glUploadDataFunc = glUploadData;
	glDrawElementFunc = glDrawElement;
	
	windowFocused = true;
	scrollAccumulator = 0.0f;
	lastMouseX = lastMouseY = 0;
	
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = NULL;
	
	// Build texture atlas
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height); // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
	
	unsigned int fontTextureID;
	glInit(pixels, width, height, &fontTextureID);
	
	// Store our identifier
	io.Fonts->TexID = (void *)(intptr_t) fontTextureID;
	
	io.RenderDrawListsFn = renderImGuiDrawLists;
	
	ImGui::StyleColorsDark();
}

EXTERN_C void lb_ui_destroy(void(*glDestroy)()) {
	ImGui::GetIO().Fonts->TexID = 0;
	ImGui::Shutdown();
	glDestroy();
}

static void drawMainMenuBar() {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("Show Demo Panel")) guiState.showDemoPanel = true;
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}


EXTERN_C void lb_ui_render(int windowWidth, int windowHeight, int framebufferWidth, int framebufferHeight, double dt) {
	
	ImGuiIO& io = ImGui::GetIO();
	
	io.DisplaySize = ImVec2((float)windowWidth, (float)windowHeight);
	io.DisplayFramebufferScale = ImVec2(windowWidth > 0 ? ((float)framebufferWidth / windowWidth) : 0, windowHeight > 0 ? ((float)framebufferHeight / windowHeight) : 0);
	io.DeltaTime = dt;
	
	// Manage inputs
	// (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
	//if(windowFocused) {
		// if (io.WantMoveMouse) {
		// 	glfwSetCursorPos(g_Window, (double)io.MousePos.x, (double)io.MousePos.y);   // Set mouse position if requested by io.WantMoveMouse flag (used when io.NavMovesTrue is enabled by user and using directional navigation)
		// } else {
	//}
	
	if(windowFocused) {
		io.MousePos = ImVec2((float) lastMouseX, (float) lastMouseY);
	} else {
		io.MousePos = ImVec2(-FLT_MAX,-FLT_MAX);
	}
	
	io.MouseWheel = scrollAccumulator;
	scrollAccumulator = 0.0f;
	
	// Start the frame. This call will update the io.WantCaptureMouse, io.WantCaptureKeyboard flag that you can use to dispatch inputs (or not) to your application.
	ImGui::NewFrame();
	
	drawMainMenuBar();
	if(guiState.showDemoPanel) ImGui::ShowDemoWindow(&guiState.showDemoPanel);
	
	ImGui::Render();
}

EXTERN_C bool lb_ui_isDrawingCursor() {
	return ImGui::GetIO().MouseDrawCursor;
}
