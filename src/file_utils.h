#pragma once

#include <stdbool.h>

typedef file_string char[1024]

bool mkdir(char* path);
bool get_file_name(char* path, bool keep_extension, file_string* out);
bool get_dir_path(char* path, file_string* out);
bool get_absolute_path(char* path, file_string* out);
bool file_exists(char* path, bool* exists);

