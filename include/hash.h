/*
 * hash.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_hash_h
#define INCLUDED_hash_h

#include "stdinc.h"
#include "config.h"

#define HASHCLIENTS     8192  /* size of client hash table */
#define HASHCHANNELS    4096  /* size of channel hash table */
#define HASHSERVERS     16    /* size of server hash table */
#define NICKLIST_MAX    1024  /* size of NickServ hash table */
#define CHANLIST_MAX    1024  /* size of ChanServ hash table */
#define MEMOLIST_MAX    1024  /* size of MemoServ hash table */

struct Luser;
struct Channel;
struct Server;

typedef struct hashentry
{
	void *list; /* pointer to first element in the bucket */
}
aHashEntry;

struct Luser *FindClient(const char *);
void ClearHashes(int);
int HashUhost(char *);
int CloneMatch(struct Luser *, struct Luser *);
int IsClone(struct Luser *);
struct Luser *HashAddClient(struct Luser *, int);
int HashDelClient(struct Luser *, int);
struct Channel *FindChannel(const char *);
int HashAddChan(struct Channel *);
int HashDelChan(struct Channel *);
struct Server *FindServer(const char *);
int HashAddServer(struct Server *);
int HashDelServer(struct Server *);

#ifdef NICKSERVICES

unsigned int NSHashNick(const char *);

#ifdef CHANNELSERVICES
unsigned int CSHashChan(const char *);
#endif

#ifdef MEMOSERVICES
unsigned int MSHashMemo(const char *);
#endif

#endif /* NICKSERVICES */

extern aHashEntry cloneTable[HASHCLIENTS];

#ifdef STATSERVICES
extern aHashEntry hostTable[HASHCLIENTS];
#endif

#endif /* INCLUDED_hash_h */
