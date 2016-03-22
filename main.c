/* +-------------------------------------------------------------------+ */
/* | Copyright 1991, David Koblas.                                     | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <netdb.h>
#ifdef __hpux
extern int innetgr(__const char *__netgroup, __const char *__host,
		   __const char *__user, __const char *__domain);
#endif
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <ctype.h>
#include "defs.h"
#include "regexp.h"

#ifdef sun
#  if defined(__SVR4) || defined(__svr4__)
#    define SOLARIS
#  endif
#endif

#if defined(USE_SHADOW) && defined(USE_PAM)
#error USE_SHADOW and USE_PAM are mutually exclusive
#endif

#ifdef USE_SHADOW
#include <shadow.h>
#endif

#ifdef USE_PAM
#include <security/pam_appl.h>
#endif

#ifdef SECURID
#include "sdi_athd.h"
#include "sdconf.h"
union config_record configure;
#endif

#ifndef LOG_AUTH
/*
**  Pmax's don't have LOG_AUTH
*/
#define LOG_AUTH LOG_WARNING
#endif

#define LOG_PRINT	(1UL << 31)

#define	MAXARG	1024
#define	MAXENV	MAXARG

extern char *savestr();
/* Flawfinder: ignore */
extern char *getpass(), *crypt();

char *format_cmd(int argc, char **argv, char *retbuf, /* UNUSED */ size_t buflen);
char *GetCode();
int Verify(cmd_t * cmd, size_t num, int argc, char **argv);
int VerifyPermissions(cmd_t * cmd);
cmd_t *Find(char *name);
char *FindOpt(cmd_t * cmd, char *str);
void ListCommands();
int Go(cmd_t * cmd, /* UNUSED */ size_t num, int argc, char **argv);
cmd_t *First = NULL;
var_t *Variables = NULL;
char *realuser = NULL;
int gargc = -1;
char **gargv = NULL;
sigset_t sig_mask, old_sig_mask;
unsigned minimum_logging_level = 99;

void
Usage()
{
    fatal(0, "Usage: %s mnemonic [args]\n"
	  "       %s -l     List available commands\n"
	  "       %s -V     Show op version", gargv[0], gargv[0], gargv[0]);
}

int
FileCompare(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

int
SortCommandList(const void *a, const void *b)
{
    return strcmp((*(cmd_t **) a)->name, (*(cmd_t **) b)->name);
}

void
ListCommands()
{
    cmd_t *def, *cmd;
    array_t *cmds = array_alloc();
    size_t length = 0, i;

    def = Find("DEFAULT");
    /*      first pass, get maximum command length and number of commands we have
       permission to use */
    for (cmd = First; cmd != NULL; cmd = cmd->next) {
	if (strcmp(cmd->name, "DEFAULT")) {
	    cmd_t *new = BuildSingle(def, cmd);

	    if (VerifyPermissions(new) >= 0) {
		/* Flawfinder: ignore (strlen) */
		size_t l = strlen(new->name);

		for (i = 0; i < cmds->size; ++i)
		    if (!strcmp(((cmd_t *) cmds->data[i])->name, new->name))
			break;
		if (i == cmds->size) {
		    if (l > length)
			length = l;
		    array_push(cmds, new);
		}
	    }
	}
    }

    qsort(cmds->data, cmds->size, sizeof(void *), SortCommandList);

    /* second pass, display */
    for (i = 0; i < cmds->size; ++i) {
	cmd = cmds->data[i];

	if (strcmp(cmd->name, "DEFAULT")) {
	    cmd_t *new = BuildSingle(def, cmd);

	    if (VerifyPermissions(new) >= 0) {
		char *help = FindOpt(new, "help");

		if (!help || !*help) {
		    size_t j, len = 0;

		    for (j = 0; j < cmd->nargs; ++j)
			/* Flawfinder: ignore (strlen) */
			len += strlen(cmd->args[j]) + 1;
		    help = (char *)malloc(len);
		    /* Flawfinder: fix (strcpy) */
		    strlcpy(help, cmd->args[0], len);
		    for (j = 1; j < cmd->nargs; ++j) {
			/* Flawfinder: fix (strcat) */
			strlcat(help, " ", len);
			if (strchr(cmd->args[j], ' ')
			    || strchr(cmd->args[j], '\t')) {
			    /* Flawfinder: fix (strcat) */
			    strlcat(help, "'", len);
			    strlcat(help, cmd->args[j], len);
			    strlcat(help, "'", len);
			} else
			    /* Flawfinder: fix (strcat) */
			    strlcat(help, cmd->args[j], len);
		    }
		}
		printf("%-*s", (int)length + 2, new->name);
		printf("%-*.*s", 77 - (int)length, 77 - (int)length, help);
		/* Flawfinder: ignore (strlen) */
		if (strlen(help) > 77 - length)
		    printf("...\n");
		else
		    printf("\n");
	    }
	}
    }
    array_free(cmds);
}

int
ReadDir(char *dir)
{
    DIR *d;

    if ((d = opendir(dir)) != NULL) {
	struct dirent *f;
	size_t i;
	int successes = 0;
	array_t *dir_list = array_alloc();

	while ((f = readdir(d))) {
	    if (f->d_name[0] == '.'
		/* Flawfinder: ignore (strlen) */
		|| (strlen(f->d_name) > 5
		    /* Flawfinder: ignore (strlen) */
		    && strcmp(f->d_name + strlen(f->d_name) - 5, ".conf")))
		continue;
	    if (!array_push(dir_list, savestr(f->d_name)))
		fatal(1, "failed to malloc space for directory entry");
	}
	closedir(d);
	qsort(dir_list->data, dir_list->size, sizeof(void *), FileCompare);
	for (i = 0; i < dir_list->size; ++i) {
	    /* Flawfinder: ignore (char) */
	    char full_path[PATH_MAX];
	    snprintf(full_path, PATH_MAX, "%s/%s", OP_ACCESS_DIR,
		     (char *)dir_list->data[i]);
	    if (ReadFile(full_path))
		successes++;
	}
	return successes;
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    int num, argStart = 1;
    /* Flawfinder: ignore (char) */
    char user[MAXSTRLEN];
    cmd_t *cmd, *def, *new;
    struct passwd *pw;
    int lflag = 0, hflag = 0, read_conf = 0, read_conf_dir = 0;
    char *uptr = NULL;
    /* Flawfinder: ignore (char) */
    /* XXX cppcheck unusedVariable:Unused variable: cmd_s, pcmd_s */
    /* char cmd_s[MAXSTRLEN]; */
    /* char *pcmd_s; */

    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGINT);
    sigaddset(&sig_mask, SIGQUIT);
    sigaddset(&sig_mask, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &sig_mask, &old_sig_mask))
	fatal(1, "could not set signal mask");

    gargv = argv;
    gargc = argc;

    while (1) {
	if (argStart >= argc)
	    break;

	if (strcmp("-V", argv[argStart]) == 0) {
	    printf("%s\n", VERSION);
	    return 0;
	} else if (strcmp("-l", argv[argStart]) == 0) {
	    lflag++;
	    argStart++;
	} else if (strcmp("-H", argv[argStart]) == 0) {
	    hflag++;
	    argStart++;
	} else if (strcmp("-u", argv[argStart]) == 0) {
	    /* Flawfinder: ignore (strlen) */
	    if (strlen(argv[argStart]) == 2) {
		if (argStart + 1 >= argc)
		    Usage();
		argStart++;
		uptr = argv[argStart];
	    }
	    argStart++;
	} else if (strcmp("-uH", argv[argStart]) == 0) {
	    hflag++;
	    /* Flawfinder: ignore (strlen) */
	    if (strlen(argv[argStart]) == 3) {
		if (argStart + 1 >= argc)
		    Usage();
		argStart++;
		uptr = argv[argStart];
	    }
	    argStart++;
	} else if (strcmp("-Hu", argv[argStart]) == 0) {
	    hflag++;
	    /* Flawfinder: ignore (strlen) */
	    if (strlen(argv[argStart]) == 3) {
		if (argStart + 1 >= argc)
		    Usage();
		argStart++;
		uptr = argv[argStart];
	    }
	    argStart++;
	} else {
	    break;
	}
    }

#if OPENLOG_VOID
    openlog("op", LOG_PID | LOG_CONS, LOG_AUTH);
#else
    if (openlog("op", LOG_PID | LOG_CONS, LOG_AUTH) < 0)
	fatal(0, "openlog failed");
#endif
    read_conf = ReadFile(OP_ACCESS);
    read_conf_dir = ReadDir(OP_ACCESS_DIR);

    if (!read_conf && !read_conf_dir)
	fatal(1,
	      "could not open %s or any configuration files in %s"
	      "(check that file permissions are 600)",
	      OP_ACCESS, OP_ACCESS_DIR);

    if ((pw = getpwuid(getuid())) == NULL)
	exit(EXIT_FAILURE);
    realuser = (char *)strdup(pw->pw_name);
    /* Flawfinder: fix (strncpy) */
    strlcpy(user, pw->pw_name, MAXSTRLEN);

    if (lflag) {
	ListCommands();
	exit(EXIT_SUCCESS);
    }

    if (hflag) {
	if (uptr != NULL) {
	    if (getuid() != 0)
		fatal(1, "permission denied for -u option");
	}
    }
    if (uptr != NULL)
	Usage();

    if (argStart >= argc)
	Usage();

    def = Find("DEFAULT");

    /* Reduce fully qualifed path to basename and see if that is a command */
    uptr = strrchr(argv[argStart], '/');
    if (uptr == NULL)
	uptr = argv[argStart];
    else {
	uptr++;
	/* Flawfinder: ignore (race condition) */
	if (access(argv[argStart], F_OK) != 0)
	    /* Flawfinder: ignore (race condition) */
	    if (access(argv[argStart], X_OK) != 0)
		fatal(1, "unknown or non executable command");
    }
    cmd = Find(uptr);

    if (cmd == NULL)
	fatal(1, "no such command %s", argv[1]);

    argc -= argStart;
    argv += argStart;

    new = Build(def, cmd);

    num = CountArgs(new);

    if ((num < 0) && ((argc - 1) < -num))
	fatal(1, "%s: improper number of arguments", cmd->name);
    if ((num > 0) && ((argc - 1) != num))
	fatal(1, "%s: improper number of arguments", cmd->name);
    if (num < 0)
	num = -num;

    /* XXX cppcheck unreadVariable:Variable 'pcmd_s' is assigned
     *     a value that is never used */
    /* pcmd_s = format_cmd(argc, argv, cmd_s, MAXSTRLEN); */
    if (Verify(new, num, argc, argv) < 0)
	fatal(0, "%s: permission denied by op", cmd->name);

    return Go(new, num, argc, argv);
}

