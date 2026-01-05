#pragma once

#include <stdbool.h>

#include "database.h"

enum search_sample_type
{
	SEARCH_SAMPLE_TYPE_ANY = 0,
	SEARCH_SAMPLE_TYPE_ONE_SHOTS = 1,
	SEARCH_SAMPLE_TYPE_LOOPS = 2,
};

enum search_scale
{
	SEARCH_SCALE_ANY = 0,
	SEARCH_SCALE_MAJOR = 1,
	SEARCH_SCALE_MINOR = 2,
};

enum search_key
{
	SEARCH_KEY_ANY,
	SEARCH_KEY_C,
	SEARCH_KEY_C_SHARP,
	SEARCH_KEY_D,
	SEARCH_KEY_D_SHARP,
	SEARCH_KEY_E,
	SEARCH_KEY_F,
	SEARCH_KEY_F_SHARP,
	SEARCH_KEY_G,
	SEARCH_KEY_G_SHARP,
	SEARCH_KEY_A,
	SEARCH_KEY_A_SHARP,
	SEARCH_KEY_B,
	SEARCH_KEY_LENGTH,
};

enum search_sort
{
	SEARCH_SORT_MOST_RELEVANT,
	SEARCH_SORT_MOST_POPULAR,
	SEARCH_SORT_MOST_RECENT,
	SEARCH_SORT_RANDOM,
	SEARCH_SORT_LENGTH,
};

enum search_query_parameters
{
	SEARCH_QUERY_PARAMETERS_NONE = 0,
	SEARCH_QUERY_PARAMETERS_TAGS = 1,
	SEARCH_QUERY_PARAMETERS_BPM = 2,
	SEARCH_QUERY_PARAMETERS_SORT = 4,
	SEARCH_QUERY_PARAMETERS_SAMPLE_TYPE = 8,
	SEARCH_QUERY_PARAMETERS_KEY = 16,
	SEARCH_QUERY_PARAMETERS_SCALE = 32,
};

struct search_item_data
{
	char* name;
	char* name_full;
	char* audio_url;
	char* key;
	char* chord_type;
	char** tags;
	char* file_on_disk;
	int tags_length;
	int bpm;
	int duration;
};

struct search_bpm_range
{
	int min;
	int max;
};

struct search_tags
{
	char** tags;
	int length;
};

struct search_query
{
	char* search_string;
	enum search_query_parameters used_parameters;
	struct search_tags tags;
	struct search_bpm_range bpm_range;
	enum search_sort sort;
	enum search_sample_type sample_type;
	enum search_key key;
	enum search_scale scale;
};

struct search_item
{
	struct search_session* session;
	struct search_item_data data;
};

struct search_page
{
	struct search_item* items;
	int items_length;
};

struct search_session
{
	struct search_context* context;
	struct search_query query;
	struct search_page* pages;
	int pages_length;
	int total_pages;
};

struct search_context;

typedef bool (*search_item_update_func)(struct database* db, struct search_item_data* item_data);

struct search_context* search_context_init(struct database* db, search_item_update_func item_update_func, int max_results_per_page);
void search_context_uninit(struct search_context* context);

struct search_session* search_session_init(struct search_context* context, struct search_query query);
void search_session_uninit(struct search_session* session);
bool search_session_fetch_next_page(struct search_session* session);

bool search_item_update(struct search_item* item);
