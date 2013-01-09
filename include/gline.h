/*
 * gline.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_gline_h
#define INCLUDED_gline_h

#include "stdinc.h"
#include "config.h"

#ifdef ALLOW_GLINES

struct Luser;

struct Gline
{
	struct Gline *next, *prev;
	char *username;      /* username of gline */
	char *hostname;      /* hostname of gline */
	char *reason;        /* reason for Gline */
	char *who;           /* who made the gline */
	time_t expires;      /* 0 if its not a temp gline */
};

void AddGline(char *, char *, char *, long);
void DeleteGline(struct Gline *);
struct Gline *IsGline(char *, char *);
void CheckGlined(struct Luser *);
void ExpireGlines(time_t);

#ifdef HYBRID_GLINES
void ExecuteGline(char *, char *, char *);
#endif /* HYBRID_GLINES */

#ifdef HYBRID7_GLINES
void Execute7Gline(char *, char *, char *, time_t);
#endif /* HYBRID7_GLINES */

extern struct Gline *GlineList;

#endif /* ALLOW_GLINES */

#endif /* INCLUDED_gline_h */
