#include "database.h"

#include <string.h>

#include <curl/curl.h>

#include "unscrambler.h"
#include "platform.h"

static bool download_file(char* url, struct buffer* buffer)
{
	*buffer = (struct buffer){0};

	CURL* request = curl_easy_init();
	if (!request)
		return false;

	struct buffer response = {0};
	bool return_value = false;

	curl_easy_setopt(request, CURLOPT_URL, url);
	curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, write_response);
	curl_easy_setopt(request, CURLOPT_WRITEDATA, &response);

	if (curl_easy_perform(request))
		goto cleanup;

	uint8_t* data;
	int length;
	if (!unscramble(response.data, response.length, &data, &length))
		goto cleanup;

	buffer->data = malloc(length);
	if (!buffer->data)
		goto cleanup;
	buffer->length = length;
	memcpy(buffer->data, data, length);

	return_value = true;

cleanup:
	if (!return_value)
		free(buffer->data);
	free(response.data);
	curl_easy_cleanup(request);

	return return_value;
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

bool database_get(char* file, char* url, struct buffer* buffer)
{
	bool return_value = false;

	file = alloc_file_name_no_extension(file);
	if (!file)
		goto cleanup;

	char extension[] = ".mp3";
	file = realloc(file, strlen(file) + COUNTOF(extension));
	if (!file)
		goto cleanup;

	strcat(file, extension);

	if (check_file_access(file, false, false)) // if file exists
	{
		printf("reading %s\n", file);

		if (is_regular_file(file) <= 0)
			goto cleanup;

		if (!check_file_access(file, true, false)) // if file isnt readable
			goto cleanup;

		if (!read_file(file, buffer))
			goto cleanup;
	}
	else 
	{
		printf("downloading %s\n", file);

		if (!download_file(url, buffer))
			goto cleanup;

		if (!write_file(file, *buffer))
			goto cleanup;
	}

	return_value = true;

cleanup:
	free(file);
	return return_value;
}
