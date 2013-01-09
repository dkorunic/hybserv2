/*
 * flood.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_flood_h
#define INCLUDED_flood_h

#include "stdinc.h"
#include "hybdefs.h"
#include "config.h"

struct Luser;
struct Channel;

#ifdef ADVFLOOD
struct connectInfo
{
	char user[USERLEN + 1];
	char host[HOSTLEN + 1];
	time_t last;
	int frequency;
};
#endif /* ADVFLOOD */

int FloodCheck(struct Channel *, struct Luser *, struct Luser *, int);
int IsFlood(void);
#ifdef ADVFLOOD
void updateConnectTable(char *, char *);
#endif /* ADVFLOOD */

extern time_t FloodTable[2];
extern int FloodHits;
extern int ActiveFlood;

#endif /* INCLUDED_flood_h */
