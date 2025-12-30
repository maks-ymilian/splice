#pragma once

#include <stdbool.h>

#include "database.h"

struct search_result
{
	char* name;
	char* name_full;
	char* audio_url;
	char* file_on_disk;
};

struct search_results
{
	struct search_result* ptr;
	int length;
};

bool search(struct database* db, char* text, struct search_results* results);
void update_search_result(struct database* db, struct search_result* result);

void free_search_results(struct search_results results);
