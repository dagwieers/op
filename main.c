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

extern char	*savestr();
extern char	*getpass(), *crypt();

char    *format_cmd(int argc, char **argv, char *retbuf, int buflen);
char    *GetCode();
cmd_t	*Find();
int Verify(cmd_t *cmd, int num, int argc, char **argv);
int VerifyPermissions(cmd_t *cmd);
cmd_t	*Find(char *name);
char	*FindOpt(cmd_t *cmd, char *str);
void	ListCommands();
int Go(cmd_t *cmd, int num, int argc, char **argv);
cmd_t	*First = NULL;
var_t	*Variables = NULL;
char *realuser = NULL;
int gargc = -1;
char **gargv = NULL;
sigset_t sig_mask, old_sig_mask;
unsigned minimum_logging_level = 99;

void Usage()
{
	fatal(0,	"Usage: %s mnemonic [args]\n"
				"       %s -l     List available commands\n"
				"       %s -V     Show op version",
			gargv[0], gargv[0], gargv[0]);
}

int FileCompare(const void *a, const void *b)
{
	return strcmp(*(char**)a, *(char**)b);
}

int SortCommandList(const void *a, const void *b)
{
	return strcmp((*(cmd_t**)a)->name, (*(cmd_t**)b)->name);
}

void ListCommands()
{
cmd_t *def, *cmd;
array_t *cmds = array_alloc();
int length = 0, i;

	def = Find("DEFAULT");
	/*	first pass, get maximum command length and number of commands we have
		permission to use */
	for (cmd = First; cmd != NULL; cmd = cmd->next) {

		if (strcmp(cmd->name, "DEFAULT")) {
		cmd_t *new = BuildSingle(def, cmd);

			if (VerifyPermissions(new) >= 0) {
			int l = strlen(new->name);

				for (i = 0; i < cmds->size; ++i)
					if (!strcmp(((cmd_t*)cmds->data[i])->name, new->name))
						break;
				if (i == cmds->size) {
					if (l > length) length = l;
					array_push(cmds, new);
				}
			}
		}
	}

	qsort(cmds->data, cmds->size, sizeof(void*), SortCommandList);

	/* second pass, display */
	for (i = 0; i < cmds->size; ++i) {
		cmd = cmds->data[i];

		if (strcmp(cmd->name, "DEFAULT")) {
		cmd_t *new = BuildSingle(def, cmd);

			if (VerifyPermissions(new) >= 0) {
			char *help = FindOpt(new, "help");

				if (!help || !*help) {
				int j, len = 0;
					
					for (j = 0; j < cmd->nargs; ++j)
						len += strlen(cmd->args[j]) + 1;
					help = (char*)malloc(len);
					strcpy(help, cmd->args[0]);
					for (j = 1; j < cmd->nargs; ++j) {
						strcat(help, " ");
						if (strchr(cmd->args[j], ' ') || strchr(cmd->args[j], '\t')) {
							strcat(help, "'");
							strcat(help, cmd->args[j]);
							strcat(help, "'");
						} else
							strcat(help, cmd->args[j]);
					}
				}
				printf("%-*s", length + 2, new->name);
				printf("%-*.*s", 77 - length, 77 - length, help);
				if (strlen(help) > 77 - length)
					printf("...\n");
				else
					printf("\n");
			}
		}
	}
	array_free(cmds);
}

int ReadDir( char *dir )
{
DIR *d;

	if ((d = opendir(dir)) != NULL)
	{
	struct dirent *f;
	int i, successes = 0;
	array_t *dir_list = array_alloc();

		while ((f = readdir(d)))
		{
			if (f->d_name[0] == '.' || (strlen(f->d_name) > 5 && strcmp(f->d_name + strlen(f->d_name) - 5, ".conf")))
				continue;
			if (!array_push(dir_list, savestr(f->d_name)))
				fatal(1, "failed to malloc space for directory entry");
		}
		closedir(d);
		qsort(dir_list->data, dir_list->size, sizeof(void*), FileCompare);
		for (i = 0; i < dir_list->size; ++i) {
		char full_path[PATH_MAX];
			strnprintf(full_path, PATH_MAX, "%s/%s", OP_ACCESS_DIR, (char*)dir_list->data[i]);
			if (ReadFile(full_path)) successes++;
		}
		return successes;
	}
	return 0;
}

