/*
 */

#ifndef INCLUDED_sprintf_irc_h
#define INCLUDED_sprintf_irc_h

#include "stdinc.h"
#include "config.h"

extern int vsprintf_irc(char *, const char *, va_list);
extern int ircsprintf(char *, const char *, ...);

#endif /* INCLUDED_sprintf_irc_h */
