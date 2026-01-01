#include "file_utils.h"

#include <stdlib.h>
#include <string.h>

#include "common.h"

#if defined(__linux__)

#include <unistd.h>
#include <libgen.h>

#elif defined(_WIN32)

#include "windows_utils.h"

#define access _access
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

#else
#error "os not supported"
#endif

static bool file_string_concat(file_string a, int* a_length, char* b, int b_length)
{
	if (!a || !b || !a_length || b_length < 0 || *a_length < 0) return false;

	if (*a_length + b_length + 1 > COUNTOF(file_string)) return false;

	memcpy(a + *a_length, b, b_length);
	a[*a_length + b_length] = '\0';
	*a_length += b_length;
	return true;
}

bool file_string_copy(char* string, int string_length, file_string out)
{
	if (!string || !out || string_length < 0) return false;

	if (string_length + 1 > COUNTOF(file_string)) return false;

	memcpy(out, string, string_length);
	out[string_length] = '\0';
	return true;
}

bool extract_file_name(char* path, int path_length, bool keep_extension, bool* has_file_name, file_string out, int* out_length)
{
	if (!path || path_length < 0 || !out) return false;

	if (path_length == 0)
	{
		*has_file_name = false;
		return true;
	}

	file_string path_copy;
	if (!file_string_copy(path, path_length, path_copy)) return false;

#if defined(_WIN32)
	string_replace_char(path_copy, path_length, '\\', '/');
#endif

	if (path_copy[path_length - 1] == '/')
	{
		*has_file_name = false;
		return true;
	}

	int file_name_pos = 0;
	for (int i = 0; i < path_length; ++i)
		if (path_copy[i] == '/')
			file_name_pos = i + 1;

	char* file_name = path_copy + file_name_pos;
	int file_name_length = path_length - file_name_pos;

	if (!keep_extension)
	{
		bool has_extension;
		file_string extension;
		int extension_length;
		if (!extract_file_extension(file_name, file_name_length, &has_extension, extension, &extension_length)) return false;
		if (has_extension)
			file_name_length = file_name_length - extension_length - 1;
	}

#if defined(_WIN32)
	string_replace_char(file_name, file_name_length, '/', '\\');
#endif

	if (!file_string_copy(file_name, file_name_length, out)) return false;
	if (out_length) *out_length = file_name_length;

	*has_file_name = true;
	return true;
}

bool extract_file_extension(char* path, int path_length, bool* has_file_extension, file_string out, int* out_length)
{
	if (!path || path_length < 0 || !has_file_extension || !out) return false;

	file_string file_name;
	int file_name_length;
	bool has_file_name;
	if (!extract_file_name(path, path_length, true, &has_file_name, file_name, &file_name_length)) return false;
	if (!has_file_name)
	{
		*has_file_extension = false;
		return true;
	}

	if (file_name_length == 0) return false;

	int last_dot_pos = -1;
	int num_leading_dots = 0;
	for (int i = 0, count_leading_dots = true; i < file_name_length; ++i)
	{
		if (file_name[i] == '.')
		{
			last_dot_pos = i;
			if (count_leading_dots)
				++num_leading_dots;
		}
		else
			count_leading_dots = false;
	}

	if (last_dot_pos == -1 ||
		last_dot_pos <= num_leading_dots ||
		last_dot_pos == file_name_length - 1)
	{
		*has_file_extension = false;
		return true;
	}

	*has_file_extension = true;
	int extension_length = file_name_length - (last_dot_pos + 1);
	if (out_length) *out_length = extension_length;
	if (!file_string_copy(&file_name[last_dot_pos + 1], extension_length, out)) return false;
	return true;
}

bool set_file_extension(char* path, int path_length, char* extension, int extension_length, bool replace, file_string out, int* out_length)
{
	if (!path || path_length <= 0 || !extension || extension_length <= 0 || !out) return false;

	if (extension[0] == '.')
	{
		++extension;
		--extension_length;
	}

	if (extension_length == 0) return false;

	int temp_out_length = 0;
	if (!out_length) out_length = &temp_out_length;

	if (replace)
	{
		bool has_extension;
		file_string curr_extension;
		int curr_extension_length;
		if (!extract_file_extension(path, path_length, &has_extension, curr_extension, &curr_extension_length)) return false;
		if (has_extension)
			path_length = path_length - curr_extension_length - 1;
	}

	*out_length = path_length;
	if (!file_string_copy(path, path_length, out) ||
		!file_string_concat(out, out_length, ".", 1) ||
		!file_string_concat(out, out_length, extension, extension_length)) return false;
	return true;
}

bool is_file_accessible(char* path, int path_length, bool read, bool write, bool execute)
{
	if (!path || path_length < 0) return false;

	file_string path_copy;
	if (!file_string_copy(path, path_length, path_copy)) return false;

#if defined(_WIN32)
	execute = false;
#endif

	int permissions = (read ? R_OK : 0) | (write ? W_OK : 0) | (execute ? X_OK : 0);
	return access(path_copy, (permissions == 0) ? F_OK : permissions) == 0;
}

