/* Based on setproctitle.c from openssh-5.6p1 */
/* Based on conf.c from UCB sendmail 8.8.8 */

/*
 * Copyright 2003 Damien Miller
 * Copyright (c) 1983, 1995-1997 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "setproctitle.h"
#include "alloc.h"

#if defined(__APPLE__)
# ifdef HAVE_CRT_EXTERNS_H
#  include <crt_externs.h>
#  undef environ
#  define environ (*_NSGetEnviron())
# endif
#endif

#define SPT_PADCHAR	'\0'

static char *original_args = NULL;
static char *original_argline = NULL;
static char *argv_start = NULL;
static char *env_start = NULL;
static size_t argv_env_len = 0;
static size_t argv_len = 0;
static size_t procline_len = 0;


#ifndef strlcpy
#define strlcpy _strlcpy
#endif

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 *
 * FreeBSD
 */
size_t _strlcpy(char * __restrict dst, const char * __restrict src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		/* NUL-terminate dst */
		if (siz != 0)
			*d = '\0';
		while (*s++);
	}

	/* count does not include NUL */
	return (s - src - 1);
}



void
init_setproctitle(int argc, char **_argv[])
{
	extern char **environ;
	char **argv = (char **)*_argv;
	char *lastargv = NULL;
	char *lastenvp = NULL;
	char **envp = environ;
	int i;
	unsigned j;

	env_start = environ[0];

	if (argc == 0 || argv[0] == NULL)
		return;

	/* Fail if we can't allocate room for the new environment */
	for (i = 0; envp[i] != NULL; i++)
		;
	environ = calloc(i + 1, sizeof(*environ));
	if (environ == NULL) {
		environ = envp;	/* put it back */
		return;
	}

	/*
	 * Find the last argv string or environment variable within
	 * our process memory area.
	 */
	for (i = 0; i < argc; i++) {
		if (lastargv == NULL || lastargv + 1 == argv[i])
			lastargv = argv[i] + strlen(argv[i]);
	}
	lastenvp = lastargv;
	for (i = 0; envp[i] != NULL; i++) {
		if (lastenvp + 1 == envp[i])
			lastenvp = envp[i] + strlen(envp[i]);
	}

	argv_start = argv[0];
	argv_len = lastargv - argv[0];
	argv_env_len = lastenvp - argv[0];

	/* keep a copy of the original args */
	original_args = memdupz(argv_start, argv_len);
	if (original_args == NULL) {
		free(environ);
		environ = envp;
		return;
	}

	original_argline = memdupz(argv_start, argv_len);
	if (original_argline == NULL) {
		free(original_args);
		free(environ);
		environ = envp;
		return;
	}
	for (j = 0; j < argv_len; j++) {
		if (original_argline[j] == '\0')
			original_argline[j] = ' ';
	}

	for (i = 0; envp[i] != NULL; i++)
		environ[i] = strdup(envp[i]);
	for (i = 0; i < argc; i++)
		argv[i] = &original_args[argv[i]-argv_start];
	*_argv = argv;
}

#if 0
char *
getproctitle()
{
	return original_argline;
}
#endif /* unused */

void
setproctitle(const char *fmt, ...)
{
	va_list ap;
	char ptitle[1024];
	size_t argvlen, len;

	if (argv_env_len <= 0)
		return;

	va_start(ap, fmt);
	if (fmt != NULL) {
		vsnprintf(ptitle, sizeof(ptitle) , fmt, ap);
	}
	va_end(ap);

	procline_len = len = strlcpy(argv_start, ptitle, argv_env_len);
	argvlen = len > argv_len ? argv_env_len : argv_len;
	for(; len < argvlen; len++)
		argv_start[len] = SPT_PADCHAR;
}

#if 0
void
setproctitle_plus(const char *fmt, ...)
{
	va_list ap;
	char ptitle[1024];

	if (argv_env_len <= 0)
		return;

	va_start(ap, fmt);
	if (fmt != NULL) {
		vsnprintf(ptitle, sizeof(ptitle) , fmt, ap);
	}
	va_end(ap);

	setproctitle("%s%s", original_argline, ptitle);
}

void
setenvironstr(const char *fmt, ...)
{
#ifdef __linux__
	va_list ap;
	char ptitle[16384];
	char *pos;
	size_t buffer_len;

	if (argv_env_len <= 0)
		return;

	va_start(ap, fmt);
	if (fmt != NULL) {
		vsnprintf(ptitle, sizeof(ptitle) , fmt, ap);
	}
	va_end(ap);

	pos = argv_start + procline_len + 1;
	if (pos < env_start) {
		pos = env_start;
		buffer_len = argv_env_len - argv_len - 1;
	} else
		buffer_len = argv_env_len - procline_len - 1;

	strlcpy(pos, ptitle, buffer_len);
#endif
}
#endif /* unused */
