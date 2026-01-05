#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_GLFW
#define CIMGUI_USE_OPENGL3
#include <cimgui.h>
#include <cimgui_impl.h>

#include <miniaudio.h>

#include "search.h"
#include "common.h"
#include "database.h"
#include "drag_drop.h" 
#include "file_utils.h" 

#define DRAW_DEBUG 0

enum align_type
{
	ALIGN_TYPE_CENTER = 0,
	ALIGN_TYPE_LEFT = 1,
	ALIGN_TYPE_RIGHT = 2,
	ALIGN_TYPE_TOP = 4,
	ALIGN_TYPE_BOTTOM = 8,
};

static char* sort_names[] = {
	[SEARCH_SORT_MOST_RELEVANT] = "most relevant",
	[SEARCH_SORT_MOST_POPULAR] = "most popular",
	[SEARCH_SORT_MOST_RECENT] = "most recent",
	[SEARCH_SORT_RANDOM] = "random",
};

static char* sample_type_names[] = {
	[SEARCH_SAMPLE_TYPE_ONE_SHOTS] = "one shots",
	[SEARCH_SAMPLE_TYPE_LOOPS] = "loops",
	[SEARCH_SAMPLE_TYPE_ANY] = "one shots & loops",
};

static char* scale_names[] = {
	[SEARCH_SCALE_MAJOR] = "major",
	[SEARCH_SCALE_MINOR] = "minor",
};

static char* key_names[] = {
	[SEARCH_KEY_ANY] = "all",
	[SEARCH_KEY_C] = "C",
	[SEARCH_KEY_C_SHARP] = "C#",
	[SEARCH_KEY_D] = "D",
	[SEARCH_KEY_D_SHARP] = "D#",
	[SEARCH_KEY_E] = "E",
	[SEARCH_KEY_F] = "F",
	[SEARCH_KEY_F_SHARP] = "F#",
	[SEARCH_KEY_G] = "G",
	[SEARCH_KEY_G_SHARP] = "G#",
	[SEARCH_KEY_A] = "A",
	[SEARCH_KEY_A_SHARP] = "A#",
	[SEARCH_KEY_B] = "B",
};

static struct database* db;

static char search_text[100];
static char bpm_min_text[4];
static char bpm_max_text[4];
static bool bpm_range = false;
static bool prev_filter_window_opened = false;

static enum search_sample_type selected_sample_type = SEARCH_SAMPLE_TYPE_ANY;
static enum search_sort selected_sort = SEARCH_SORT_MOST_POPULAR;
static enum search_scale selected_scale = SEARCH_SCALE_ANY;
static enum search_key selected_key = SEARCH_KEY_ANY;
static struct search_tag* tags;
static int tags_length;
static int min_bpm;
static int max_bpm;

static struct search_session* search_session;
static struct search_context* search_context;

static char* currently_playing_name;

static char* error_message;
static bool error_popup;

static ma_device device;
static ma_decoder decoder;
static ma_uint64 decoder_length;

static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	(void)pDevice;
	(void)pInput;

	ma_decoder_read_pcm_frames(&decoder, pOutput, frameCount, NULL);

	ma_uint64 cursor;
	if (ma_decoder_get_cursor_in_pcm_frames(&decoder, &cursor)) return;

	if (cursor >= decoder_length)
		currently_playing_name = NULL;
}

static void on_device_notification(const ma_device_notification* notification)
{
	if (notification->type != ma_device_notification_type_stopped) return;
	currently_playing_name = NULL;
}

static void stop_playback(void)
{
	if (currently_playing_name) ma_device_stop(&device);
	currently_playing_name = NULL;
}

static bool toggle_play(struct search_item_data result)
{
	if (currently_playing_name == result.name)
	{
		ma_device_stop(&device);
		return true;
	}

	ma_device_stop(&device);

	struct buffer audio;
	if (!database_get(db, result.name, result.audio_url, &audio)) return false;

	ma_decoder_uninit(&decoder);
	ma_decoder_config decoder_config = ma_decoder_config_init(device.playback.format, device.playback.channels, device.sampleRate);
	if (ma_decoder_init_memory(audio.data, audio.length, &decoder_config, &decoder)) return false;
	if (ma_decoder_get_length_in_pcm_frames(&decoder, &decoder_length)) return false;
	if (ma_device_start(&device)) return false;

	currently_playing_name = result.name;
	return true;
}

