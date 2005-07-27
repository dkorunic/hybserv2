/*
 * init.h
 * Hybserv2 Services by Hybserv2 team
 *
 * $Id$
 */

#ifndef INCLUDED_init_h
#define INCLUDED_init_h

struct Luser;

/*
 * Prototypes
 */

void ProcessSignal(int signal);
void InitListenPorts();
void InitLists();
void InitSignals();
void PostCleanup();
void InitServs(struct Luser *servptr);

extern int control_pipe;

#endif /* INCLUDED_init_h */
