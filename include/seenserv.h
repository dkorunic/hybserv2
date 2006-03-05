/*
 * seenserv.h
 * Copyright (C) 2000 demond
 *
 */

#ifndef INCLUDED_seenserv_h
#define INCLUDED_seenserv_h

#include "stdinc.h"
#include "config.h"

#ifdef SEENSERVICES

struct Seen
{
	struct Seen *prev, *next, *seen;
	int type;                /* 1 - QUIT, 2 - NICK */
	char nick[NICKLEN + 1];
	char *userhost, *msg;
	time_t time;
};

typedef struct Seen aSeen;

void es_process(char *, char *);
void es_add(char *, char *, char *, char *, time_t, int);
void ss_join(struct Channel *);
void ss_part(struct Channel *);

extern int seenc;
extern aSeen *seenp, *seenb;

#endif /* SEENSERVICES */

#endif /* INCLUDED_seenserv_h */
