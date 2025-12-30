#include "map.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include "common.h"

#define START_BUCKETS 4
#define LOAD_FACTOR 0.75

struct bucket_node
{
	struct bucket_node* next;
	char* key;
	void* value;
};

struct map
{
	struct bucket_node* buckets;
	int num_buckets;
	int data_size;
	int num_entries;
};

struct map_iterator
{
	struct map* map;
	struct bucket_node* curr_node;
	int curr_bucket;
	bool end;
};

static uint64_t hash(char* str)
{
	uint64_t hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;

	return hash;
}

static struct map* map_init_internal(int data_size, int num_buckets)
{
	if (data_size < 0)
		return NULL;

	struct map* map = malloc(sizeof(*map));
	if (!map)
		return NULL;

	map->data_size = data_size;
	map->num_buckets = num_buckets;
	map->num_entries = 0;
	map->buckets = calloc(map->num_buckets, sizeof(struct bucket_node));

	if (!map->buckets)
	{
		free(map);
		return NULL;
	}

	return map;
}

static void map_uninit_internal(struct map* map, bool free_pointer)
{
	if (!map)
		return;

	for (int i = 0; i < map->num_buckets; ++i)
	{
		struct bucket_node* node = &map->buckets[i];

		free(node->key);
		free(node->value);

		node = node->next;
		while (node)
		{
			free(node->key);
			free(node->value);

			struct bucket_node* next = node->next;
			free(node);
			node = next;
		}
	}

	free(map->buckets);
	if (free_pointer) free(map);
}

static bool expand(struct map* map)
{
	if (!map)
		return false;

	struct map* new_map = map_init_internal(map->data_size, map->num_buckets * 2);
	if (!new_map)
		return false;

	for (struct map_iterator* it = map_it_begin(map); !map_it_end(&it); map_it_next(&it))
	{
		if (!it) goto error;
		if (!map_add(new_map, map_it_key(it), map_it_value(it), NULL)) goto error;
	}

	map_uninit_internal(map, false);
	*map = *new_map;
	free(new_map);
	return true;

error:
	map_uninit(new_map);
	return false;
}

struct map* map_init(int data_size)
{
	return map_init_internal(data_size, START_BUCKETS);
}

void map_uninit(struct map* map)
{
	map_uninit_internal(map, true);
}

bool map_get(struct map* map, char* key, void* out_value, bool* out_found)
{
	if (!map || !key || (!out_value && !out_found))
		return false;

	struct bucket_node* node = &map->buckets[hash(key) % map->num_buckets];
	while (node && node->value)
	{
		if (!node->key) return false;
		if (strcmp(node->key, key) == 0)
		{
			if (out_value) memcpy(out_value, node->value, map->data_size);
			if (out_found) *out_found = true;
			return true;
		}

		node = node->next;
	}

	if (out_found) *out_found = false;
	return true;
}

bool map_add(struct map* map, char* key, void* value, bool* out_found)
{
	if (!map || !key || !value)
		return false;

	int max_entries = map->num_buckets * LOAD_FACTOR;
	if (map->num_entries >= max_entries)
		if (!expand(map))
			return false;

	struct bucket_node* first = &map->buckets[hash(key) % map->num_buckets];
	struct bucket_node** node = &first;
	while (*node && (*node)->value)
	{
		if (!(*node)->key)
			return false;
		if (strcmp((*node)->key, key) == 0)
		{
			if (out_found) *out_found = true;
			return true;
		}

		node = &(*node)->next;
	}

	struct bucket_node* alloc_node = NULL;
	if (!*node)
	{
		*node = malloc(sizeof(struct bucket_node));
		if (!*node) goto cleanup;
		alloc_node = *node;
	}
	if (!*node)
		return false;

	(*node)->next = NULL;
	(*node)->key = NULL;
	(*node)->value = NULL;

	(*node)->key = realloc_string(key);
	if (!(*node)->key) goto cleanup;

	(*node)->value = malloc(map->data_size);
	if (!(*node)->value) goto cleanup;

	memcpy((*node)->value, value, map->data_size);

	++map->num_entries;

	if (out_found) *out_found = false;
	return true;

cleanup:
	free(alloc_node);
	free((*node)->key);
	free((*node)->value);
	return false;
}

bool map_remove(struct map* map, char* key, bool* out_found)
{
	if (!map || !key)
		return false;

	struct bucket_node* node = &map->buckets[hash(key) % map->num_buckets];
	struct bucket_node* prev = NULL;
	while (node && node->value)
	{
		if (!node->key || !node->value)
			return false;
		if (strcmp(node->key, key) == 0)
		{
			free(node->key);
			free(node->value);

			if (prev)
			{
				prev->next = node->next;
				free(node);
			}
			else
			{
				if (node->next)
					memcpy(node, node->next, sizeof(*node));
				else
					*node = (struct bucket_node){0};
			}

			--map->num_entries;
			if (out_found) *out_found = true;
			return true;
		}

		prev = node;
		node = node->next;
	}

	if (out_found) *out_found = false;
	return true;
}

struct map_iterator* map_it_begin(struct map* map)
{
	if (!map)
		return NULL;

	struct map_iterator* it = malloc(sizeof(*it));
	if (!it)
		return NULL;

	it->map = map;
	it->curr_node = NULL;
	it->end = false;
	
	for (it->curr_bucket = 0; it->curr_bucket < map->num_buckets; ++it->curr_bucket)
	{
		if (map->buckets[it->curr_bucket].value)
		{
			it->curr_node = &map->buckets[it->curr_bucket];
			break;
		}
	}

	if (!it->curr_node)
		it->end = true;

	return it;
}

void map_it_next(struct map_iterator** it)
{
	if (!it || !*it)
		goto error;

	struct map* map = (*it)->map;
	if (!map)
		goto error;

	struct map_iterator* i = *it;
	while (i->curr_bucket < map->num_buckets)
	{
		if (i->curr_node->value)
		{
			i->curr_node = i->curr_node->next;
			if (i->curr_node)
				return;
		}

		++i->curr_bucket;
		if (i->curr_bucket >= map->num_buckets)
			break;

		i->curr_node = &map->buckets[i->curr_bucket];
		if (i->curr_node->value)
			return;
	}

	(*it)->end = true;
	return;

error:

	free(*it);
	*it = NULL;
	return;
}

bool map_it_end(struct map_iterator** it)
{
	if (!it || !*it)
		return false;

	if (!(*it)->end)
		return false;

	free(*it);
	*it = NULL;
	return true;
}

char* map_it_key(struct map_iterator* it)
{
	if (it && it->curr_node && it->curr_node->key)
		return it->curr_node->key;
	else
		return NULL;
}

void* map_it_value(struct map_iterator* it)
{
	if (it && it->curr_node && it->curr_node->value)
		return it->curr_node->value;
	else
		return NULL;
}
