#include "database.h"

#include <string.h>

#include <curl/curl.h>

#include "unscrambler.h"
#include "platform.h"
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

static bool read_file(char* file, struct buffer* buffer)
{
	*buffer = (struct buffer){0};

	bool return_value = false;

	FILE* fd = fopen(file, "rb");
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

static bool write_file(char* file, struct buffer buffer)
{
	if (!buffer.data || buffer.length <= 0)
		return false;

	bool return_value = false;

	FILE* fd = fopen(file, "wb");
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

static bool convert_name(char* file, char** out_file)
{
	if (!file)
		return false;

	char* new_file = alloc_file_name_no_extension(file);
	if (!new_file)
		goto cleanup;

	char extension[] = ".mp3";
	new_file = realloc(new_file, strlen(new_file) + COUNTOF(extension));
	if (!new_file)
		goto cleanup;

	strcat(new_file, extension);
	*out_file = new_file;
	return true;

cleanup:
	free(new_file);
	return false;
}

static bool cache_add(struct database* db, char* file, struct buffer buffer)
{
	if (!db || !file || !buffer.data || buffer.length < 0) return false;

	char* name;
	if (!convert_name(file, &name)) goto error;

	bool found;
	if (!map_add(db->cached_files, name, &buffer, &found) || found) goto error;

	free(name);
	return true;

error:
	free(name);
	return false;
}

static bool cache_get(struct database* db, char* file, struct buffer* buffer)
{
	if (!db || !file || !buffer) return false;

	char* name;
	if (!convert_name(file, &name)) goto error;

	bool cached;
	struct buffer cached_buffer;
	if (!map_get(db->cached_files, name, &cached_buffer, &cached)) goto error;
	if (cached)
		*buffer = cached_buffer;
	else
		*buffer = (struct buffer){.data = NULL, .length = 0};

	free(name);
	return true;

error:
	free(name);
	return false;
}

struct database* database_init(char* files_path)
{
	if (!files_path)
		return NULL;

	struct database* db = malloc(sizeof(*db));
	if (!db)
		return NULL;

	db->files_path = realloc_string(files_path);
	db->cached_files = map_init(sizeof(struct buffer));

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
		if (!database_get_file_path(db, file, &exists, NULL)) return false;
		if (exists)
		{
			printf("retrieving %s from file\n", file);

			char* name;
			if (!convert_name(file, &name)) return false;
			if (!read_file(name, buffer))
			{
				free(name);
				return false;
			}
			free(name);
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

bool database_get_file_path(struct database* db, char* file, bool* out_exists, char** file_on_disk)
{
	if (!db || !file || (!out_exists && !file_on_disk)) return false;

	char* name = NULL;
	if (!convert_name(file, &name)) goto error;

	if (check_file_access(name, false, false)) // if file exists
	{
		bool is_file;
		if (!is_regular_file(name, &is_file) || !is_file) goto error;

		if (out_exists) *out_exists = true;
		if (file_on_disk)
			*file_on_disk = name;
		else
			free(name);
		return true;
	}

	if (out_exists) *out_exists = false;
	if (file_on_disk) *file_on_disk = NULL;
	free(name);
	return true;

error:
	free(name);
	return false;
}

bool database_get_and_save(struct database* db, char* file, char* url)
{
	if (!db || !file || !url) return false;

	bool exists;
	if (!database_get_file_path(db, file, &exists, NULL) || exists) return false;

	struct buffer buffer;
	if (!database_get(db, file, url, &buffer)) return false;

	char* name = NULL;
	if (!convert_name(file, &name)) return false;
	if (!write_file(name, buffer))
	{
		free(name);
		return false;
	}

	free(name);
	return true;
}
