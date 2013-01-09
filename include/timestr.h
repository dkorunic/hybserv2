/*
 * timestr.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_timestr_h
#define INCLUDED_timestr_h


#include "stdinc.h"
#include "config.h"

char *timeago(time_t, int);
long timestr(char *);
struct timeval *GetTime(struct timeval *);
long GetTZOffset(time_t);

#endif /* INCLUDED_timestr_h */
