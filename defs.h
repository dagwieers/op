/* +-------------------------------------------------------------------+ */
/* | Copyright 1991, David Koblas.                                     | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

#include "config.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

#if HAVE_MALLOC == 0
# ifdef malloc
#  undef malloc
#  define rpl_malloc
# endif
# ifdef realloc
#  undef realloc
#  define rpl_realloc
# endif
#endif
#include <stdlib.h>
#if HAVE_MALLOC == 0
# ifdef rpl_malloc
#  undef rpl_malloc
#  define malloc	rpl_malloc
# endif
# ifdef rpl_realloc
#  undef rpl_realloc
#  define realloc	rpl_realloc
# endif
#endif

#include <unistd.h>

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#ifdef STDC_HEADERS
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr(), *strrchr();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#ifdef HAVE_LIBBSD
#include <bsd/string.h>
#else
# ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t siz);
size_t strlcpy(char *dst, const char *src, size_t siz);
# endif
#endif

#ifndef HAVE_VSNPRINTF
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
    size_t nargs, nopts;
    size_t margs, mopts;
    char **args, **opts;
    struct cmd_s *next;
} cmd_t;

typedef struct var_s {
    char *name, *value;
    struct var_s *next;
} var_t;

typedef struct array_s {
    void **data;
    size_t size, capacity;
} array_t;

/* functions to manage a dynamically extensible array of pointers */
#define ARRAY_CHUNK	32
array_t *array_alloc(void);
void array_free(array_t * array);
array_t *array_free_contents(array_t * array);
void *array_push(array_t * array, void *object);
void *array_pop(array_t * array);
int array_extend(array_t * array, size_t capacity);

char *savestr(const char *str);
cmd_t *Build(cmd_t * def, cmd_t * cmd);
cmd_t *BuildSingle(cmd_t * def, cmd_t * cmd);

extern cmd_t *First;
extern var_t *Variables;

/* cppcheck-suppress noreturn */
int logger(unsigned level, const char *format, ...);
void fatal(int logit, const char *format, ...);
char *strtolower(char *in);
/* NOLINTNEXTLINE(runtime/int) */
long strtolong(const char *str, int base);

int ReadFile(const char *file);
int CountArgs(cmd_t * cmd);

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

#ifndef PASS_MAX
#define PASS_MAX	512
#endif
