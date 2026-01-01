#pragma once

#include <stdbool.h>

#include "common.h"

typedef char file_string[1024];

bool file_string_copy(char* string, int string_length, file_string out);

bool extract_file_name(char* path, int path_length, bool keep_extension, bool* has_file_name, file_string out, int* out_length);
bool extract_file_extension(char* path, int path_length, bool* has_file_extension, file_string out, int* out_length);
bool set_file_extension(char* path, int path_length, char* extension, int extension_length, bool replace, file_string out, int* out_length);
bool join_paths(char* path, int path_length, char* join, int join_length, file_string out, int* out_length);
bool get_absolute_path(char* path, int path_length, file_string out, int* out_length);
bool is_file_accessible(char* path, int path_length, bool read, bool write, bool execute);
bool read_file(char* path, int path_length, struct buffer* buffer, bool text);
bool write_file(char* path, int path_length, struct buffer buffer, bool text);
