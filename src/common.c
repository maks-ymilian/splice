#include "common.h"

#include <string.h>

char* realloc_string(char* string)
{
	if (!string) return NULL;

	int length = strlen(string) + 1;

	char* new = malloc(length);
	if (!new) return NULL;

	memcpy(new, string, length);
	return new;
}

bool copy_buffer(struct buffer from, struct buffer* to)
{
	if (!to || !from.data) return false;

	to->length = from.length;
	to->data = malloc(to->length);
	if (!to->data) return false;

	memcpy(to->data, from.data, to->length);
	return true;
}

size_t write_response(void* buffer, size_t size, size_t nmemb, void* userp)
{
	struct buffer* response = (struct buffer*)userp;

	int old_length = response->length;
	response->length += size * nmemb;
	response->data = realloc(response->data, response->length);
	memcpy(response->data + old_length, buffer, size * nmemb);

	return size * nmemb;
}
