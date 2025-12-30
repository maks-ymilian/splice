#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_GLFW
#define CIMGUI_USE_OPENGL3
#include <cimgui.h>
#include <cimgui_impl.h>

#include <curl/curl.h>

#include <miniaudio.h>

#include "search.h"
#include "common.h"
#include "database.h"
#include "platform.h" 
#include "drag_drop.h" 

#define DRAW_DEBUG 0

static struct database* db;

static char search_text[100];
static struct search_results results;
static char* error_message;
static bool error_popup;

static ma_device device;
static ma_decoder decoder;

static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	ma_decoder_read_pcm_frames(&decoder, pOutput, frameCount, NULL);

	(void)pDevice;
	(void)pInput;
}

static bool play_result(int index)
{
	ma_device_stop(&device);
	ma_decoder_uninit(&decoder);

	struct buffer audio;
	if (!database_get(db, results.ptr[index].name, results.ptr[index].audio_url, &audio))
		return false;

	ma_decoder_config decoder_config = ma_decoder_config_init(device.playback.format, device.playback.channels, device.sampleRate);
	if (ma_decoder_init_memory(audio.data, audio.length, &decoder_config, &decoder) != MA_SUCCESS)
		return false;

	if (ma_device_start(&device) != MA_SUCCESS)
		return false;

	return true;
}

static void open_error_popup(char* text)
{
	printf("error: %s\n", text);

	error_message = text;
	error_popup = true;
}

static ImU32 color(int r, int g, int b, int a)
{
	return
		((ImU32)a << 24) |
		((ImU32)b << 16) |
		((ImU32)g << 8) |
		((ImU32)r << 0);
}

static void debug_rect(float x, float y, float width, float height, ImU32 color)
{
	if (!DRAW_DEBUG)
		return;

	x += igGetCursorScreenPos().x;
	y += igGetCursorScreenPos().y;
	ImDrawList_AddRect(
		igGetForegroundDrawList_ViewportPtr(igGetWindowViewport()),
		(ImVec2_c){x, y}, (ImVec2_c){x + width, y + height},
		color, 0, ImDrawFlags_None, 0);
}

static void debug_rect_vector(ImVec2_c pos, ImVec2_c size, ImU32 color)
{
	debug_rect(pos.x, pos.y, size.x, size.y, color);
}

static void center_cursor(float object_size_x, float object_size_y, float area_size_x, float area_size_y)
{
	igSetCursorPosX(igGetCursorPosX() + (area_size_x - object_size_x) / 2);
	igSetCursorPosY(igGetCursorPosY() + (area_size_y - object_size_y) / 2);
}

static void text_box_truncated(char* text, float width, float height)
{
	debug_rect(0, 0, width, height, color(255, 255, 255, 128));

	ImVec2_c text_size = igCalcTextSize(text, NULL, false, false);
	center_cursor(0, text_size.y, 0, height);

	debug_rect(0, 0, width > text_size.x ? text_size.x : width, text_size.y, color(255, 255, 255, 128));
	igTextAligned(0, width, text);
}

