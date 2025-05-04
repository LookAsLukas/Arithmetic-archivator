#pragma once

#include <string.h>
#include "io.h"

#define WHOLE (uint64_t)((uint64_t)1 << 32)
#define HALF (uint64_t)((uint64_t)1 << 31)
#define QUARTER (uint64_t)((uint64_t)1 << 30)

typedef struct {
    char ascii;
} flags_t;

void encode(char **filenames, int filecnt, char *archivename, flags_t flags);
void decode(char *archivename, char **filenames, int filecnt, char *directory);