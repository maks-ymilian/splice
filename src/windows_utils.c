#include "windows_utils.h"

bool convert_to_wchar(char* path, int path_length, file_string_wchar out, int* out_length)
{
	if (!path || path_length < 0 || !out) return false;

	int temp_out_length = 0;
	if (!out_length) out_length = &temp_out_length;

	if (path_length == 0)
	{
		out[0] = L'\0';
		*out_length = 0;
		return true;
	}

	*out_length = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, path, path_length, out, COUNTOF(file_string_wchar));
	if (*out_length == 0) return false;

	if (*out_length + 1 >= COUNTOF(file_string_wchar)) return false;
	out[*out_length] = L'\0';
	return true;
}

bool convert_from_wchar(file_string_wchar path, int path_length, file_string out, int* out_length)
{
	if (!path || path_length < 0 || !out) return false;

	int temp_out_length = 0;
	if (!out_length) out_length = &temp_out_length;

	if (path_length == 0)
	{
		out[0] = '\0';
		*out_length = 0;
		return true;
	}

	*out_length = WideCharToMultiByte(
		CP_UTF8, WC_COMPOSITECHECK | WC_ERR_INVALID_CHARS | WC_NO_BEST_FIT_CHARS,
		path, path_length,
		out, COUNTOF(file_string),
		NULL, NULL);
	if (*out_length == 0) return false;

	if (*out_length + 1 >= COUNTOF(file_string)) return false;
	out[*out_length] = '\0';
	return true;
}
