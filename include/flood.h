/*
 * flood.h
 * Copyright (C) 1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_flood_h
#define INCLUDED_flood_h

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>         /* time_t */
#define INCLUDED_sys_types_h
#endif

struct Luser;
struct Channel;

/*
 * Function prototypes
 */

int FloodCheck(struct Channel *chptr, struct Luser *lptr,
               struct Luser *servptr, int kick);
int IsFlood();

/*
 * External declarations
 */

extern time_t     FloodTable[2];
extern int        FloodHits;
extern int        ActiveFlood;

#endif /* INCLUDED_flood_h */