cmd_t *
Find(char *name)
{
    cmd_t *cmd;

    for (cmd = First; cmd != NULL; cmd = cmd->next) {
	if (strcmp(cmd->name, name) == 0)
	    break;
    }

    return cmd;
}

char *
FindOpt(cmd_t * cmd, char *str)
{
    /* Flawfinder: ignore (char) */
    static char nul[1] = "";
    size_t i;
    char *cp;

    for (i = 0; i < cmd->nopts; i++) {
	if ((cp = strchr(cmd->opts[i], '=')) == NULL) {
	    if (strcmp(cmd->opts[i], str) == 0)
		return nul;
	} else {
	    size_t l = cp - cmd->opts[i];
	    if (strncmp(cmd->opts[i], str, l) == 0)
		return cp + 1;
	}
    }

    return NULL;
}

char *
GetField(char *cp, char *str, size_t len)
{
    char *end = str + len - 2;

    if (*cp == '\0')
	return NULL;

    while ((*cp != '\0') && (*cp != ',')) {
	if (*cp == '\\')
	    if (*(cp + 1) == ',') {
		*str++ = ',';
		cp++;
	    } else
		*str++ = '\\';
	else
	    *str++ = *cp;
	cp++;
	/* string exceeded target buffer length */
	if (str >= end)
	    return NULL;
    }

    *str = '\0';

    return (*cp == '\0') ? cp : (cp + 1);
}

#ifdef USE_PAM
#if defined(__hpux) || defined(SOLARIS) || defined(_AIX)
#define noconst
#else
#define noconst const
#endif
/* ARGUSED3 */
int
pam_conversation(int num_msg, noconst struct pam_message **msg,
		 struct pam_response **response, void *appdata_ptr)
{
    size_t i;
    const struct pam_message *pm;
    struct pam_response *pr;
    char *pass;

    UNUSED(appdata_ptr);
    if ((*response = malloc(sizeof(struct pam_response) * num_msg)) == NULL)
	return PAM_CONV_ERR;
    memset(*response, 0, num_msg * sizeof(struct pam_response));

