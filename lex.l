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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <syslog.h>
#include "defs.h"

#if 0
#define _msg(x) msg x
#else
#define _msg(x)
#endif

static cmd_t *newcmd();
char *savestr();
void addvar(const char *str);
void preproc(const char *str);
void preprocerror(const char *str);
static void addquotedarg(int state, cmd_t * cmd, const char *instr);
static void addarg(int state, cmd_t * cmd, const char *instr);

int yyline = 1;

#define YY_NO_INPUT

%}

WS		[ \t]*
NWS		[^ \n\t;]+

%s ARGS
%a 2700
%o 3700

%option nounput

%%
	int	state = 0;
	cmd_t	*cmd = NULL;

#[^\n]*			;
^%(((if|elsif)[ \t]+.*)|(else|endif))		{ preproc(yytext); }
^%.*			{ preprocerror(yytext); }
\n			{ yyline++; BEGIN 0; }
^[A-Z_][A-Z_0-9]*=[^\n\r]*	{ addvar(yytext); }
^[^ \n\t]+		{ cmd = newcmd(yytext);
				state = strcmp(yytext, "DEFAULT") == 0 ? 1 : 0;
				BEGIN ARGS; }
^{WS}			BEGIN ARGS;
<ARGS>";"		state++;
<ARGS>([^ \n\t'"]*'(\\.|[^'])*')+	addquotedarg(state, cmd, yytext);
<ARGS>([^ \n\t'"]*\"(\\.|[^\"])*\")+	addquotedarg(state, cmd, yytext);
<ARGS>{NWS}		addarg(state, cmd, yytext);
<ARGS>{WS}		;
%%
/* ' for emacs */

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

void
msg(const char *format, ...)
{
#if 0
    va_list ap;
    char *s;

    va_start(ap);
    s = va_arg(ap, char *);
    fprintf(stderr, "line %d: ", yyline);
    /* Flawfinder: ignore (vfprintf) */
    vfprintf(stderr, s, ap);
    fputc('\n', stderr);
    va_end(ap);
#else
    UNUSED(format);
#endif
}

char *
expandvars(const char *instr)
{
    char *str = (char *)malloc(VAR_EXPAND_LEN);
    int i = 0;

    if (str == NULL)
	fatal(1, "Unable to allocate variable expansion buffer");

    while (*instr)
	if (isupper((int)*instr) || *instr == '_') {
	    const char *mark = instr;
	    var_t *var;

	    while (*instr
		   && (isupper((int)*instr) || isdigit((int)*instr)
		       || *instr == '_'))
		++instr;
	    for (var = Variables; var != 0; var = var->next) {
		/* Flawfinder: ignore (strlen) */
		int l = strlen(var->name);

		if (instr - mark > l)
		    l = instr - mark;
		if (!strncmp(mark, var->name, l)) {
		    str[i] = 0;
		    /* Flawfinder: fix (strcat) */
		    strlcat(str, var->value, VAR_EXPAND_LEN);
		    /* Flawfinder: ignore (strlen) */
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

void
preprocerror(const char *str)
{
    fprintf(stderr, "Invalid preprocessor command '%s'\n", str);
    exit(1);
}

void
preproc(const char *str)
{
    if (!strncmp(str, "%if", 3)) {
    } else if (!strncmp(str, "%elseif", 7)) {
    } else if (!strcmp(str, "%else")) {
    } else if (!strcmp(str, "%endif")) {
    } else
	preprocerror(str);
}

void
addvar(const char *str)
{
    /* Flawfinder: ignore (char) */
    char name[VAR_NAME_LEN], value[VAR_EXPAND_LEN];
    const char *eq = strchr(str, '=');

    if (eq && str - eq < VAR_NAME_LEN) {
	int i, o, len;
	var_t *var;

	/* Flawfinder: fix (strncpy) */
	strlcpy(name, str, MIN((size_t) (eq - str + 1), sizeof(name)));

	for (o = 0, i = eq - str + 1; o < VAR_EXPAND_LEN - 1 && str[i]; ++i)
	    if (str[i] == '\\') {
		switch (str[++i]) {
		case 'n':
		    value[o++] = '\n';
		    break;
		case 'r':
		    value[o++] = '\r';
		    break;
		case 't':
		    value[o++] = '\t';
		    break;
		case 'a':
		    value[o++] = '\a';
		    break;
		case 'b':
		    value[o++] = '\b';
		    break;
		default:
		    value[o++] = str[i];
		    break;
		}
	    } else if (str[i] == '"')
		break;
	    else
		value[o++] = str[i];
	value[o++] = 0;

	if ((var = (var_t *) malloc(sizeof(var_t))) == NULL)
	    fatal(1, "Unable to allocate var_t");
	/* Flawfinder: fix (strlen) */
	len = strlen(name) + 1;
	if ((var->name = malloc(len)) == NULL)	/* expected-warning */
	    fatal(1, "Unable to allocate variable name");
	/* Flawfinder: fix (strcpy) */
	strlcpy(var->name, name, len);
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

static void
addquotedarg(int state, cmd_t * cmd, const char *instr)
{
    /* Flawfinder: ignore (char) */
    char buffer[MAXSTRLEN];
    int i, o, q;

    /* Flawfinder: ignore (strlen) */
    if (strlen(instr) + 2 > MAXSTRLEN) {
	fatal(1, "Quoted argument too long\n");
	exit(1);
    }
    for (o = 0; !strchr("'\"", instr[o]); ++o)
	buffer[o] = instr[o];
    q = o;

    for (i = o + 1; instr[i] && instr[i] != instr[q]; ++i, ++o) {
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

static void
addarg(int state, cmd_t * cmd, const char *instr)
{
    char *str = expandvars(instr);

    if (state == 0) {
	_msg(("cmd='%s' add arg '%s'", cmd->name, str));
	if (cmd->margs == cmd->nargs) {
	    cmd->margs += cmd->margs;
	    cmd->args = (char **)realloc(cmd->args,
					 sizeof(char *) * cmd->margs);
	    if (cmd->args == NULL)
		fatal(1, "Unable to groupw args");
	}
	cmd->args[cmd->nargs++] = savestr(str);
    } else if (state == 1) {
	_msg(("cmd='%s' add opt '%s'", cmd->name, str));
	if (cmd->mopts == cmd->nopts) {
	    cmd->mopts += cmd->mopts;
	    cmd->opts = (char **)realloc(cmd->opts,
					 sizeof(char *) * cmd->mopts);
	    if (cmd->opts == NULL)
		fatal(1, "Unable to groupw opts");
	}
	cmd->opts[cmd->nopts++] = savestr(str);
    } else {
	fatal(1, "bad state (%d) received\n", state);
    }
    free(str);
}

char *
savestr(char *str)
{
    /* Flawfinder: ignore (strlen) */
    size_t len = strlen(str) + 1;
    char *s = (char *)malloc(len);

    if (s == NULL)
	fatal(1, "No string space");

    /* Flawfinder: fix (strcpy) */
    strlcpy(s, str, len);
    return s;
}

static cmd_t *
alloccmd(char *name)
{
    cmd_t *cmd = (cmd_t *) malloc(sizeof(cmd_t));

    if (cmd == NULL)
	fatal(1, "Unable to alloc space for new command");

    cmd->name = savestr(name);
    cmd->nargs = 0;
    cmd->margs = 16;
    cmd->nopts = 0;
    cmd->mopts = 16;
    cmd->args = (char **)malloc(sizeof(char *) * cmd->margs);
    cmd->opts = (char **)malloc(sizeof(char *) * cmd->mopts);

    if (cmd->args == NULL || cmd->opts == NULL)
	fatal(1, "Unable to alloc args/opts");

    return cmd;
}

void
freecmd(cmd_t *cmd)
{
    if (cmd == NULL)
	return;

    free(cmd->name);
    free(cmd->args);
    free(cmd->opts);
    free(cmd);
}

static cmd_t *
newcmd(char *name)
{
    cmd_t *cmd = alloccmd(name);

    cmd->next = First;
    First = cmd;

    return cmd;
}

int
ReadFile(char *file)
{
    struct stat statbuf;
    FILE *fd;

    if ((stat(file, &statbuf) < 0))
	return 0;
    if ((statbuf.st_uid != 0) ||	/* Owned by root */
	((statbuf.st_mode & 0077) != 0)) {	/* SD - no perm */
	logger(LOG_ERR, "Permission problems on %s", file);
	return 0;
    }
    /* Flawfinder: ignore (fopen) race condition */
    if ((fd = fopen(file, "r")) == NULL)
	return 0;

    yyin = fd;
    yylex();

    return 1;
}

int
CountArgs(cmd_t * cmd)
{
    int i, val;
    int wild = 0, max = 0;
    /* Flawfinder: ignore (char) */
    char *cp, *np, str[MAXSTRLEN];

    for (i = 0; i < cmd->nargs; i++) {
	np = cmd->args[i];

	while ((cp = strchr(np, '$')) != NULL) {
	    if ((cp != cmd->args[i]) && (*(cp - 1) == '\\'))
		np = cp + 1;
	    else {
		if (*(cp + 1) == '*') {
		    wild = 1;
		    ++cp;
		    np = cp;
		    continue;
		}
		cp++;
		np = cp;

		while (isdigit((int)*cp))
		    cp++;
		if ((cp - np) == 0)
		    continue;
		/* Flawfinder: fix (strncpy) */
		strlcpy(str, np, MIN((size_t) (cp - np + 1), sizeof(str)));
		/* Flawfinder: fix (atoi -> strtolong) */
		val = strtolong(str, 10);
		if (val > max)
		    max = val;
	    }
	}
    }

    if (wild)
	return -max;
    return max;
}

static int
cmpopts(char *a, char *b)
{
    char *cp_a, *cp_b;
    int val_a, val_b;
    /* Flawfinder: ignore (char) */
    char str_a[MAXSTRLEN], str_b[MAXSTRLEN];

    if (*a != '$' && *b != '$')
	return 0;
    if (*a == '$' && *b != '$')
	return -1;
    if (*a != '$' && *b == '$')
	return 1;

    cp_a = ++a;
    cp_b = ++b;
    while ((*cp_a != '\0') && (*cp_a != '='))
	if (!isdigit((int)*cp_a))
	    break;
    while ((*cp_b != '\0') && (*cp_b != '='))
	if (!isdigit((int)*cp_b))
	    break;

    if (*cp_a != '=' && *cp_b != '=')
	return 0;
    if (*cp_a == '=' && *cp_b != '=')
	return -1;
    if (*cp_a != '=' && *cp_b == '=')
	return 1;

    /* flawfinder: fix (strncpy) */
    strlcpy(str_a, a, MIN((size_t) (cp_a - a + 1), sizeof(str_a)));
    /* flawfinder: fix (atoi -> strtolong) */
    val_a = strtolong(str_a, 10);
    /* flawfinder: fix (strncpy) */
    strlcpy(str_b, b, MIN((size_t) (cp_b - a + 1), sizeof(str_b)));
    /* Flawfinder: fix (atoi -> strtolong) */
    val_b = strtolong(str_b, 10);

    if (val_a < val_b)
	return -1;
    if (val_a > val_b)
	return 1;
    return 0;
}

void
sortopts(cmd_t * cmd)
{
    qsort(cmd->opts, cmd->nopts, sizeof(char *),
	  (int(*)(const void *, const void *))cmpopts);
}

/* Build a new command but don't merge it into the global list */
cmd_t *
BuildSingle(cmd_t * def, cmd_t * cmd)
{
    cmd_t *new = alloccmd(cmd->name ? cmd->name : "");
    /* Flawfinder: ignore (char) */
    char defname[MAXSTRLEN], optname[MAXSTRLEN], *cp;
    int i, j;

    if (cmd == NULL) {
	freecmd(new);
	return def;
    }
    if (def == NULL) {
	freecmd(new);
	return cmd;
    }

    for (i = 0; i < cmd->nargs; i++)
	addarg(0, new, cmd->args[i]);

    for (i = 0; i < def->nopts; i++) {
	int skipped = 0;

	if ((cp = strchr(def->opts[i], '=')) == NULL)
	    /* Flawfinder: fix (strcpy) */
	    strlcpy(defname, def->opts[i], sizeof(defname));
	else {
	    int l = cp - def->opts[i];
	    /* Flawfinder: fix (strncpy) */
	    strlcpy(defname, def->opts[i], MIN(l + 1, sizeof(defname)));
	}
	for (j = 0; j < cmd->nopts; j++) {
	    if ((cp = strchr(cmd->opts[j], '=')) == NULL)
		/* Flawfinder: fix (strcpy) */
		strlcpy(optname, cmd->opts[j], sizeof(optname));
	    else {
		int l = cp - cmd->opts[j];
		/* Flawfinder: fix (strncpy) */
		strlcpy(optname, cmd->opts[j], MIN(l + 1, sizeof(optname)));
	    }
	    if (strcmp(defname, optname) == 0) {
		skipped = 1;
		break;
	    }
	}
	if (skipped)
	    continue;
	if (def->opts[i][0] != '\0')
	    addarg(1, new, def->opts[i]);
    }
    for (j = 0; j < cmd->nopts; j++)
	addarg(1, new, cmd->opts[j]);

    /* sortopts(new); */

    return new;
}

/* Build a new command *and* merge it with the global command list */
cmd_t *
Build(cmd_t * def, cmd_t * cmd)
{
    cmd_t *new = BuildSingle(def, cmd);

    new->next = First;
    First = new;

    return new;
}
