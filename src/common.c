#include "common.h"

#include <string.h>

char* realloc_string(char* string)
{
	int length = strlen(string) + 1;
	char* new = malloc(length);
	memcpy(new, string, length);
	return new;
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
