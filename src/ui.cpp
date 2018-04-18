#include "ui.h"

#define IMGUI_DEFINE_MATH_OPERATORS

#include <inttypes.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <stdio.h>
#include <dirent.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <stb_image.h>

EXTERN_C {
	#include "gl.h"
	#include "strokes.h"
	#include "easing.h"
	
	#include <GLFW/glfw3.h>
}

static bool windowFocused;
static double lastMouseX, lastMouseY;
static float scrollAccumulator;

static void(*glPrepFrameStateFunc)(int, int, int, int);
static void(*glUploadDataFunc)(uint32_t, const void*, uint32_t, const void*);
static void(*glDrawElementFunc)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, const void*);

static bool FileModal(bool* open, const char* action, char* selectedPathOut);


static const ImVec2 dir_icon_uv0 = ImVec2(0.0f, 0.75f);
static const ImVec2 dir_icon_uv1 = ImVec2(0.125f, 0.625f);
static const ImVec2 file_icon_uv0 = ImVec2(0.125f, 0.75f);
static const ImVec2 file_icon_uv1 = ImVec2(0.25f, 0.625f);
static const ImVec2 up_icon_uv0 = ImVec2(0.25f, 0.75f);
static const ImVec2 up_icon_uv1 = ImVec2(0.375f, 0.625f);
static const ImVec2 check_icon_uv0 = ImVec2(0.375f, 0.75f);
static const ImVec2 check_icon_uv1 = ImVec2(0.5f, 0.625f);



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

void SetStyleColors() {
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;
	style.FramePadding = ImVec2(4.0f,2.0f);
	style.ItemSpacing = ImVec2(8.0f,4.0f);
	style.WindowRounding = 2.0f;
	style.ChildRounding = 2.0f;
	style.FrameRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabRounding = 1.0f;
	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	colors[ImGuiCol_Text]                   = ImVec4(0.00f, 0.00f, 0.00f, 0.85f);
	colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_WindowBg]               = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
	colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg]                = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
	colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.44f);
	colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg]                = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.64f, 0.65f, 0.66f, 0.40f);
	colors[ImGuiCol_FrameBgActive]          = ImVec4(0.64f, 0.65f, 0.66f, 0.40f);
	colors[ImGuiCol_TitleBg]                = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
	colors[ImGuiCol_TitleBgActive]          = ImVec4(0.71f, 0.70f, 0.70f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg]              = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
	colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
	colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
	colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
	colors[ImGuiCol_CheckMark]              = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
	colors[ImGuiCol_SliderGrab]             = ImVec4(0.63f, 0.63f, 0.63f, 0.78f);
	colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.43f, 0.44f, 0.46f, 0.78f);
	colors[ImGuiCol_Button]                 = ImVec4(0.61f, 0.61f, 0.62f, 0.40f);
	colors[ImGuiCol_ButtonHovered]          = ImVec4(0.57f, 0.57f, 0.57f, 0.52f);
	colors[ImGuiCol_ButtonActive]           = ImVec4(0.61f, 0.63f, 0.64f, 1.00f);
	colors[ImGuiCol_Header]                 = ImVec4(0.64f, 0.64f, 0.65f, 0.31f);
	colors[ImGuiCol_HeaderHovered]          = ImVec4(0.58f, 0.58f, 0.59f, 0.55f);
	colors[ImGuiCol_HeaderActive]           = ImVec4(0.52f, 0.52f, 0.52f, 0.55f);
	colors[ImGuiCol_Separator]              = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
	colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.17f, 0.17f, 0.17f, 0.89f);
	colors[ImGuiCol_SeparatorActive]        = ImVec4(0.17f, 0.17f, 0.17f, 0.89f);
	colors[ImGuiCol_ResizeGrip]             = ImVec4(0.80f, 0.80f, 0.80f, 0.56f);
	colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.39f, 0.39f, 0.40f, 0.67f);
	colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.39f, 0.39f, 0.40f, 0.67f);
	colors[ImGuiCol_PlotLines]              = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram]          = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
	colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.71f, 0.72f, 0.73f, 0.57f);
	colors[ImGuiCol_ModalWindowDarkening]   = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
	colors[ImGuiCol_DragDropTarget]         = ImVec4(0.16f, 0.16f, 0.17f, 0.95f);
}

static struct {
	bool showFileSettingsPanel = false;
	
	#ifdef DEBUG
	bool showDemoPanel = false;
	#endif
} guiState;

