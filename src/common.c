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

bool string_concat(char* dest, int* dest_length, int dest_max_length, char* src, int src_length)
{
	if (!dest || !src || !dest_length || src_length < 0 || *dest_length < 0) return false;

	if (*dest_length + src_length + 1 > dest_max_length) return false;

	memcpy(dest + *dest_length, src, src_length);
	dest[*dest_length + src_length] = '\0';
	*dest_length += src_length;
	return true;
}

void string_set_case(char* string, int string_length, bool uppercase)
{
	if (!string || string_length < 0) return;

	for (int i = 0; i < string_length; ++i)
	{
		if (uppercase && string[i] >= 'a' && string[i] <= 'z')
			string[i] -= 32;

		if (!uppercase && string[i] >= 'A' && string[i] <= 'Z')
			string[i] += 32;
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