bool read_file(char* path, int path_length, struct buffer* buffer, bool text)
{
	*buffer = (struct buffer){0};

	file_string path_copy;
	if (!file_string_copy(path, path_length, path_copy)) return false;

	bool return_value = false;

	FILE* fd = fopen(path_copy, text ? "r" : "rb");
	if (!fd)
		goto cleanup;

	if (fseek(fd, 0, SEEK_END) != 0)
		goto cleanup;

	buffer->length = ftell(fd);
	if (buffer->length < 0)
		goto cleanup;

	buffer->data = malloc(buffer->length);
	if (!buffer->data)
		goto cleanup;

	if (fseek(fd, 0, SEEK_SET) != 0)
		goto cleanup;

	if (fread(buffer->data, sizeof(*buffer->data), buffer->length, fd) < buffer->length)
		goto cleanup;

	if (ferror(fd) != 0)
		goto cleanup;

	return_value = true;

cleanup:
	if (!return_value) free(buffer->data);
	if (fd) fclose(fd);
	return return_value;
}

bool write_file(char* path, int path_length, struct buffer buffer, bool text)
{
	if (!buffer.data || buffer.length <= 0)
		return false;

	file_string path_copy;
	if (!file_string_copy(path, path_length, path_copy)) return false;

	bool return_value = false;

	FILE* fd = fopen(path_copy, text ? "w" : "wb");
	if (!fd)
		goto cleanup;

	if (fwrite(buffer.data, sizeof(*buffer.data), buffer.length, fd) < buffer.length)
		goto cleanup;

	if (ferror(fd) != 0)
		goto cleanup;

	return_value = true;

cleanup:
	if (fd) fclose(fd);
	return return_value;
}

#if defined(__linux__)

bool join_paths(char* path, int path_length, char* join, int join_length, file_string out, int* out_length)
{
	if (!path || path_length < 0 || !join || join_length < 0 || !out) return false;

	int temp_out_length = 0;
	if (!out_length) out_length = &temp_out_length;

	if (join_length != 0 && join[0] == '/')
	{
		*out_length = join_length;
		if (!file_string_copy(join, join_length, out)) return false;
		return true;
	}

	*out_length = path_length;
	if (!file_string_copy(path, path_length, out)) return false;

	if (join_length == 0)
		return true;

	if (*out_length != 0 && out[*out_length - 1] != '/')
	{
		if (!file_string_concat(out, out_length, "/", 1)) return false;
	}
	if (!file_string_concat(out, out_length, join, join_length)) return false;
	return true;
}

bool get_absolute_path(char* path, int path_length, file_string out, int* out_length)
{
	if (!path || path_length < 0 || !out) return false;

	int temp_out_length = 0;
	if (!out_length) out_length = &temp_out_length;

	file_string temp;
	if (!file_string_copy(path, path_length, temp)) return false;

	char* absolute_path = realpath(temp, NULL);
	if (!absolute_path) return false;

	*out_length = string_length(absolute_path);
	if (!file_string_copy(absolute_path, *out_length, out))
	{
		free(absolute_path);
		return false;
	}

	free(absolute_path);
	return true;
}

#elif defined(_WIN32)

bool join_paths(char* path, int path_length, char* join, int join_length, file_string out, int* out_length)
{
	if (!path || path_length < 0 || !join || join_length < 0 || !out) return false;

	int temp_out_length = 0;
	if (!out_length) out_length = &temp_out_length;

	if (path_length == 0)
	{
		if (!file_string_copy(join, join_length, out)) return false;
		*out_length = join_length;
		return true;
	}
	if (join_length == 0)
	{
		if (!file_string_copy(path, path_length, out)) return false;
		*out_length = path_length;
		return true;
	}

	file_string path_copy;
	file_string join_copy;
	if (!file_string_copy(path, path_length, path_copy)) return false;
	if (!file_string_copy(join, join_length, join_copy)) return false;
	string_replace_char(path_copy, path_length, '/', '\\');
	string_replace_char(join_copy, join_length, '/', '\\');

	file_string_wchar path_w;
	file_string_wchar join_w;
	int path_w_length;
	int join_w_length;;
	if (!convert_to_wchar(path_copy, path_length, path_w, &path_w_length) ||
		!convert_to_wchar(join_copy, join_length, join_w, &join_w_length)) return false;

	file_string_wchar out_w;
	if (PathCchCombineEx(out_w, COUNTOF(file_string_wchar), path_w, join_w,
		PATHCCH_ALLOW_LONG_PATHS | PATHCCH_DO_NOT_NORMALIZE_SEGMENTS) != S_OK) return false;

	int out_w_length = 0;
	while (true)
	{
		if (out_w[out_w_length] == L'\0')
			break;
		++out_w_length;
	}

	if (!convert_from_wchar(out_w, out_w_length, out, out_length)) return false;
	return true;
}

bool get_absolute_path(char* path, int path_length, file_string out, int* out_length)
{
	if (!path || path_length < 0 || !out) return false;

	int temp_out_length = 0;
	if (!out_length) out_length = &temp_out_length;

	file_string path_copy;
	if (!file_string_copy(path, path_length, path_copy)) return false;
	string_replace_char(path_copy, path_length, '/', '\\');

	file_string_wchar path_w;
	int path_w_length;
	if (!convert_to_wchar(path_copy, path_length, path_w, &path_w_length)) return false;

	file_string_wchar full_path_w;
	int full_path_w_length = GetFullPathNameW(path_w, COUNTOF(full_path_w), full_path_w, NULL);
	if (full_path_w_length == 0 || full_path_w_length > COUNTOF(full_path_w)) return false;

	if (!convert_from_wchar(full_path_w, full_path_w_length, out, out_length)) return false;
	return true;
}

#else
#error "os not supported"
#endif