static GLuint ui_sprite_texID;

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
	
	ImGui::CreateContext();
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
		
	SetStyleColors();
	
	// Load custom spritesheet
	int ui_sprite_width, ui_sprite_height, ui_sprite_channels;
	stbi_set_flip_vertically_on_load(1);
	GLubyte* ui_sprite_pix = stbi_load("src/assets/images/ui.png", &ui_sprite_width, &ui_sprite_height, &ui_sprite_channels, 0);
	assert(ui_sprite_pix);
	
	glGenTextures(1, &ui_sprite_texID);
	glBindTexture(GL_TEXTURE_2D, ui_sprite_texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ui_sprite_width, ui_sprite_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ui_sprite_pix);

	stbi_image_free(ui_sprite_pix);
}

EXTERN_C void lb_ui_destroy(void(*glDestroy)()) {
	ImGui::GetIO().Fonts->TexID = 0;
	ImGui::DestroyContext();
	glDestroy();
}

static void drawTimeline() {
	static const int timeline_height = 18;
	static const int handle_height = 18;
	static const int total_height = timeline_height + handle_height;
	
	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::SetNextWindowBgAlpha(0);
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, total_height));
	ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - total_height));
	ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	const ImVec2 timeline_min = ImVec2(0, io.DisplaySize.y - timeline_height);
	const ImVec2 timeline_max = ImVec2(io.DisplaySize.x, io.DisplaySize.y);
	float playhead_pos_x = lb_strokes_timelinePosition / lb_strokes_timelineDuration * io.DisplaySize.x;
	draw_list->AddRectFilled(timeline_min, timeline_max, ImGui::GetColorU32(ImGuiCol_WindowBg), 0);
	// draw_list->AddRectFilled(timeline_min, ImVec2(timeline_min.x + playhead_pos_x, timeline_max.y), ImGui::GetColorU32(ImGuiCol_FrameBg), 0);
	
	
	if(lb_strokes_export_range_set) {
		draw_list->AddRectFilled(timeline_min, ImVec2(timeline_min.x + (lb_strokes_export_range_begin/lb_strokes_timelineDuration*io.DisplaySize.x), timeline_max.y), ImGui::GetColorU32(ImGuiCol_Separator), 0);
		draw_list->AddRectFilled(ImVec2(timeline_min.x + (lb_strokes_export_range_begin + lb_strokes_export_range_duration)/lb_strokes_timelineDuration*io.DisplaySize.x, timeline_min.y), timeline_max, ImGui::GetColorU32(ImGuiCol_Separator), 0);
	} else if(lb_strokes_export_range_set_idx == 0) {
		draw_list->AddRectFilled(timeline_min, ImVec2(timeline_min.x + (lb_strokes_timelinePosition/lb_strokes_timelineDuration*io.DisplaySize.x), timeline_max.y), ImGui::GetColorU32(ImGuiCol_Separator), 0);
	} else if(lb_strokes_export_range_set_idx == 1) {
		draw_list->AddRectFilled(timeline_min, ImVec2(timeline_min.x + (lb_strokes_export_range_begin/lb_strokes_timelineDuration*io.DisplaySize.x), timeline_max.y), ImGui::GetColorU32(ImGuiCol_Separator), 0);
		draw_list->AddRectFilled(ImVec2(timeline_min.x + lb_strokes_timelinePosition/lb_strokes_timelineDuration*io.DisplaySize.x, timeline_min.y), timeline_max, ImGui::GetColorU32(ImGuiCol_Separator), 0);
	}
	
	bool mouse_hovering_playhead = ImGui::IsMouseHoveringRect(ImVec2(timeline_min.x + playhead_pos_x - timeline_height/2, timeline_min.y), ImVec2(timeline_min.x + playhead_pos_x + timeline_height/2, timeline_max.y));
	bool mouse_hovering_timeline = ImGui::IsMouseHoveringRect(timeline_min, timeline_max);

	draw_list->AddTriangleFilled(
		ImVec2(timeline_min.x + playhead_pos_x, timeline_min.y),
		ImVec2(timeline_min.x + playhead_pos_x - timeline_height/2, timeline_min.y + timeline_height/2),
		ImVec2(timeline_min.x + playhead_pos_x + timeline_height/2, timeline_min.y + timeline_height/2),
		ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
	draw_list->AddTriangleFilled(
		ImVec2(timeline_min.x + playhead_pos_x - timeline_height/2, timeline_min.y + timeline_height/2),
		ImVec2(timeline_min.x + playhead_pos_x + timeline_height/2, timeline_min.y + timeline_height/2),
		ImVec2(timeline_min.x + playhead_pos_x, timeline_max.y),
		ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));

	if(mouse_hovering_timeline && ImGui::IsMouseClicked(0)) {
		lb_strokes_draggingPlayhead = true;
	} else if(ImGui::IsMouseReleased(0)) {
		lb_strokes_draggingPlayhead = false;
	}

	if(lb_strokes_draggingPlayhead) {
		lb_strokes_setTimelinePosition(ImGui::GetMousePos().x / io.DisplaySize.x * lb_strokes_timelineDuration);
	}

	if(lb_strokes_selected) {
		const float handle_left_time = lb_strokes_selected->global_start_time;
		const float handle_enter_time = handle_left_time + (lb_strokes_selected->enter.animate_method == ANIMATE_NONE ? 0 : lb_strokes_selected->enter.duration);
		const float handle_exit_time = handle_enter_time + lb_strokes_selected->full_duration;
		const float handle_right_time = handle_exit_time + (lb_strokes_selected->exit.animate_method == ANIMATE_NONE ? 0 : lb_strokes_selected->exit.duration);
		
		ImVec2 handle_left = ImVec2(handle_left_time / lb_strokes_timelineDuration * io.DisplaySize.x, timeline_min.y - 3);
		ImVec2 handle_enter = ImVec2(handle_enter_time / lb_strokes_timelineDuration * io.DisplaySize.x, timeline_min.y - 3);
		ImVec2 handle_exit = ImVec2(handle_exit_time / lb_strokes_timelineDuration * io.DisplaySize.x, timeline_min.y - 3);
		ImVec2 handle_right = ImVec2(handle_right_time / lb_strokes_timelineDuration * io.DisplaySize.x, timeline_min.y - 3);
		
		bool mouse_hovering_handle_left = ImGui::IsMouseHoveringRect(ImVec2(handle_left.x - 6, handle_left.y - 6), ImVec2(handle_left.x + 6, handle_left.y));
		bool mouse_hovering_handle_right = ImGui::IsMouseHoveringRect(ImVec2(handle_right.x - 6, handle_right.y - 6), ImVec2(handle_right.x + 6, handle_right.y));
		
		bool mouse_hovering_handle_enter;
		if(lb_strokes_selected->enter.animate_method != ANIMATE_NONE) mouse_hovering_handle_enter = ImGui::IsMouseHoveringRect(ImVec2(handle_enter.x - 6, handle_enter.y - 15), ImVec2(handle_enter.x, handle_enter.y - 9));
		bool mouse_hovering_handle_exit;
		if(lb_strokes_selected->exit.animate_method != ANIMATE_NONE) mouse_hovering_handle_exit = ImGui::IsMouseHoveringRect(ImVec2(handle_exit.x, handle_exit.y - 15), ImVec2(handle_exit.x + 6, handle_exit.y - 9));
		
		static bool dragging_handle_left = false;
		static bool dragging_handle_enter = false;
		static bool dragging_handle_exit = false;
		static bool dragging_handle_right = false;
		
		if(mouse_hovering_handle_left && ImGui::IsMouseClicked(0)) {
			dragging_handle_left = true;
		} else if(ImGui::IsMouseReleased(0)) {
			dragging_handle_left = false;
		}
		if(mouse_hovering_handle_enter && ImGui::IsMouseClicked(0)) {
			dragging_handle_enter = true;
		} else if(ImGui::IsMouseReleased(0)) {
			dragging_handle_enter = false;
			if(lb_strokes_selected->enter.duration == 0) lb_strokes_selected->enter.animate_method = ANIMATE_NONE;
		}
		if(mouse_hovering_handle_exit && ImGui::IsMouseClicked(0)) {
			dragging_handle_exit = true;
		} else if(ImGui::IsMouseReleased(0)) {
			dragging_handle_exit = false;
			if(lb_strokes_selected->exit.duration == 0) lb_strokes_selected->exit.animate_method = ANIMATE_NONE;
		}
		if(mouse_hovering_handle_right && ImGui::IsMouseClicked(0)) {
			dragging_handle_right = true;
		} else if(ImGui::IsMouseReleased(0)) {
			dragging_handle_right = false;
		}

		if(dragging_handle_left && ImGui::IsMouseDragging()) {
			lb_strokes_selected->global_start_time = ImGui::GetMousePos().x / io.DisplaySize.x * lb_strokes_timelineDuration;
			if(lb_strokes_selected->global_start_time < 0) {
				lb_strokes_selected->global_start_time = 0;
			} else if(lb_strokes_selected->global_start_time + lb_strokes_selected->enter.duration + lb_strokes_selected->full_duration + lb_strokes_selected->exit.duration > lb_strokes_timelineDuration) {
				lb_strokes_selected->global_start_time = lb_strokes_timelineDuration - lb_strokes_selected->enter.duration - lb_strokes_selected->full_duration - lb_strokes_selected->exit.duration;
			}
		} else if(dragging_handle_right && ImGui::IsMouseDragging()) {
			lb_strokes_selected->full_duration = (ImGui::GetMousePos().x / io.DisplaySize.x * lb_strokes_timelineDuration)
			- lb_strokes_selected->global_start_time
			- lb_strokes_selected->enter.duration
			- lb_strokes_selected->exit.duration;
			if(lb_strokes_selected->full_duration < 0) {
				lb_strokes_selected->full_duration = 0;
			} else if(lb_strokes_selected->global_start_time + lb_strokes_selected->enter.duration + lb_strokes_selected->full_duration + lb_strokes_selected->exit.duration > lb_strokes_timelineDuration) {
				lb_strokes_selected->full_duration = lb_strokes_timelineDuration - lb_strokes_selected->global_start_time - lb_strokes_selected->enter.duration - lb_strokes_selected->exit.duration;
			}
		} else if(dragging_handle_enter && ImGui::IsMouseDragging()) {
			float original_full = lb_strokes_selected->full_duration;
			float original_enter = lb_strokes_selected->enter.duration;
			lb_strokes_selected->enter.duration = (ImGui::GetMousePos().x / io.DisplaySize.x * lb_strokes_timelineDuration) - lb_strokes_selected->global_start_time;
			lb_strokes_selected->full_duration -= lb_strokes_selected->enter.duration - original_enter;
			if(lb_strokes_selected->enter.duration < 0) {
				lb_strokes_selected->enter.duration = 0;
				lb_strokes_selected->full_duration = original_full;
			} else if(lb_strokes_selected->full_duration < 0) {
				lb_strokes_selected->full_duration = 0;
				lb_strokes_selected->enter.duration = original_enter;
			}
			
		} else if(dragging_handle_exit && ImGui::IsMouseDragging()) {
			float original_full = lb_strokes_selected->full_duration;
			float original_exit = lb_strokes_selected->exit.duration;
			
			lb_strokes_selected->full_duration = (ImGui::GetMousePos().x / io.DisplaySize.x * lb_strokes_timelineDuration) - lb_strokes_selected->global_start_time - lb_strokes_selected->enter.duration;
			lb_strokes_selected->exit.duration -= lb_strokes_selected->full_duration - original_full;
			if(lb_strokes_selected->exit.duration < 0) {
				lb_strokes_selected->exit.duration = 0;
				lb_strokes_selected->full_duration = original_full;
			} else if(lb_strokes_selected->full_duration < 0) {
				lb_strokes_selected->full_duration = 0;
				lb_strokes_selected->exit.duration = original_exit;
			}
		}
		
		
		draw_list->AddLine(ImVec2(handle_left.x, handle_left.y - 4), ImVec2(handle_right.x, handle_right.y - 4), ImGui::GetColorU32(ImGuiCol_TextDisabled));
		
		if(lb_strokes_selected->enter.animate_method != ANIMATE_NONE) {
			draw_list->AddLine(ImVec2(handle_left.x, handle_left.y), ImVec2(handle_enter.x, handle_enter.y - 4), ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
			draw_list->AddLine(ImVec2(handle_enter.x, handle_enter.y - 3), ImVec2(handle_enter.x, handle_enter.y - 15), ImGui::GetColorU32(mouse_hovering_handle_enter || dragging_handle_enter ? ImGuiCol_PlotHistogram : ImGuiCol_PlotHistogramHovered));
			draw_list->AddRectFilled(ImVec2(handle_enter.x - 6, handle_enter.y - 15), ImVec2(handle_enter.x, handle_enter.y - 9), ImGui::GetColorU32(mouse_hovering_handle_enter || dragging_handle_enter ? ImGuiCol_PlotHistogram : ImGuiCol_PlotHistogramHovered));
		}
		if(lb_strokes_selected->exit.animate_method != ANIMATE_NONE) {
			draw_list->AddLine(ImVec2(handle_exit.x, handle_exit.y - 4), ImVec2(handle_right.x, handle_right.y), ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
			draw_list->AddLine(ImVec2(handle_exit.x, handle_exit.y - 3), ImVec2(handle_exit.x, handle_exit.y - 15), ImGui::GetColorU32(mouse_hovering_handle_enter || dragging_handle_enter ? ImGuiCol_PlotHistogram : ImGuiCol_PlotHistogramHovered));
			draw_list->AddRectFilled(ImVec2(handle_exit.x, handle_exit.y - 15), ImVec2(handle_exit.x + 6, handle_exit.y - 9), ImGui::GetColorU32(mouse_hovering_handle_exit || dragging_handle_exit ? ImGuiCol_PlotHistogram : ImGuiCol_PlotHistogramHovered));
		}
		
		draw_list->AddTriangleFilled(
			handle_left,
			ImVec2(handle_left.x - 6, handle_left.y - 6),
			ImVec2(handle_left.x + 6, handle_left.y - 6),
			ImGui::GetColorU32(mouse_hovering_handle_left || dragging_handle_left ? ImGuiCol_ButtonHovered : ImGuiCol_ButtonActive));
		draw_list->AddTriangleFilled(
			handle_right,
			ImVec2(handle_right.x - 6, handle_right.y - 6),
			ImVec2(handle_right.x + 6, handle_right.y - 6),
			ImGui::GetColorU32(mouse_hovering_handle_right || dragging_handle_right ? ImGuiCol_ButtonHovered : ImGuiCol_ButtonActive));
	}

	ImGui::End();

	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	if((mouse_hovering_playhead || lb_strokes_draggingPlayhead) && input_mode != INPUT_ARTBOARD && input_mode != INPUT_TRIM) {
		// TODO: Calculate this better
		ImGui::SetNextWindowPos(ImVec2(playhead_pos_x - 5.0f - 25.0f, timeline_min.y - style.ItemInnerSpacing.y - 5.0f - 20.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 5.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
		ImGui::BeginTooltip();
		ImGui::Text("%.2fs", lb_strokes_timelinePosition);
		ImGui::EndTooltip();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
	}
}

static void drawTools() {
	
	static const ImVec2 cursor_uv0 = ImVec2(0.0f,1.0f);
	static const ImVec2 cursor_uv1 = ImVec2(0.25f, 0.75f);
	static const ImVec2 pencil_uv0 = ImVec2(0.25f, 1.0f);
	static const ImVec2 pencil_uv1 = ImVec2(0.5f, 0.75f);
	static const ImVec2 artbrd_uv0 = ImVec2(0.5f, 1.0f);
	static const ImVec2 artbrd_uv1 = ImVec2(0.75f, 0.75f);
	static const ImVec2 menu_uv0 = ImVec2(0.75f, 1.0f);
	static const ImVec2 menu_uv1 = ImVec2(1.0f, 0.75f);
	static const ImVec2 pan_uv0 = ImVec2(0.75f, 0.75f);
	static const ImVec2 pan_uv1 = ImVec2(1.0f, 0.5f);
	static const ImVec2 trim_uv0 = ImVec2(0.5f, 0.75f);
	static const ImVec2 trim_uv1 = ImVec2(0.75f, 0.5f);
	
	ImGuiStyle& style = ImGui::GetStyle();
	
	ImGui::SetNextWindowSize(ImVec2(32, 32));
	ImGui::SetNextWindowPos(style.WindowPadding);
	ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	
	ImVec2 const* uv0;
	ImVec2 const* uv1;
	if(drag_mode == DRAG_PAN) {
		uv0 = &pan_uv0;
		uv1 = &pan_uv1;
	} else {
		switch(input_mode) {
			case INPUT_DRAW:
				uv0 = &pencil_uv0;
				uv1 = &pencil_uv1;
				break;
			case INPUT_SELECT:
				uv0 = &cursor_uv0;
				uv1 = &cursor_uv1;
				break;
			case INPUT_ARTBOARD:
				uv0 = &artbrd_uv0;
				uv1 = &artbrd_uv1;
				break;
			case INPUT_TRIM:
				uv0 = &trim_uv0;
				uv1 = &trim_uv1;
				break;
		}
	}
	
	if(ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringWindow() && input_mode != INPUT_ARTBOARD && input_mode != INPUT_TRIM) {
		switch(input_mode) {
			case INPUT_DRAW:
				input_mode = INPUT_SELECT;
				break;
			case INPUT_SELECT:
				input_mode = INPUT_DRAW;
				break;
			default:
				input_mode = INPUT_SELECT;
		}
	}
	
	if(ImGui::IsMouseHoveringWindow() && input_mode != INPUT_ARTBOARD && input_mode != INPUT_TRIM) {
		ImGui::BeginTooltip();
		
		switch(input_mode) {
			case INPUT_DRAW:
				ImGui::Text("Draw");
				break;
			case INPUT_SELECT:
				ImGui::Text("Select");
				break;
		}
		ImGui::EndTooltip();
	}
	
	ImGui::Image((void *)(intptr_t)ui_sprite_texID, ImVec2(16,16), *uv0, *uv1, ImVec4(1,1,1,1));
	
	ImGui::End();	
	
	static bool hovering_settings = false;
	static bool show_settings_menu = false;
	
	
	if(input_mode != INPUT_ARTBOARD && input_mode != INPUT_TRIM) {
		ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImVec4(0,0,0,0)));
		ImGui::SetNextWindowSize(ImVec2(32, 32));
		ImGui::SetNextWindowBgAlpha(0);
		ImGui::SetNextWindowPos(style.WindowPadding + ImVec2(0, 32));
		ImGui::Begin("Settings Button", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
		
		ImGui::Image((void *)(intptr_t)ui_sprite_texID, ImVec2(16,16), menu_uv0, menu_uv1, ImVec4(1,1,1,1));
			
		if(ImGui::IsMouseHoveringWindow()) {
			hovering_settings = true;
			if(ImGui::IsMouseClicked(0)) show_settings_menu = true;
		} else {
			hovering_settings = false;
		}
		
		ImGui::End();
		ImGui::PopStyleColor();
		
		if(hovering_settings && !ImGui::IsPopupOpen("settings_popup")) {
			ImGui::BeginTooltip();
			ImGui::Text("Menu");
			ImGui::EndTooltip();
		}
	}
	
	static bool show_save_modal = false;
	static bool show_open_modal = false;
	static bool show_export_modal = false;
	static char outpath[PATH_MAX];
	if(show_settings_menu) ImGui::OpenPopup("settings_popup");
	if(ImGui::BeginPopup("settings_popup", ImGuiWindowFlags_NoMove)) {
		show_settings_menu = false;
		
		if(ImGui::MenuItem("Open...")) show_open_modal = true;
		if(ImGui::MenuItem("Save...")) show_save_modal = true;
		if(ImGui::MenuItem("Export...")) show_export_modal = true;
		ImGui::Separator();
		
		ImGui::MenuItem("About Linebaby");
		ImGui::Separator();
		
		ImGui::MenuItem("Quit");
		ImGui::EndPopup();
	}
	
	if(show_open_modal) {
		if(FileModal(&show_open_modal, "Open", outpath)) lb_strokes_open(outpath);
	}
	
	if(show_save_modal) {
		if(FileModal(&show_save_modal, "Save", outpath)) lb_strokes_save(outpath);
	}
	
	if(show_export_modal && !ImGui::IsPopupOpen("Export") && input_mode != INPUT_ARTBOARD && input_mode != INPUT_TRIM) {
		ImGui::OpenPopup("Export");
		ImGui::SetNextWindowSize(ImVec2(250, 250));
	}
	
	if(ImGui::BeginPopupModal("Export", &show_export_modal, ImGuiWindowFlags_NoResize)) {
		ImGui::Text("1.");
		ImGui::SameLine();
		if(ImGui::Button("Set Artboard")) {
			input_mode = INPUT_ARTBOARD;
			lb_strokes_artboard_set = false;
			lb_strokes_artboard_set_idx = 0;
			guiState.showFileSettingsPanel = false;
			ImGui::CloseCurrentPopup();
		}
		if(lb_strokes_artboard_set) {
			ImGui::SameLine();
			ImGui::Image((void *)(intptr_t)ui_sprite_texID, ImVec2(16,16), check_icon_uv0, check_icon_uv1, ImVec4(1,1,1,1));
		}
		
		
		ImGui::Text("2.");
		ImGui::SameLine();
		if(ImGui::Button("Set Range")) {
			input_mode = INPUT_TRIM;
			lb_strokes_export_range_set = false;
			lb_strokes_export_range_set_idx = 0;
			ImGui::CloseCurrentPopup();
		}
		if(lb_strokes_export_range_set) {
			ImGui::SameLine();
			ImGui::Image((void *)(intptr_t)ui_sprite_texID, ImVec2(16,16), check_icon_uv0, check_icon_uv1, ImVec4(1,1,1,1));
		}
		
		ImGui::Separator();
		
		static const char* export_types[] = { "Sprite Sheet", "Image Sequence" };
		static struct lb_export_options export_options;
		ImGui::Combo("##Export Type", (int*)&export_options.type, export_types, 2);
		switch(export_options.type) {
			case EXPORT_SPRITESHEET:
				
				break;
			case EXPORT_IMAGE_SEQUENCE:
			
				break;
		}
		
		bool disabled = !lb_strokes_artboard_set || !lb_strokes_export_range_set;
		if(disabled) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
		ImGui::Button("Export");
		if(disabled) {
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
		//if(FileModal(&show_export_modal, "Export", outpath))
		// lb_strokes_render_export("/home/matt/Downloads", 24);
		
		ImGui::EndPopup();
	}
}

static void drawOverlays() {
	if(input_mode == INPUT_ARTBOARD) {
		if(lb_strokes_artboard_set_idx == 0) {
			ImGui::BeginTooltip();
			ImGui::Text("Click to set first corner");
			ImGui::EndTooltip();
		} else if(lb_strokes_artboard_set_idx == 1) {
			ImGui::BeginTooltip();
			ImVec2 mouse = ImGui::GetMousePos();
			ImGui::Text("%d x %d", (int)abs(lb_strokes_artboard[0].x - mouse.x), (int)abs(lb_strokes_artboard[0].y - mouse.y));
			ImGui::EndTooltip();
		}
	} else if(input_mode == INPUT_TRIM) {
		if(lb_strokes_export_range_set_idx == 0) {
			ImGui::BeginTooltip();
			ImGui::Text("Click to set animation start\n%0.2fs", lb_strokes_timelinePosition);
			ImGui::EndTooltip();
		} else if(lb_strokes_export_range_set_idx == 1) {
			ImGui::BeginTooltip();
			ImGui::Text("Click to set animation end\n%0.2fs", lb_strokes_timelinePosition);
			ImGui::EndTooltip();
		}
	}
}

static void drawStrokeProperties() {
	if(!lb_strokes_selected || input_mode == INPUT_ARTBOARD || input_mode == INPUT_TRIM) return;

	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowSize(ImVec2(200, 300));
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 200 - 5, 5));
	ImGui::Begin("Stroke Properties", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	
	ImGui::Text("Color");
	ImGui::ColorEdit4("##Color", (float*)&lb_strokes_selected->color, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_Uint8);

	ImGui::Separator();
	
	ImGui::Text("Thickness");
	ImGui::SliderFloat("##Thickness", &lb_strokes_selected->scale, 1.0f, 15.0f, "pixels = %.2f");
	ImGui::Text("Jitter");
	ImGui::SliderFloat("##Jitter", &lb_strokes_selected->jitter, 0.0f, 0.30f, "%.2f");
	ImGui::Separator();
	
	static const char* animation_mode_combo = "None\0Draw\0Fade\0";
	static const char* animation_ease_combo = "Linear\0Quadratic In\0Quadratic Out\0Quadratic In/Out\0Cubic In\0Cubic Out\0Cubic In/Out\0Quartic In\0Quartic Out\0Quartic In/Out\0Quintic In\0Quintic Out\0Quintic In/Out\0Sine In\0Sine Out\0Sine In/Out\0Circular In\0Circular Out\0Circular In/Out\0Exponential In\0Exponential Out\0Exponential In/Out\0Elastic In\0Elastic Out\0Elastic In/Out\0Back In\0Back Out\0Back In/Out\0Bounce In\0Bounce Out\0Bounce In/Out\0";
	
	ImGui::Text("Entrance Animation");
	if(ImGui::Combo("##enter_method", (int*)&lb_strokes_selected->enter.animate_method, animation_mode_combo)) {
		if(lb_strokes_selected->enter.animate_method == ANIMATE_NONE) lb_strokes_selected->enter.duration = 0;
	}
	if(lb_strokes_selected->enter.animate_method != ANIMATE_NONE) {
		ImGui::Combo("##enter_ease", (int*)&lb_strokes_selected->enter.easing_method, animation_ease_combo);
	}
	
	switch(lb_strokes_selected->enter.animate_method) {
		case ANIMATE_DRAW:
			ImGui::PushID(&lb_strokes_selected->enter.draw_reverse);
			ImGui::Checkbox("Reverse", &lb_strokes_selected->enter.draw_reverse);
			ImGui::PopID();
		default:
			break;
	}
	
	ImGui::Separator();
	
	ImGui::Text("Exit Animation");
	ImGui::Combo("##exit_method", (int*)&lb_strokes_selected->exit.animate_method, animation_mode_combo);
	if(lb_strokes_selected->exit.animate_method != ANIMATE_NONE) {
		ImGui::Combo("##exit_ease", (int*)&lb_strokes_selected->exit.easing_method, animation_ease_combo);
	}
	
	switch(lb_strokes_selected->exit.animate_method) {
		case ANIMATE_DRAW:
			ImGui::PushID(&lb_strokes_selected->exit.draw_reverse);
			ImGui::Checkbox("Reverse", &lb_strokes_selected->exit.draw_reverse);
			ImGui::PopID();
		default:
			break;
	}
	
	/*
	ImGui::Separator();
	
	ImGui::Text("Stroke Thickness");
	draw2PointBezierGraph(&lb_strokes_selected->thickness_curve);
	*/
	
	ImGui::End();
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
	
	#ifdef DEBUG
	if(guiState.showDemoPanel) ImGui::ShowDemoWindow(&guiState.showDemoPanel);
	#endif
	
	drawTools();
	drawTimeline();
	drawOverlays();
	drawStrokeProperties();

	ImGui::Render();
	renderImGuiDrawLists(ImGui::GetDrawData());
}

EXTERN_C bool lb_ui_isDrawingCursor() {
	return ImGui::GetIO().MouseDrawCursor;
}

EXTERN_C bool lb_ui_capturedMouse() {
	return ImGui::GetIO().WantCaptureMouse;
}

EXTERN_C bool lb_ui_capturedKeyboard() {
	return ImGui::GetIO().WantCaptureKeyboard;
}

// TODO: Save these in ImGui state
static char* files[256];
static char* directories[256];
static char cur_path[PATH_MAX+1];
static char out_name[256];
static uint8_t num_files = 0;
static uint8_t num_directories = 0;
static int selected_file_idx = -1;
static DIR* dir;
static char* initial_start_path;

static void readDirectoryContents(const char* path) {
	if(dir) closedir(dir);
	
	strncpy(cur_path, path, PATH_MAX);
	num_files = 0;
	num_directories = 0;
	selected_file_idx = -1;
	
	dir = opendir(path);
	if (dir == NULL) {
		fprintf(stderr, "Could not open directory: %s\n", path);
		return;
	}
 
	struct dirent* entry;
	while((entry = readdir(dir)) != NULL) {
		switch(entry->d_type) {
			case DT_REG:
				files[num_files++] = entry->d_name;
				break;
			case DT_DIR:
				if(strncmp(entry->d_name, ".", 2) == 0) break;
				if(strncmp(entry->d_name, "..", 3) == 0) break;
				directories[num_directories++] = entry->d_name;
				break;
		}
	}
	
	// closedir(dir);
}


static bool FileList(char* selectedPathOut, char* startPath) {
	
	if(initial_start_path != startPath) {
		initial_start_path = startPath;
		readDirectoryContents(initial_start_path);
	}
	
	if(ImGui::ImageButton((void *)(intptr_t)ui_sprite_texID, ImVec2(16,16), up_icon_uv0, up_icon_uv1)) {
		readDirectoryContents(dirname(cur_path));
	}
	
	ImGui::SameLine();
	ImGui::Text(cur_path);
	ImGui::Separator();
	
	ImGui::BeginChild("file_list", ImVec2(ImGui::GetWindowContentRegionWidth(), 240), false);
	
	for(int i = 0; i < num_directories; i++) {
		ImGui::Image((void *)(intptr_t)ui_sprite_texID, ImVec2(16,16), dir_icon_uv0, dir_icon_uv1, ImVec4(1,1,1,1));
		ImGui::SameLine();
		if(ImGui::Selectable(directories[i], false, ImGuiSelectableFlags_DontClosePopups | ImGuiSelectableFlags_SpanAllColumns)) {
			strncat(cur_path, "/", PATH_MAX+1);
			strncat(cur_path, directories[i], PATH_MAX+1);
			readDirectoryContents(cur_path);
		}
	}
	
	for(int i = 0; i < num_files; i++) {
		ImGui::Image((void *)(intptr_t)ui_sprite_texID, ImVec2(16,16), file_icon_uv0, file_icon_uv1, ImVec4(1,1,1,1));		
		ImGui::SameLine();
		if(ImGui::Selectable(files[i], selected_file_idx == i, ImGuiSelectableFlags_DontClosePopups | ImGuiSelectableFlags_SpanAllColumns)) {
			selected_file_idx = i;
			strncpy(out_name, files[i], 256);
		}
	}
	
	ImGui::EndChild();
	
	// TODO: Close dir when done
	return false;
}

static bool FileModal(bool* open, const char* action, char* selectedPathOut) {
	assert(open);
	if(!*open) return false;
	
	if(ImGui::IsKeyPressed(GLFW_KEY_ESCAPE)) {
		*open = false;
		return false;
	}
	
	ImGui::OpenPopup(action);
	bool confirmed_button = false;
	bool confirmed_enter = false;
	
	if(ImGui::IsPopupOpen(action)) ImGui::SetNextWindowSize(ImVec2(250,350));
	if(ImGui::BeginPopupModal(action, open, ImGuiWindowFlags_NoResize)) {
		// TODO: Don't run every time
		char startpath[PATH_MAX];
		getcwd(startpath, PATH_MAX);
		
		FileList(selectedPathOut, startpath);
		confirmed_enter = ImGui::InputText("##filename", out_name, 256, ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_EnterReturnsTrue);
		if(ImGui::Button("Cancel")) {
			*open = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		confirmed_button = ImGui::Button(action);
		
		ImGui::EndPopup();
	}
	if(confirmed_button || confirmed_enter) {
		if(strnlen(out_name, 256) > 0) {
			ImGui::CloseCurrentPopup();
			closedir(dir);
			*open = false;
			strncpy(selectedPathOut, cur_path, PATH_MAX);
			strncat(selectedPathOut, "/", PATH_MAX);
			strncat(selectedPathOut, out_name, PATH_MAX);
			return true;
		}
	}
	return false;
}