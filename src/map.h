#pragma once

#include <stdbool.h>

struct map;
struct map_iterator;

struct map* map_init(int data_size);
void map_uninit(struct map* map);

bool map_get(struct map* map, char* key, void* out_value, bool* out_found);
bool map_add(struct map* map, char* key, void* value, bool* out_found);
bool map_remove(struct map* map, char* key, bool* out_found);

struct map_iterator* map_it_begin(struct map* map);
void map_it_next(struct map_iterator** it);
bool map_it_end(struct map_iterator** it);
char* map_it_key(struct map_iterator* it);
void* map_it_value(struct map_iterator* it);
