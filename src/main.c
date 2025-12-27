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

#define SEARCH_RESULT_HEIGHT 50

static char search_text[100];
static struct search_results results;

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
	struct buffer file;
	if (!database_get(results.ptr[index].name, results.ptr[index].sound_url, &file))
		return false;

	ma_device_stop(&device);
	ma_decoder_uninit(&decoder);

	if (ma_decoder_init_memory(file.data, file.length, NULL, &decoder) != MA_SUCCESS)
		return false;

	printf("playing sound\n");
	if (ma_device_start(&device) != MA_SUCCESS)
		return false;

	return true;
}

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(void)
{
	if (curl_global_init(CURL_GLOBAL_ALL))
		return EXIT_FAILURE;

	ma_context context;
	if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS)
		return EXIT_FAILURE;

	ma_device_info* pPlaybackDeviceInfos;
	ma_uint32 playbackDeviceCount;
	ma_device_info* pCaptureDeviceInfos;
	ma_uint32 captureDeviceCount;
	ma_result result = ma_context_get_devices(&context, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount);
	if (result != MA_SUCCESS)
		return EXIT_FAILURE;

	printf("Playback Devices\n");
	for (ma_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice)
		printf("    %u: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);

	printf("\n");

	printf("Capture Devices\n");
	for (ma_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice)
		printf("    %u: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);

	ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.pDeviceID= &pPlaybackDeviceInfos[0].id;
	deviceConfig.playback.format   = ma_format_f32;
	deviceConfig.playback.channels = 2;
	deviceConfig.sampleRate        = 48000;
	deviceConfig.dataCallback      = data_callback;
	deviceConfig.pUserData         = NULL;
	if (ma_device_init(&context, &deviceConfig, &device) != MA_SUCCESS)
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
	GLFWwindow* window = glfwCreateWindow((int)(600 * main_scale), (int)(900 * main_scale), "Piracy", NULL, NULL);
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

	bool font_found = false;
	for (int i = 0; i < COUNTOF(fonts); ++i)
	{
		if (check_file_access(fonts[i], false, false)) // if file exists
		{
			printf("found font %s\n", fonts[i]);
			ImFontAtlas_AddFontFromFileTTF(io->Fonts, fonts[i], 100, font_config, NULL);
			font_found = true;
			break;
		}
	}

	if (!font_found)
	{
		printf("using default font\n");
		ImFontAtlas_AddFontDefault(io->Fonts, font_config);
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
			ImGuiWindowFlags_NoDocking
		);
		igSetWindowPos_Vec2((ImVec2_c){0, 0}, 0);

		int width, height;
		glfwGetWindowSize(window, &width, &height);
		igSetWindowSize_Vec2((ImVec2_c){width, height}, 0);

		if (first_frame)
			igSetKeyboardFocusHere(0);
		if (igInputText("search", search_text, COUNTOF(search_text), ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL))
		{
			free_search_results(results);
			results = (struct search_results){0};

			if (!search(search_text, &results))
				printf("fail\n");
		}

		igSeparator();

		igBeginChild_Str("results", (ImVec2_c){0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
		for (int i = 0; i < results.length; ++i)
		{
			float height = SEARCH_RESULT_HEIGHT * main_scale;
			float spacing = 10 * main_scale;

			igPushID_Int(i);
			igPushStyleColor_Vec4(ImGuiCol_ChildBg, (ImVec4_c){0.1, 0.1, 0.1, 1});
			igBeginChild_Str("result item", (ImVec2_c){0, height}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse);

			float start_y = igGetCursorPosY();

			float button_size = height - height / 5;
			igSameLine(0, spacing);
			igSetCursorPosY(start_y + (height - button_size) / 2.f);
			if (igButton(">", (ImVec2_c){button_size, button_size}))
			{
				if (!play_result(i))
					printf("failed\n");
			}
			igSameLine(0, spacing);

			igSetCursorPosY(start_y + (height - igGetTextLineHeightWithSpacing()) / 2.f);
			igText(results.ptr[i].name);

			igEndChild();
			igPopStyleColor(1);
			igPopID();
		}
		igEndChild();

		igEnd();

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

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	igDestroyContext(ig_context);

	glfwDestroyWindow(window);
	glfwTerminate();

	ma_decoder_uninit(&decoder);
	ma_device_uninit(&device);
	ma_context_uninit(&context);

	curl_global_cleanup();

	return EXIT_SUCCESS;
}