int main(argc, argv)
int	argc;
char	*argv[];
{
	int		num, argStart = 1;
	char		user[MAXSTRLEN];
	cmd_t		*cmd, *def, *new;
	struct passwd	*pw;
	int		lflag = 0, hflag = 0, read_conf = 0, read_conf_dir = 0;
	char		*uptr = NULL;
	char		cmd_s[MAXSTRLEN];
	char            *pcmd_s;

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
			if (strlen(argv[argStart]) == 2) {
				if (argStart+1 >= argc)
					Usage();
				argStart++;
				uptr = argv[argStart];
			}
			argStart++;
		} else if (strcmp("-uH", argv[argStart]) == 0) {
			hflag++;
			if (strlen(argv[argStart]) == 3) {
				if (argStart+1 >= argc)
					Usage();
				argStart++;
				uptr = argv[argStart];
			}
			argStart++;
		} else if (strcmp("-Hu", argv[argStart]) == 0) {
			hflag++;
			if (strlen(argv[argStart]) == 3) {
				if (argStart+1 >= argc)
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
	read_conf = ReadFile( OP_ACCESS );
	read_conf_dir = ReadDir( OP_ACCESS_DIR );

	if (!read_conf && !read_conf_dir)
		fatal(1, "could not open %s or any configuration files in %s (check that file permissions are 600)", OP_ACCESS, OP_ACCESS_DIR);

	if ((pw = getpwuid(getuid())) == NULL) 
		exit(1);
	realuser = (char*)strdup(pw->pw_name);
	strncpy(user, pw->pw_name, MAXSTRLEN);

	if (lflag) {
		ListCommands();
		return 0;
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
	uptr=strrchr(argv[argStart],'/');
	if (uptr == NULL)
		uptr=argv[argStart];
	else {
		uptr++;
		if (access(argv[argStart], F_OK) != 0)
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

	if ((num < 0) && ((argc-1) < -num))
		fatal(1, "%s: improper number of arguments", cmd->name);
	if ((num > 0) && ((argc-1) != num)) 
		fatal(1, "%s: improper number of arguments", cmd->name);
	if (num <0)
		num = -num;

	pcmd_s = format_cmd(argc, argv, cmd_s, MAXSTRLEN);
	if (Verify(new, num, argc, argv) < 0)
		fatal(0, "%s: permission denied by op", cmd->name);

	return Go(new, num, argc, argv);
}

cmd_t	*Find(name)
char	*name;
{
	cmd_t	*cmd;

	for (cmd = First; cmd != NULL; cmd = cmd ->next) {
		if (strcmp(cmd->name, name) == 0)
			break;
	}

	return cmd;
}

char	*FindOpt(cmd, str)
cmd_t	*cmd;
char	*str;
{
	static char	nul[2] = "";
	int		i;
	char		*cp;

	for (i = 0; i < cmd->nopts; i++) {
		if ((cp = strchr(cmd->opts[i], '=')) == NULL) {
			if (strcmp(cmd->opts[i], str) == 0)
				return nul;
		} else {
			int	l = cp - cmd->opts[i];
			if (strncmp(cmd->opts[i], str, l) == 0)
				return cp+1;
		}
	}

	return NULL;
}

char	*GetField(cp, str, len)
char	*cp, *str;
int len;
{
char *end = str + len - 2;

	if (*cp == '\0')
		return NULL;

	while ((*cp != '\0') && (*cp != ',')) {
		if (*cp == '\\')
			if (*(cp+1) == ',') {
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

	return (*cp == '\0') ? cp : (cp+1);
}

#ifdef USE_PAM
int pam_conversation(int num_msg, const struct pam_message **msg, struct pam_response **response, void *appdata_ptr) {
int i;
const struct pam_message *pm;
struct pam_response *pr;
char *pass;

	if ((*response = malloc(sizeof(struct pam_response) * num_msg)) == NULL)
		return PAM_CONV_ERR;
	memset(*response, 0, num_msg * sizeof(struct pam_response));

	for (i = 0, pm = *msg, pr = *response; i < num_msg; ++i, ++pm, ++pr) {
		switch (pm->msg_style) {
			case PAM_PROMPT_ECHO_ON :
				if (!(pass = malloc(512))) return PAM_CONV_ERR;
				puts(pm->msg);
				fgets(pass, 512, stdin);
				pr->resp = pass;
			break;
			case PAM_PROMPT_ECHO_OFF :
				if ((pass = getpass(pm->msg)) == NULL) {
					for (pr = *response, i = 0; i < num_msg; ++i, ++pr)
						if (pr->resp) {
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
			case PAM_TEXT_INFO :
				if (pm->msg)
					puts(pm->msg);
			break;
			case PAM_ERROR_MSG :
				if (pm->msg) {
					fputs(pm->msg, stderr);
					fputc('\n', stderr);
				}
			break;
			default :
				for (pr = *response, i = 0; i < num_msg; ++i, ++pr)
					if (pr->resp) {
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

int VerifyPermissions(cmd_t *cmd)
{
int		gr_fail = 1, uid_fail = 1, netgr_fail = 1;
int		i;
char		*cp, str[MAXSTRLEN], hostname[HOST_NAME_MAX];
regexp		*reg1 = NULL;
struct passwd	*pw;
struct group	*gr;

	/* root always has access - it is pointless refusing */
	if (getuid() == 0) 
		return 0;

	if (gethostname(hostname, HOST_NAME_MAX) == -1)
		return logger(LOG_ERR, "Could not get hostname");

	if ((pw = getpwuid(getuid())) == NULL) 
		return logger(LOG_ERR, "Could not get uid of current effective uid");

	if ((cp = FindOpt(cmd, "groups")) != NULL) {
	char grouphost[MAXSTRLEN + HOST_NAME_MAX],
		regstr[MAXSTRLEN];

		for (cp = GetField(cp, str, MAXSTRLEN - 5); cp != NULL; cp = GetField(cp, str, MAXSTRLEN - 5)) {
			strcpy(regstr, "^(");
			strcat(regstr, str);
			strcat(regstr, ")$");

			if ((reg1 = regcomp(regstr)) == NULL)
				return logger(LOG_ERR, "Invalid regex '%s'", regstr);

			if ((gr = getgrgid(pw->pw_gid)) != NULL) {
				strcpy(grouphost, gr->gr_name);
				strcat(grouphost, "@");
				strcat(grouphost, hostname);

				if (regexec(reg1,gr->gr_name) == 1 || regexec(reg1, grouphost)) {
					gr_fail = 0;
					break;
				}
			}

			setgrent();
			while ((gr = getgrent()) != NULL) {
				i = 0;
				while (gr->gr_mem[i] != NULL) {
					if (strcmp(gr->gr_mem[i], pw->pw_name)==0) break;
					i++;
				}

				if (gr->gr_mem[i] != NULL) {
					strcpy(grouphost, gr->gr_name);
					strcat(grouphost, "@");
					strcat(grouphost, hostname);
					if (regexec(reg1, gr->gr_name) == 1 || regexec(reg1, grouphost)) {
						gr_fail = 0;
						break;
					}
				}
			}
		}
	}
	if(reg1 != NULL){
		free(reg1);
		reg1=NULL;
	}

	if (gr_fail && ((cp = FindOpt(cmd, "users")) != NULL)) {
	char currenttime[13], userhost[MAXSTRLEN + HOST_NAME_MAX],
		regstr[MAXSTRLEN];
	time_t now = time(NULL);
		
		strftime(currenttime, 13, "%Y%m%d%H%M", localtime(&now));

		for (cp=GetField(cp, str, MAXSTRLEN - 5); cp!=NULL; cp=GetField(cp, str, MAXSTRLEN - 5)) {
		char expiretime[13], *expirestart = strchr(str, '/');

			if (expirestart) *expirestart = 0;

			strcpy(regstr, "^(");
			strcat(regstr, str);
			strcat(regstr, ")$");

			strcpy(userhost, pw->pw_name);
			strcat(userhost, "@");
			strcat(userhost, hostname);

			if ((reg1=regcomp(regstr)) == NULL)
				return logger(LOG_ERR, "Invalid regex '%s'", regstr);

			if (regexec(reg1,pw->pw_name) == 1 || regexec(reg1, userhost) == 1) {
				/* valid user, check expiry (if any) */
				if (expirestart) {
				int i;

					++expirestart;

					/* ensure at least some sanity in the expiry time */
					for (i = 0; expirestart[i]; ++i) {
						if (i > 11)
							return logger(LOG_ERR, "Expiry value (%s) has too many digits", expirestart);
						if (!isdigit(expirestart[i]))
							return logger(LOG_ERR, "Expiry value (%s) has non-numeric characters", expirestart);
					}

					strcpy(expiretime, "000000000000"); /* YYYYMMDD[HHmm] */
					strncpy(expiretime, expirestart, strlen(expirestart));

					if (strcmp(currenttime, expiretime) >= 0)
						return logger(LOG_ERR, "Access expired at %s", expiretime);
				}

				uid_fail = 0;
				break;
			}
		}
	}
	if(reg1 != NULL){
		free(reg1);
		reg1=NULL;
	}

	if (uid_fail && (cp = FindOpt(cmd, "netgroups")) != NULL) {
		for (cp = GetField(cp, str, MAXSTRLEN - 5); cp != NULL && netgr_fail; cp = GetField(cp, str, MAXSTRLEN - 5)) {
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

int Verify(cmd, num, argc, argv)
cmd_t	*cmd;
int	argc;
int	num;
char	**argv;
{
int		i, j, val;
char		*np, *cp, str[MAXSTRLEN], buf[MAXSTRLEN];
regexp		*reg1 = NULL;
regexp		*reg2 = NULL;
struct passwd	*pw;
#ifdef USE_SHADOW
struct spwd *spw;
#endif
#ifdef USE_PAM
struct pam_conv pamconv = { pam_conversation, NULL };
pam_handle_t *pam;
#endif
#ifdef SECURID
struct          SD_CLIENT sd_dat, *sd;
int             k;
char            input[64],*p;
#endif

	if ((pw = getpwuid(getuid())) == NULL) return -1;

#ifdef SECURID
	if ((cp=FindOpt(cmd, "securid")) != NULL) {
		memset(&sd_dat, 0, sizeof(sd_dat));   /* clear sd_auth struct */
		sd = &sd_dat;
		creadcfg();		/*  accesses sdconf.rec  */
		if (sd_init(sd)){
			return logger(LOG_WARNING | LOG_PRINT, "Cannot contact ACE server");
		}
		if (sd_auth(sd)) return -1;
	}
#else
	if ((cp=FindOpt(cmd, "securid")) != NULL) {
		return logger(LOG_ERR | LOG_PRINT, "SecureID not supported by op. Access denied");
	}
#endif

	if (getuid() != 0 && (cp=FindOpt(cmd, "password")) != NULL) {
#ifdef USE_PAM
		if ((cp = GetField(cp, str, MAXSTRLEN)) != NULL) {
			if ((np = getpass("Password:")) == NULL)
				return logger(LOG_ERR, "Could not get user password");

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
				return logger(LOG_ERR, "pam_authticate: %s", pam_strerror(pam, resp));
			}
			pam_end(pam, resp);
		}
#else
		if ((np = getpass("Password:")) == NULL)
			return logger(LOG_ERR, "Could not get user password");

		if ((cp = GetField(cp, str, MAXSTRLEN)) != NULL) {
			if (strcmp(crypt(np, str), str) != 0)
				return logger(LOG_ERR, "Incorrect direct password");
		} else {
#ifdef USE_SHADOW
			if (strcmp(pw->pw_passwd,"x")==0){ /* Shadow passwords */
				if ((spw = getspnam(pw->pw_name)) == NULL)
					return logger(LOG_ERR, "No shadow entry for '%s'", pw->pw_name);
				pw->pw_passwd=spw->sp_pwdp;
			}
#endif

			if (!cp && strcmp(crypt(np, pw->pw_passwd), pw->pw_passwd) != 0)
				return logger(LOG_ERR, "Invalid user password");
		}
#endif
	}

	if ((i = VerifyPermissions(cmd) < 0))
		return logger(LOG_ERR, "Both user, group and netgroup authentication failed");

	for (i = 0; i < cmd->nopts; i++) {
		if ((cmd->opts[i][0] != '$') || ((cp = strchr(cmd->opts[i], '=')) == NULL))
			continue;
		if (cmd->opts[i][1] != '*') {
			for (np = cmd->opts[i] + 1; np != cp; np++) 
			if (!isdigit(*np)) break;
			if (np != cp) continue;
		} else {
			if (cmd->opts[i][2] != '=') continue;
			np = cmd->opts[i] + 3;
			for (j = num+1; j < argc; j++) {
				cp = np;
				for (cp=GetField(cp, str, MAXSTRLEN - 5); cp!=NULL; cp=GetField(cp, str, MAXSTRLEN - 5)) {
				char regstr[MAXSTRLEN];
					
					strcpy(regstr, "^(");
					strcat(regstr, str);
					strcat(regstr, ")$");

					if ((reg1=regcomp(regstr)) == NULL) return logger(LOG_ERR, "Invalid regex '%s'", regstr);
					if (regexec(reg1,argv[j]) == 1) break;
				}
				if (cp == NULL) return logger(LOG_ERR, "%s: argument %i (%s) did not pass wildcard constraint", cmd->name, j, argv[j]);
			}
		}
		if(reg1 != NULL){
			free(reg1);
			reg1=NULL;
		}

		strncpy(str, cmd->opts[i] + 1, cp - cmd->opts[i] - 1);
		str[cp - cmd->opts[i] - 1] = '\0';

		if (!isdigit(*str)) continue;

		val = atoi(str);

		if (val >= argc) continue;

		cp++;
		np = cp;
		if (reg2 != NULL) {
			for (cp=GetField(cp, str, MAXSTRLEN); cp!=NULL; cp=GetField(cp, str, MAXSTRLEN)) {
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
			for (cp=GetField(cp, str, MAXSTRLEN - 5); cp!=NULL; cp=GetField(cp, str, MAXSTRLEN - 5)) {
			char regstr[MAXSTRLEN];

				strcpy(regstr, "^(");
				strcat(regstr, str);
				strcat(regstr, ")$");

				if ((reg2=regcomp(regstr)) == NULL) return logger(LOG_ERR, "Invalid regex '%s'", regstr);
				if (regexec(reg2,argv[val]) == 1) break;

				free(reg2);
				reg2 = NULL;
			}
		}
		if (cp == NULL) return logger(LOG_ERR, "%s: argument '%s' did not pass constraint '%s'", cmd->name, argv[val], np);
	}
	return 0;
}

/*
*/
char *str_replace(const char *source, int offset, int length, const char *paste) {
char *buffer = malloc(strlen(source) - length + strlen(paste) + 1);

	if (!buffer) fatal(1, "Can't allocate buffer");

	strncpy(buffer, source, offset);
	buffer[offset] = 0;
	strcat(buffer, paste);
	strcat(buffer, source + offset + length);
	return buffer;
}

int Go(cmd, num, argc, argv)
cmd_t	*cmd;
int	argc;
int	num;
char	**argv;
{
	extern char	**environ;
	int		i, j, flag, val, len = 0;
	char		*cp, *np;
	struct passwd	*pw;
	struct group	*gr;
	int		ngroups = 0;
	gid_t gidset[NGROUPS_MAX];
	int		curenv = 0, curarg = 0;
	char		*new_envp[MAXENV];
	char		*new_argv[MAXARG];
	char		str[MAXSTRLEN];
	struct stat st;

#ifdef XAUTH
	if (getenv("DISPLAY") != NULL && (cp = FindOpt(cmd, "xauth")) != NULL) {
	struct passwd *currentpw;
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
				if ((pw = getpwuid(atoi(cp))) == NULL)
					fatal(1, "Can't get password entry for %s", cp);
		}
		if ((display = strchr(getenv("DISPLAY"), ':')) == NULL)
			fatal(1, "Could not extract X server from $DISPLAY '%s'", getenv("DISPLAY"));
		strcpy(xauth, pw->pw_dir);
		strcat(xauth, "/.Xauthority");
		uid = pw->pw_uid;
		gid = pw->pw_gid;
		currentpw = getpwuid(getuid());
		/* Now that we know the target user, we can copy the xauth cookies */
		if (getenv("XAUTHORITY") != NULL) {
			strcpy(cxauth, getenv("XAUTHORITY"));
		} else {
			strcpy(cxauth, currentpw->pw_dir);
			strcat(cxauth, "/.Xauthority");
		}
		/* Do not continue if the source .Xauthority does not exist */
		if (stat(cxauth, &st) == 0) {
			strcpy(tmpxauth, "/var/tmp/op-xauth-XXXXXX");
			if (mkstemp(tmpxauth) == -1)
				fatal(1, "mkstemp(%s) failed with %i", tmpxauth, errno);
			if (chown(tmpxauth, currentpw->pw_uid, currentpw->pw_gid) < 0) {
				unlink(tmpxauth);
				fatal(1, "Failed to change ownership of %s", tmpxauth);
			}
			/* Fork out to extract current X server to an XAUTH file */
			if (fork() == 0) {
			char *argv[] = { XAUTH, "-f", cxauth, "extract", tmpxauth, display, NULL };

				/*	We need to be root to be sure that access to both Xauthority files
					will work */
				umask(077); setuid(currentpw->pw_uid); setgid(currentpw->pw_gid);
				if (execv(XAUTH, argv) == -1) {
					logger(LOG_ERR, "Unable to exec xauth, return code %i", errno);
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
				fatal(1, "Unable to export X authorisation entry, return code %i", status);
			}
			/* Fork out to insert extracted X server into new users XAUTH file */
			if (fork() == 0) {
			char *argv[] = { XAUTH, "-f", xauth, "merge", tmpxauth, NULL };

				/*	We need to be root to be sure that access to both Xauthority files
					will work */
				if (chown(tmpxauth, uid, gid) < 0) {
					unlink(tmpxauth);
					fatal(1, "Failed to change ownership of %s", tmpxauth);
				}
				umask(077); setuid(uid); setgid(gid);
				if (execv(XAUTH, argv) == -1) {
					logger(LOG_ERR, "Unable to import X authorisation entry, return code %i", errno);
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
			new_envp[curenv] = malloc(strlen("XAUTHORITY=") + strlen(xauth) + 1);
			strcpy(new_envp[curenv], "XAUTHORITY=");
			strcat(new_envp[curenv], xauth);
			if (curenv + 1 >= MAXENV)
				fatal(1, "%s: environment length exceeded",cmd->name);
			++curenv;
			/* Propagate $DISPLAY to new environment */
			new_envp[curenv] = malloc(strlen("DISPLAY=") + strlen(getenv("DISPLAY")) + 1);
			strcpy(new_envp[curenv], "DISPLAY=");
			strcat(new_envp[curenv], getenv("DISPLAY"));
			if (curenv + 1 >= MAXENV)
				fatal(1, "%s: environment length exceeded",cmd->name);
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
		for (i = 0, cp = GetField(cp, str, MAXSTRLEN); i < NGROUPS_MAX && cp != NULL; cp = GetField(cp, str, MAXSTRLEN), ++i) {
			if ((gr = getgrnam(str)) != NULL)
				gidset[ngroups++] = gr->gr_gid;
			else
				gidset[ngroups++] = atoi(str);
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
			if (setuid(atoi(cp)) < 0)
				fatal(1, "Unable to set uid to %s", cp);
		} else {
			if (setuid(pw->pw_uid) < 0)
				fatal(1, "Unable to set uid to %s", cp);
		}
	}

	if ((cp = FindOpt(cmd, "umask")) == NULL) {
		if (umask(0022) < 0) {
			fatal(1, "Unable to set umask to default");
		}
	} else {
		if (umask(atov(cp, 8)) < 0) {
			fatal(1, "Unable to set umask to %s", cp);
		}
	}

	if ((cp = FindOpt(cmd, "chroot")) == NULL) {
		;		/* don't have a default */
	} else {
		if (chroot(cp) < 0) {
			fatal(1, "Unable to chroot to %s", cp);
		}
	}

	if ((cp = FindOpt(cmd, "dir")) == NULL) {
		;		/* don't have a default */
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
				if (! isdigit(*cp))
					flag = 1;
				cp++;
			}
			if (!flag)
				continue;
			/* Propagate variable into environment if it exists */
			for (j = 0; environ[j] != NULL ; j++) {
				if ((cp = strchr(environ[j], '=')) == NULL)
					continue;
				if (strncmp(cmd->opts[i] + 1, environ[j], cp - environ[j]) == 0) {
					if (curenv + 1 >= MAXENV)
						fatal(1, "%s: environment length exceeded",cmd->name);
					new_envp[curenv++] = environ[j];
					break;
				}
			}
		}
	} else {
		for (i = 0; environ[i] != NULL; i++) {
			if (curenv + 1 >= MAXENV)
				fatal(1, "%s: environment length exceeded",cmd->name);
			new_envp[curenv++] = environ[i];
		}
	}
	/* Allow over-ride of environment variables. */
	for (i = 0; i < cmd->nopts; ++i) {
		/* Skip positional constraints */
		cp = cmd->opts[i] + 1;
		flag = 0;
		while ((*cp != '\0') && (*cp != '=')) {
			if (! isdigit(*cp))
				flag = 1;
			cp++;
		}
		if (!flag)
			continue;
		if (cmd->opts[i][0] == '$' && strchr(cmd->opts[i], '=') != NULL) {
			if (curenv + 1 >= MAXENV)
				fatal(1, "%s: environment length exceeded",cmd->name);
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
		struct passwd * pwbuf;
		struct group * grbuf;
		struct stat statbuf;
		char * ptr;
		char usergroup[MAXSTRLEN];

		/* Get user and group name of the owner of the file */
		stat(cmd->args[0],&statbuf);

		pwbuf = getpwuid(statbuf.st_uid);
		if (pwbuf == NULL)
			fatal(1,"%s: no identified user for uid %d", cmd->name, statbuf.st_uid);
		grbuf = getgrgid(statbuf.st_gid);
		if (grbuf == NULL)
			fatal(1,"%s: no identified group for gid %d", cmd->name, statbuf.st_gid);

		if (strlen(pwbuf->pw_name) + strlen(grbuf->gr_name) + 1 >= MAXSTRLEN)
			fatal(1, "%s: user/group string buffer length exceeded", cmd->name);
		strcpy(usergroup, pwbuf->pw_name);
		strcat(usergroup, ":");
		strcat(usergroup, grbuf->gr_name);

		/* check users,groups candidates */
		
		for (cp = GetField(cp, str, MAXSTRLEN - 5); cp != NULL; cp = GetField(cp, str, MAXSTRLEN - 5)) {
			regexp		*reg1 = NULL;
			char regstr[MAXSTRLEN];

			ptr=strchr(str,':');
			if (ptr == NULL)
				fatal(1,"%s: fowners argument must respect the user:group format", cmd->name);

			strcpy(regstr, "^(");
			strcat(regstr, str);
			strcat(regstr, ")$");
		
			if ((reg1 = regcomp(regstr)) == NULL)
				return logger(LOG_ERR, "Invalid regex '%s'", str);
	
			if ((regexec(reg1, usergroup) == 1))
				break;
		}
		if (cp == NULL)
			fatal(1,"%s: file %s (%s) did not pass ownership constraints",
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
		char   mode[5];

		stat(cmd->args[0],&buf);
		strnprintf(mode, 5, "%o", buf.st_mode & 07777);

		for (cp = GetField(cp, str, MAXSTRLEN - 5); cp != NULL; cp = GetField(cp, str, MAXSTRLEN - 5)) {
			regexp		*reg1 = NULL;
			char regstr[MAXSTRLEN];

			strcpy(regstr, "^(");
			strcat(regstr, str);
			strcat(regstr, ")$");
		
			if ((reg1 = regcomp(regstr)) == NULL)
				return logger(LOG_ERR, "Invalid regex '%s'", str);

			if (regexec(reg1,mode) == 1)
				break;
		}
		if (cp == NULL)
			fatal(1,"%s: file %s (%s) did not pass permissions constraints",
			  cmd->name, cmd->args[0],mode);
	}

	if (strcmp("MAGIC_SHELL", cmd->args[0]) == 0) {
		for (i = 0; environ[i] != NULL; i++) 
			if (strncmp("SHELL=", environ[i], 6) == 0)
				break;

		if (environ[i] != NULL) {
			if (curarg >= MAXARG - 1)
				fatal(1, "%s: argument length exceeded",cmd->name);
			new_argv[curarg++] = environ[i] + 6;
		} else {
			fatal(1, "%s: no shell", cmd->name);
		}

		if (argc != 1) {
			if (curarg >= MAXARG - 1)
				fatal(1, "%s: argument length exceeded",cmd->name);
			new_argv[curarg++] = "-c";

			for (i = 1; i < argc; i++)
				len += strlen(argv[i]) + 1;

			if ((cp = (char *)malloc(len + 10)) == NULL)
				fatal(1, "%s: unable to create buffer", cmd->name);

			len = 0;
			*cp = '\0';

			for (i = 1; i < argc; i++) {
				strcat(cp, argv[i]);
				strcat(cp, " ");
			}
			if (curarg >= MAXARG - 1)
				fatal(1, "%s: argument length exceeded",cmd->name);
			new_argv[curarg++] = cp;
		}
	} else {
	int consumed_args = 1;

		for (i = 0; i < cmd->nargs; i++) {
			np = cmd->args[i];

			/* Complete argument is a variable expansion. */
			if (strlen(np) == 2 && np[0] == '$') {
				if (np[1] == '*') {
					if (curarg + argc >= MAXARG - 1)
						fatal(1, "%s: argument length exceeded",cmd->name);
					for (j = consumed_args; j < argc; j++)
						new_argv[curarg++] = argv[j];
				} else
				if (isdigit(np[1])) {
				int argi = atoi(np + 1);

					if (argi > argc)
						fatal(1, "%s Referenced argument out of range",cmd->name);
					if (curarg >= MAXARG - 1)
						fatal(1, "%s: argument length exceeded",cmd->name);
					new_argv[curarg++] = argv[argi];
					if (argi >= consumed_args)
						consumed_args = argi + 1;
				}
				continue;
			} else {
				/* Embedded match */
				while ((cp = strchr(np, '$')) != NULL) {
					if ((cp != cmd->args[i]) && (*(cp-1) == '\\'))
						np = cp + 1;
					else {
					char *tmp;

						np = cp + 1;
						++cp;

						if (*cp == '*') {
						int len = 1;
						char *buffer;

							++cp;
							/* Find total length of all arguments */
							for (j = 1; j < argc; j++)
								len += strlen(argv[j]) + 1;

							if ((buffer = malloc(len)) == NULL)
								fatal(1, "Can't allocate buffer");

							buffer[0] = 0;

							/* Expand all arguments */
							for (j = 1; j < argc; j++) {
								strcat(buffer, argv[j]);
								if (j < argc - 1) strcat(buffer, " ");
							}
							tmp = str_replace(cmd->args[i],
								np - cmd->args[i] - 1, cp - np + 1, buffer);
							cp = tmp + (cp - cmd->args[i]);
							np = cp;
							cmd->args[i] = tmp;
						} else {
							while (isdigit(*cp)) ++cp;

							if (cp != np) {
								val = atoi(np);

								tmp = str_replace(cmd->args[i],
									np - cmd->args[i] - 1, cp - np + 1, argv[val]);
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
					fatal(1, "%s: argument length exceeded",cmd->name);
				new_argv[curarg++] = cmd->args[i];
				continue;
			}
		}
	}
	new_argv[curarg] = NULL;

	if (stat(new_argv[0], &st) != -1 && st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
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

void output(cmd)
cmd_t	*cmd;
{
	int	i;

	printf("cmd '%s'\n",cmd->name);
	printf("\n  args\t");
	for (i = 0; i < cmd->nargs; i++)
		printf("'%s' ",cmd->args[i]);
	printf("\n  opts\t");
	for (i = 0; i < cmd->nopts; i++)
		printf("'%s' ",cmd->opts[i]);
	printf("\n");
}
char
*format_cmd(int argc, char **argv, char *retbuf, int buflen) 
/*   
     Format command and args for printing to syslog
     If length (command + args) is too long, try length(command). If THATS
     too long, return an error message.
*/
{   
int	i,l=0,s,ss,m=0;
char *buf =0;

	s = strlen(argv[0]);
	if ((s>MAXSTRLEN) ){
		retbuf = strcpy(retbuf, "unknown cmd (name too long in format_cmd)");
		return retbuf;
	}
	ss=s;
	for (i = 1; i < argc; i++) { 
		l=strlen(argv[i]);
		m=l>m?l:m;
		s+=l;
	}
	if (l) s+=argc-1; /* count spaces if there are arguments*/
	if (s > MAXSTRLEN){ /* Ooops, we've gone over. */
		s=ss; /* Just print command name */
		m=0;
		argc=0;
	}
/*	sprintf(retbuf,"%s",argv[0]);*/
	strcpy(retbuf, "");
	if (m) buf=(char *)malloc(m + 2);
	if (buf) {
		for (i = 1; i < argc; i++) {
			sprintf(buf," %s",argv[i]);
			strcat(retbuf,buf);
		}
		free(buf);
	}
	return(retbuf);
}

int vlogger(unsigned level, const char *format, va_list args) {
char buffer[MAXSTRLEN], buffer2[MAXSTRLEN], buffer3[MAXSTRLEN];
char *username = "unknown";

	if (level >= minimum_logging_level) return -1;

	if (realuser) username = realuser;

	vstrnprintf(buffer2, MAXSTRLEN, format, args);
	if (level & LOG_PRINT) printf("%s\n", buffer2);
	level &= ~LOG_PRINT;
	strnprintf(buffer, MAXSTRLEN, "%s%s: %s", username, 
		format_cmd(gargc, gargv, buffer3, MAXSTRLEN),
		buffer2);
	syslog(level, "%s", buffer);
	return -1;
}

int logger(unsigned level, const char *format, ...) {
va_list va;

	va_start(va, format);
	vlogger(level, format, va);
	va_end(va);
	return -1;
}

void fatal(int logit, const char *format, ...) {
char buffer[MAXSTRLEN];
va_list	ap;

	va_start(ap, format);
	vstrnprintf(buffer, MAXSTRLEN, format, ap);
	fprintf(stderr, "%s\n", buffer);
	if (logit) logger(LOG_ERR, "%s", buffer);
	va_end(ap);
	exit(1);
}

