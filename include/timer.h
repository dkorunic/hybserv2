/*
 * timer.h
 * Copyright (C) 1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_timer_h
#define INCLUDED_timer_h

#ifndef INCLUDED_defs_h
#include "defs.h"        /* HAVE_PTHREADS */
#define INCLUDED_defs_h
#endif

/* timer.c prototypes */

#if defined HAVE_PTHREADS || defined HAVE_SOLARIS_THREADS

void *p_CheckSignals();
void *p_CheckTime();

#endif /* HAVE_PTHREADS || HAVE_SOLARIS_THREADS */

void DoTimer(time_t unixtime);

#endif /* INCLUDED_timer_h */
