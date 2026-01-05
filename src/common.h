#pragma once

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

#define COUNTOF(x) (sizeof(x) / sizeof((*((__typeof__(x)*)0))[0]))

struct buffer
{
	uint8_t* data;
	int64_t length;
};

int string_length(char* string);
char* string_alloc(char* string, int string_length);
void string_replace_char(char* string, int string_length, char find, char replace);
bool string_find_char(char* string, int string_length, char find);
bool string_concat(char* dest, int* dest_length, int dest_max_length, char* src, int src_length);
void string_set_case(char* string, int string_length, bool uppercase);
bool string_parse_int(char* string, int string_length, int min_value, int max_value, int* out_int);

bool copy_buffer(struct buffer from, struct buffer* to);

size_t write_response(void* buffer, size_t size, size_t nmemb, void* userp);
