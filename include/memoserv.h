/*
 * memoserv.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_memoserv_h
#define INCLUDED_memoserv_h

#include "stdinc.h"
#include "config.h"
#include "hash.h"

#if defined NICKSERVICES && defined MEMOSERVICES

/* MemoServ flags */
#define MS_READ         0x00000001 /* memo has been read */
#define MS_DELETE       0x00000002 /* marked for deletion */
#define MS_RDELETE      0x00000004 /* delete after a RELOAD */
#define MS_REPLIED      0x00000008 /* has been replied */

struct Luser;
struct NickInfo;

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

void ms_process(char *, char *);
int StoreMemo(char *, char *, struct Luser *);
void DeleteMemoList(struct MemoInfo *);
int PurgeMemos(struct MemoInfo *);
void ExpireMemos(time_t);
void PurgeCheck(void);
void MoveMemos(struct NickInfo *, struct NickInfo *);
struct MemoInfo *FindMemoList(char *);

extern struct MemoInfo *memolist[MEMOLIST_MAX];

#endif /* NICKSERVICES && MEMOSERVICES */

#endif /* INCLUDED_memoserv_h */
