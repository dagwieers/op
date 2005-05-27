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

typedef struct cmd_s {
	char		*name;
	int		nargs, nopts;
	int		margs, mopts;
	char		**args, **opts;
	struct cmd_s	*next;
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
void array_free(array_t *array);
array_t *array_free_contents(array_t *array);
void *array_push(array_t *array, void *object);
void *array_pop(array_t *array);
int array_extend(array_t *array, int capacity);

extern cmd_t	*First, *Build(), *BuildSingle();
extern var_t	*Variables;
extern unsigned	minimum_logging_level;

void fatal(int logit, const char *format, ...);
int logger(unsigned flags, const char *format, ...);
void strnprintf(char *out, int len, const char *format, va_list ap);
char *strtolower(char *in);

int ReadFile(char *file);
int ReadDir(char *dir);
int CountArgs(cmd_t *cmd);
int atov(char *str, int type);

#define MAXSTRLEN	2048
#define OP_ACCESS	"/etc/op.conf"
#define OP_ACCESS_DIR	"/etc/op.d"
#define VERSION     "1.30"

#define VAR_EXPAND_LEN	8192
#define	VAR_NAME_LEN	64	

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX	255
#endif
