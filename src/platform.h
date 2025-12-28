#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "drag_drop.h"

int is_same_file(char* path1, char* path2);
char* alloc_absolute_path(char* path);
char* alloc_file_name(char* path);
char* alloc_file_name_no_extension(char* path);
char* alloc_dir_name(char* path);
bool change_dir(char* path);
bool check_file_access(char* path, bool read, bool write);
int is_regular_file(char* path);
void print_stack_trace(void);
