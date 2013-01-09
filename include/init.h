/*
 * init.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_init_h
#define INCLUDED_init_h

#include "stdinc.h"
#include "config.h"

struct Luser;

RETSIGTYPE ProcessSignal(int);
void InitListenPorts(void);
void InitLists(void);
void InitSignals(void);
void PostCleanup(void);
void InitServs(struct Luser *);

extern int control_pipe;

#endif /* INCLUDED_init_h */
