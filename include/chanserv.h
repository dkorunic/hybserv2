/*
 * chanserv.h
 * Copyright (C) 1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_chanserv_h
#define INCLUDED_chanserv_h

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>        /* time_t */
#define INCLUDED_sys_types_h
#endif

#ifndef INCLUDED_config_h
#include "config.h"       /* CHANNELSERVICES, HYBRID_ONLY */
#define INCLUDED_config_h
#endif

#ifndef INCLUDED_hash_h
#include "hash.h"         /* CHANLIST_MAX */
#define INCLUDED_hash_h
#endif

#ifdef CHANNELSERVICES

struct Luser;
struct Channel;

/* ChanServ flags */
#define CS_PRIVATE      0x00000001 /* channel won't show up in LIST */
#define CS_TOPICLOCK    0x00000002 /* must be changed via SET TOPIC */
#define CS_SECURE       0x00000004 /* channel is secure */
#define CS_SECUREOPS    0x00000008 /* only aop/sop/founders can be opped */
#define CS_SUSPENDED    0x00000010 /* channel is suspended */
#define CS_FORBID       0x00000020 /* channel is forbidden */
#define CS_RESTRICTED   0x00000040 /* channel is restricted */
#define CS_FORGET       0x00000080 /* channel is forgotten */
#define CS_DELETE       0x00000100 /* delete after a RELOAD */
#define CS_NOEXPIRE     0x00000200 /* never expires */
#define CS_GUARD        0x00000400 /* have ChanServ join the channel */
#define CS_SPLITOPS     0x00000800 /* let people keep ops from splits */
#define CS_VERBOSE      0x00001000 /* notify chanops for access changes */


/* access_lvl[] indices */
/* We will happily FUBAR old databases by changing this. However, it had
 * to be done -kre && Janos
 * PS, I have added upgrade-chan target in Makefile for fixing this
 * properly - it relies on awk and DefaultAccess as well as ALVL in
 * chan.db -kre */
#ifdef HYBRID7
# define CA_AUTODEOP     0
# define CA_AUTOVOICE    1
# define CA_CMDVOICE     2
# define CA_ACCESS       3
# define CA_CMDINVITE    4
# define CA_AUTOHALFOP   5
# define CA_CMDHALFOP    6
# define CA_AUTOOP       7
# define CA_CMDOP        8
# define CA_CMDUNBAN     9
# define CA_AKICK        10
# define CA_CMDCLEAR     11
# define CA_SET          12
# define CA_SUPEROP      13
# define CA_FOUNDER      14
# define CA_SIZE         15 /* number of indices */
#else
# define CA_AUTODEOP     0
# define CA_AUTOVOICE    1
# define CA_CMDVOICE     2
# define CA_ACCESS       3
# define CA_CMDINVITE    4
# define CA_AUTOOP       5
# define CA_CMDOP        6
# define CA_CMDUNBAN     7
# define CA_AKICK        8
# define CA_CMDCLEAR     9
# define CA_SET          10
# define CA_SUPEROP      11
# define CA_FOUNDER      12
# define CA_SIZE         13 /* number of indices */
#endif /* HYBRID7 */

struct ChanAccess
{
  struct ChanAccess *next, *prev;
  int level;               /* privs mask has */
  char *hostmask;          /* hostmask that has access */
  struct NickInfo *nptr;   /* nickname */

  /*
   * pointer to corresponding AccessChannel structure on nptr's
   * AccessChannels list - this way, when we delete a ChanAccess
   * structure, we don't have to loop through all of nptr's
   * access channels to find the corresponding pointer.
   */
  struct AccessChannel *acptr;
};

struct AutoKick
{
  struct AutoKick *next;
  char *hostmask; /* mask to autokick */
  char *reason;   /* reason for autokick */
};

struct ChanInfo
{
  struct ChanInfo *next, *prev;
  char *name;                   /* channel name */
  char *founder;                /* founder nick (must be registered) */
  char *successor;              /* successor nick (must be registered) */
  char *password;               /* founder password */
  char *topic;                  /* NULL if no topic lock */
  long limit;                   /* 0 if no limit */
  char *key;                    /* NULL if no key */
  int modes_on,                 /* modes to enforce */
      modes_off;                /* modes to enforce off */
  struct ChanAccess *access;    /* access list */
  int akickcnt;                 /* number of akicks */
  struct AutoKick *akick;       /* autokick list */

  char *entrymsg;               /* msg to send to users upon entry to channel */
  char *email;                  /* email address of channel */
  char *url;                    /* url of channel */

  /* list of users who have founder access */
  struct f_users
  {
    struct f_users *next;
    struct Luser *lptr;
  } *founders;

  time_t created;               /* timestamp when it was registered */
  time_t lastused;              /* for expiration purposes */
  int *access_lvl;              /* customized access levels for this channel */
  long flags;                   /* channel flags */
};

/*
 * Prototypes
 */

void cs_process(char *nick, char *command);
void cs_join(struct ChanInfo *cptr);
void cs_join_ts_minus_1(struct ChanInfo *cptr);
void cs_part(struct Channel *chptr);
void cs_CheckOp(struct Channel *chptr, struct ChanInfo *cptr,
                char *nick);
void cs_CheckJoin(struct Channel *chptr, struct ChanInfo *cptr,
                  char *nick);
void cs_CheckSjoin(struct Channel *chptr, struct ChanInfo *cptr,
                   int nickcnt, char **nicks, int newchan);
void cs_CheckModes(struct Luser *source, struct ChanInfo *cptr,
                   int isminus, int mode, struct Luser *lptr);
void cs_CheckTopic(char *who, char *channel);
int cs_ShouldBeOnChan(struct ChanInfo *cptr);
void cs_RejoinChannels();
void PromoteSuccessor(struct ChanInfo *cptr);
void ExpireChannels(time_t unixtime);

#ifndef HYBRID_ONLY
void CheckEmptyChans();
#endif /* !HYBRID_ONLY */

struct ChanInfo *FindChan(char *channel);
void DeleteChan(struct ChanInfo *cptr);
void RemFounder(struct Luser *lptr, struct ChanInfo *cptr);
void DeleteAccess(struct ChanInfo *cptr, struct ChanAccess *ptr);
int HasAccess(struct ChanInfo *cptr, struct Luser *lptr, int level);
void SetDefaultALVL(struct ChanInfo *cptr);

/*
 * Extern declarations
 */

extern struct ChanInfo *chanlist[CHANLIST_MAX];
extern struct Channel *ChannelList;
extern long MaxTSDelta;

#endif /* CHANNELSERVICES */

#endif /* INCLUDED_chanserv_h */
