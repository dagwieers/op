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
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
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

#ifndef _AIX
extern char	*strchr();
#endif
extern char	*savestr();
extern char	*getpass(), *crypt();

char    *format_cmd(int argc, char **argv, char *retbuf, int buflen);
char    *GetCode();
cmd_t	*Find();
int Verify(cmd_t *cmd, int num, int argc, char **argv);
cmd_t	*Find(char *name);
int Go(cmd_t *cmd, int num, int argc, char **argv);
cmd_t	*First = NULL;
var_t	*Variables = NULL;
struct passwd *realuser = NULL;
int gargc = -1;
char **gargv = NULL;
sigset_t sig_mask, old_sig_mask;

void Usage()
{
	fatal(0, "Usage: %s mnemonic [args]\n       %s -V -H [-u username] mnemonic",
			gargv[0], gargv[0]);
}

int ReadDir( char *dir )
{
DIR *d;

	if ((d = opendir(dir)) != NULL)
	{
	struct dirent *f;
	int i, successes = 0, dir_entries = 0, dir_capacity = 32;
	char **dir_list;

		if (!(dir_list = malloc(sizeof(char*) * dir_capacity)))
			fatal(1, "failed to malloc space for directory entries");
		memset(dir_list, 0, sizeof(char*) * dir_capacity);

		while ((f = readdir(d)))
		{
		char full_path[PATH_MAX];

			if (f->d_name[0] == '.' || (strlen(f->d_name) > 5 && strcmp(f->d_name + strlen(f->d_name) - 5, ".conf")))
				continue;
#ifdef HAVE_SNPRINTF
			snprintf(full_path, PATH_MAX, "%s/%s", OP_ACCESS_DIR, f->d_name);
#else
			sprintf(full_path, "%s/%s", OP_ACCESS_DIR, f->d_name);
#endif
			if (!(dir_list[dir_entries] = strdup(full_path)))
				fatal(1, "failed to malloc space for directory entry");
			++dir_entries;
		}
		closedir(d);
		qsort(dir_list, dir_entries, sizeof(char*),
			(int(*)(const void *, const void *))strcmp);
		for (i = 0; i < dir_entries; ++i)
			if (ReadFile(dir_list[i])) successes++;
		return successes;
	}
	return 0;
}

