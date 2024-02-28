#include <libc/string.h>

#include "libc/internal.h"

char* strcpy(char* LIBC_RESTRICT dest, const char* LIBC_RESTRICT src)
{
    char* temp = dest;

    while ((*dest++ = *src++)) { }

    return temp;
}