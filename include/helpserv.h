/*
 * helpserv.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_helpserv_h
#define INCLUDED_helpserv_h

#include "stdinc.h"
#include "config.h"

#ifdef HELPSERVICES
void hs_process(char *, char *);
#endif /* HELPSERVICES */

void GiveHelp(char *, char *, char *, int);

#endif /* INCLUDED_helpserv_h */
