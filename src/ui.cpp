#include "ui.h"

#include <inttypes.h>
#include <imgui/imgui.h>
#include <stdio.h>

#include <stdlib.h>

EXTERN_C {
	#include "gl.h"
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
	bool showDemoPanel = true;
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
	
	// Load custom spritesheet
	uint32_t ui_sprite_width, ui_sprite_height;
	bool ui_sprite_alpha;
	GLubyte* ui_sprite_pix = loadPNG("src/assets/images/ui.png", &ui_sprite_width, &ui_sprite_height, &ui_sprite_alpha);
	assert(ui_sprite_pix);
	
	
	glGenTextures(1, &ui_sprite_texID);
	glBindTexture(GL_TEXTURE_2D, ui_sprite_texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ui_sprite_width, ui_sprite_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ui_sprite_pix);

	free(ui_sprite_pix);
}

EXTERN_C void lb_ui_destroy(void(*glDestroy)()) {
	ImGui::GetIO().Fonts->TexID = 0;
	ImGui::Shutdown();
	glDestroy();
}

static void drawTimeline() {
	const int timeline_height = 18;
	const int handle_height = 18;
	const int total_height = timeline_height + handle_height;

	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
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
	bool mouse_hovering_timeline = ImGui::IsMouseHoveringRect(timeline_min, timeline_max);

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
		const float handle_enter_time = handle_left_time + (lb_strokes_selected->enter.method == ANIMATE_NONE ? 0 : lb_strokes_selected->enter.duration);
		const float handle_exit_time = handle_enter_time + lb_strokes_selected->full_duration;
		const float handle_right_time = handle_exit_time + (lb_strokes_selected->exit.method == ANIMATE_NONE ? 0 : lb_strokes_selected->exit.duration);
		
		ImVec2 handle_left = ImVec2(handle_left_time / lb_strokes_timelineDuration * io.DisplaySize.x, timeline_min.y - 3);
		ImVec2 handle_enter = ImVec2(handle_enter_time / lb_strokes_timelineDuration * io.DisplaySize.x, timeline_min.y - 3);
		ImVec2 handle_exit = ImVec2(handle_exit_time / lb_strokes_timelineDuration * io.DisplaySize.x, timeline_min.y - 3);
		ImVec2 handle_right = ImVec2(handle_right_time / lb_strokes_timelineDuration * io.DisplaySize.x, timeline_min.y - 3);
		
		bool mouse_hovering_handle_left = ImGui::IsMouseHoveringRect(ImVec2(handle_left.x - 6, handle_left.y - 6), ImVec2(handle_left.x + 6, handle_left.y));
		bool mouse_hovering_handle_right = ImGui::IsMouseHoveringRect(ImVec2(handle_right.x - 6, handle_right.y - 6), ImVec2(handle_right.x + 6, handle_right.y));
		
		bool mouse_hovering_handle_enter;
		if(lb_strokes_selected->enter.method != ANIMATE_NONE) mouse_hovering_handle_enter = ImGui::IsMouseHoveringRect(ImVec2(handle_enter.x - 6, handle_enter.y - 15), ImVec2(handle_enter.x, handle_enter.y - 9));
		bool mouse_hovering_handle_exit;
		if(lb_strokes_selected->exit.method != ANIMATE_NONE) mouse_hovering_handle_exit = ImGui::IsMouseHoveringRect(ImVec2(handle_exit.x, handle_exit.y - 15), ImVec2(handle_exit.x + 6, handle_exit.y - 9));
		
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
			if(lb_strokes_selected->enter.duration == 0) lb_strokes_selected->enter.method = ANIMATE_NONE;
		}
		if(mouse_hovering_handle_exit && ImGui::IsMouseClicked(0)) {
			dragging_handle_exit = true;
		} else if(ImGui::IsMouseReleased(0)) {
			dragging_handle_exit = false;
			if(lb_strokes_selected->exit.duration == 0) lb_strokes_selected->exit.method = ANIMATE_NONE;
		}
		if(mouse_hovering_handle_right && ImGui::IsMouseClicked(0)) {
			dragging_handle_right = true;
		} else if(ImGui::IsMouseReleased(0)) {
			dragging_handle_right = false;
		}

		if(dragging_handle_left && ImGui::IsMouseDragging()) {
			lb_strokes_selected->global_start_time = ImGui::GetMousePos().x / io.DisplaySize.x * lb_strokes_timelineDuration;
		
		} else if(dragging_handle_right && ImGui::IsMouseDragging()) {
			lb_strokes_selected->full_duration = (ImGui::GetMousePos().x / io.DisplaySize.x * lb_strokes_timelineDuration)
			- lb_strokes_selected->global_start_time
			- lb_strokes_selected->enter.duration
			- lb_strokes_selected->exit.duration;
			if(lb_strokes_selected->full_duration < 0) {
				lb_strokes_selected->full_duration = 0;
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
		
		if(lb_strokes_selected->enter.method != ANIMATE_NONE) {
			draw_list->AddLine(ImVec2(handle_left.x, handle_left.y), ImVec2(handle_enter.x, handle_enter.y - 4), ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
			draw_list->AddLine(ImVec2(handle_enter.x, handle_enter.y - 3), ImVec2(handle_enter.x, handle_enter.y - 15), ImGui::GetColorU32(mouse_hovering_handle_enter || dragging_handle_enter ? ImGuiCol_PlotHistogram : ImGuiCol_PlotHistogramHovered));
			draw_list->AddRectFilled(ImVec2(handle_enter.x - 6, handle_enter.y - 15), ImVec2(handle_enter.x, handle_enter.y - 9), ImGui::GetColorU32(mouse_hovering_handle_enter || dragging_handle_enter ? ImGuiCol_PlotHistogram : ImGuiCol_PlotHistogramHovered));
		}
		if(lb_strokes_selected->exit.method != ANIMATE_NONE) {
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
	ImGui::PopStyleColor();

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
}

static void drawTools() {
	
	static const ImVec2 cursor_uv0 = ImVec2(0.0f,1.0f);
	static const ImVec2 cursor_uv1 = ImVec2(0.25f, 0.75f);
	static const ImVec2 pencil_uv0 = ImVec2(0.25f, 1.0f);
	static const ImVec2 pencil_uv1 = ImVec2(0.5f, 0.75f);
	
	//ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	
	ImColor bg = style.Colors[ImGuiCol_WindowBg];
	bg.Value.w = 0.2f;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, bg.Value);
	ImGui::SetNextWindowSize(ImVec2(32, 32));
	ImGui::SetNextWindowPos(style.WindowPadding);
	ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	
	ImVec2 const* uv0;
	ImVec2 const* uv1;
	switch(input_mode) {
		case INPUT_DRAW:
			uv0 = &pencil_uv0;
			uv1 = &pencil_uv1;
			break;
		case INPUT_SELECT:
			uv0 = &cursor_uv0;
			uv1 = &cursor_uv1;
			break;
	}
	
	if(ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringWindow()) {
		switch(input_mode) {
			case INPUT_DRAW:
				input_mode = INPUT_SELECT;
				break;
			case INPUT_SELECT:
				input_mode = INPUT_DRAW;
				break;
		}
	}
	
	ImGui::Image((void *)(intptr_t)ui_sprite_texID, ImVec2(16,16), *uv0, *uv1, ImVec4(1,1,1,1));
	
	ImGui::End();
	ImGui::PopStyleColor();
}

static void drawStrokeProperties() {
	if(!lb_strokes_selected) return;

	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	ImColor bg = style.Colors[ImGuiCol_WindowBg];
	bg.Value.w = 0.2f;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, bg.Value);
	ImGui::SetNextWindowSize(ImVec2(200, 300));
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 200 - 5, 5));
	ImGui::Begin("Stroke Properties", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

	ImGui::Text("Thickness");
	ImGui::SliderFloat("##Thickness", &lb_strokes_selected->scale, 1.0f, 15.0f, "pixels = %.2f");
	
	ImGui::Separator();
	
	static const char* animation_mode_combo = "None\0Draw\0Fade\0";
	
	ImGui::Text("Entrance Animation");
	ImGui::Combo("##enter_method", (int*)&lb_strokes_selected->enter.method, animation_mode_combo);
	switch(lb_strokes_selected->enter.method) {
		case ANIMATE_DRAW:
			ImGui::Checkbox("Reverse", &lb_strokes_selected->enter.draw_reverse);
		default:
			break;
	}
	
	ImGui::Separator();
	
	ImGui::Text("Exit Animation");
	ImGui::Combo("##exit_method", (int*)&lb_strokes_selected->exit.method, animation_mode_combo);
	switch(lb_strokes_selected->exit.method) {
		case ANIMATE_DRAW:
			ImGui::Checkbox("Reverse", &lb_strokes_selected->exit.draw_reverse);
		default:
			break;
	}
	
	ImGui::End();
	ImGui::PopStyleColor();
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
	
	// if(guiState.showDemoPanel) ImGui::ShowDemoWindow(&guiState.showDemoPanel);
	drawTools();
	drawTimeline();
	drawStrokeProperties();

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