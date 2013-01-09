/*
 * mystring.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_mystring_h
#define INCLUDED_mystring_h

#include "stdinc.h"
#include "config.h"

char *StrToupper(char *);
char *StrTolower(char *);
char *GetString(int, char **);
int SplitBuf(char *, char ***);

#ifndef HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_STRLCAT
size_t strlcat(char *, const char *, size_t);
#endif

#endif /* INCLUDED_mystring_h */
