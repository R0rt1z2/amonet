#include <libc.h>

void*  memset(void*  dst, int c, u32_t n)
{
    char*  q   = dst;
    char*  end = q + n;

    for (;;) {
        if (q >= end) break; *q++ = (char) c;
        if (q >= end) break; *q++ = (char) c;
        if (q >= end) break; *q++ = (char) c;
        if (q >= end) break; *q++ = (char) c;
    }

  return dst;
}

u32_t
strlen(const char *str)
{
    const char *s;

    for (s = str; *s; ++s)
        ;
    return (s - str);
}

char *
strcpy(char *to, const char *from)
{
    char *save = to;

    for (; (*to = *from) != '\0'; ++from, ++to);
    return(save);
}

char *strncpy(char *dest, char const *src, size_t count) {
	char *tmp = dest;
    while(count-- && (*dest++ = *src++) != '\0')
    ;
	return tmp;
}

char *
strcat(char *dest, const char *src)
{
    strcpy(dest + strlen(dest), src);
    return dest;
}

/*
 * Compare strings.
 */
int
strcmp(const char *s1, const char *s2)
{
    while (*s1 == *s2++)
        if (*s1++ == 0)
            return (0);
    return (*(unsigned char *)s1 - *(unsigned char *)--s2);
}

int
strncmp(const char *s1, const char *s2, u32_t n)
{
    if (n == 0)
        return (0);
    do {
        if (*s1 != *s2++)
            return (*(unsigned char *)s1 - *(unsigned char *)--s2);
        if (*s1++ == 0)
            break;
    } while (--n != 0);
    return (0);
}

void *memcpy(void *dest, const void *src, size_t n)
{
    char *dp = dest;
    const char *sp = src;
    while (n--)
        *dp++ = *sp++;
    return dest;
}

int memcmp(const void* s1, const void* s2,size_t n)
{
    const unsigned char *p1 = s1, *p2 = s2;
    while(n--)
        if( *p1 != *p2 )
            return *p1 - *p2;
        else
            p1++,p2++;
    return 0;
}

char *
strstr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	c = *find++;
	if (c != 0) {
		len = strlen(find);
		do {
			do {
				sc = *s++;
				if (sc == 0)
				return NULL;
			} while (sc != c);
		} while (strncmp(s, find, len) != 0);
	s--;
	}
	return (char *)s;
}

int strwcmp(const uint8_t *s1, const char *s2)
{
    int i = 0;
    
    while (s2[i] != '\0' && i < 36) {
        if (s1[i * 2] != s2[i] || s1[i * 2 + 1] != 0) {
            return 1;
        }
        i++;
    }
    
    if (s1[i * 2] != 0 || s1[i * 2 + 1] != 0) {
        return 1;
    }
    
    return 0;
}