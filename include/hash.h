/*
 * hash.h
 * HybServ2 Services by HybServ2 team
 *
 * $Id$
 */

#ifndef INCLUDED_hash_h
#define INCLUDED_hash_h

#ifndef INCLUDED_config_h
#include "config.h"        /* NICKSERVICES ... */
#define INCLUDED_config_h
#endif

struct Luser;
struct Channel;
struct Server;

/*
 * size of client hash table
 */
#define HASHCLIENTS     8192

/*
 * size of channel hash table
 */
#define HASHCHANNELS    4096

/*
 * size of server hash table
 */
#define HASHSERVERS     16

/*
 * size of NickServ hash table
 */
#define NICKLIST_MAX    256

/*
 * size of ChanServ hash table
 */
#define CHANLIST_MAX    256

/*
 * size of MemoServ hash table
 */
#define MEMOLIST_MAX    256

typedef  struct hashentry
{
  void *list; /* pointer to first element in the bucket */
} aHashEntry;

/*
 * Function prototypes
 */

struct Luser *FindClient(const char *name);
void ClearHashes(int rehash);
int HashUhost(char *userhost);
int CloneMatch(struct Luser *lptr1, struct Luser *lptr2);
int IsClone(struct Luser *lptr);
struct Luser *HashAddClient(struct Luser *lptr, int nickchange);
int HashDelClient(struct Luser *lptr, int nickchange);
struct Channel *FindChannel(const char *name);
int HashAddChan(struct Channel *chptr);
int HashDelChan(struct Channel *chptr);
struct Server *FindServer(const char *name);
int HashAddServer(struct Server *sptr);
int HashDelServer(struct Server *sptr);

#ifdef NICKSERVICES

unsigned int NSHashNick(const char *);

#ifdef CHANNELSERVICES
unsigned int CSHashChan(const char *);
#endif

#ifdef MEMOSERVICES
unsigned int MSHashMemo(const char *);
#endif

#endif /* NICKSERVICES */

/*
 * Extern declarations
 */

extern aHashEntry cloneTable[HASHCLIENTS];

#ifdef STATSERVICES
extern aHashEntry hostTable[HASHCLIENTS];
#endif

#endif /* INCLUDED_hash_h */
