#ifndef __EXISO_PLATFORM
#define __EXISO_PLATFORM


#if defined(__LINUX__)
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#endif
#if defined(__GNUC__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include <sys/types.h>

#if defined(__DARWIN__)
#define exiso_target   "macos-x"

#define PATH_CHAR      '/'
#define PATH_CHAR_STR  "/"

#define FORCE_ASCII    1
#define READFLAGS      O_RDONLY
#define WRITEFLAGS     O_WRONLY | O_CREAT | O_TRUNC
#define READWRITEFLAGS O_RDWR

typedef off_t          xoff_t;

#elif defined(__FREEBSD__)
#define exiso_target   "freebsd"

#define PATH_CHAR      '/'
#define PATH_CHAR_STR  "/"

#define FORCE_ASCII    1
#define READFLAGS      O_RDONLY
#define WRITEFLAGS     O_WRONLY | O_CREAT | O_TRUNC
#define READWRITEFLAGS O_RDWR

typedef off_t          xoff_t;

#elif defined(__LINUX__)
#define exiso_target   "linux"

#define PATH_CHAR      '/'
#define PATH_CHAR_STR  "/"

#define FORCE_ASCII    0
#define READFLAGS      O_RDONLY | O_LARGEFILE
#define WRITEFLAGS     O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE
#define READWRITEFLAGS O_RDWR | O_LARGEFILE

#define lseek          lseek64
#define stat           stat64
        
typedef off64_t        xoff_t;

#elif defined(__OPENBSD__)
#define exiso_target                            "openbsd"

#elif defined(_WIN32)
#define exiso_target       "win32"

#define PATH_CHAR           '\\'
#define PATH_CHAR_STR       "\\"

#define FORCE_ASCII         0
#define READFLAGS           O_RDONLY | O_BINARY
#define WRITEFLAGS          O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
#define READWRITEFLAGS      O_RDWR   | O_BINARY

#define S_ISDIR(x)          ((x) & _S_IFDIR)
#define S_ISREG(x)          ((x) & _S_IFREG)

#include "win32/getopt.c"
#ifdef _MSC_VER
#include "win32/asprintf.c"
#endif
#define lseek               _lseeki64
#define mkdir(a, b)         mkdir(a)

typedef __int32             int32_t;
typedef __int64             xoff_t;

#else
#error unknown target, cannot compile!
#endif


#define swap16(n) ((n) = (n) << 8 | (n) >> 8)
#define swap32(n) ((n) = (n) << 24 | (n) << 8 & 0xff0000 | (n) >> 8 & 0xff00 | (n) >> 24)

#ifdef USE_BIG_ENDIAN
#define big16(n)
#define big32(n)
#define little16(n) swap16(n)
#define little32(n) swap32(n)
#else
#define big16(n)    swap16(n)
#define big32(n)    swap32(n)
#define little16(n)
#define little32(n)
#endif

#endif
