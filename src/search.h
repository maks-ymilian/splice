#pragma once

#include <stdbool.h>

struct search_result
{
	char* name;
	char* name_full;
	char* sound_url;
};

struct search_results
{
	struct search_result* ptr;
	int length;
};

bool search(char* text, struct search_results* results);
void free_search_results(struct search_results results);
