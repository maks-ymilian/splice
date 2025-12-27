#include "unscrambler.h"

static uint64_t decode(uint64_t i, uint8_t* data, uint8_t* data_end, uint64_t data_length, uint8_t* key, int key_length)
{
	int key_i = 0;
	for (; i < data_length; ++i)
	{
		if (key_i > key_length - 1)
			key_i = 0;

		if (data + i >= data_end)
			return data_end - data;

		data[i] ^= key[key_i];

		++key_i;
	}

	return i;
}

static uint64_t bytes_to_integer(uint8_t* data, int length)
{
	uint64_t num = 0;
	for (int i = length - 1; i >= 0; i--)
		num = num * 256 + data[i];
	return num;
}

bool unscramble(uint8_t* data, int length, uint8_t** out_data, int* out_length)
{
	uint64_t size = bytes_to_integer(data + 2, 8);

	if (length == 0 || size > length)
		return false;

	uint8_t* key = data + 10;
	int key_length = 18;

	for (int i = 0; i < key_length; ++i)
		if (key[i] == 0)
			return false;

	uint8_t* payload = data + 28;
	int index = decode(0, payload, data + length, size, key, key_length);
	index += size;
	decode(index, payload, data + length, index + size, key, key_length);

	*out_data = payload;
	*out_length = length - 28;

	return true;
}