int main(argc, argv)
int	argc;
char	**argv;
{
	int		num, argStart = 1;
	char		user[MAXSTRLEN];
	cmd_t		*cmd, *def, *new;
	struct passwd	*pw;
	int		hflag = 0, read_conf = 0, read_conf_dir = 0;
	char		*uptr = NULL;
	char		cmd_s[MAXSTRLEN];
	char            *pcmd_s;

	sigemptyset(&sig_mask);
	sigaddset(&sig_mask, SIGINT);
	sigaddset(&sig_mask, SIGQUIT);
	sigaddset(&sig_mask, SIGTERM);

	if (sigprocmask(SIG_BLOCK, &sig_mask, &old_sig_mask))
		fatal(1, "Could not set signal mask");

	gargv = argv;
	gargc = argc;
	realuser = getpwuid(getuid());

	while (1) {
		if (argStart >= argc)
			break;

		if (strcmp("-V", argv[argStart]) == 0) {
			printf("%s\n", VERSION);
			return 0;
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

#if defined (bsdi) || defined (SOLARIS) || defined (__linux__)
        openlog("op", LOG_PID | LOG_CONS, LOG_AUTH);
#else
	if (openlog("op", LOG_PID | LOG_CONS, LOG_AUTH) < 0) 
                fatal(0, "openlog failed");
#endif
	read_conf = ReadFile( OP_ACCESS );
	read_conf_dir = ReadDir( OP_ACCESS_DIR );

	if (!read_conf && !read_conf_dir)
		fatal(1, "Could not open %s or any configuration files in %s", OP_ACCESS, OP_ACCESS_DIR);

	if (hflag) {
		if (uptr != NULL) {
			if (getuid() != 0) 
				fatal(1, "Permission denied for -u option");
		}
	}
	if (uptr != NULL) 
		Usage();

	if (argStart >= argc)
		Usage();

	def = Find("DEFAULT");
	cmd = Find(argv[argStart]);

	if (cmd == NULL) 
		fatal(1, "No such command %s", argv[1]);

	argc -= argStart;
	argv += argStart;

	new = Build(def, cmd);
	num = CountArgs(new);

	if ((num < 0) && ((argc-1) < -num))
		fatal(1, "Improper number of arguments");
	if ((num > 0) && ((argc-1) != num)) 
		fatal(1, "Improper number of arguments");
	if (num <0)
		num = -num;

	if ((pw = getpwuid(getuid())) == NULL) 
		exit(1);
	realuser = getpwuid(getuid());
	strncpy(user, pw->pw_name, MAXSTRLEN);
	pcmd_s = format_cmd(argc, argv, cmd_s, MAXSTRLEN);
	if (Verify(new, num, argc, argv) < 0)
		fatal(0, "Permission denied by op");

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
				pr->resp = strdup(pass);
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

int Verify(cmd, num, argc, argv)
cmd_t	*cmd;
int	argc;
int	num;
char	**argv;
{
int		gr_fail = 1, uid_fail = 1, netgr_fail = 1;
int		i, j, val;
char		*np, *cp, str[MAXSTRLEN], buf[MAXSTRLEN], hostname[HOST_NAME_MAX];
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
struct group	*gr;
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

	if ((cp=FindOpt(cmd, "password")) != NULL) {
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
				if (cp == NULL) return logger(LOG_ERR, "Argument %i (%s) did not pass wildcard constraint", j, argv[j]);
			}
		}
		if(reg1 != NULL){
			free(reg1);
			reg1=NULL;
		}

		strncpy(str, cmd->opts[i] + 1, cp - cmd->opts[i] - 1);
		str[cp - cmd->opts[i] - 1] = '\0';
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
		if (cp == NULL) return logger(LOG_ERR, "Argument '%s' did not pass constraint '%s'", argv[val], np);
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

#ifdef XAUTH
	if (getenv("DISPLAY") != NULL && (cp = FindOpt(cmd, "xauth")) != NULL) {
	struct passwd *currentpw;
	char tmpxauth[MAXSTRLEN], xauth[MAXSTRLEN], cxauth[MAXSTRLEN], *display;
	int status;
	uid_t uid;
	gid_t gid;
	struct stat st;

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
				//logger(LOG_DEBUG, "Executing '%s %s %s %s %s %s' as %i", argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], currentpw->pw_uid);
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

				//logger(LOG_DEBUG, "Executing '%s %s %s %s %s' as %i", argv[0], argv[1], argv[2], argv[3], argv[4], uid);
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
			++curenv;
			/* Propagate $DISPLAY to new environment */
			new_envp[curenv] = malloc(strlen("DISPLAY=") + strlen(getenv("DISPLAY")) + 1);
			strcpy(new_envp[curenv], "DISPLAY=");
			strcat(new_envp[curenv], getenv("DISPLAY"));
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
			cp = cmd->opts[i] + 1;
			flag = 0;
			while ((*cp != '\0') && (*cp != '=')) {
				if (! isdigit(*cp))
					flag = 1;
				cp++;
			}
			if (! flag)
				continue;
			if (strchr(cmd->opts[i], '=') != NULL) {
				new_envp[curenv++] = cmd->opts[i] + 1;
				continue;
			}
			for (j = 0; environ[j] != NULL ; j++) {
				if ((cp = strchr(environ[j], '=')) == NULL)
					continue;
				if (strncmp(cmd->opts[i] + 1, environ[j],
						cp - environ[j]) == 0) {
					new_envp[curenv++] = environ[j];
					break;
				}
			}
		}
	} else {
		for (i = 0; environ[i] != NULL; i++)
			new_envp[curenv++] = environ[i];
	}
	new_envp[curenv] = NULL;

	if (strcmp("MAGIC_SHELL", cmd->args[0]) == 0) {
		for (i = 0; environ[i] != NULL; i++) 
			if (strncmp("SHELL=", environ[i], 6) == 0)
				break;

		if (environ[i] != NULL)
			new_argv[curarg++] = environ[i] + 6;
		else {
			fatal(1, "No shell");
		}

		if (argc != 1) {
			new_argv[curarg++] = "-c";

			for (i = 1; i < argc; i++)
				len += strlen(argv[i]) + 1;

			if ((cp = (char *)malloc(len + 10)) == NULL)
				fatal(1, "Unable to create buffer");

			len = 0;
			*cp = '\0';

			for (i = 1; i < argc; i++) {
				strcat(cp, argv[i]);
				strcat(cp, " ");
			}
			new_argv[curarg++] = cp;
		}
	} else {
		for (i = 0; i < cmd->nargs; i++) {
			np = cmd->args[i];

			/* Match whole arguments */
			if (*np == '$') {
				cp = np = np + 1;

				if (*cp == '*' && cp[1] == 0) {
					for (j = num + 1; j < argc; j++) {
						new_argv[curarg++] = argv[j];
					}
					continue;
				}

				while (isdigit(*cp)) ++cp;

				/* Huh? */
				if (cp == np)  {
					new_argv[curarg++] = cmd->args[i];
					continue;
				}

				/* Full match... */
				if (!*cp)  {
					strncpy(str, np, cp - np);
					str[cp - np] = '\0';
					val = atoi(str);

					new_argv[curarg++] = argv[val];
					continue;
				}
			}

			/* Embedded match */
			while ((cp = strchr(np, '$')) != NULL) {
				if ((cp != cmd->args[i]) && (*(cp-1) == '\\'))
					np = cp + 1;
				else {
				char *tmp;

					np = cp + 1;
					++cp;

					if (*cp == '*') {
					int len = 0;
					char *buffer;

						++cp;
						/* Find total length of all arguments */
						for (j = num + 1; j < argc; j++)
							len += strlen(argv[j]) + 1;

						if ((buffer = malloc(len)) == NULL)
							fatal(1, "Can't allocate buffer");

						buffer[0] = 0;

						/* Expand all arguments */
						for (j = num + 1; j < argc; j++) {
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

						/* Huh? */
						if (cp == np)  {
							new_argv[curarg++] = cmd->args[i];
							continue;
						}

						strncpy(str, np, cp - np);
						str[cp - np] = '\0';
						val = atoi(str);

						tmp = str_replace(cmd->args[i],
							np - cmd->args[i] - 1, cp - np + 1, argv[val]);
						cp = tmp + (cp - cmd->args[i]);
						np = cp;
						cmd->args[i] = tmp;
					}
				}
			}

			if (cp == NULL) {
				new_argv[curarg++] = cmd->args[i];
				continue;
			}
		}
	}
	new_argv[curarg] = NULL;

/*	for (i = 0; i < curarg; ++i)
		printf("arg[%i] = '%s'\n", i, new_argv[i]);*/

	logger(LOG_INFO, "SUCCESS");
	if (sigprocmask(SIG_SETMASK, &old_sig_mask, NULL))
		fatal(1, "Could not restore signal mask");
	if (execve(new_argv[0], new_argv, new_envp) < 0)
		perror("execve");
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

#ifndef HAVE_SNPRINTF
#warning You have not compiled op with snprintf support, presumably because
#warning your system does not have it. This leaves op potentially open to
#warning buffer overflows.
#endif

unsigned minimum_logging_level = LOG_INFO;

int vlogger(unsigned level, const char *format, va_list args) {
char buffer[MAXSTRLEN], buffer2[MAXSTRLEN], buffer3[MAXSTRLEN];
char *username = "unknown";

	if (level >= minimum_logging_level) return -1;

	if (realuser) username = realuser->pw_name;

#ifdef HAVE_SNPRINTF
	vsnprintf(buffer2, MAXSTRLEN, format, args);
#else
	vsprintf(buffer2, format, args);
#endif
	if (level & LOG_PRINT) printf("%s\n", buffer2);
	level &= ~LOG_PRINT;
#ifdef HAVE_SNPRINTF
	snprintf(buffer, MAXSTRLEN, "%s =>%s: %s", username, 
		format_cmd(gargc, gargv, buffer3, MAXSTRLEN),
		buffer2);
#else
	sprintf(buffer, "%s =>%s: %s", username, 
		format_cmd(gargc, gargv, buffer3, MAXSTRLEN),
		buffer2);
#endif
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
#ifdef HAVE_SNPRINTF
	vsnprintf(buffer, MAXSTRLEN, format, ap);
#else
	vsprintf(buffer, format, ap);
#endif
	fprintf(stderr, "%s\n", buffer);
	if (logit) logger(LOG_ERR, "%s", buffer);
	va_end(ap);
	exit(1);
}

