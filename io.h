#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void bits(uint8_t n);
uint8_t emit(FILE *out, uint64_t s, int mode);
void print_options();
char* print_directory(char *path);
void print_size(double size);