static void text_box(char* text, float width, float height)
{
	debug_rect(0, 0, width, height, color(255, 255, 255, 128));

	ImVec2_c text_size = igCalcTextSize(text, NULL, false, false);
	center_cursor(text_size.x, text_size.y, width, height);

	debug_rect(0, 0, text_size.x, text_size.y, color(255, 255, 255, 128));
	igText(text);
}

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(void)
{
	db = database_init("files");

	drag_drop_init();

	if (curl_global_init(CURL_GLOBAL_ALL))
		return EXIT_FAILURE;

	ma_context context;
	if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS)
		return EXIT_FAILURE;

	ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
	device_config.playback.format   = ma_format_unknown;
	device_config.playback.channels = 0;
	device_config.sampleRate        = 0;
	device_config.dataCallback      = data_callback;
	device_config.pUserData         = NULL;
	if (ma_device_init(&context, &device_config, &device) != MA_SUCCESS)
		return EXIT_FAILURE;

	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return EXIT_FAILURE;

	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

	// Setup Dear ImGui context
	// IMGUI_CHECKVERSION();
	ImGuiContext* ig_context = igCreateContext(NULL);
	igSetCurrentContext(ig_context);
	ImGuiIO* io = igGetIO_Nil(); 
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	// Create window with graphics context
	float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
	GLFWwindow* window = glfwCreateWindow((int)(480 * main_scale), (int)(720 * main_scale), "Samples", NULL, NULL);
	if (window == NULL)
		return 1;
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	// Setup Dear ImGui style
	igStyleColorsDark(NULL);
	//igStyleColorsLight();

	// Setup scaling
	ImGuiStyle* style = igGetStyle();
	ImGuiStyle_ScaleAllSizes(style, main_scale); // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style->FontScaleDpi = main_scale; // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Load Fonts
	// - If fonts are not explicitly loaded, Dear ImGui will call AddFontDefault() to select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
	//   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
	// - You can load multiple fonts and use igPushFont()/PopFont() to select them.
	// - If a file cannot be loaded, AddFont functions will return a NULL. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !

	// - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.

	char* fonts[] = {
		"C:/Windows/Fonts/arial.ttf",
		"/mnt/c/Windows/Fonts/arial.ttf",
	};

	ImFontConfig* font_config = ImFontConfig_ImFontConfig();
	style->FontSizeBase = 22;

	for (int i = 0; i < COUNTOF(fonts); ++i)
	{
		if (!check_file_access(fonts[i], true, false))
			continue;

		printf("found font %s\n", fonts[i]);
		ImFontAtlas_AddFontFromFileTTF(io->Fonts, fonts[i], 100, font_config, NULL);
		break;
	}

	ImFontConfig_destroy(font_config);

	// igSetFontRasterizerDensity(10);

	ImVec4 clear_color = (ImVec4){0, 0, 0, 1};

	for (bool first_frame = true; !glfwWindowShouldClose(window); first_frame = false)
	{
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.

		glfwPollEvents();
		if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
		{
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		igNewFrame();

		igBegin("main window", NULL, 
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoDocking);
		{
			igSetWindowPos_Vec2((ImVec2_c){0, 0}, 0);

			ImVec2_c window_size = igGetMainViewport()->WorkSize;
			igSetWindowSize_Vec2(window_size, ImGuiCond_None);
			window_size = igGetWindowViewport()->WorkSize;

			if (first_frame)
				igSetKeyboardFocusHere(0);
			if (igInputText("search", search_text, COUNTOF(search_text), ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL))
			{
				free_search_results(results);
				results = (struct search_results){0};

				if (!search(db, search_text, &results))
					open_error_popup("search failed");
			}

			igSeparator();

			igBeginChild_Str("results", (ImVec2_c){0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
			float full_width = igGetContentRegionAvail().x;
			float spacing = 10;
			float height = 50;
			float button_size = 40;
			float left_width = spacing + button_size;
			float right_width = spacing + button_size;
			float middle_width = full_width - spacing - left_width - right_width;
			{
				for (int i = 0; i < results.length; ++i)
				{
					struct search_result* result = &results.ptr[i];

					igPushID_Int(i);
					igPushStyleColor_Vec4(ImGuiCol_ChildBg, (ImVec4_c){0.1, 0.1, 0.1, 1});
					igBeginChild_Str("result item", (ImVec2_c){0, height}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse);
					{
						igSameLine(0, 0);
						igBeginChild_Str("left_area", (ImVec2_c){left_width, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
						debug_rect_vector((ImVec2_c){0}, igGetContentRegionAvail(), color(255, 0, 0, 255));
						{
							igSameLine(0, spacing);
							center_cursor(0, button_size, 0, height);
							if (igButton(">", (ImVec2_c){button_size, button_size}))
							{
								if (!play_result(i))
									open_error_popup("failed to play search result");
							}

						}
						igEndChild();

						igSameLine(0, 0);
						igBeginChild_Str("middle_area", (ImVec2_c){middle_width, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
						debug_rect_vector((ImVec2_c){0}, igGetContentRegionAvail(), color(0, 255, 0, 255));
						{
							ImVec2_c pos = igGetCursorScreenPos();
							if (result->file_on_disk &&
								igIsMouseHoveringRect(pos, (ImVec2_c){pos.x + middle_width, pos.y + height}, true))
							{
								igSetMouseCursor(ImGuiMouseCursor_Hand);
								if (igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
								{
									drag_drop_start(result->file_on_disk);
									io->MouseDown[0] = false; // the drag drop function blocks and steals the mouse up event so it must be set manually
								}
							}

							igSameLine(0, spacing);
							text_box_truncated(result->name, middle_width - spacing, height);
						}
						igEndChild();

						igSameLine(0, 0);
						igBeginChild_Str("right_area", (ImVec2_c){right_width, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
						debug_rect_vector((ImVec2_c){0}, igGetContentRegionAvail(), color(0, 0, 255, 255));
						{
							igSameLine(0, spacing);
							if (result->file_on_disk)
							{
								center_cursor(0, button_size, 0, height);
								text_box("x", button_size, button_size);
							}
							else
							{
								center_cursor(0, button_size, 0, height);
								if (igButton("v", (ImVec2_c){button_size, button_size}))
								{
									if (!database_get_and_save(db, result->name, result->audio_url))
										open_error_popup("failed to save file");

									update_search_result(db, result);
								}
							}
						}
						igEndChild();
					}
					igEndChild();
					igPopStyleColor(1);
					igPopID();
				}
			}
			igEndChild();
		}
		igEnd();

		if (error_popup)
		{
			igOpenPopup_Str("error", ImGuiPopupFlags_None);
			error_popup = !error_popup;
		}

		ImVec2 center = ImGuiViewport_GetCenter(igGetMainViewport());
		igSetNextWindowPos(center, ImGuiCond_Appearing, (ImVec2_c){0.5f, 0.5f});
		igSetNextWindowSize((ImVec2_c){500, 500}, ImGuiCond_Appearing);
		if (igBeginPopupModal("error", NULL, ImGuiWindowFlags_None))
		{
			igText(error_message ? error_message : "no message");

			if (igButton("close", (ImVec2_c){100, 100}))
				igCloseCurrentPopup();

			igEndPopup();
		}

		// Rendering
		igRender();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());

		glfwSwapBuffers(window);
	}

	free_search_results(results);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	igDestroyContext(ig_context);

	glfwDestroyWindow(window);
	glfwTerminate();

	ma_decoder_uninit(&decoder);
	ma_device_uninit(&device);
	ma_context_uninit(&context);

	curl_global_cleanup();

	drag_drop_uninit();

	database_uninit(db);

	return EXIT_SUCCESS;
}
