/*
 * flood.h
 * Hybserv2 Services by Hybserv2 team
 *
 * $Id$
 */

#ifndef INCLUDED_flood_h
#define INCLUDED_flood_h

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>         /* time_t */
#define INCLUDED_sys_types_h
#endif

#ifndef INCLUDED_hybdefs_h
#include "hybdefs.h"		/* USERLEN, HOSTLEN */
#define INCLUDE_hybdefs_h
#endif

#ifndef INCLUDED_config_h
#include "config.h"		/* ADVFLOOD */
#define INCLUDED_config_h
#endif

struct Luser;
struct Channel;

#ifdef ADVFLOOD
struct connectInfo
{
  char user[USERLEN];
  char host[HOSTLEN]; 
  time_t last;
  int frequency;
};
#endif /* ADVFLOOD */

/*
 * Function prototypes
 */

int FloodCheck(struct Channel *chptr, struct Luser *lptr,
               struct Luser *servptr, int kick);
int IsFlood();

#ifdef ADVFLOOD
void updateConnectTable(char *user, char *host);
#endif /* ADVFLOOD */

/*
 * External declarations
 */

extern time_t     FloodTable[2];
extern int        FloodHits;
extern int        ActiveFlood;

#endif /* INCLUDED_flood_h */
