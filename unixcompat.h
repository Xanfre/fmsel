#ifndef UNIXCOMPAT_H
#define UNIXCOMPAT_H

#include <stddef.h>
#include <stdint.h>

#define __int64 long long
#define rsize_t size_t
#define BOOL int
#define TRUE 1
#define FALSE 0

#if !defined(_WIN32) && defined(PATH_MAX) && defined(USE_PATH_MAX)
#define MAX_PATH PATH_MAX
#else
#define MAX_PATH 260
#endif
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

#define _mkgmtime timegm
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define strnicmp strncasecmp
#define _strdup strdup
#define _strtoui64 strtoull
#define _snprintf_s(a,b,c,d,...) snprintf(a,b,d,__VA_ARGS__)
#define sprintf_s snprintf
#define vsnprintf_s(a,b,c,d,...) vsnprintf(a,b,d,__VA_ARGS__)
#define vswprintf_s vswprintf
#define strcat_s(a,b,c) strncat(a,c,b-strlen(a)-1)
#define strcpy_s(a,b,c) do { memset(a,0,b); strncpy(a,c,b-1); } while (0)
#define _chmod chmod
#define _mkdir(a) mkdir(a,0755)
#define _rmdir rmdir

#define _S_IREAD S_IREAD
#define _S_IWRITE S_IWRITE

#endif
