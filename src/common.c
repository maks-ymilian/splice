#include "common.h"

#include <string.h>

int string_length(char* string)
{
	if (!string) return 0;
	int length = strlen(string);
	if (length < 0) return 0;
	return length;
}

char* string_alloc(char* string, int string_length)
{
	if (!string || string_length <= 0) return NULL;

	char* new = malloc(string_length + 1);
	if (!new) return NULL;

	memcpy(new, string, string_length);
	new[string_length] = '\0';
	return new;
}

void string_replace_char(char* string, int string_length, char find, char replace)
{
	if (!string || string_length < 0) return;

	for (int i = 0; i < string_length; ++i)
	{
		if (string[i] == find)
			string[i] = replace;
	}
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
