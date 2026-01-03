#pragma once

#include <stdbool.h>

#include "database.h"

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

struct search_query
{
	char* search_string;
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