    for (i = 0, pm = *msg, pr = *response; i < num_msg; ++i, ++pm, ++pr) {
	switch (pm->msg_style) {
	case PAM_PROMPT_ECHO_ON:
	    if (!(pass = malloc(512)))
		return PAM_CONV_ERR;
	    puts(pm->msg);
	    fgets(pass, 512, stdin);
	    pr->resp = pass;
	    break;
	case PAM_PROMPT_ECHO_OFF:
	    /* Flawfinder: ignore (getpass) */
	    if ((pass = getpass(pm->msg)) == NULL) {
		for (pr = *response, i = 0; i < num_msg; ++i, ++pr)
		    if (pr->resp) {
			/* Flawfinder: ignore (strlen) */
			memset(pr->resp, 0, strlen(pr->resp));
			free(pr->resp);
			pr->resp = NULL;
		    }
		memset(*response, 0, num_msg * sizeof(struct pam_response));
		free(*response);
		*response = NULL;
		return PAM_CONV_ERR;
	    }
	    pr->resp = savestr(pass);
	    break;
	case PAM_TEXT_INFO:
	    if (pm->msg)
		puts(pm->msg);
	    break;
	case PAM_ERROR_MSG:
	    if (pm->msg) {
		fputs(pm->msg, stderr);
		fputc('\n', stderr);
	    }
	    break;
	default:
	    for (pr = *response, i = 0; i < num_msg; ++i, ++pr)
		if (pr->resp) {
		    /* Flawfinder: ignore (strlen) */
		    memset(pr->resp, 0, strlen(pr->resp));
		    free(pr->resp);
		    pr->resp = NULL;
		}
	    memset(*response, 0, num_msg * sizeof(struct pam_response));
	    free(*response);
	    *response = NULL;
	    return PAM_CONV_ERR;
	    break;
	}
    }
    return PAM_SUCCESS;
}

#endif

int
VerifyPermissions(cmd_t * cmd)
{
    int gr_fail = 1, uid_fail = 1, netgr_fail = 1;
    size_t i;
    /* Flawfinder: ignore (char) */
    char *cp, str[MAXSTRLEN], hostname[HOST_NAME_MAX];
    regexp *reg1 = NULL;
    struct passwd *pw;
    /* cppcheck-suppress variableScope */
    struct group *gr;

    /* root always has access - it is pointless refusing */
    if (getuid() == 0)
	return 0;

    if (gethostname(hostname, HOST_NAME_MAX) == -1)
	return logger(LOG_ERR, "Could not get hostname");

    if ((pw = getpwuid(getuid())) == NULL)
	return logger(LOG_ERR, "Could not get uid of current effective uid");

    if ((cp = FindOpt(cmd, "groups")) != NULL) {
	/* Flawfinder: ignore (char) */
	char grouphost[MAXSTRLEN + HOST_NAME_MAX], regstr[MAXSTRLEN];

	for (cp = GetField(cp, str, MAXSTRLEN - 5); cp != NULL;
	     cp = GetField(cp, str, MAXSTRLEN - 5)) {
	    /* Flawfinder: fix (strcpy, strcat) */
	    strlcpy(regstr, "^(", sizeof(regstr));
	    strlcat(regstr, str, sizeof(regstr));
	    strlcat(regstr, ")$", sizeof(regstr));

	    if ((reg1 = regcomp(regstr)) == NULL)
		return logger(LOG_ERR, "Invalid regex '%s'", regstr);

	    if ((gr = getgrgid(pw->pw_gid)) != NULL) {
		/* Flawfinder: fix (strcpy, strcat) */
		strlcpy(grouphost, gr->gr_name, sizeof(grouphost));
		strlcat(grouphost, "@", sizeof(grouphost));
		strlcat(grouphost, hostname, sizeof(grouphost));

		if (regexec(reg1, gr->gr_name) == 1 || regexec(reg1, grouphost)) {
		    gr_fail = 0;
		    break;
		}
	    }

	    setgrent();
	    while ((gr = getgrent()) != NULL) {
		/* while -> for */
		for (i = 0; gr->gr_mem[i] != NULL; i++)
		    if (strcmp(gr->gr_mem[i], pw->pw_name) == 0)
			break;

		if (gr->gr_mem[i] != NULL) {
		    /* Flawfinder: fix (strcpy, strcat) */
		    strlcpy(grouphost, gr->gr_name, sizeof(grouphost));
		    strlcat(grouphost, "@", sizeof(grouphost));
		    strlcat(grouphost, hostname, sizeof(grouphost));
		    if (regexec(reg1, gr->gr_name) == 1
			|| regexec(reg1, grouphost)) {
			gr_fail = 0;
			break;
		    }
		}
	    }
	}
    }
    if (reg1 != NULL) {
	free(reg1);
	reg1 = NULL;
    }

    if (gr_fail && ((cp = FindOpt(cmd, "users")) != NULL)) {
	/* Flawfinder: ignore (char) */
	char currenttime[13], userhost[MAXSTRLEN + HOST_NAME_MAX],
	    regstr[MAXSTRLEN];
	time_t now = time(NULL);

	strftime(currenttime, 13, "%Y%m%d%H%M", localtime(&now));

	for (cp = GetField(cp, str, MAXSTRLEN - 5); cp != NULL;
	     cp = GetField(cp, str, MAXSTRLEN - 5)) {
	    /* cppcheck-suppress variableScope */
	    /* Flawfinder: ignore (char) */
	    char expiretime[13], *expirestart = strchr(str, '/');

	    if (expirestart)
		*expirestart = 0;

	    /* Flawfinder: fix (strcpy, strcat) */
	    strlcpy(regstr, "^(", sizeof(regstr));
	    strlcat(regstr, str, sizeof(regstr));
	    strlcat(regstr, ")$", sizeof(regstr));

	    /* Flawfinder: fix (strcpy, strcat) */
	    strlcpy(userhost, pw->pw_name, sizeof(userhost));
	    strlcat(userhost, "@", sizeof(userhost));
	    strlcat(userhost, hostname, sizeof(userhost));

	    if ((reg1 = regcomp(regstr)) == NULL)
		return logger(LOG_ERR, "Invalid regex '%s'", regstr);

	    if (regexec(reg1, pw->pw_name) == 1 || regexec(reg1, userhost) == 1) {
		/* valid user, check expiry (if any) */
		if (expirestart) {
		    ++expirestart;

		    /* ensure at least some sanity in the expiry time */
		    for (i = 0; expirestart[i]; ++i) {
			if (i > 11)
			    return logger(LOG_ERR,
					  "Expiry value (%s) has too many digits",
					  expirestart);
			if (!isdigit((int)expirestart[i]))
			    return logger(LOG_ERR,
					  "Expiry value (%s) has non-numeric characters",
					  expirestart);
		    }

		    /* Flawfinder: fix (strcpy, strncpy -> strlcpy, strlcat) */
		    strlcpy(expiretime, expirestart, sizeof(expiretime));
		    /* YYYYMMDD[HHmm] */
		    strlcat(expiretime, "000000000000", sizeof(expiretime));

		    if (strcmp(currenttime, expiretime) >= 0)
			return logger(LOG_ERR, "Access expired at %s",
				      expiretime);
		}

		uid_fail = 0;
		break;
	    }
	}
    }
    if (reg1 != NULL) {
	/* cppcheck-suppress doubleFree
	   Memory pointed to by 'reg1' is freed twice. */
	free(reg1);
	reg1 = NULL;
    }

    if (uid_fail && (cp = FindOpt(cmd, "netgroups")) != NULL) {
	for (cp = GetField(cp, str, MAXSTRLEN - 5); cp != NULL && netgr_fail;
	     cp = GetField(cp, str, MAXSTRLEN - 5)) {
	    if (innetgr(str, hostname, pw->pw_name, NULL)) {
		netgr_fail = 0;
		break;
	    }
	}
    }

    if (gr_fail && uid_fail && netgr_fail)
	return -1;
    return 0;
}

