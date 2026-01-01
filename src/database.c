#include "database.h"

#include <curl/curl.h>

#include "unscrambler.h"
#include "map.h"

struct database
{
	char* files_path;
	struct map* cached_files;
};

static bool download_file(char* url, struct buffer* buffer)
{
	if (!url || !buffer)
		return false;

	*buffer = (struct buffer){0};

	CURL* request = curl_easy_init();
	if (!request)
		return false;

	struct buffer response = {0};
	curl_easy_setopt(request, CURLOPT_URL, url);
	curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, write_response);
	curl_easy_setopt(request, CURLOPT_WRITEDATA, &response);
	if (curl_easy_perform(request)) goto error;

	if (!unscramble(response, buffer)) goto error;

	free(response.data);
	curl_easy_cleanup(request);
	return true;

error:
	free(buffer->data);
	free(response.data);
	curl_easy_cleanup(request);
	return false;
}

static bool convert_to_file_name(char* dir, int dir_length, char* file, int file_length, file_string out, int* out_length)
{
	if (!file || file_length <= 0 || (dir && dir_length <= 0)) return false;

	file_string path;
	int path_length;
	if (dir)
	{
		if (!join_paths(dir, dir_length, file, file_length, path, &path_length)) return false;
	}
	else
	{
		path_length = file_length;
		if (!file_string_copy(file, file_length, path)) return false;
	}

	char extension[] = "mp3";
	if (!set_file_extension(path, path_length, extension, sizeof(extension) - 1, true, out, out_length)) return false;
	return true;
}

static bool cache_add(struct database* db, char* file, struct buffer buffer)
{
	if (!db || !file || !buffer.data || buffer.length < 0) return false;

	bool found;
	if (!map_add(db->cached_files, file, &buffer, &found) || found) return false;

	return true;
}

static bool cache_get(struct database* db, char* file, struct buffer* buffer)
{
	if (!db || !file || !buffer) return false;

	bool cached;
	struct buffer cached_buffer;
	if (!map_get(db->cached_files, file, &cached_buffer, &cached)) return false;
	if (cached)
		*buffer = cached_buffer;
	else
		*buffer = (struct buffer){.data = NULL, .length = 0};

	return true;
}

struct database* database_init(char* files_path)
{
	struct database* db = malloc(sizeof(*db));
	if (!db) return NULL;

	db->files_path = string_alloc(files_path, string_length(files_path));
	db->cached_files = map_init(sizeof(struct buffer));
	if (!db->files_path || !db->cached_files)
	{
		free(db->files_path);
		free(db);
		return NULL;
	}

	return db;
}

void database_uninit(struct database* db)
{
	if (!db)
		return;

	for (struct map_iterator* it = map_it_begin(db->cached_files); !map_it_end(&it); map_it_next(&it))
	{
		if (!it) break;

		struct buffer* buffer = map_it_value(it);
		if (!buffer) continue;
		free(buffer->data);
	}

	map_uninit(db->cached_files);
	free(db->files_path);
	free(db);
}

bool database_get(struct database* db, char* file, char* url, struct buffer* buffer)
{
	if (!db || !file || !url || !buffer) return false;

	struct buffer cached_buffer;
	if (!cache_get(db, file, &cached_buffer)) return false;
	if (cached_buffer.data)
	{
		printf("retrieving %s from cache\n", file);
		*buffer = cached_buffer;
	}
	else
	{
		bool exists;
		if (!database_get_file_path(db, file, &exists, NULL, NULL)) return false;
		if (exists)
		{
			printf("retrieving %s from file\n", file);

			file_string file_name;
			int file_name_length;
			if (!convert_to_file_name(db->files_path, string_length(db->files_path), file, string_length(file), file_name, &file_name_length)) return false;
			if (!read_file(file_name, file_name_length, buffer, false)) return false;
		}
		else
		{
			printf("retrieving %s from url\n", file);
			if (!download_file(url, buffer)) return false;
		}

		if (!cache_add(db, file, *buffer))
		{
			free(buffer->data);
			return false;
		}
	}

	return true;
}

bool database_get_file_path(struct database* db, char* file, bool* out_exists, file_string out, int* out_length)
{
	if (!db || !file || (!out_exists)) return false;

	file_string file_name;
	int file_name_length;
	if (!convert_to_file_name(db->files_path, string_length(db->files_path), file, string_length(file), file_name, &file_name_length)) return false;
	if (!is_file_accessible(file_name, file_name_length, false, false, false))
	{
		if (out_exists) *out_exists = false;
		return true;
	}

	if (out)
	{
		if (!get_absolute_path(file_name, file_name_length, out, out_length)) return false;
	}
	if (out_exists) *out_exists = true;
	return true;
}

bool database_get_and_save(struct database* db, char* file, char* url)
{
	if (!db || !file || !url) return false;

	bool exists;
	if (!database_get_file_path(db, file, &exists, NULL, NULL) || exists) return false;

	struct buffer buffer;
	if (!database_get(db, file, url, &buffer)) return false;

	file_string name;
	int name_length;
	if (!convert_to_file_name(db->files_path, string_length(db->files_path), file, string_length(file), name, &name_length)) return false;
	if (!write_file(name, name_length, buffer, false)) return false;
	return true;
}
