%{
/* +-------------------------------------------------------------------+ */
/* | Copyright 1991, David Koblas.                                     | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

#include <stdio.h>
#include <varargs.h>
#include <ctype.h>
#include "defs.h"

static cmd_t	*newcmd();
char	*savestr();

#ifndef _AIX
extern char	*strchr();
#endif
int	yyline = 1;

%}

WS		[ \t]*
NWS		[^ \n\t;]+

%s ARGS

%%
	int	state = 0;
	cmd_t	*cmd;

#[^\n]*			;
\n			{ yyline++; BEGIN 0; }
^[^ \n\t]+		{ cmd = newcmd(yytext); 
				state = (strcmp(yytext,"DEFAULT")==0) ? 1 : 0;
				BEGIN ARGS; }
^{WS}			BEGIN ARGS;
<ARGS>";"		state++;
<ARGS>{NWS}		addarg(state, cmd, yytext);
<ARGS>{WS}		;
%%
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

msg(va_alist)
 va_dcl
{
#if 0
	va_list	ap;
	char	*s;

	va_start(ap);
	s = va_arg(ap, char *);
	fprintf(stderr,"line %d: ",yyline);
	vfprintf(stderr, s, ap);
	fputc('\n', stderr);
	va_end(ap);
#endif
}

static addarg(state, cmd, str)
int	state;
cmd_t	*cmd;
char	*str;
{
	if (state == 0) {
		msg("cmd='%s' add arg '%s'",cmd->name,str);
		if (cmd->margs == cmd->nargs)  {
			cmd->margs += cmd->margs;
			cmd->args = (char **)realloc(cmd->args, 
					sizeof(char *) * cmd->margs);
			if (cmd->args == NULL)
				fatal("Unable to groupw args");
		}
		cmd->args[cmd->nargs++] = savestr(str);
	} else if (state == 1) {
		msg("cmd='%s' add opt '%s'",cmd->name,str);
		if (cmd->mopts == cmd->nopts) {
			cmd->mopts += cmd->mopts;
			cmd->opts = (char **)realloc(cmd->opts, 
					sizeof(char *) * cmd->mopts);
			if (cmd->opts == NULL)
				fatal("Unable to groupw opts");
		}
		cmd->opts[cmd->nopts++] = savestr(str);
	} else {
		fatal("bad state (%d) received\n",state);
	}
}

char	*savestr(str)
char	*str;
{
	char	*s = (char *)malloc(strlen(str)+1);

	if (s == NULL) 
		fatal("No string space");

	strcpy(s, str);
	return s;
}

static cmd_t	*newcmd(name)
char	*name;
{
	cmd_t	*cmd = (cmd_t *)malloc(sizeof(cmd_t));

	cmd->next = First;
	First = cmd;

	if (cmd == NULL)
		fatal("Unable to alloc space for new command");

	cmd->name = savestr(name);
	cmd->nargs = 0;		cmd->margs = 16;
	cmd->nopts = 0;		cmd->mopts = 16;
	cmd->args = (char **)malloc(sizeof(char *)*cmd->margs);
	cmd->opts = (char **)malloc(sizeof(char *)*cmd->mopts);

	return cmd;
}

ReadFile(file)
char	*file;
{
	struct stat	statbuf;
	FILE		*fd;
	
	if ((stat(file, &statbuf) < 0) || 
		(statbuf.st_uid != 0) || /* Owned by root */
		((statbuf.st_mode & 0033) != 0)) { /* SD /* no perm */
		/* ((statbuf.st_mode & 0077) != 0)) {/* no perm */
		syslog(LOG_ERR, "Permission problems on %s", file);
		fatal("Permission problems on %s", file);
	}
	if ((fd = fopen(file,"r")) == NULL) {
		syslog(LOG_ERR, "Couldn't open on %s", file);
		fatal("Couldn't open %s", file);
	}

	yyin = fd;
	yylex();

	return 0;
}

CountArgs(cmd)
cmd_t	*cmd;
{
	int	i, val;
	int	wild = 0, max = 0;
	char	*cp, *np, str[MAXSTRLEN];

	for (i = 0; i < cmd->nargs; i++) {
		np = cmd->args[i];

		while ((cp = strchr(np, '$')) != NULL) {
			if ((cp != cmd->args[i]) && (*(cp-1) == '\\'))
				np = cp + 1;
			else
				break;
		}

		if (cp == NULL)
			continue;
		if (*(cp+1) == '*') {
			wild = 1;
			continue;
		}
		
		cp++;
		np = cp;
		
		while (isdigit(*cp))
			cp++;
		if ((cp - np) == 0)
			continue;
		strncpy(str, np, cp - np);
		str[cp - np] = '\0';
		val = atoi(str);
		if (val > max)
			max = val;
	}

	if (wild)
		return -max;
	return max;
}

static int	cmpopts(a, b)
char	*a, *b;
{
	char	*cp_a, *cp_b;
	int	val_a, val_b;
	char	str_a[MAXSTRLEN], str_b[MAXSTRLEN];

	if (*a != '$' && *b != '$')
		return 0;
	if (*a == '$' && *b != '$')
		return -1;
	if (*a != '$' && *b == '$')
		return  1;

	cp_a = ++a;
	cp_b = ++b;
	while ((*cp_a != '\0') && (*cp_a != '='))
		if (! isdigit(*cp_a))
			break;
	while ((*cp_b != '\0') && (*cp_b != '='))
		if (! isdigit(*cp_b))
			break;
	
	if (*cp_a != '=' && *cp_b != '=')
		return 0;
	if (*cp_a == '=' && *cp_b != '=')
		return -1;
	if (*cp_a != '=' && *cp_b == '=')
		return  1;

	strncpy(str_a, a, cp_a - a);
	str_a[cp_a - a] = '\0';
	val_a = atoi(str_a);
	strncpy(str_b, b, cp_b - a);
	str_a[cp_b - b] = '\0';
	val_b = atoi(str_b);

	if (val_a < val_b)
		return -1;
	if (val_a > val_b)
		return  1;
	return 0;
}

sortopts(cmd)
cmd_t	*cmd;
{
	qsort(cmd->opts, cmd->nopts, sizeof(char *), cmpopts);
}

cmd_t	*Build(def, cmd)
cmd_t	*def, *cmd;
{
	cmd_t		*new = newcmd("");
	char		defname[MAXSTRLEN], optname[MAXSTRLEN], *cp;
	int		i, j;
#ifndef _AIX
	extern char	*strchr();
#endif
	if (cmd == NULL)
		return def;
	if (def == NULL)
		return cmd;

	for (i = 0; i < cmd->nargs; i++)
		addarg(0, new, cmd->args[i]);

	for (i = 0; i < def->nopts; i++) {
		if ((cp = strchr(def->opts[i], '=')) == NULL)
			strcpy(defname, def->opts[i]);
		else {
			int	l = cp - def->opts[i];
			strncpy(defname, def->opts[i], l);
			defname[l] = '\0';
		}
		for (j = 0; j < cmd->nopts; j++) {
			if ((cp = strchr(cmd->opts[j], '=')) == NULL)
				strcpy(optname, cmd->opts[j]);
			else {
				int	l = cp - cmd->opts[j];
				strncpy(optname, cmd->opts[j], l);
				optname[l] = '\0';
			}
			if (strcmp(defname, optname) == 0)
				def->opts[i][0] = '\0';
		}
		if (def->opts[i][0] != '\0')
			addarg(1, new, def->opts[i]);
	}
	for (j = 0; j < cmd->nopts; j++)
		addarg(1, new, cmd->opts[j]);

	/* sortopts(new); */

	return new;
}
