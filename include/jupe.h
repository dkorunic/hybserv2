/*
 * jupe.h
 * HybServ2 Services by HybServ2 team
 *
 * $Id$
 */

#ifndef INCLUDED_jupe_h
#define INCLUDED_jupe_h

#ifndef INCLUDED_config_h
#include "config.h"        /* ALLOW_JUPES */
#define INCLUDED_config_h
#endif

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>        /* time_t */
#define INCLUDED_sys_types_h
#endif

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
#endif

struct Jupe
{
  struct Jupe *next, *prev;
  char *name;        /* name of juped server/nick */
  char *reason;      /* reason it's juped (if server) */
  char *who;         /* who made the jupe */
  int isnick;        /* 1 if juped nick, 0 if server */
};

/*
 * Prototypes
 */

void AddJupe(char *, char *, char *);
void DeleteJupe(struct Jupe *);

void CheckJupes();
void DoJupeSquit(char *, char *, char *);
int CheckJuped(char *);
struct Jupe *IsJupe(char *);
void InitJupes();

#ifdef JUPEVOTES
int AddVote(char *, char *);
int CountVotes(char *);
void DeleteVote(char *);
void ExpireVotes(time_t);
#endif

/*
 * External declarations
 */

extern struct Jupe       *JupeList;
#ifdef JUPEVOTES
extern struct JupeVote   *VoteList;
#endif

#endif /* ALLOW_JUPES */

#endif /* INCLUDED_jupe_h */
