/*
 * show.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_show_h
#define INCLUDED_show_h

#include "stdinc.h"
#include "config.h"

#define  SHOW_HELP        0x000001 /* -h/-? */
#define  SHOW_TOTALCOUNT  0x000002 /* -n */
#define  SHOW_FILE        0x000004 /* -f file */
#define  SHOW_DETAIL      0x000008 /* display specific nickname(s) */

extern char *Filename;
extern int ncidx;

#ifdef NICKSERVICES

void ShowNicknames(int, char **);

#ifdef CHANNELSERVICES
void ShowChannels(int, char **);
#endif /* CHANNELSERVICES */

#endif /* NICKSERVICES */

#endif /* INCLUDED_show_h */
