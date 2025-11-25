#pragma once

#include "nanoprintf.h"

int printf(const char* fmt, ...);
int video_printf(const char* fmt, ...);
void hex_dump(const void* data, size_t size);