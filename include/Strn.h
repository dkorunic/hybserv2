/*
 * Strn.h
 * Copyright (C) 1998-1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_Strn_h
#define INCLUDED_Strn_h

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>        /* size_t */
#define INCLUDED_sys_types_h
#endif

#ifndef INCLUDED_stdarg_h
#include <stdarg.h>          /* va_list */
#define INCLUDED_stdarg_h
#endif

/*
 * Prototypes
 */

char *Strncpy(char *dest, const char *source, const size_t bytes);
int vSnprintf(register char *dest, register const size_t bytes,
              register char *format, register va_list args);
int vSprintf(register char *dest, register char *format,
             register va_list args);
int Snprintf(register char *dest, register const size_t bytes,
             register char *format, ...);
int Sprintf(register char *dest, register char *format, ...);

#endif /* INCLUDED_Strn_h */
