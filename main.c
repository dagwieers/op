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
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <string.h>
#include "defs.h"
#include "regexp.h"

#ifdef sun 
#  if defined(__SVR4) || defined(__svr4__)
#    define SOLARIS
#  endif
#endif

#ifdef SHADOW
#include <shadow.h>
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

#define	MAXARG	1024
#define	MAXENV	MAXARG

#ifndef _AIX
extern char	*strchr();
#endif
extern char	*savestr();
extern char	*getpass(), *crypt();

char	*Progname;
char    *format_cmd();
char    *GetCode();
cmd_t	*Find();
cmd_t	*First = NULL;

Usage()
{
	fatal("Usage: %s mnemonic [args]\n       %s -H [-u username] mnemonic",
			Progname, Progname);
}

main(argc, argv)
int	argc;
char	**argv;
{
	int		num, argStart = 1;
	char		user[MAXSTRLEN];
	cmd_t		*cmd, *def, *new;
	struct passwd	*pw;
	int		hflag = 0;
	char		*uptr = NULL;
	char		cmd_s[MAXSTRLEN];
	char            *pcmd_s;

	Progname = argv[0];

	while (1) {
		if (argStart >= argc)
			break;

		if (strcmp("-H", argv[argStart]) == 0) {
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
        openlog("op", 0, LOG_AUTH);
#else
	if (openlog("op", 0, LOG_AUTH) < 0) 
                fatal("openlog failed");
#endif
	ReadFile( OP_ACCESS );

	if (hflag) {
		if (uptr != NULL) {
			if (getuid() != 0) 
				fatal("Permission denied for -u option");
		}
	}
	if (uptr != NULL) 
		Usage();

	if (argStart >= argc)
		Usage();

	def = Find("DEFAULT");
	cmd = Find(argv[argStart]);

	if (cmd == NULL) 
		fatal("No such command %s", argv[1]);

	argc -= argStart;
	argv += argStart;

	new = Build(def, cmd);
	num = CountArgs(new);

	if ((num < 0) && ((argc-1) < -num))
		fatal("Improper number of arguments");
	if ((num > 0) && ((argc-1) != num)) 
		fatal("Improper number of arguments");
	if (num <0)
		num = -num;

	if ((pw = getpwuid(getuid())) == NULL) 
		exit(1);
	strcpy(user, pw->pw_name);
	pcmd_s=format_cmd(argc,argv,cmd_s);
	if (Verify(new, num, argc, argv) < 0) {
		syslog(LOG_NOTICE, "user %s FAILED to execute '%s'", 
				user, cmd_s);
		fatal("Permission denied by op");
	} else {
		syslog(LOG_NOTICE, "user %s SUCCEDED executing '%s'",
				user, cmd_s);
	}

	return Go(new, num, argc, argv);
}

fatal(va_alist)
 va_dcl
{
	va_list	ap;
	char	*s;

	va_start(ap);
	s = va_arg(ap, char *);
	vfprintf(stderr, s, ap);
	fputc('\n', stderr);
	va_end(ap);

	exit(1);
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

char	*GetField(cp, str)
char	*cp, *str;
{
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
	}

	*str = '\0';

	return (*cp == '\0') ? cp : (cp+1);
}

Verify(cmd, num, argc, argv)
cmd_t	*cmd;
int	argc;
int	num;
char	**argv;
{
  int		gr_fail = 1, uid_fail = 1;
  int		i, j, val;
  char		*np, *cp, str[MAXSTRLEN], buf[MAXSTRLEN];
  regexp		*reg1 = NULL;
  regexp		*reg2 = NULL;
  struct passwd	*pw;
#ifdef SHADOW
  struct spwd *spw;
#endif
  struct group	*gr;
#ifdef SECURID
	struct          SD_CLIENT sd_dat, *sd;
	int             k;
	char            input[64],*p;
#endif
  
  if ((pw = getpwuid(getuid())) == NULL) 
    return -1;
#ifdef SECURID
  if ((cp=FindOpt(cmd, "securid")) != NULL) {
      memset(&sd_dat, 0, sizeof(sd_dat));   /* clear sd_auth struct */
      sd = &sd_dat;
      creadcfg();		/*  accesses sdconf.rec  */
      if (sd_init(sd)){
	  printf("Cannot contact ACE server\n");
	  return -1;
      }
      if (sd_auth(sd))
	return -1;
  }
#else
  if ((cp=FindOpt(cmd, "securid")) != NULL) {
      printf("SecureID not supported by op.\nAccess denied\n");
      return -1;
  }
#endif	
  if ((cp=FindOpt(cmd, "password")) != NULL) {
    if ((np = getpass("Password:")) == NULL)
      return -1;

    if (((cp = GetField(cp, str)) != NULL) && 
	((pw = getpwnam(str)) == NULL))
      return -1;
#ifdef SHADOW
    if (strcmp(pw->pw_passwd,"x")==NULL){ /* Shadow passwords */
	if ((spw = getspnam(pw->pw_name)) == NULL)
	  return -1;
	pw->pw_passwd=spw->sp_pwdp;
    }
#endif
	
    if (strcmp(crypt(np, pw->pw_passwd), pw->pw_passwd) != 0)
      return -1;

  }
  
  if ((pw = getpwuid(getuid())) == NULL) 
    return -1;

  if ((cp = FindOpt(cmd, "groups")) != NULL) {
    for (cp=GetField(cp, str); cp!=NULL; cp=GetField(cp, str)) {
      if ((reg1=regcomp(str)) == NULL)
	return -1;
      if ((gr = getgrgid(pw->pw_gid)) != NULL) {
	if (regexec(reg1,gr->gr_name) == 1) {
	  gr_fail = 0;
	  break;
	}
      }

      setgrent();
      while ((gr = getgrent()) != NULL) {
	i = 0;
	while (gr->gr_mem[i] != NULL) {
	  if (strcmp(gr->gr_mem[i],
		     pw->pw_name)==0)
	    break;
	  i++;
	}
	if ((gr->gr_mem[i] != NULL) && 
	    (regexec(reg1,gr->gr_name) == 1)) {
	  gr_fail = 0;

	  break;
	}
	if (gr->gr_mem[i] != NULL) {
	  if (regexec(reg1,gr->gr_name) == 1) {
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
    for (cp=GetField(cp, str); cp!=NULL; cp=GetField(cp, str)) {
      if ((reg1=regcomp(str)) == NULL)
	return -1;
      if (regexec(reg1,pw->pw_name) == 1) {
	uid_fail = 0;
	break;
      }
    }
  }
  if(reg1 != NULL){
    free(reg1);
    reg1=NULL;
  }

  if (gr_fail && uid_fail)
    return -1;
  for (i = 0; i < cmd->nopts; i++) {
    if ((cmd->opts[i][0] != '$') || 
	((cp = strchr(cmd->opts[i], '=')) == NULL))
      continue;
    if (cmd->opts[i][1] != '*') {
      for (np = cmd->opts[i] + 1; np != cp; np++) 
	if (!isdigit(*np))
	  break;
      if (np != cp)
	continue;
    } else {
      if (cmd->opts[i][2] != '=')
	continue;
      np = cmd->opts[i] + 3;
      for (j = num+1; j < argc; j++) {
	cp = np;
	for (cp=GetField(cp, str); cp!=NULL; 
	     cp=GetField(cp, str)) {
	  if ((reg1=regcomp(str)) == NULL)
	    return -1;
	  if (regexec(reg1,argv[j]) == 1)
	    break;
	}
	if (cp == NULL)
	  return -1;
      }
    }
    if(reg1 != NULL){
      free(reg1);
      reg1=NULL;
    }
		
    strncpy(str, cmd->opts[i] + 1, cp - cmd->opts[i] - 1);
    str[cp - cmd->opts[i] - 1] = '\0';
    val = atoi(str);
    
    if (val >= argc)
      continue;
    cp++;
    np = cp;
    if (reg2 != NULL) {
      for (cp=GetField(cp, str); cp!=NULL; 
	   cp=GetField(cp, str)) {
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
      for (cp=GetField(cp, str); cp!=NULL; 
	   cp=GetField(cp, str)) {
	if ((reg2=regcomp(str)) == NULL) 
	  return -1;
	if (regexec(reg2,argv[val]) == 1)
	  break;

	free(reg2);
	reg2 = NULL;
      }
    }
    if (cp == NULL)
      return -1;
  }
}

Go(cmd, num, argc, argv)
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
	int		ngroups, gidset[256];
	int		curenv = 0, curarg = 0;
	char		*new_envp[MAXENV];
	char		*new_argv[MAXARG];
	char		str[MAXSTRLEN], buf[4*MAXSTRLEN];

	if ((cp = FindOpt(cmd, "uid")) == NULL) {
		if (setuid(0) < 0)
			fatal("Unable to set uid to default", cp);
	} else {
		if ((pw = getpwnam(cp)) == NULL) {
			if (setuid(atoi(cp)) < 0)
				fatal("Unable to set uid to %s", cp);
		}
		if (setuid(pw->pw_uid) < 0)
			fatal("Unable to set uid to %s", cp);
	}

	if ((cp = FindOpt(cmd, "gid")) == NULL) {
		;		/* don't have a default */
	} else {
		for (cp=GetField(cp, str); cp!=NULL; cp=GetField(cp, str)) {
			if ((gr = getgrnam(cp)) != NULL)
				gidset[ngroups++] = gr->gr_gid;
		}
		if (ngroups == 0) 
			fatal("Unable to setgid to any group");
		if (setgroups(ngroups, gidset) < 0)
			fatal("Set group failed");
	}

	if ((cp = FindOpt(cmd, "umask")) == NULL) {
		if (umask(0022) < 0)
			fatal("Unable to set umask to default");
	} else {
		if (umask(atov(cp, 8)) < 0)
			fatal("Unable to set umask to %s", cp);
	}

	if ((cp = FindOpt(cmd, "chroot")) == NULL) {
		;		/* don't have a default */
	} else {
		if (chroot(cp) < 0)
			fatal("Unable to chroot to %s", cp);
	}

	if ((cp = FindOpt(cmd, "dir")) == NULL) {
		;		/* don't have a default */
	} else {
		if (chdir(cp) < 0) 
			fatal("Unable to chdir to %s", cp);
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
			fprintf(stderr,"No shell\n");
			exit(1);
		}

		if (argc != 1) {
			new_argv[curarg++] = "-c";

			for (i = 1; i < argc; i++)
				len += strlen(argv[i]) + 1;

			if ((cp = (char *)malloc(len + 10)) == NULL) {
				fprintf(stderr, "Unable to create buffer");
				exit(1);
			}

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

			while ((cp = strchr(np, '$')) != NULL) {
				if ((cp != cmd->args[i]) && (*(cp-1) == '\\'))
					np = cp + 1;
				else
					break;
			}

			if (cp == NULL) {
				new_argv[curarg++] = cmd->args[i];
				continue;
			}
			if (*(cp+1) == '*') {
				for (j = num + 1; j < argc; j++) {
					new_argv[curarg++] = argv[j];
				}
				continue;
			}

			cp++;
			np = cp;
			while (isdigit(*cp))
				cp++;
			if ((cp - np) == 0) {
				new_argv[curarg++] = cmd->args[i];
				continue;
			}
			strncpy(str, np, cp - np);
			str[cp - np] = '\0';
			val = atoi(str);
			buf[0] = '\0';
			strncpy(buf, cmd->args[i], np - cmd->args[i] - 1);
			strcat(buf, argv[val]);
			strcat(buf, cp);
			new_argv[curarg++] = savestr(buf);
		}
	}
	new_argv[curarg] = NULL;

	if (execve(new_argv[0], new_argv, new_envp) < 0)
		perror("execve");
}

output(cmd)
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
*format_cmd(argc,argv,retbuf) 
/*   
     Format command and args for printing to syslog
     If length (command + args) is too long, try length(command). If THATS
     too long, return an error message.
*/
int     argc;
char	**argv;
char    *retbuf;
{   
  int	i,l=0,s,ss,m=0;
  char *buf =0;
  s = strlen(argv[0]);
  if ((s>MAXSTRLEN) ){
    retbuf=strcpy(retbuf,"unknown cmd (name too long in format_cmd)");
    return(retbuf);
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
  sprintf(retbuf,"%s",argv[0]);
  if (m)
    buf=(char *)malloc(m);
  if (buf) {
    for (i = 1; i < argc; i++) {
      sprintf(buf," %s",argv[i]);
      strcat(retbuf,buf);
    }
    free(buf);
  }
  return(retbuf);
}