int
Verify(cmd_t * cmd, size_t num, int argc, char **argv)
{
    size_t i, j;
    /* NOLINTNEXTLINE(runtime/int) */
    long val;
    /* Flawfinder: ignore (char) */
    char *np, *cp, str[MAXSTRLEN], buf[MAXSTRLEN];
    regexp *reg1 = NULL;
    regexp *reg2 = NULL;
    struct passwd *pw;
#ifdef USE_SHADOW
    struct spwd *spw;
#endif
#ifdef USE_PAM
    struct pam_conv pamconv = { pam_conversation, NULL };
    pam_handle_t *pam;
#endif
#ifdef SECURID
    struct SD_CLIENT sd_dat, *sd;
#endif

    if ((pw = getpwuid(getuid())) == NULL)
	return -1;

#ifdef SECURID
    if ((cp = FindOpt(cmd, "securid")) != NULL) {
	memset(&sd_dat, 0, sizeof(sd_dat));	/* clear sd_auth struct */
	sd = &sd_dat;
	creadcfg();		/*  accesses sdconf.rec  */
	if (sd_init(sd)) {
	    return logger(LOG_WARNING | LOG_PRINT, "Cannot contact ACE server");
	}
	if (sd_auth(sd))
	    return -1;
    }
#else
    if ((cp = FindOpt(cmd, "securid")) != NULL) {
	return logger(LOG_ERR | LOG_PRINT,
		      "SecureID not supported by op. Access denied");
    }
#endif

    if (getuid() != 0 && (cp = FindOpt(cmd, "password")) != NULL) {
#ifdef USE_PAM
	if ((cp = GetField(cp, str, MAXSTRLEN)) != NULL) {
	    /* Flawfinder: ignore (getpass) */
	    if ((np = getpass("Password:")) == NULL)
		return logger(LOG_ERR, "Could not get user password");

	    /* Flawfinder: ignore (crypt) */
	    if (strcmp(crypt(np, str), str) != 0)
		return logger(LOG_ERR, "Incorrect direct password");
	} else {
	    int resp;

	    resp = pam_start("op", pw->pw_name, &pamconv, &pam);
	    if (resp == PAM_SUCCESS)
		resp = pam_authenticate(pam, PAM_SILENT);
	    if (resp == PAM_SUCCESS)
		resp = pam_acct_mgmt(pam, 0);
	    if (resp != PAM_SUCCESS) {
		return logger(LOG_ERR, "pam_authticate: %s",
			      pam_strerror(pam, resp));
	    }
	    pam_end(pam, resp);
	}
#else
	/* Flawfinder: ignore (getpass) */
	if ((np = getpass("Password:")) == NULL)
	    return logger(LOG_ERR, "Could not get user password");

	if ((cp = GetField(cp, str, MAXSTRLEN)) != NULL) {
	    /* Flawfinder: ignore (crypt) */
	    if (strcmp(crypt(np, str), str) != 0)
		return logger(LOG_ERR, "Incorrect direct password");
	} else {
#ifdef USE_SHADOW
	    if (strcmp(pw->pw_passwd, "x") == 0) {	/* Shadow passwords */
		if ((spw = getspnam(pw->pw_name)) == NULL)
		    return logger(LOG_ERR, "No shadow entry for '%s'",
				  pw->pw_name);
		pw->pw_passwd = spw->sp_pwdp;
	    }
#endif

	    /* Flawfinder: ignore (crypt) */
	    if (!cp && strcmp(crypt(np, pw->pw_passwd), pw->pw_passwd) != 0)
		return logger(LOG_ERR, "Invalid user password");
	}
#endif
    }

    /* XXX cppcheck clarifyCondition:
     *     Suspicious condition (assignment + comparison);
     *     Clarify expression with parentheses
     */
    if (VerifyPermissions(cmd) < 0)
	return logger(LOG_ERR,
		      "Both user, group and netgroup authentication failed");

    for (i = 0; i < cmd->nopts; i++) {
	if ((cmd->opts[i][0] != '$')
	    || ((cp = strchr(cmd->opts[i], '=')) == NULL))
	    continue;
	if (cmd->opts[i][1] != '*') {
	    for (np = cmd->opts[i] + 1; np != cp; np++)
		if (!isdigit((int)*np))
		    break;
	    if (np != cp)
		continue;
	} else {
	    if (cmd->opts[i][2] != '=')
		continue;
	    np = cmd->opts[i] + 3;
	    for (j = num + 1; j < argc; j++) {
		cp = np;
		for (cp = GetField(cp, str, MAXSTRLEN - 5); cp != NULL;
		     cp = GetField(cp, str, MAXSTRLEN - 5)) {
		    /* Flawfinder: ignore (char) */
		    char regstr[MAXSTRLEN];

		    /* Flawfinder: fix (strcpy, strcat) */
		    strlcpy(regstr, "^(", sizeof(regstr));
		    strlcat(regstr, str, sizeof(regstr));
		    strlcat(regstr, ")$", sizeof(regstr));

		    if ((reg1 = regcomp(regstr)) == NULL)
			return logger(LOG_ERR, "Invalid regex '%s'", regstr);
		    if (regexec(reg1, argv[j]) == 1)
			break;
		}
		if (cp == NULL)
		    return logger(LOG_ERR,
				  "%s: argument %i (%s) did not pass wildcard constraint",
				  cmd->name, j, argv[j]);
	    }
	}
	if (reg1 != NULL) {
	    free(reg1);
	    reg1 = NULL;
	}

	/* Flawfinder: fix (strncpy) */
	strlcpy(str, cmd->opts[i] + 1,
		MIN((size_t) (cp - cmd->opts[i]), sizeof(str)));

	if (!isdigit((int)*str))
	    continue;

	/* Flawfinder: fix (atoi -> strtolong) */
	val = strtolong(str, 10);

	if (val >= argc)
	    continue;

	cp++;
	np = cp;
	if (reg2 != NULL) {
	    for (cp = GetField(cp, str, MAXSTRLEN); cp != NULL;
		 cp = GetField(cp, str, MAXSTRLEN)) {
		regsub(reg2, str, buf);
		if (strcmp(buf, argv[val]) == 0)
		    break;
	    }
	    if (cp != NULL)
		continue;

	    free(reg2);
	    reg2 = NULL;
	}

	if ((reg2 == NULL) || (cp == NULL)) {
	    cp = np;
	    for (cp = GetField(cp, str, MAXSTRLEN - 5); cp != NULL;
		 cp = GetField(cp, str, MAXSTRLEN - 5)) {
		/* Flawfinder: ignore (char) */
		char regstr[MAXSTRLEN];

		/* Flawfinder: fix (strcpy, strcat) */
		strlcpy(regstr, "^(", sizeof(regstr));
		strlcat(regstr, str, sizeof(regstr));
		strlcat(regstr, ")$", sizeof(regstr));

		if ((reg2 = regcomp(regstr)) == NULL)
		    return logger(LOG_ERR, "Invalid regex '%s'", regstr);
		if (regexec(reg2, argv[val]) == 1)
		    break;

		free(reg2);
		reg2 = NULL;
	    }
	}
	if (cp == NULL)
	    return logger(LOG_ERR,
			  "%s: argument '%s' did not pass constraint '%s'",
			  cmd->name, argv[val], np);
    }
    return 0;
}

