/* +-------------------------------------------------------------------+ */
/* | Copyright 1988,1991, David Koblas.                                | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

#include <string.h>
#include	<ctype.h>

#ifdef TEST
main(argc,argv)
int	argc;
char	**argv;
{
	int	i;
	for (i=1;i<argc;i++)
		printf("%10s  == %d\n",argv[i],atov(argv[i],0));
}
#endif

int atov(str,type)
char	*str;
int	type;
{
	int		sign = 1;
	int		i;
	char		c;
	int		val=0,n;

	i=0;
	while ((str[i]==' ') || (str[i]=='\t')) i++;
	if (str[i]=='-')  {
		sign = -1;
		i++;
	} else if (str[i]=='+') {
		sign = 1;
		i++;
	}
	if (type==0)  {
		if (str[i]=='0') {
			i++;
			if (str[i]=='%') {
				i++;
				type=2;
			} else if (str[i]=='x') {
				i++;
				type=16;
			} else {
				type=8;
			}
		} else {
			type=10;
		}
	}
	for (;i<strlen(str);i++) {
		c=str[i];
		if (isdigit(c)) {
			n = c - '0';
		} else if (isupper(c)) {
			n = c - 'A' + 10;
		} else if (islower(c)) {
			n = c - 'a' + 10;
		} else {
			goto	out;
		}
		if (n>=type)
			goto out;
		val = (val*type)+n;
	}
out:	
	return(val * sign);
}
