/*
 * nickserv.h
 * Copyright (C) 1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_nickserv_h
#define INCLUDED_nickserv_h

#ifndef INCLUDED_sys_time_h
#include <sys/time.h>        /* time_t */
#define INCLUDED_sys_time_h
#endif

#ifndef INCLUDED_client_h
#include "client.h"        /* struct aChannelPtr */
#define INCLUDED_client_h
#endif

#ifndef INCLUDED_config_h
#include "config.h"        /* NICKSERVICES, LINKED_NICKNAMES */
#define INCLUDED_config_h
#endif

#ifndef INCLUDED_hash_h
#include "hash.h"        /* NICKLIST_MAX */
#define INCLUDED_hash_h
#endif

#ifdef NICKSERVICES

/* NickServ flags */
#define NS_IDENTIFIED   0x00000001 /* nick has IDENTIFY'd */
#define NS_PROTECTED    0x00000002 /* kill to regain nick */
#define NS_NOEXPIRE     0x00000004 /* nickname which doesn't expire */
#define NS_AUTOMASK     0x00000008 /* auto-add new hostmasks */
#define NS_PRIVATE      0x00000010 /* nick won't show up in a LIST */
#define NS_COLLIDE      0x00000020 /* stole a nick - kill them */
#define NS_RELEASE      0x00000040 /* to release an enforced nick */
#define NS_FORBID       0x00000080 /* nick is forbidden */
#define NS_SECURE       0x00000100 /* nick is secure */
#define NS_DELETE       0x00000200 /* delete after a RELOAD */
#define NS_UNSECURE     0x00000400 /* don't need to IDENTIFY */
#define NS_MEMOSIGNON   0x00000800 /* tell about memos on signon */
#define NS_MEMONOTIFY   0x00001000 /* tell about new memos */
#define NS_MEMOS        0x00002000 /* allow others to send memos */
#define NS_HIDEALL      0x00004000 /* hide everything from INFO */
#define NS_HIDEEMAIL    0x00008000 /* hide email from INFO */
#define NS_HIDEURL      0x00010000 /* hide url from INFO */
#define NS_HIDEQUIT     0x00020000 /* hide quitmsg from INFO */
#define NS_HIDEADDR     0x00040000 /* hide last addy from INFO */
#define NS_KILLIMMED    0x00080000 /* kill immediately to regain nick */
#define NS_NOREGISTER   0x00100000 /* cannot register channels */
#define NS_NOCHANOPS    0x00200000 /* not allowed to be opped */
#define NS_NUMERIC      0x00400000 /* ignores 432 numeric */

struct NickHost
{
  struct NickHost *next;
  char *hostmask; /* hostmask for nickname */
};

#ifdef CHANNELSERVICES

/*
 * Each nickname has a list of pointers to channels they have
 * access on
 */
struct AccessChannel
{
  struct AccessChannel *next, *prev;
  struct ChanInfo *cptr;        /* pointer to channel */
  struct ChanAccess *accessptr; /* pointer to ChanAccess structure on cptr */
};

#endif /* CHANNELSERVICES */

/* structure for registered nicknames */
struct NickInfo
{
  struct NickInfo *next, *prev;
  char *nick;                /* registered nickname */
  char *password;            /* password */
  struct NickHost *hosts;    /* recognized hosts for this nick */
  time_t created;            /* timestamp when it was registered */
  time_t lastseen;           /* for expiration purposes */
  long flags;                /* nick flags */

  char *email;               /* email address */
  char *url;                 /* url */

  char *gsm;                 /* GSM number */
  char *phone;               /* Phone */
  char *UIN;                 /* ICQ UIN */

  char *lastu;               /* last seen username */
  char *lasth;               /* last seen hostname */
  char *lastqmsg;            /* last quit message */

  time_t collide_ts;         /* TS of when to collide them */

#ifdef RECORD_SPLIT_TS
  /*
   * if they split, record their TS, so if they rejoin, we can
   * check if their TS's match up and don't make them re-IDENTIFY
   */
  time_t split_ts;
  time_t whensplit;          /* for expiration purposes */
#endif /* RECORD_SPLIT_TS */

#ifdef LINKED_NICKNAMES
  /*
   * Pointer to next nick in the nickname link list
   */
  struct NickInfo *nextlink;

  /*
   * If this is the "hub" nickname for a list of linked nicknames,
   * master will be NULL. If this nickname is a leaf, master will
   * point to the "hub" nickname of this list
   */
  struct NickInfo *master;

  /*
   * Number of links in the list this nickname is a part of.
   * This is only non-zero if this nickname is a master
   */
  int numlinks;
#endif /* LINKED_NICKNAMES */

#ifdef CHANNELSERVICES
  /*
   * List of channels for which this nickname is a founder
   */
  struct aChannelPtr *FounderChannels;
  int fccnt; /* number of channels registered */

  /*
   * List of channels this nickname has access on
   */
  struct AccessChannel *AccessChannels;
  int accnt; /* number of channels they have access to */
#endif /* CHANNELSERVICES */

};

/*
 * Prototypes
 */

void ns_process(char *nick, char *command);
int CheckNick(char *nick);
void CheckOper(char *nick);
void ExpireNicknames(time_t unixtime);
void AddFounderChannelToNick(struct NickInfo **nptr,
                             struct ChanInfo *cptr);
void RemoveFounderChannelFromNick(struct NickInfo **nptr,
                                  struct ChanInfo *cptr);
struct AccessChannel *AddAccessChannel(struct NickInfo *nptr,
                                       struct ChanInfo *chanptr,
                                       struct ChanAccess *accessptr);
void DeleteAccessChannel(struct NickInfo *nptr,
                         struct AccessChannel *acptr);

struct NickInfo *FindNick(char *nick);
struct NickInfo *GetLink(char *nick);
void DelNick(struct NickInfo *nptr);
void DeleteNick(struct NickInfo *nptr);
int HasFlag(char *nick, int flag);

void collide(char *nick);
void release(char *nick);
void CollisionCheck(time_t unixtime);

#ifdef LINKED_NICKNAMES
int IsLinked(struct NickInfo *nick1, struct NickInfo *nick2);
#endif /* LINKED_NICKNAMES */

struct NickInfo *GetMaster(struct NickInfo *nptr);

/*
 * Extern declarations
 */

extern struct NickInfo *nicklist[NICKLIST_MAX];

#endif /* NICKSERVICES */

#endif /* INCLUDED_nickserv_h */