char *
str_replace(const char *source, size_t offset, size_t length, const char *paste)
{
    /* Flawfinder: ignore (strlen) */
    size_t len = strlen(source) - length + strlen(paste) + 1;
    char *buffer = malloc(len);

    if (!buffer)
	fatal(1, "Can't allocate buffer");

    /* Flawfinder: fix (strcpy, strcat) */
    strlcpy(buffer, source, len);
    if (offset <= len)
	buffer[offset] = 0;	/* expected-warning */
    strlcat(buffer, paste, len);
    strlcat(buffer, source + offset + length, len);
    return buffer;
}

int
Go(cmd_t * cmd, size_t num, int argc, char **argv)
{
    extern char **environ;
    /* cppcheck-suppress variableScope */
    size_t i, j, len;
    int flag;
    /* NOLINTNEXTLINE(runtime/int) */
    long val;
    /* cppcheck-suppress variableScope */
    char *cp, *np;
    struct passwd *pw;
    struct group *gr;
    /* cppcheck-suppress variableScope */
    int ngroups = 0;
    gid_t gidset[NGROUPS_MAX];
    size_t curenv = 0, curarg = 0;
    /* Flawfinder: ignore (char) */
    char *new_envp[MAXENV];
    /* Flawfinder: ignore (char) */
    char *new_argv[MAXARG];
    /* Flawfinder: ignore (char) */
    char str[MAXSTRLEN];
    struct stat st;

    UNUSED(num);
#ifdef XAUTH
    /* Flawfinder: ignore (getenv) */
    if (getenv("DISPLAY") != NULL && (cp = FindOpt(cmd, "xauth")) != NULL) {
	struct passwd *currentpw;
	/* cppcheck-suppress variableScope */
	/* Flawfinder: ignore (char) */
	char tmpxauth[MAXSTRLEN], xauth[MAXSTRLEN], cxauth[MAXSTRLEN], *display;
	int status;
	uid_t uid;
	gid_t gid;

	/* We need to find the destination user's info */
	if (cp == NULL && (cp = FindOpt(cmd, "uid")) == NULL) {
	    if ((pw = getpwuid(0)) == NULL)
		fatal(1, "Can't get password entry for UID 0");
	} else {
	    if ((pw = getpwnam(cp)) == NULL)
		/* Flawfinder: fix (atoi => strtolong) */
		if ((pw = getpwuid((uid_t) strtolong(cp, 10))) == NULL)
		    fatal(1, "Can't get password entry for %s", cp);
	}
	/* Flawfinder: ignore (getenv) */
	if ((display = strchr(getenv("DISPLAY"), ':')) == NULL)
	    fatal(1, "Could not extract X server from $DISPLAY '%s'",
		  /* Flawfinder: ignore (getenv) */
		  getenv("DISPLAY"));
	/* Flawfinder: fix (strcpy, strcat) */
	strlcpy(xauth, pw->pw_dir, sizeof(xauth));
	strlcat(xauth, "/.Xauthority", sizeof(xauth));
	uid = pw->pw_uid;
	gid = pw->pw_gid;
	currentpw = getpwuid(getuid());
	/* Now that we know the target user, we can copy the xauth cookies */
	/* Flawfinder: ignore (getenv) */
	if (getenv("XAUTHORITY") != NULL) {
	    /* Flawfinder: fix (strcpy) */
	    strlcpy(cxauth, getenv("XAUTHORITY"), sizeof(cxauth));
	} else {
	    /* Flawfinder: fix (strcpy, strcat) */
	    strlcpy(cxauth, currentpw->pw_dir, sizeof(cxauth));
	    strlcat(cxauth, "/.Xauthority", sizeof(cxauth));
	}
	/* Do not continue if the source .Xauthority does not exist */
	if (stat(cxauth, &st) == 0) {
	    /* Flawfinder: fix (strcpy) */
	    strlcpy(tmpxauth, "/var/tmp/op-xauth-XXXXXX", sizeof(tmpxauth));
	    /* Flawfinder: ignore (mkstemp) */
	    if (mkstemp(tmpxauth) == -1)
		fatal(1, "mkstemp(%s) failed with %i", tmpxauth, errno);
	    /* Flawfinder: ignore (race condition) */
	    if (chown(tmpxauth, currentpw->pw_uid, currentpw->pw_gid) < 0) {
		unlink(tmpxauth);
		fatal(1, "Failed to change ownership of %s", tmpxauth);
	    }
	    /* Fork out to extract current X server to an XAUTH file */
	    if (fork() == 0) {
		char *argv[] =
		    { XAUTH, "-f", cxauth, "extract", tmpxauth, display, NULL };

		/*      We need to be root to be sure that access to both Xauthority files
		   will work */
		/* Flawfinder: ignore (umask) */
		umask(077);
		setuid(currentpw->pw_uid);
		setgid(currentpw->pw_gid);
		/* Flawfinder: ignore (execv) */
		if (execv(XAUTH, argv) == -1) {
		    logger(LOG_ERR, "Unable to exec xauth, return code %i",
			   errno);
		    exit(errno);
		}
		exit(0);
	    }
	    if (wait(&status) == -1) {
		unlink(tmpxauth);
		fatal(1, "fork/wait failed");
	    }
	    if (status > 0) {
		unlink(tmpxauth);
		fatal(1,
		      "Unable to export X authorisation entry, return code %i",
		      status);
	    }
	    /* Fork out to insert extracted X server into new users XAUTH file */
	    if (fork() == 0) {
		char *argv[] = { XAUTH, "-f", xauth, "merge", tmpxauth, NULL };

		/*      We need to be root to be sure that access to both Xauthority files
		   will work */
		/* Flawfinder: ignore (race condition) */
		if (chown(tmpxauth, uid, gid) < 0) {
		    unlink(tmpxauth);
		    fatal(1, "Failed to change ownership of %s", tmpxauth);
		}
		/* Flawfinder: ignore (umask) */
		umask(077);
		setuid(uid);
		setgid(gid);
		/* Flawfinder: ignore (execv) */
		if (execv(XAUTH, argv) == -1) {
		    logger(LOG_ERR,
			   "Unable to import X authorisation entry, return code %i",
			   errno);
		    exit(errno);
		}
		exit(0);
	    }
	    if (wait(&status) == -1) {
		unlink(tmpxauth);
		fatal(1, "fork/wait failed");
	    }
	    unlink(tmpxauth);
	    if (status > 0)
		fatal(1, "Unable to exec xauth, return code %i", status);
	    /* Update $XAUTHORITY */
	    /* Flawfinder: ignore (strlen) */
	    len = strlen("XAUTHORITY=") + strlen(xauth) + 1;
	    new_envp[curenv] = malloc(len);
	    /* Flawfinder: fix (strcpy, strcat) */
	    strlcpy(new_envp[curenv], "XAUTHORITY=", len);
	    strlcat(new_envp[curenv], xauth, len);
	    if (curenv + 1 >= MAXENV)
		fatal(1, "%s: environment length exceeded", cmd->name);
	    ++curenv;
	    /* Propagate $DISPLAY to new environment */
	    /* Flawfinder: ignore (getenv) */
	    len = strlen("DISPLAY=") + strlen(getenv("DISPLAY")) + 1;
	    new_envp[curenv] = malloc(len);
	    /* Flawfinder: fix (strcpy, strcat) */
	    strlcpy(new_envp[curenv], "DISPLAY=", len);
	    /* Flawfinder: ignore (getenv) */
	    strlcat(new_envp[curenv], getenv("DISPLAY"), len);
	    if (curenv + 1 >= MAXENV)
		fatal(1, "%s: environment length exceeded", cmd->name);
	    ++curenv;
	}
    }
#else
    if (FindOpt(cmd, "xauth") != NULL)
	fatal(1, "X authority support is not compiled into this version of op");
#endif

    if ((cp = FindOpt(cmd, "gid")) == NULL) {
	if (setgid(0) < 0)
	    fatal(1, "Unable to set gid to default");
    } else {
	for (i = 0, cp = GetField(cp, str, MAXSTRLEN);
	     i < NGROUPS_MAX && cp != NULL;
	     cp = GetField(cp, str, MAXSTRLEN), ++i) {
	    if ((gr = getgrnam(str)) != NULL)
		gidset[ngroups++] = gr->gr_gid;
	    else
		/* Flawfinder: fix (atoi -> strtolong) */
		gidset[ngroups++] = (gid_t) strtolong(str, 10);
	}
	if (i == NGROUPS_MAX)
	    fatal(1, "Exceeded maximum number of groups");
	if (ngroups == 0)
	    fatal(1, "Unable to set gid to any group");
	if (setgroups(ngroups, gidset) < 0)
	    fatal(1, "Unable to set auxiliary groups");
	if (setgid(gidset[0]) < 0)
	    fatal(1, "Unable to set gid to %d", gidset[0]);
    }

    if ((cp = FindOpt(cmd, "uid")) == NULL) {
	if (setuid(0) < 0)
	    fatal(1, "Unable to set uid to default");
    } else {
	if ((pw = getpwnam(cp)) == NULL) {
	    /* Flawfinder: fix (atoi -> strtolong) */
	    if (setuid((uid_t) strtolong(cp, 10)) < 0)
		fatal(1, "Unable to set uid to %s", cp);
	} else {
	    if (setuid(pw->pw_uid) < 0)
		fatal(1, "Unable to set uid to %s", cp);
	}
    }

    if ((cp = FindOpt(cmd, "umask")) == NULL) {
	mode_t m = 0022;
	/* Flawfinder: ignore (umask) */
	if (!umask(m) || umask(m) != m) {
	    fatal(1, "Unable to set umask to default");
	}
    } else {
	mode_t m = (mode_t) strtolong(cp, 8);
	/* Flawfinder: ignore (umask) */
	if (!umask(m) || umask(m) != m) {
	    fatal(1, "Unable to set umask to %s", cp);
	}
    }

    if ((cp = FindOpt(cmd, "chroot")) == NULL) {
	/* don't have a default */
    } else {
	/* Flawfinder: ignore (chroot) */
	if (chroot(cp) < 0) {
	    fatal(1, "Unable to chroot to %s", cp);
	}
    }

    if ((cp = FindOpt(cmd, "dir")) == NULL) {
	/* don't have a default */
    } else {
	if (chdir(cp) < 0) {
	    fatal(1, "Unable to chdir to %s", cp);
	}
    }

    if (FindOpt(cmd, "nolog") != NULL) {
	minimum_logging_level = LOG_NOTICE;
    }

    if (FindOpt(cmd, "environment") == NULL) {
	for (i = 0; i < cmd->nopts; i++) {
	    if (cmd->opts[i][0] != '$')
		continue;
	    /* Skip positional constraints */
	    cp = cmd->opts[i] + 1;
	    flag = 0;
	    while ((*cp != '\0') && (*cp != '=')) {
		if (!isdigit((int)*cp))
		    flag = 1;
		cp++;
	    }
	    if (!flag)
		continue;
	    /* Propagate variable into environment if it exists */
	    for (j = 0; environ[j] != NULL; j++) {
		if ((cp = strchr(environ[j], '=')) == NULL)
		    continue;
		if (strncmp(cmd->opts[i] + 1, environ[j], cp - environ[j]) == 0) {
		    if (curenv + 1 >= MAXENV)
			fatal(1, "%s: environment length exceeded", cmd->name);
		    new_envp[curenv++] = environ[j];
		    break;
		}
	    }
	}
    } else {
	for (i = 0; environ[i] != NULL; i++) {
	    if (curenv + 1 >= MAXENV)
		fatal(1, "%s: environment length exceeded", cmd->name);
	    new_envp[curenv++] = environ[i];
	}
    }
    /* Allow over-ride of environment variables. */
    for (i = 0; i < cmd->nopts; ++i) {
	/* Skip positional constraints */
	cp = cmd->opts[i] + 1;
	flag = 0;
	while ((*cp != '\0') && (*cp != '=')) {
	    if (!isdigit((int)*cp))
		flag = 1;
	    cp++;
	}
	if (!flag)
	    continue;
	if (cmd->opts[i][0] == '$' && strchr(cmd->opts[i], '=') != NULL) {
	    if (curenv + 1 >= MAXENV)
		fatal(1, "%s: environment length exceeded", cmd->name);
	    new_envp[curenv++] = cmd->opts[i] + 1;
	    continue;
	}
    }
    new_envp[curenv] = NULL;

    /* --------------------------------------------------- */
    /* fowners constraint must respect the syntax :        */
    /* fowners=user:group,...                              */
    /* Notice : user and/or group are regular expressions  */
    /* --------------------------------------------------- */

    if ((cp = FindOpt(cmd, "fowners")) != NULL) {
	struct passwd *pwbuf;
	struct group *grbuf;
	struct stat statbuf;
	/* cppcheck-suppress variableScope */
	char *ptr;
	/* Flawfinder: ignore (char) */
	char usergroup[MAXSTRLEN];

	/* Get user and group name of the owner of the file */
	stat(cmd->args[0], &statbuf);

	pwbuf = getpwuid(statbuf.st_uid);
	if (pwbuf == NULL)
	    fatal(1, "%s: no identified user for uid %d", cmd->name,
		  statbuf.st_uid);
	grbuf = getgrgid(statbuf.st_gid);
	if (grbuf == NULL)
	    fatal(1, "%s: no identified group for gid %d", cmd->name,
		  statbuf.st_gid);

	/* Flawfinder: ignore (strlen) */
	if (strlen(pwbuf->pw_name) + strlen(grbuf->gr_name) + 1 >= MAXSTRLEN)
	    fatal(1, "%s: user/group string buffer length exceeded", cmd->name);
	/* Flawfinder: fix (strcpy, strcat) */
	strlcpy(usergroup, pwbuf->pw_name, sizeof(usergroup));
	strlcat(usergroup, ":", sizeof(usergroup));
	strlcat(usergroup, grbuf->gr_name, sizeof(usergroup));

	/* check users,groups candidates */

	for (cp = GetField(cp, str, MAXSTRLEN - 5); cp != NULL;
	     cp = GetField(cp, str, MAXSTRLEN - 5)) {
	    regexp *reg1 = NULL;
	    /* Flawfinder: ignore (char) */
	    char regstr[MAXSTRLEN];

	    ptr = strchr(str, ':');
	    if (ptr == NULL)
		fatal(1,
		      "%s: fowners argument must respect the user:group format",
		      cmd->name);

	    /* Flawfinder: fix (strcpy, strcat) */
	    strlcpy(regstr, "^(", sizeof(regstr));
	    strlcat(regstr, str, sizeof(regstr));
	    strlcat(regstr, ")$", sizeof(regstr));

	    if ((reg1 = regcomp(regstr)) == NULL)
		return logger(LOG_ERR, "Invalid regex '%s'", str);

	    if ((regexec(reg1, usergroup) == 1))
		break;
	}
	if (cp == NULL)
	    fatal(1, "%s: file %s (%s) did not pass ownership constraints",
		  cmd->name, cmd->args[0], usergroup);
    }

    /* ---------------------------------------------------------------------- */
    /* fperms constraint must respect the syntax :                            */
    /* fperms=NNNN,MMMM,... where NNNN and MMMM are octal representation of   */
    /*                            the target requested authorised permissions */
    /* Notice : NNNN and MMMM can be regular expressions                      */
    /* ---------------------------------------------------------------------- */

    if ((cp = FindOpt(cmd, "fperms")) != NULL) {
	struct stat buf;
	/* Flawfinder: ignore (char) */
	char mode[5];

	stat(cmd->args[0], &buf);
	/* NOLINTNEXTLINE (runtime/printf runtime/int) */
	snprintf(mode, 5, "%lo", (unsigned long)(buf.st_mode & 07777));

	for (cp = GetField(cp, str, MAXSTRLEN - 5); cp != NULL;
	     cp = GetField(cp, str, MAXSTRLEN - 5)) {
	    regexp *reg1 = NULL;
	    /* Flawfinder: ignore (char) */
	    char regstr[MAXSTRLEN];

	    /* Flawfinder: fix (strcpy, strcat) */
	    strlcpy(regstr, "^(", sizeof(regstr));
	    strlcat(regstr, str, sizeof(regstr));
	    strlcat(regstr, ")$", sizeof(regstr));

	    if ((reg1 = regcomp(regstr)) == NULL)
		return logger(LOG_ERR, "Invalid regex '%s'", str);

	    if (regexec(reg1, mode) == 1)
		break;
	}
	if (cp == NULL)
	    fatal(1, "%s: file %s (%s) did not pass permissions constraints",
		  cmd->name, cmd->args[0], mode);
    }

    if (strcmp("MAGIC_SHELL", cmd->args[0]) == 0) {
	for (i = 0; environ[i] != NULL; i++)
	    if (strncmp("SHELL=", environ[i], 6) == 0)
		break;

	if (environ[i] != NULL) {
	    if (curarg >= MAXARG - 1)
		fatal(1, "%s: argument length exceeded", cmd->name);
	    new_argv[curarg++] = environ[i] + 6;
	} else {
	    fatal(1, "%s: no shell", cmd->name);
	}

	if (argc != 1) {
	    if (curarg >= MAXARG - 1)
		fatal(1, "%s: argument length exceeded", cmd->name);
	    new_argv[curarg++] = "-c";

	    len = 0;
	    for (i = 1; i < argc; i++)
		/* Flawfinder: ignore (strlen) */
		len += strlen(argv[i]) + 1;

	    len += 10;
	    if ((cp = (char *)malloc(len)) == NULL)
		fatal(1, "%s: unable to create buffer", cmd->name);

	    *cp = '\0';

	    for (i = 1; i < argc; i++) {
		/* Flawfinder: fix (strcat) */
		strlcat(cp, argv[i], len);
		strlcat(cp, " ", len);
	    }
	    if (curarg >= MAXARG - 1)
		fatal(1, "%s: argument length exceeded", cmd->name);
	    new_argv[curarg++] = cp;
	}
    } else {
	size_t consumed_args = 1;

	for (i = 0; i < cmd->nargs; i++) {
	    np = cmd->args[i];

	    /* Complete argument is a variable expansion. */
	    /* Flawfinder: ignore (strlen) */
	    if (strlen(np) == 2 && np[0] == '$') {
		if (np[1] == '*') {
		    if (curarg + argc >= MAXARG - 1)
			fatal(1, "%s: argument length exceeded", cmd->name);
		    for (j = consumed_args; j < argc; j++)
			new_argv[curarg++] = argv[j];
		} else if (isdigit((int)np[1])) {
		    /* Flawfinder: fix (atoi -> strtolong) */
		    size_t argi = strtolong(np + 1, 10);

		    if (argi > argc)
			fatal(1, "%s Referenced argument out of range",
			      cmd->name);
		    if (curarg >= MAXARG - 1)
			fatal(1, "%s: argument length exceeded", cmd->name);
		    new_argv[curarg++] = argv[argi];
		    if (argi >= consumed_args)
			consumed_args = argi + 1;
		}
		continue;
	    } else {
		/* Embedded match */
		while ((cp = strchr(np, '$')) != NULL) {
		    if ((cp != cmd->args[i]) && (*(cp - 1) == '\\'))
			np = cp + 1;
		    else {
			char *tmp;

			np = cp + 1;
			++cp;

			if (*cp == '*') {
			    char *buffer;

			    ++cp;
			    /* Find total length of all arguments */
			    for (len = j = 1; j < argc; j++)
				/* Flawfinder: ignore (strlen) */
				len += strlen(argv[j]) + 1;

			    if ((buffer = malloc(len)) == NULL)
				fatal(1, "Can't allocate buffer");

			    buffer[0] = 0;

			    /* Expand all arguments */
			    for (j = 1; j < argc; j++) {
				/* Flawfinder: fix (strcat) */
				strlcat(buffer, argv[j], len);
				if (j < argc - 1)
				    strlcat(buffer, " ", len);
			    }
			    tmp = str_replace(cmd->args[i],
					      np - cmd->args[i] - 1,
					      cp - np + 1, buffer);
			    cp = tmp + (cp - cmd->args[i]);
			    np = cp;
			    cmd->args[i] = tmp;
			} else {
			    while (isdigit((int)*cp))
				++cp;

			    if (cp != np) {
				/* Flawfinder: fix (atoi -> strtolong) */
				val = strtolong(np, 10);

				tmp = str_replace(cmd->args[i],
						  np - cmd->args[i] - 1,
						  cp - np + 1, argv[val]);
				cp = tmp + (cp - cmd->args[i]) + 1;
				np = cp;
				cmd->args[i] = tmp;
			    }
			}
		    }
		}
	    }

	    if (cp == NULL) {
		if (curarg >= MAXARG - 1)
		    fatal(1, "%s: argument length exceeded", cmd->name);
		new_argv[curarg++] = cmd->args[i];
		continue;
	    }
	}
    }
    new_argv[curarg] = NULL;

    if (stat(new_argv[0], &st) != -1
	&& st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
	logger(LOG_INFO, "SUCCESS");

    if (sigprocmask(SIG_SETMASK, &old_sig_mask, NULL))
	fatal(1, "could not restore signal mask");
    if ((i = execve(new_argv[0], new_argv, new_envp)) < 0) {
	perror("execve");
	logger(LOG_ERR, "execve(3) failed with error code %i", i);
	exit(i);
    }
    return 0;
}

void
output(cmd_t * cmd)
{
    size_t i;

    printf("cmd '%s'\n", cmd->name);
    printf("\n  args\t");
    for (i = 0; i < cmd->nargs; i++)
	printf("'%s' ", cmd->args[i]);
    printf("\n  opts\t");
    for (i = 0; i < cmd->nopts; i++)
	printf("'%s' ", cmd->opts[i]);
    printf("\n");
}

char *
format_cmd(int argc, char **argv, char *retbuf, size_t buflen)
/*   
     Format command and args for printing to syslog
     If length (command + args) is too long, try length(command). If THATS
     too long, return an error message.
*/
{
    size_t i, l = 0, s, m = 0;
    char *buf = 0;

    UNUSED(buflen);
    /* Flawfinder: ignore (strlen) */
    s = strlen(argv[0]);
    if ((s > MAXSTRLEN)) {
	/* Flawfinder: fix (strcpy) */
	strlcpy(retbuf, "unknown cmd (name too long in format_cmd)", MAXSTRLEN);
	return retbuf;
    }
    for (i = 1; i < argc; i++) {
	/* Flawfinder: ignore (strlen) */
	l = strlen(argv[i]);
	m = l > m ? l : m;
	s += l;
    }
    if (l)
	s += argc - 1;		/* count spaces if there are arguments */
    if (s > MAXSTRLEN) {	/* Ooops, we've gone over. */
	m = 0;
	argc = 0;
    }
    *retbuf = '\0';
    if (m)
	/* Flawfinder: fix (m += 2) */
	buf = (char *)malloc(m += 2);
    if (buf) {
	for (i = 1; i < argc; i++) {
	    /* Flawfinder: fix (sprintf) */
	    snprintf(buf, m, " %s", argv[i]);
	    /* Flawfinder: fix (strcat) */
	    strlcat(retbuf, buf, MAXSTRLEN);
	}
	free(buf);
    }
    return (retbuf);
}

int
vlogger(unsigned level, const char *format, va_list args)
{
    /* Flawfinder: ignore (char) */
    char buffer[MAXSTRLEN], buffer2[MAXSTRLEN], buffer3[MAXSTRLEN];
    char *username = "unknown";

    if (level >= minimum_logging_level)
	return -1;

    if (realuser)
	username = realuser;

    /* Flawfinder: ignore (vsnprintf) */
    vsnprintf(buffer2, MAXSTRLEN, format, args);
    if (level & LOG_PRINT)
	printf("%s\n", buffer2);
    level &= ~LOG_PRINT;
    snprintf(buffer, MAXSTRLEN, "%s%s: %s", username,
	     format_cmd(gargc, gargv, buffer3, MAXSTRLEN), buffer2);
    syslog(level, "%s", buffer);
    return -1;
}

int
logger(unsigned level, const char *format, ...)
{
    va_list va;

    va_start(va, format);
    vlogger(level, format, va);
    va_end(va);
    return -1;
}

void
fatal(int logit, const char *format, ...)
{
    /* Flawfinder: ignore (char) */
    char buffer[MAXSTRLEN];
    va_list ap;

    va_start(ap, format);
    /* Flawfinder: ignore (vsnprintf) */
    vsnprintf(buffer, MAXSTRLEN, format, ap);
    fprintf(stderr, "%s\n", buffer);
    if (logit)
	logger(LOG_ERR, "%s", buffer);
    va_end(ap);
    exit(1);
}
