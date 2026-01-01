#define UNICODE
#include <windows.h>
#include <io.h>
#include <shlwapi.h>
#include <Pathcch.h>
#include <ole2.h>
#include <strsafe.h>
#include <shlobj.h>
#include <process.h>

#include "file_utils.h"

typedef wchar_t file_string_wchar[COUNTOF(file_string)];

bool convert_to_wchar(char* path, int path_length, file_string_wchar out, int* out_length);
bool convert_from_wchar(file_string_wchar path, int path_length, file_string out, int* out_length);
