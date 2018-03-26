#include "ui.h"

#include <inttypes.h>
#include <imgui/imgui.h>
#include <stdio.h>

EXTERN_C {
	#include "strokes.h"
}

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
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_WindowBg].w = 0;
	style.WindowPadding = ImVec2(0,0);
	style.WindowRounding = 0;
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

static void drawTimeline() {
	const int timeline_height = 18;
	const int handle_height = 18;
	const int total_height = timeline_height + handle_height;

	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, total_height));
	ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - total_height));
	ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	const ImVec2 timeline_min = ImVec2(0, io.DisplaySize.y - timeline_height);
	const ImVec2 timeline_max = ImVec2(io.DisplaySize.x, io.DisplaySize.y);
	float playhead_pos_x = lb_strokes_timelinePosition / lb_strokes_timelineDuration * io.DisplaySize.x;
	draw_list->AddRectFilled(timeline_min, timeline_max, ImGui::GetColorU32(ImGuiCol_Border), 0);
	draw_list->AddRectFilled(timeline_min, ImVec2(timeline_min.x + playhead_pos_x, timeline_max.y), ImGui::GetColorU32(ImGuiCol_FrameBg), 0);
	
	bool mouse_hovering_playhead = ImGui::IsMouseHoveringRect(ImVec2(timeline_min.x + playhead_pos_x - timeline_height/2, timeline_min.y), ImVec2(timeline_min.x + playhead_pos_x + timeline_height/2, timeline_max.y));

	ImGuiCol playhead_color = (lb_strokes_draggingPlayhead || mouse_hovering_playhead) ? ImGuiCol_ButtonHovered : ImGuiCol_ButtonActive;
	draw_list->AddTriangleFilled(
		ImVec2(timeline_min.x + playhead_pos_x, timeline_min.y),
		ImVec2(timeline_min.x + playhead_pos_x - timeline_height/2, timeline_min.y + timeline_height/2),
		ImVec2(timeline_min.x + playhead_pos_x + timeline_height/2, timeline_min.y + timeline_height/2),
		ImGui::GetColorU32(playhead_color));
	draw_list->AddTriangleFilled(
		ImVec2(timeline_min.x + playhead_pos_x - timeline_height/2, timeline_min.y + timeline_height/2),
		ImVec2(timeline_min.x + playhead_pos_x + timeline_height/2, timeline_min.y + timeline_height/2),
		ImVec2(timeline_min.x + playhead_pos_x, timeline_max.y),
		ImGui::GetColorU32(playhead_color));

	if(mouse_hovering_playhead && ImGui::IsMouseClicked(0)) {
		lb_strokes_draggingPlayhead = true;
	} else if(ImGui::IsMouseReleased(0)) {
		lb_strokes_draggingPlayhead = false;
	}

	if(mouse_hovering_playhead || lb_strokes_draggingPlayhead) {
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

	if(lb_strokes_draggingPlayhead && ImGui::IsMouseDragging()) {
		lb_strokes_setTimelinePosition(ImGui::GetMousePos().x / io.DisplaySize.x * lb_strokes_timelineDuration);
	}

	struct lb_stroke* selected = lb_strokes_getSelectedStroke();
	if(selected) {
		float handle_l_x = selected->global_start_time / lb_strokes_timelineDuration * io.DisplaySize.x;
		float handle_r_x = (selected->global_start_time + selected->global_duration) / lb_strokes_timelineDuration * io.DisplaySize.x;
		ImVec2 handle_l = ImVec2(handle_l_x, timeline_min.y - 3);
		ImVec2 handle_r = ImVec2(handle_r_x, timeline_min.y - 3);

		draw_list->AddLine(ImVec2(handle_l.x, handle_l.y - 4), ImVec2(handle_r.x, handle_r.y - 4), ImGui::GetColorU32(ImGuiCol_TextDisabled));

		draw_list->AddTriangleFilled(
			handle_l, ImVec2(handle_l.x - 6, handle_l.y - 6), ImVec2(handle_l.x + 6, handle_l.y - 6), ImGui::GetColorU32(playhead_color));
		draw_list->AddTriangleFilled(
			handle_r, ImVec2(handle_r.x - 6, handle_r.y - 6), ImVec2(handle_r.x + 6, handle_r.y - 6), ImGui::GetColorU32(playhead_color));
	}

	// draw_list->AddLine(ImVec2(pos.x, pos.y + handleHeight/2), ImVec2(pos.x + io.DisplaySize.x - style.WindowPadding.x*2, pos.y + handleHeight/2), ImGui::GetColorU32(ImGuiCol_TextDisabled));
	
	// ImVec2 playheadTop = pos;
	// ImVec2 playheadBottom = ImVec2(playheadTop.x, playheadTop.y + timelineHeight - style.WindowPadding.y*2);
 //    draw_list->AddLine(playheadTop, playheadBottom, ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
	// draw_list->AddTriangleFilled(
	// 	ImVec2(playheadBottom.x - 4, playheadBottom.y - 10),
	// 	ImVec2(playheadBottom.x + 4, playheadBottom.y - 10),
	// 	ImVec2(playheadBottom.x, playheadBottom.y - 5 - 10),
	// 	ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
	// draw_list->AddRectFilled(ImVec2(playheadBottom.x - 4, playheadBottom.y - 10), ImVec2(playheadBottom.x + 4, playheadBottom.y), ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
	
	// ImVec2 h1_min = pos;
	// ImVec2 h1_max = ImVec2(pos.x + handleWidth, pos.y + handleHeight);
	// ImVec2 h2_min = ImVec2(pos.x + 200, pos.y);
	// ImVec2 h2_max = ImVec2(pos.x + 200 + handleWidth, pos.y + handleHeight);
	
	// bool h1_hovered = ImGui::IsMouseHoveringRect(h1_min, h1_max);
	// bool h2_hovered = ImGui::IsMouseHoveringRect(h2_min, h2_max);

	// draw_list->AddRectFilled(h1_min, h1_max, ImGui::GetColorU32(h1_hovered ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.FrameRounding);
 //    draw_list->AddRectFilled(h2_min, h2_max, ImGui::GetColorU32(h2_hovered ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.FrameRounding);

	/*
	float radius_outer = 20.0f;
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 center = ImVec2(pos.x + radius_outer, pos.y + radius_outer);
	float line_height = ImGui::GetTextLineHeight();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	float ANGLE_MIN = 3.141592f * 0.75f;
	float ANGLE_MAX = 3.141592f * 2.25f;

	ImGui::InvisibleButton(label, ImVec2(radius_outer*2, radius_outer*2 + line_height + style.ItemInnerSpacing.y));
	bool value_changed = false;
	bool is_active = ImGui::IsItemActive();
	bool is_hovered = ImGui::IsItemActive();
	if (is_active && io.MouseDelta.x != 0.0f)
	{
		float step = (v_max - v_min) / 200.0f;
		*p_value += io.MouseDelta.x * step;
		if (*p_value < v_min) *p_value = v_min;
		if (*p_value > v_max) *p_value = v_max;
		value_changed = true;
	}

	float t = (*p_value - v_min) / (v_max - v_min);
	float angle = ANGLE_MIN + (ANGLE_MAX - ANGLE_MIN) * t;
	float angle_cos = cosf(angle), angle_sin = sinf(angle);
	float radius_inner = radius_outer*0.40f;
	
	draw_list->AddCircleFilled(center, radius_outer, ImGui::GetColorU32(ImGuiCol_FrameBg), 16);
	draw_list->AddLine(ImVec2(center.x + angle_cos*radius_inner, center.y + angle_sin*radius_inner), ImVec2(center.x + angle_cos*(radius_outer-2), center.y + angle_sin*(radius_outer-2)), ImGui::GetColorU32(ImGuiCol_SliderGrabActive), 2.0f);
	draw_list->AddCircleFilled(center, radius_inner, ImGui::GetColorU32(is_active ? ImGuiCol_FrameBgActive : is_hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 16);
	draw_list->AddText(ImVec2(pos.x, pos.y + radius_outer * 2 + style.ItemInnerSpacing.y), ImGui::GetColorU32(ImGuiCol_Text), label);

	if (is_active || is_hovered)
	{
		ImGui::SetNextWindowPos(ImVec2(pos.x - style.WindowPadding.x, pos.y - line_height - style.ItemInnerSpacing.y - style.WindowPadding.y));
		ImGui::BeginTooltip();
		ImGui::Text("%.3f", *p_value);
		ImGui::EndTooltip();
	}

	return value_changed;
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
	
	drawMainMenuBar();
	if(guiState.showDemoPanel) ImGui::ShowDemoWindow(&guiState.showDemoPanel);
	
	drawTimeline();

	ImGui::Render();
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