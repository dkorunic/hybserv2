/*
 * mystring.h
 * Hybserv2 Services by Hybserv2 team
 *
 * $Id$
 */

#ifndef INCLUDED_mystring_h
#define INCLUDED_mystring_h

#include "stdinc.h"
#include "config.h"

char *StrToupper(char *);
char *StrTolower(char *);
char *GetString(int, char **);
int SplitBuf(char *, char ***);

#endif /* INCLUDED_mystring_h */
