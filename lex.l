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
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <syslog.h>
#include "defs.h"

static cmd_t	*newcmd();
char	*savestr();
void addvar(const char *str);
void preproc(const char *str);
void preprocerror(const char *str);
static void addquotedarg(int state, cmd_t *cmd, const char *instr);
static void addarg(int state, cmd_t *cmd, const char *instr);

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
^%(((if|elsif)[ \t]+.*)|(else|endif))		{ preproc(yytext); }
^%.*			{ preprocerror(yytext); }
\n			{ yyline++; BEGIN 0; }
^[A-Z_][A-Z_0-9]*=[^\n\r]*	{ addvar(yytext); }
^[^ \n\t]+		{ cmd = newcmd(yytext); 
				state = (strcmp(yytext,"DEFAULT")==0) ? 1 : 0;
				BEGIN ARGS; }
^{WS}			BEGIN ARGS;
<ARGS>";"		state++;
<ARGS>'(\\.|[^'])*'	addquotedarg(state, cmd, yytext);
<ARGS>\"(\\.|[^\"])*\"	addquotedarg(state, cmd, yytext);
<ARGS>{NWS}		addarg(state, cmd, yytext);
<ARGS>{WS}		;
%%
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

void msg(const char *format, ...)
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

char *expandvars(const char *instr) {
char *str = (char*)malloc(VAR_EXPAND_LEN);
int i = 0;

	if (str == NULL)
		fatal(1, "Unable to allocate variable expansion buffer");

	while (*instr)
		if (isupper(*instr) || *instr == '_') {
		const char *mark = instr;
		var_t *var;

			while (*instr && (isupper(*instr) || isdigit(*instr) || *instr == '_'))
				++instr;
			for (var = Variables; var != 0; var = var->next) {
			int l = strlen(var->name);

				if (instr - mark > l) l = instr - mark;
				if (!strncmp(mark, var->name, l)) {
					str[i] = 0;
					strcat(str, var->value);
					i += strlen(var->value);
					break;
				}
			}
			if (!var) {
				instr = mark + 1;
				str[i++] = *mark;
			}
		} else
			str[i++] = *instr++;
	str[i] = 0;
	return str;
}

void preprocerror(const char *str) {
	fprintf(stderr, "Invalid preprocessor command '%s'\n", str);
	exit(1);
}

void preproc(const char *str) {
	if (!strncmp(str, "%if", 3)) {
	} else
	if (!strncmp(str, "%elseif", 7)) {
	} else
	if (!strcmp(str, "%else")) {
	} else
	if (!strcmp(str, "%endif")) {
	} else
		preprocerror(str);
}

void addvar(const char *str) {
char name[VAR_NAME_LEN], value[VAR_EXPAND_LEN];
const char *eq = strchr(str, '=');

	if (eq && str - eq < VAR_NAME_LEN) {
	int i, o;
	var_t *var;

		strncpy(name, str, eq - str);
		name[eq - str] = 0;

		for (o = 0, i = eq - str + 1; o < VAR_EXPAND_LEN - 1 && str[i]; ++i)
			if (str[i] == '\\') {
				switch (str[++i]) {
					case 'n' : value[o++] = '\n'; break;
					case 'r' : value[o++] = '\r'; break;
					case 't' : value[o++] = '\t'; break;
					case 'a' : value[o++] = '\a'; break;
					case 'b' : value[o++] = '\b'; break;
					default : value[o++] = str[i]; break;
				}
			} else
			if (str[i] == '"')
				break;
			else
				value[o++] = str[i];
		value[o++] = 0;

		if ((var = (var_t*)malloc(sizeof(var_t))) == NULL)
			fatal(1, "Unable to allocate var_t");
		if ((var->name = malloc(strlen(name) + 1)) == NULL)
			fatal(1, "Unable to allocate variable name");
		strcpy(var->name, name);
		var->value = expandvars(value);
		var->next = 0;

		if (Variables) {
		var_t *i;

			for (i = Variables; i->next; i = i->next) ;
			i->next = var;
		} else
			Variables = var;
	} else
		fatal(1, "Invalid alias");
}

static void addquotedarg(int state, cmd_t *cmd, const char *instr) {
char buffer[MAXSTRLEN];
int i, o;

	if (strlen(instr) + 2 > MAXSTRLEN) {
		fatal(1, "Quoted argument too long\n");
		exit(1);
	}
	for (o = 0, i = 1; instr[i] != instr[0]; ++i, ++o) {
		if (instr[i] == '\\') {
		int c = instr[++i];

			if (strchr("'\"", c)) {
				buffer[o] = c;
			} else {
				buffer[o++] = '\\';
				buffer[o] = c;
			}
		} else
			buffer[o] = instr[i];
	}
	buffer[o] = 0;
	addarg(state, cmd, buffer);
}

static void addarg(int state, cmd_t *cmd, const char *instr) {
char *str = expandvars(instr);

	if (state == 0) {
		msg("cmd='%s' add arg '%s'",cmd->name,str);
		if (cmd->margs == cmd->nargs)  {
			cmd->margs += cmd->margs;
			cmd->args = (char **)realloc(cmd->args, 
					sizeof(char *) * cmd->margs);
			if (cmd->args == NULL)
				fatal(1, "Unable to groupw args");
		}
		cmd->args[cmd->nargs++] = savestr(str);
	} else if (state == 1) {
		msg("cmd='%s' add opt '%s'",cmd->name,str);
		if (cmd->mopts == cmd->nopts) {
			cmd->mopts += cmd->mopts;
			cmd->opts = (char **)realloc(cmd->opts, 
					sizeof(char *) * cmd->mopts);
			if (cmd->opts == NULL)
				fatal(1, "Unable to groupw opts");
		}
		cmd->opts[cmd->nopts++] = savestr(str);
	} else {
		fatal(1, "bad state (%d) received\n",state);
	}
	free(str);
}

char	*savestr(str)
char	*str;
{
	char	*s = (char *)malloc(strlen(str)+1);

	if (s == NULL) 
		fatal(1, "No string space");

	strcpy(s, str);
	return s;
}

static cmd_t	*newcmd(name)
char	*name;
{
	cmd_t	*cmd = (cmd_t *)malloc(sizeof(cmd_t));

	if (cmd == NULL)
		fatal(1, "Unable to alloc space for new command");

	cmd->next = First;
	First = cmd;

	cmd->name = savestr(name);
	cmd->nargs = 0;		cmd->margs = 16;
	cmd->nopts = 0;		cmd->mopts = 16;
	cmd->args = (char **)malloc(sizeof(char *)*cmd->margs);
	cmd->opts = (char **)malloc(sizeof(char *)*cmd->mopts);

	if (cmd->args == NULL || cmd->opts == NULL)
		fatal(1, "Unable to alloc args/opts");

	return cmd;
}

int ReadFile(file)
char	*file;
{
	struct stat	statbuf;
	FILE		*fd;
	
	if ((stat(file, &statbuf) < 0))
		return 0;
	if ((statbuf.st_uid != 0) || /* Owned by root */
			((statbuf.st_mode & 0077) != 0)) { /* SD - no perm */
		logger(LOG_ERR, "Permission problems on %s", file);
		return 0;
	}
	if ((fd = fopen(file,"r")) == NULL)
		return 0;

	yyin = fd;
	yylex();

	return 1;
}

int CountArgs(cmd)
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
			else {
				if (*(cp+1) == '*') {
					wild = 1;
					++cp;
					np = cp;
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
		}
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

void sortopts(cmd)
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
