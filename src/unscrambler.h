#pragma once

#include <stdbool.h>
#include <inttypes.h>

bool unscramble(uint8_t* data, int length, uint8_t** out_data, int* out_length);
