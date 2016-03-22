/* +-------------------------------------------------------------------+ */
/* | Copyright 1991, David Koblas.                                     | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

#include <unistd.h>
#include <limits.h>
#include "config.h"

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <sys/types.h>
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#if STDC_HEADERS
# include <string.h>
#else
# if !HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr(), *strrchr();
# if !HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#if HAVE_LIBBSD
#include <bsd/string.h>
#else
# if !HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t siz);
size_t strlcpy(char *dst, const char *src, size_t siz);
# endif
#endif

#if !HAVE_VSNPRINTF
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list arg);
#endif

#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#define UNUSED(x) (void)(x)

#ifdef malloc
void *rpl_malloc(size_t n);
#endif
#ifdef realloc
void *rpl_realloc(void *ptr, size_t n);
#endif

typedef struct cmd_s {
    char *name;
    int nargs, nopts;
    int margs, mopts;
    char **args, **opts;
    struct cmd_s *next;
} cmd_t;

typedef struct var_s {
    char *name, *value;
    struct var_s *next;
} var_t;

typedef struct array_s {
    void **data;
    int size, capacity;
} array_t;

/* functions to manage a dynamically extensible array of pointers */
#define ARRAY_CHUNK	32
array_t *array_alloc();
void array_free(array_t * array);
array_t *array_free_contents(array_t * array);
void *array_push(array_t * array, void *object);
void *array_pop(array_t * array);
int array_extend(array_t * array, int capacity);

extern cmd_t *First, *Build(), *BuildSingle();
extern var_t *Variables;
extern unsigned minimum_logging_level;

void fatal(int logit, const char *format, ...);
int logger(unsigned flags, const char *format, ...);
char *strtolower(char *in);
/* NOLINTNEXTLINE(runtime/int) */
long strtolong(char *str, int base);

int ReadFile(char *file);
int ReadDir(char *dir);
int CountArgs(cmd_t * cmd);
int atov(char *str, int type);

#define MAXSTRLEN	2048
#ifndef SYSCONFDIR
#define SYSCONFDIR	"/etc"
#endif
#define OP_ACCESS	SYSCONFDIR "/op.conf"
#define OP_ACCESS_DIR	SYSCONFDIR "/op.d"

#define VAR_EXPAND_LEN	8192
#define	VAR_NAME_LEN	64

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX	255
#endif
