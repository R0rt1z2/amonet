#pragma once

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#include "nanoprintf.h"

int printf(const char* fmt, ...);
void hex_dump(const void* data, size_t size);