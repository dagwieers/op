/* +-------------------------------------------------------------------+ */
/* | Copyright 1991, David Koblas.                                     | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

typedef struct cmd_s {
	char		*name;
	int		nargs, nopts;
	int		margs, mopts;
	char		**args, **opts;
	struct cmd_s	*next;
} cmd_t;

extern cmd_t	*First, *Build();

#define MAXSTRLEN	256
#define OP_ACCESS	"/usr/local/etc/op.access"
