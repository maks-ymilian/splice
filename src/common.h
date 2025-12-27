#pragma once

#define COUNTOF(x) (sizeof(x) / sizeof(*x))

#include <stdlib.h>
#include <inttypes.h>

struct buffer
{
	uint8_t* data;
	int length;
};

char* realloc_string(char* string);

size_t write_response(void* buffer, size_t size, size_t nmemb, void* userp);