static bool update_search_item(struct database* db, struct search_item_data* item_data)
{
	if (!db || !item_data) return false;

	bool exists;
	file_string path;
	int path_length;
	if (!database_get_file_path(db, item_data->name, &exists, path, &path_length))
		return false;
	else if (exists)
	{
		item_data->file_on_disk = string_alloc(path, path_length);
		if (!item_data->file_on_disk) return false;
	}
	else 
		item_data->file_on_disk = NULL;

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

static void align_cursor(enum align_type align, float object_size_x, float object_size_y, float area_size_x, float area_size_y)
{
	if ((align & ALIGN_TYPE_LEFT) && (align & ALIGN_TYPE_RIGHT))
		align &= ~(ALIGN_TYPE_LEFT | ALIGN_TYPE_RIGHT);
	if ((align & ALIGN_TYPE_TOP) && (align & ALIGN_TYPE_BOTTOM))
		align &= ~(ALIGN_TYPE_TOP | ALIGN_TYPE_BOTTOM);

	if (align & ALIGN_TYPE_RIGHT)
		igSetCursorPosX(igGetCursorPosX() + area_size_x - object_size_x);
	else if (!(align & ALIGN_TYPE_LEFT))
		igSetCursorPosX(igGetCursorPosX() + (area_size_x - object_size_x) / 2);

	if (align & ALIGN_TYPE_BOTTOM)
		igSetCursorPosY(igGetCursorPosY() + area_size_y - object_size_y);
	else if (!(align & ALIGN_TYPE_TOP))
		igSetCursorPosY(igGetCursorPosY() + (area_size_y - object_size_y) / 2);
}

static void text_box_truncated(char* text, float width, float height, enum align_type align)
{
	ImVec2_c pos = igGetCursorPos();

	debug_rect(0, 0, width, height, color(255, 255, 255, 128));

	ImVec2_c text_size = igCalcTextSize(text, NULL, false, false);
	align_cursor(align, 0, text_size.y, 0, height);

	debug_rect(0, 0, width > text_size.x ? text_size.x : width, text_size.y, color(255, 255, 255, 128));
	igTextAligned(0, width, text);

	igSetCursorPos(pos);
	ImRect_c rect = {(ImVec2_c){pos.x, pos.y}, (ImVec2_c){width, height}};
    igItemSize_Rect(rect, 0);
	igItemAdd(rect, igGetID_Str(text), NULL, ImGuiItemFlags_None);
}

static void text_box(char* text, float width, float height, enum align_type align)
{
	debug_rect(0, 0, width, height, color(255, 255, 255, 128));

	ImVec2_c text_size = igCalcTextSize(text, NULL, false, false);
	align_cursor(align, text_size.x, text_size.y, width, height);

	debug_rect(0, 0, text_size.x, text_size.y, color(255, 255, 255, 128));
	igText(text);
}

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void search(void)
{
	search_session_uninit(search_session);

	char** uuids = NULL;

	struct search_query query = {0};
	query.search_string = search_text;
	if (selected_sample_type != SEARCH_SAMPLE_TYPE_ANY)
	{
		query.used_parameters |= SEARCH_QUERY_PARAMETERS_SAMPLE_TYPE;
		query.sample_type = selected_sample_type;
	}
	if (selected_sort != SEARCH_SORT_MOST_POPULAR)
	{
		query.used_parameters |= SEARCH_QUERY_PARAMETERS_SORT;
		query.sort = selected_sort;
	}
	if (selected_scale != SEARCH_SCALE_ANY)
	{
		query.used_parameters |= SEARCH_QUERY_PARAMETERS_SCALE;
		query.scale = selected_scale;
	}
	if (selected_key != SEARCH_KEY_ANY)
	{
		query.used_parameters |= SEARCH_QUERY_PARAMETERS_KEY;
		query.key = selected_key;
	}
	if (tags != NULL && tags_length > 0)
	{
		query.used_parameters |= SEARCH_QUERY_PARAMETERS_TAGS;
		uuids = malloc(tags_length * sizeof(char*));
		for (int i = 0; i < tags_length; ++i)
			uuids[i] = tags[i].uuid;
		query.tags = (struct search_tags){.tags = uuids, .length = tags_length};
	}
	if (min_bpm != 0 && max_bpm != 0)
	{
		query.used_parameters |= SEARCH_QUERY_PARAMETERS_BPM;
		query.bpm_range = (struct search_bpm_range){.min = min_bpm, .max = max_bpm};
	}

	if (!(search_session = search_session_init(search_context, query)) ||
		!search_session_fetch_next_page(search_session))
		open_error_popup("search failed");
	
	free(uuids);
}

int main(void)
{
	drag_drop_init();

	if (!(db = database_init("C:\\REAPER\\Samples\\splice")) ||
		!(search_context = search_context_init(db, update_search_item, 20)))
		return EXIT_FAILURE;

	ma_context context;
	if (ma_context_init(NULL, 0, NULL, &context))
		return EXIT_FAILURE;

	ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
	device_config.playback.format = ma_format_unknown;
	device_config.playback.channels = 0;
	device_config.sampleRate = 0;
	device_config.dataCallback = data_callback;
	device_config.pUserData = NULL;
	device_config.notificationCallback = on_device_notification;
	if (ma_device_init(&context, &device_config, &device))
		return EXIT_FAILURE;

	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return EXIT_FAILURE;

	// GL 3.0 + GLSL 130
	char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	// glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
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
	GLFWwindow* window = glfwCreateWindow((int)(580 * main_scale), (int)(580 * main_scale), "Samples", NULL, NULL);
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
	style->FontScaleDpi = main_scale * 0.75; // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	char* fonts[] = {
		"C:/Windows/Fonts/arial.ttf",
		"/mnt/c/Windows/Fonts/arial.ttf",
	};

	ImFont* normal_font = NULL;
	for (int i = 0; i < COUNTOF(fonts); ++i)
	{
		if (!is_file_accessible(fonts[i], string_length(fonts[i]), true, false, false))
			continue;

		ImFontConfig* font_config = ImFontConfig_ImFontConfig();
		normal_font = ImFontAtlas_AddFontFromFileTTF(io->Fonts, fonts[i], 22, font_config, NULL);
		ImFontConfig_destroy(font_config);
		break;
	}

	// style->FontSizeBase = 22;

	// igSetFontRasterizerDensity(10);

	ImVec4 clear_color = (ImVec4){0, 0, 0, 1};

	char* currently_dragging_name = NULL;
	bool mouse_left_dragging_item = false;
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
			stop_playback();
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}
		if (glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0)
			stop_playback();

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

			float full_width = igGetContentRegionAvail().x;

			bool filter_window_opened = false;
			{
				int num_columns = 3;
				float column_spacing = igGetCursorPosX();
				float column_width = (float)full_width / num_columns;
				igSameLine(0, 0);
				igSetNextItemWidth(column_width - column_spacing);

				char name[64];
				int name_length = 0;
				if (selected_key == SEARCH_KEY_ANY)
				{
					char all_keys[] = "all keys";
					char keys[] = " keys";
					if (selected_scale == SEARCH_SCALE_ANY)
						string_concat(name, &name_length, COUNTOF(name), all_keys, COUNTOF(all_keys) - 1);
					else
					{
						string_concat(name, &name_length, COUNTOF(name), scale_names[selected_scale], string_length(scale_names[selected_scale]));
						string_concat(name, &name_length, COUNTOF(name), keys, COUNTOF(keys) - 1);
					}
				}
				else
				{
					string_concat(name, &name_length, COUNTOF(name), key_names[selected_key], string_length(key_names[selected_key]));
					if (selected_scale != SEARCH_SCALE_ANY)
					{
						string_concat(name, &name_length, COUNTOF(name), " ", 1);
						string_concat(name, &name_length, COUNTOF(name), scale_names[selected_scale], string_length(scale_names[selected_scale]));
					}
				}

				if (igBeginCombo("##key_dropdown", name, ImGuiComboFlags_HeightLargest))
				{
					filter_window_opened = true;

					bool checked = selected_scale & SEARCH_SCALE_MAJOR;
					if (igCheckbox("major", &checked))
					{
						selected_scale ^= SEARCH_SCALE_MAJOR;
						
						if (selected_scale == (SEARCH_SCALE_MAJOR | SEARCH_SCALE_MINOR))
							selected_scale = SEARCH_SCALE_MAJOR;
					}

					checked = selected_scale & SEARCH_SCALE_MINOR;
					if (igCheckbox("minor", &checked))
					{
						selected_scale ^= SEARCH_SCALE_MINOR;

						if (selected_scale == (SEARCH_SCALE_MAJOR | SEARCH_SCALE_MINOR))
							selected_scale = SEARCH_SCALE_MINOR;
					}

					igSeparator();

					for (int i = 0; i < SEARCH_KEY_LENGTH; ++i)
					{
						if (igSelectable_Bool(key_names[i], selected_key == i, ImGuiSelectableFlags_None, (ImVec2_c){0, 0}))
							selected_key = i;
					}
					igEndCombo();
				}
				igSameLine(0, column_spacing);
				igSetNextItemWidth(column_width - column_spacing);
				if (igBeginCombo("##type_dropdown", sample_type_names[selected_sample_type], ImGuiComboFlags_HeightLargest))
				{
					filter_window_opened = true;

					bool checked = selected_sample_type & SEARCH_SAMPLE_TYPE_ONE_SHOTS;
					if (igCheckbox(sample_type_names[SEARCH_SAMPLE_TYPE_ONE_SHOTS], &checked))
					{
						selected_sample_type ^= SEARCH_SAMPLE_TYPE_ONE_SHOTS;

						if (selected_sample_type == (SEARCH_SAMPLE_TYPE_ONE_SHOTS | SEARCH_SAMPLE_TYPE_LOOPS))
							selected_sample_type = SEARCH_SAMPLE_TYPE_ONE_SHOTS;
					}

					checked = selected_sample_type & SEARCH_SAMPLE_TYPE_LOOPS;
					if (igCheckbox(sample_type_names[SEARCH_SAMPLE_TYPE_LOOPS], &checked))
					{
						selected_sample_type ^= SEARCH_SAMPLE_TYPE_LOOPS;

						if (selected_sample_type == (SEARCH_SAMPLE_TYPE_ONE_SHOTS | SEARCH_SAMPLE_TYPE_LOOPS))
							selected_sample_type = SEARCH_SAMPLE_TYPE_LOOPS;
					}

					igEndCombo();
				}
				igSameLine(0, column_spacing);
				igSetNextItemWidth(column_width);
				if (igBeginCombo("##sort_dropdown", sort_names[selected_sort], ImGuiComboFlags_HeightLargest))
				{
					filter_window_opened = true;

					for (int i = 0; i < SEARCH_SORT_LENGTH; ++i)
					{
						if (igSelectable_Bool(sort_names[i], selected_sort == i, ImGuiSelectableFlags_None, (ImVec2_c){0, 0}))
							selected_sort = i;
					}
					igEndCombo();
				}

				igNewLine();
			}

			{
				int num_columns = 2;
				float column_spacing = igGetCursorPosX();
				float column_width = (float)full_width / num_columns;
				igSameLine(0, 0);
				igSetNextItemWidth(column_width - column_spacing);

				igSameLine(0, 0);
				igSetNextItemWidth(column_width - column_spacing);

				char tags_text[32];
				if (tags_length == 0)
					snprintf(tags_text, COUNTOF(tags_text), "no tags");
				else if (tags_length == 1)
					snprintf(tags_text, COUNTOF(tags_text), "1 tag");
				else
					snprintf(tags_text, COUNTOF(tags_text), "%d tags", tags_length);
				if (igBeginCombo("##tags_dropdown",tags_text, ImGuiComboFlags_HeightLargest))
				{
					filter_window_opened = true;

					float full_width = igGetContentRegionAvail().x;

					if (igButton("clear", (ImVec2_c){0}))
					{
						for (int i = 0; i < tags_length; ++i)
						{
							free(tags[i].name);
							free(tags[i].uuid);
						}
						free(tags);
						tags = NULL;
						tags_length = 0;
					}

					igSeparator();
					igText("applied tags:");

					igPushID_Str("applied_tags");
					for (int i = 0; i < tags_length && tags; ++i)
					{
						igPushID_Int(i);
						if (igButton(tags[i].name, (ImVec2_c){full_width, 0}))
						{
							free(tags[i].name);
							free(tags[i].uuid);
							if (i != tags_length - 1)
								memmove(tags + i, tags + i + 1, (tags_length - i - 1) * sizeof(*tags));

							--tags_length;
							--i;
						}
						igPopID();
					}
					igPopID();
					if (tags_length == 0)
						igText("none");

					igSeparator();
					igText("available tags:");
					igPushID_Str("tag_summary");
					float spacing = igGetCursorPosX();
					if (search_session && search_session->tag_summary)
					{
						for (int i = 0; i < search_session->tag_summary_length; ++i)
						{
							igPushID_Int(i);

							float text_width = igCalcTextSize(search_session->tag_summary[i].name, NULL, false, false).x;
							float button_width = text_width + 20;
							float button_height = 30;
							float next_cursor_pos = igGetCursorPosX() + button_width + spacing;
							if (next_cursor_pos > full_width)
							{
								igSetCursorPosY(igGetCursorPosY() + button_height + spacing);
								igSetCursorPosX(spacing);
								next_cursor_pos = igGetCursorPosX() + button_width + spacing;
							}
							float cursor_y = igGetCursorPosY();
							if (igButton(search_session->tag_summary[i].name, (ImVec2_c){button_width, button_height}))
							{
								char* name = search_session->tag_summary[i].name;
								char* uuid = search_session->tag_summary[i].uuid;
								int uuid_length = string_length(uuid);
								int name_length = string_length(name);

								bool exists = false;
								for (int i = 0; i < tags_length; ++i)
								{
									if (tags[i].uuid && strcmp(tags[i].uuid, uuid) == 0)
									{
										exists = true;
										break;
									}
								}

								if (!exists && uuid[0] != '\0')
								{
									++tags_length;
									tags = realloc(tags, tags_length * sizeof(*tags));

									tags[tags_length - 1].uuid = malloc(uuid_length + 1);
									memcpy(tags[tags_length - 1].uuid, uuid, uuid_length + 1);
									tags[tags_length - 1].name = malloc(name_length + 1);
									memcpy(tags[tags_length - 1].name, name, name_length + 1);
								}
							}
							igSetCursorPosY(cursor_y);
							igSetCursorPosX(next_cursor_pos);

							igPopID();
						}
						igNewLine();
					}
					else
						igText("search to see a list of tags");
					igPopID();

					igEndCombo();
				}

				igSameLine(0, column_spacing);
				igSetNextItemWidth(column_width);

				char bpm_text[32];
				if (min_bpm == 0 || max_bpm == 0)
					snprintf(bpm_text, COUNTOF(bpm_text), "any bpm");
				else if (min_bpm == max_bpm)
					snprintf(bpm_text, COUNTOF(bpm_text), "%d bpm", min_bpm);
				else
					snprintf(bpm_text, COUNTOF(bpm_text), "%d - %d bpm", min_bpm, max_bpm);
				if (igBeginCombo("##bpm_dropdown", bpm_text, ImGuiComboFlags_HeightLargest))
				{
					filter_window_opened = true;

					igCheckbox("range", &bpm_range);
					ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsDecimal;
					if (bpm_range)
					{
						igInputTextEx("##min_bpm_box", "min", bpm_min_text, COUNTOF(bpm_min_text), (ImVec2_c){70, 0}, flags, NULL, NULL);
						igSameLine(0, 0);
						igText(" - ");
						igSameLine(0, 0);
						igInputTextEx("##max_bpm_box", "max", bpm_max_text, COUNTOF(bpm_max_text), (ImVec2_c){70, 0}, flags, NULL, NULL);
					}
					else
						igInputTextEx("##bpm_box", "bpm", bpm_min_text, COUNTOF(bpm_min_text), (ImVec2_c){70, 0}, flags, NULL, NULL);

					if (!string_parse_int(bpm_min_text, string_length(bpm_min_text), 1, 999, &min_bpm))
						min_bpm = 0;
					if (!string_parse_int(bpm_max_text, string_length(bpm_max_text), 1, 999, &max_bpm))
						max_bpm = 0;

					if (!bpm_range || max_bpm == 0)
						max_bpm = min_bpm;

					if (min_bpm == 0)
						min_bpm = max_bpm;

					if (max_bpm < min_bpm)
					{
						int temp = min_bpm;
						min_bpm = max_bpm;
						max_bpm = temp;
					}

					if (igButton("clear", (ImVec2_c){0}))
					{
						memset(bpm_min_text, 0, COUNTOF(bpm_min_text));
						memset(bpm_max_text, 0, COUNTOF(bpm_max_text));
					}

					igEndCombo();
				}
			}

			if (prev_filter_window_opened && !filter_window_opened)
				search();
			prev_filter_window_opened = filter_window_opened;

			if (first_frame)
				igSetKeyboardFocusHere(0);
			if (igInputTextEx("##search_box", "search", search_text, COUNTOF(search_text), (ImVec2_c){full_width, 0}, ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL))
				search();

			igSeparator();

			if (search_session && search_session->pages && search_session->pages_length > 0)
			{
				igBeginChild_Str("results", (ImVec2_c){0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
				float full_width = igGetContentRegionAvail().x;
				float spacing = 10;
				float height = 50;
				float button_size = 40;
				float play_column_width = spacing + button_size;
				float download_column_width = spacing + button_size;
				float time_column_width = spacing + 45;
				float key_column_width = spacing + 65;
				float bpm_column_width = spacing + 40;
				float text_column_width = full_width - spacing - play_column_width - download_column_width - time_column_width - key_column_width - bpm_column_width;
				{
					int unique_id = 0;
					for (int i = 0; i < search_session->pages_length; ++i)
					{
						for (int j = 0; j < search_session->pages[i].items_length; ++j)
						{
							struct search_item* result = &search_session->pages[i].items[j];

							bool hovering_button = false;

							igPushID_Int(++unique_id);
							igPushStyleColor_Vec4(ImGuiCol_ChildBg, (ImVec4_c){0.1, 0.1, 0.1, 1});
							igBeginChild_Str("result item", (ImVec2_c){0, height}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse);
							{
								igSameLine(0, 0);
								igBeginChild_Str("play_button_column", (ImVec2_c){play_column_width, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
								debug_rect_vector((ImVec2_c){0}, igGetContentRegionAvail(), color(255, 0, 0, 255));
								{
									igSameLine(0, spacing);
									align_cursor(ALIGN_TYPE_CENTER, 0, button_size, 0, height);
									if (igButton(result->data.name == currently_playing_name ? "l l" : ">", (ImVec2_c){button_size, button_size}))
										if (!toggle_play(result->data))
											open_error_popup("failed to play search result");
									if (igIsItemHovered(ImGuiHoveredFlags_None))
										hovering_button = true;
								}
								igEndChild();

								igSameLine(0, 0);
								igBeginChild_Str("name_column", (ImVec2_c){text_column_width, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
								debug_rect_vector((ImVec2_c){0}, igGetContentRegionAvail(), color(0, 255, 0, 255));
								{
									(void)text_box_truncated;

									int tag_font_size = 17;
									char* title = result->data.name;
									char tags[256] = {0};
									if (result->data.tags_length > 0 && result->data.tags)
									{
										int tags_length = 0;
										for (int i = 0; i < result->data.tags_length; ++i)
										{
											if (!string_concat(tags, &tags_length, COUNTOF(tags), "  ", 2) ||
												!string_concat(tags, &tags_length, COUNTOF(tags), result->data.tags[i], string_length(result->data.tags[i])))
											{
												tags[0] = 0;
												break;
											}
										}
									}

									ImVec2_c title_size = igCalcTextSize(title, NULL, false, false);
									igPushFont(normal_font, tag_font_size);
									ImVec2_c tags_size = tags[0] == 0 ? (ImVec2_c){0} : igCalcTextSize(tags, NULL, false, false);
									igPopFont();

									igSameLine(0, spacing);
									align_cursor(ALIGN_TYPE_CENTER, 0, title_size.y + tags_size.y, 0, height);
									struct ImVec2_c og_pos = igGetCursorPos();
									igTextAligned(0, text_column_width - spacing, title);

									if (tags[0] != 0)
									{
										igSetCursorPos(og_pos);
										igSetCursorPosY(igGetCursorPosY() + title_size.y);
										igPushFont(normal_font, tag_font_size);
										igPushStyleColor_Vec4(ImGuiCol_Text, (ImVec4_c){0.8, 0.8, 0.8, 1});
										igTextAligned(0, text_column_width - spacing, tags);
										igPopStyleColor(1);
										igPopFont();
									}
								}
								igEndChild();

								igSameLine(0, 0);
								igBeginChild_Str("time_column", (ImVec2_c){time_column_width, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
								debug_rect_vector((ImVec2_c){0}, igGetContentRegionAvail(), color(0, 0, 255, 255));
								{
									igSameLine(0, spacing);

									char main_string[32];
									int main_length = 0;
									char seconds_string[32];
									int seconds_length = 0;

									int duration_seconds = (result->data.duration % 1000) >= 500 ? (result->data.duration / 1000 + 1) : (result->data.duration / 1000);
									int minutes = duration_seconds / 60;
									int seconds = duration_seconds % 60;
									if (result->data.duration <= 0 ||
										(main_length = snprintf(main_string, COUNTOF(main_string), "%d", minutes)) <= 0 ||
										(seconds_length = snprintf(seconds_string, COUNTOF(seconds_string), "%02d", seconds)) <= 0 ||
										!string_concat(main_string, &main_length, COUNTOF(main_string), ":", 1) ||
										!string_concat(main_string, &main_length, COUNTOF(main_string), seconds_string, seconds_length))
										text_box("-", time_column_width - spacing, height, ALIGN_TYPE_CENTER);
									else
										text_box(main_string, time_column_width - spacing, height, ALIGN_TYPE_CENTER);
								}
								igEndChild();

								igSameLine(0, 0);
								igBeginChild_Str("key_column", (ImVec2_c){key_column_width, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
								debug_rect_vector((ImVec2_c){0}, igGetContentRegionAvail(), color(0, 0, 255, 255));
								{
									igSameLine(0, spacing);

									char string[32];
									int length = 0;
									int chord_type_length = string_length(result->data.chord_type);
									if (chord_type_length > 3) chord_type_length = 3;

									if (!string_concat(string, &length, COUNTOF(string), result->data.key, string_length(result->data.key)) ||
										(string_set_case(string, length, true), 0) ||
										!string_concat(string, &length, COUNTOF(string), " ", 1) ||
										!string_concat(string, &length, COUNTOF(string), result->data.chord_type, chord_type_length))
										text_box("-", key_column_width - spacing, height, ALIGN_TYPE_CENTER);
									else
										text_box(string, key_column_width - spacing, height, ALIGN_TYPE_CENTER);
								}
								igEndChild();

								igSameLine(0, 0);
								igBeginChild_Str("bpm_column", (ImVec2_c){bpm_column_width, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
								debug_rect_vector((ImVec2_c){0}, igGetContentRegionAvail(), color(0, 0, 255, 255));
								{
									igSameLine(0, spacing);

									char string[32];
									if (result->data.bpm <= 0 || snprintf(string, COUNTOF(string), "%d", result->data.bpm) <= 0)
										text_box("-", bpm_column_width - spacing, height, ALIGN_TYPE_CENTER);
									else
										text_box(string, bpm_column_width - spacing, height, ALIGN_TYPE_CENTER);
								}
								igEndChild();

								igSameLine(0, 0);
								igBeginChild_Str("download_button_column", (ImVec2_c){download_column_width, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_None);
								debug_rect_vector((ImVec2_c){0}, igGetContentRegionAvail(), color(0, 0, 255, 255));
								{
									igSameLine(0, spacing);
									if (result->data.file_on_disk)
									{
										align_cursor(ALIGN_TYPE_CENTER, 0, button_size, 0, height);
										text_box("x", button_size, button_size, ALIGN_TYPE_CENTER);
									}
									else
									{
										align_cursor(ALIGN_TYPE_CENTER, 0, button_size, 0, height);
										if (igButton("v", (ImVec2_c){button_size, button_size}))
										{
											if (!database_get_and_save(db, result->data.name, result->data.audio_url))
												open_error_popup("failed to save file");
											if (!search_item_update(result))
												open_error_popup("failed to update search result");
										}
										if (igIsItemHovered(ImGuiHoveredFlags_None))
											hovering_button = true;
									}
								}
								igEndChild();

								bool hovered = false;
								if (igIsWindowHovered(ImGuiHoveredFlags_ChildWindows))
								{
									hovered = true;

									if (!hovering_button)
										igSetMouseCursor(ImGuiMouseCursor_Hand);

									if (igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
									{
										currently_dragging_name = result->data.name;
										mouse_left_dragging_item = false;
									}

									if (!io->MouseDown[0] && currently_dragging_name == result->data.name && !mouse_left_dragging_item)
										if (!toggle_play(result->data))
											open_error_popup("failed to play search result");

								}
								else if (currently_dragging_name == result->data.name)
									mouse_left_dragging_item = true;

								if (hovered || currently_dragging_name == result->data.name)
								{
									ImVec2_c size = igGetWindowSize();
									ImVec2_c min = igGetWindowPos();
									ImVec2_c max = (ImVec2_c){min.x + size.x, min.y + size.y};
									ImDrawList_AddRectFilled(igGetWindowDrawList(), min, max,
										io->MouseDown[0]
										? igGetColorU32_Vec4((ImVec4_c){0.2, 0.2, 0.2, 1})
										: igGetColorU32_Vec4((ImVec4_c){0.17, 0.17, 0.17, 1}), 0, ImDrawFlags_None);
								}
							}
							igEndChild();
							igPopStyleColor(1);
							igPopID();

							if (currently_dragging_name == result->data.name &&
								igIsMouseDragPastThreshold(ImGuiMouseButton_Left, 50))
							{
								if (!database_get_and_save(db, result->data.name, result->data.audio_url) ||
									!search_item_update(result) || !result->data.file_on_disk)
									open_error_popup("failed to save file or update ui or somethign");
								else
								{
									drag_drop_start(result->data.file_on_disk);
									io->MouseDown[0] = false; // the drag drop function blocks and steals the mouse up event so it must be set manually
									currently_dragging_name = NULL;
								}
							}
						}
					}

					if (search_session->total_pages <= search_session->pages_length)
						text_box("no more results", full_width, 50, ALIGN_TYPE_CENTER);
					else if (igButton("load more", (ImVec2_c){full_width, 50}))
						search_session_fetch_next_page(search_session);
				}
				igEndChild();
			}
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

		if (!io->MouseDown[0])
			currently_dragging_name = NULL;

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

	drag_drop_uninit();

	search_session_uninit(search_session);
	search_context_uninit(search_context);
	database_uninit(db);

	return EXIT_SUCCESS;
}
