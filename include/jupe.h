/*
 * jupe.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_jupe_h
#define INCLUDED_jupe_h

#include "stdinc.h"
#include "config.h"

#ifdef ALLOW_JUPES

#ifdef JUPEVOTES
struct JupeVote
{
	struct JupeVote *next, *prev;
	char *name;            /* name of juped server */
	char *who[JUPEVOTES];  /* array of people who've voted */
	int count;             /* how many times it's been voted */
	time_t lasttime;       /* unixtime that it was last voted for */
};
#endif /* JUPEVOTES */

struct Jupe
{
	struct Jupe *next, *prev;
	char *name;        /* name of juped server/nick */
	char *reason;      /* reason it's juped (if server) */
	char *who;         /* who made the jupe */
	int isnick;        /* 1 if juped nick, 0 if server */
};

void AddJupe(char *, char *, char *);
void DeleteJupe(struct Jupe *);
void CheckJupes(void);
void DoJupeSquit(char *, char *, char *);
void FakeServer(char *, char *);
int CheckJuped(char *);
struct Jupe *IsJupe(char *);
void InitJupes(void);

#ifdef JUPEVOTES
int AddVote(char *, char *);
int CountVotes(char *);
void DeleteVote(char *);
void ExpireVotes(time_t);
#endif /* JUPEVOTES */

extern struct Jupe *JupeList;

#ifdef JUPEVOTES
extern struct JupeVote *VoteList;
#endif /* JUPEVOTES */

#endif /* ALLOW_JUPES */

#endif /* INCLUDED_jupe_h */
