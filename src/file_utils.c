#include "file_utils.h"

#ifdef __linux__ 

bool mkdir(char* path)
{
}

bool get_file_name(char* path, bool keep_extension, file_string* out)
{
}

bool get_dir_path(char* path, file_string* out)
{
}

bool get_absolute_path(char* path, file_string* out)
{
}

bool file_exists(char* path, bool* exists)
{
}

#elif _WIN32

bool mkdir(char* path)
{
}

bool get_file_name(char* path, bool keep_extension, file_string* out)
{
}

bool get_dir_path(char* path, file_string* out)
{
}

bool get_absolute_path(char* path, file_string* out)
{
}

bool file_exists(char* path, bool* exists)
{
}

#else
#error "os not supported"
#endif

