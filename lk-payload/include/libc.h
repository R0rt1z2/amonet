#pragma once

#include <inttypes.h>

/** \name Fixed width integers
 *  @{
 */
typedef unsigned char u8_t;             ///< Unsigned 8-bit type
typedef unsigned short int u16_t;       ///< Unsigned 16-bit type
typedef unsigned int u32_t;             ///< Unsigned 32-bit type
typedef unsigned long long u64_t;        ///< Unsigned 64-bit type

typedef u64_t u64;
typedef u32_t u32;
typedef u16_t u16;
typedef u8_t u8;

#define NULL ((void *)0)

typedef unsigned size_t;

size_t strlen(const char *str);
char *strcpy(char *to, const char *from);
char *strncpy(char *dest, char const *src, size_t count);
int strncmp(const char *s1, const char *s2, u32_t n);
void*  memset(void*  dst, int c, u32_t n);
void *memcpy(void *dest, const void *src, size_t n);
int strcmp(const char *s1, const char *s2);
char *strstr(const char *s, const char *find);

int puts(const char *line);
int memcmp(const void* s1, const void* s2,size_t n);
int strwcmp(const uint8_t *s1, const char *s2);