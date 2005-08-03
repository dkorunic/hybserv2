/*
 * $Id$ 
 */

#ifndef INCLUDED_sprintf_irc_h
#define INCLUDED_sprintf_irc_h

#include "stdinc.h"
#include "config.h"

extern int vsprintf_irc(register char *, register const char *, va_list);
extern int ircsprintf(register char *, register const char *, ...);

#endif /* INCLUDED_sprintf_irc_h */
