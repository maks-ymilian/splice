#pragma once

#define COUNTOF(x) (sizeof(x) / sizeof(*x))

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

#define UNREACHABLE()                                                       \
	do                                                                      \
	{                                                                       \
		fprintf(stderr, "Assertion failed in %s:%u\n", __FILE__, __LINE__); \
		abort();                                                            \
	}                                                                       \
	while (0)

#define ASSERT(x)          \
	do                     \
	{                      \
		if (!(x))          \
			UNREACHABLE(); \
	}                      \
	while (0)

struct buffer
{
	uint8_t* data;
	int length;
};

char* realloc_string(char* string);
bool copy_buffer(struct buffer from, struct buffer* to);

size_t write_response(void* buffer, size_t size, size_t nmemb, void* userp);
