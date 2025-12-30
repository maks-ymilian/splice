#pragma once

#include <stdbool.h>

#include "common.h"

struct database;

struct database* database_init(char* files_path);
void database_uninit(struct database* db);

bool database_get(struct database* db, char* file, char* url, struct buffer* buffer);
bool database_get_and_save(struct database* db, char* file, char* url);
bool database_get_file_path(struct database* db, char* file, bool* out_exists, char** file_on_disk);
