/*
** Copyright (c) 2016, Cyrille Lefevre <cyrille.lefevre-regs@laposte.net>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 
**   1. Redistributions of source code must retain the above copyright
**      notice, this list of conditions and the following disclaimer.
**   2. Redistributions in binary form must reproduce the above copyright
**      notice, this list of conditions and the following disclaimer in the
**      documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
** PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
** BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
** OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
** OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
** BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
** OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
** EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rplregex.h"

#ifndef HAVE_REGEX
static char *_regerrorstr;

void regerror(char *s)
{
    _regerrorstr = s;
}
#endif

void rpl_regfree(REGEXP_T **_prog)
{
    REGEXP_T *prog = *_prog;
#ifdef HAVE_REGEX
    if (prog->preg.re_nsub)
	free(prog->pmatch);
    regfree(&prog->preg);
#endif
    free(prog);
    *_prog = (REGEXP_T *) NULL;
}

int rpl_regcomp(REGEXP_T **_prog, const char *regex, int cflags)
{
#ifdef HAVE_REGEX
    int rc;
    REGEXP_T *prog = *_prog = (REGEXP_T *) malloc(sizeof(REGEXP_T));
    if (prog == NULL)
	return REG_ESPACE;
    prog->cflags = cflags;
    prog->pmatch = NULL;
    rc = regcomp(&prog->preg, regex, cflags|REG_EXTENDED);
    if (rc || cflags & REG_NOSUB || prog->preg.re_nsub == 0)
	return rc;
    prog->pmatch = calloc(prog->preg.re_nsub + 1, sizeof(regmatch_t));
    if (prog->pmatch == NULL) {
	prog->preg.re_nsub = 0;
	rpl_regfree(&prog);
	return REG_ESPACE;
    }
    return 0;
#else
    *_prog = regcomp((char *)regex);
    return *_prog == NULL;
#endif
}

int rpl_regexec(REGEXP_T * const *_prog, const char *string)
{
    REGEXP_T *prog = *_prog;
#ifdef HAVE_REGEX
    if (!(prog->cflags & REG_NOSUB))
	prog->string = string;
    return regexec(&prog->preg, string, prog->preg.re_nsub + 1, prog->pmatch, 0);
#else
    int rc = !regexec(prog, (char *)string);
    return rc && _regerrorstr ? REG_ESPACE : rc;
#endif
}

int rpl_regsub(REGEXP_T * const *_prog, const char *source, char *dest, size_t size)
{
    REGEXP_T *prog = *_prog;
    const char *src;
    char *dst, c;
    int no;
    size_t len;

    if (prog == NULL || source == NULL || dest == NULL || size == 0)
        return REG_ESPACE;

    src = source;
    dst = dest;
    while ((c = *src++) != '\0') {
        if (c == '&')
            no = 0;
        else if (c == '\\' && '0' <= *src && *src <= '9')
            no = *src++ - '0';
        else
            no = -1;
        if (no < 0) {           /* Ordinary character. */
            if (c == '\\' && (*src == '\\' || *src == '&'))
                c = *src++;
            if ((size_t) (dst - dest) + 1 >= size)
            	return REG_ESPACE;
            *dst++ = c;
#ifdef HAVE_REGEX
        } else if (prog->preg.re_nsub &&
        	   (size_t) no <= prog->preg.re_nsub &&
        	   prog->pmatch[no].rm_so >= 0 &&
        	   prog->pmatch[no].rm_eo > prog->pmatch[no].rm_so) {
            len = (size_t) (prog->pmatch[no].rm_eo - prog->pmatch[no].rm_so);
            if ((size_t) (dst - dest) + len >= size)
            	return REG_ESPACE;
            /* Flawfinder: ignore (strncpy) */
            strncpy(dst, prog->string + prog->pmatch[no].rm_so, len);
#else
        } else if (prog->startp[no] != NULL && prog->endp[no] != NULL &&
        	   prog->endp[no] > prog->startp[no]) {
            len = (size_t) (prog->endp[no] - prog->startp[no]);
            if ((size_t) (dst - dest) + len >= size)
            	return REG_ESPACE;
            /* Flawfinder: ignore (strncpy) */
            strncpy(dst, prog->startp[no], len);
#endif
            dst += len;
            if (len != 0 && *(dst - 1) == '\0')	/* strncpy hit NUL. */
            	return REG_ESUBREG;
        }
    }
    *dst = '\0';
    return 0;
}

char *rpl_regerror(int error, REGEXP_T * const *_prog)
{
    char *buf;
#ifdef HAVE_REGEX
    REGEXP_T *prog = *_prog;
    size_t len = regerror(error, &prog->preg, NULL, 0);

    buf = malloc(len);
    if (buf)
	regerror(error, &prog->preg, buf, len);
#else
    if (_regerrorstr) {
	buf = strdup(_regerrorstr);
	_regerrorstr = NULL;
    } else {
	size_t len = 16;

	buf = malloc(len);
	if (buf)
	    snprintf(buf, sizeof(buf), "Error %d\n", error);
    }
#endif
    return buf;
}

#ifdef WANT_REGMAIN
int main(int argc, char **argv)
{
    REGEXP_T *prog;
    int rc, no;
    char *str = argv[1];
    char *re = argv[2];
    char *sub = argv[3];
    char dst[1024];

    rc = rpl_regcomp(&prog, re, 0);
    if (rc == 0)
	rc = rpl_regexec(&prog, str);
    if (rc == 0) {
	fprintf(stderr, "match\n");
#ifdef HAVE_REGEX
	if (prog->preg.re_nsub)
	    for (no = 0; no <= prog->preg.re_nsub; no++)
		fprintf(stderr, "[%d]:%2d-%2d %-.*s\n", no,
			prog->pmatch[no].rm_so, prog->pmatch[no].rm_eo,
			prog->pmatch[no].rm_eo - prog->pmatch[no].rm_so,
			str+prog->pmatch[no].rm_so);
#else
	for (no = 0; no <= NSUBEXP; no++)
	    if (prog->startp[no] && prog->endp[no])
		fprintf(stderr, "[%d]:%2ld-%2ld %-.*s\n", no,
			prog->startp[no] - str, prog->endp[no] - str,
			(int)(prog->endp[no] - prog->startp[no]),
			prog->startp[no]);
#endif
	rc = rpl_regsub(&prog, sub, dst, sizeof(dst));
    }
    if (rc == 0)
    	printf("%s\n", dst);
    else if (rc == REG_NOMATCH)
	fprintf(stderr, "nomatch\n");
    else {
	char *buf = rpl_regerror(rc, &prog);
	fprintf(stderr, "regerror: %s\n", buf);
	free(buf);
    }
    rpl_regfree(&prog);
    return rc;
}
#endif
