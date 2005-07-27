/*
 * $Id$ 
 */

#ifndef INCLUDED_sprintf_irc_h
#define INCLUDED_sprintf_irc_h

#include <stdarg.h>

/* Prototypes */

extern int vsprintf_irc(register char *str, register const char *format,
    va_list);
extern int ircsprintf(register char *str, register const char *format, ...);

#endif /* INCLUDED_sprintf_irc_h */
