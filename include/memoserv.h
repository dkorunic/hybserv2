/*
 * memoserv.h
 * HybServ2 Services by HybServ2 team
 *
 * $Id$
 */

#ifndef INCLUDED_memoserv_h
#define INCLUDED_memoserv_h

#ifndef INCLUDED_config_h
#include "config.h"        /* NICKSERVICES, MEMOSERVICES */
#define INCLUDED_config_h
#endif

#ifndef INCLUDED_hash_h
#include "hash.h"        /* MEMOLIST_MAX */
#define INCLUDED_hash_h
#endif

#if defined(NICKSERVICES) && defined(MEMOSERVICES)

struct NickInfo;

/* MemoServ flags */
#define MS_READ         0x00000001 /* memo has been read */
#define MS_DELETE       0x00000002 /* marked for deletion */
#define MS_RDELETE      0x00000004 /* delete after a RELOAD */
#define MS_REPLIED      0x00000008 /* has been replied */

struct Memo
{
  struct Memo *next, *prev;
  char *sender;
  long index;     /* index number */
  time_t sent;    /* time it was sent */
  char *text;
  long flags;
};

struct MemoInfo
{
  struct MemoInfo *next, *prev;
  char *name;            /* who the memo was sent to */
  long memocnt;          /* number of memos */
  long newmemos;         /* number of unread memos */
  struct Memo *memos;    /* the actual memos */
  struct Memo *lastmemo; /* ptr to last memo in list */
  long flags;
};

/*
 * Prototypes
 */

void ms_process(char *nick, char *command);
int StoreMemo(char *target, char *text, struct Luser *lptr);
void DeleteMemoList(struct MemoInfo *mptr);
int PurgeMemos(struct MemoInfo *mptr);
void ExpireMemos(time_t unixtime);
void PurgeCheck();
void MoveMemos(struct NickInfo *source, struct NickInfo *dest);
struct MemoInfo *FindMemoList(char *nick);

/*
 * Extern declarations
 */

extern struct MemoInfo *memolist[MEMOLIST_MAX];

#endif /* defined(NICKSERVICES) && defined(MEMOSERVICES) */

#endif /* INCLUDED_memoserv_h */
