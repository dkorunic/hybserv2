/*
 * nickserv.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_nickserv_h
#define INCLUDED_nickserv_h

#include "stdinc.h"
#include "config.h"
#include "client.h"
#include "hash.h"

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
#define NS_PRIVMSG      0x00800000 /* PRIVMSG or NOTICE */
#define NS_HIDEHOST     0x01000000 /* hide userhost */

/* these are yet to be coded, probably as separate set of flags */
#if 0
# define NS_NOHALFOPS    0x01000000 /* can't halfop */
# define NS_NOVOICE      0x02000000 /* can't +v */
# define NS_NOACCESS     0x04000000 /* can't get access to chans */
#endif
#define NS_NOLINK       0x08000000 /* can't link nicknames */

#define NS_NEVEROP     0x10000000 /* Never op/hop/voice user on join */
#define NS_NOADD       0x20000000 /* nickname impossible to add to channel/etc. lists */

struct ChanInfo;
struct ChanAccess;

struct NickHost
{
	struct NickHost *next;
	char *hostmask; /* hostmask for nickname */
};

#ifdef CHANNELSERVICES

/* Each nickname has a list of pointers to channels they have
 * access on */
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
	char *phrase;              /* password recovery phrase */
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
	char *forbidby;            /* who forbidded the nickname */
	char *forbidreason;        /* why was it forbidden [optional] */
	time_t collide_ts;         /* TS of when to collide them */

#ifdef RECORD_RESTART_TS
	/* Record TS of nicks so that when services are restarted
	 * they won't have to reidentify */
	time_t nick_ts;
    char *last_server;
#endif

#ifdef LINKED_NICKNAMES
	/* Pointer to next nick in the nickname link list */
	struct NickInfo *nextlink;

	/* If this is the "hub" nickname for a list of linked nicknames,
	 * master will be NULL. If this nickname is a leaf, master will
	 * point to the "hub" nickname of this list */
	struct NickInfo *master;

	/* Number of links in the list this nickname is a part of.
	 * This is only non-zero if this nickname is a master */
	int numlinks;
#endif /* LINKED_NICKNAMES */

#ifdef CHANNELSERVICES
	/* List of channels for which this nickname is a founder */
	struct aChannelPtr *FounderChannels;
	int fccnt; /* number of channels registered */

	/* List of channels this nickname has access on */
	struct AccessChannel *AccessChannels;
	int accnt; /* number of channels they have access to */
#endif /* CHANNELSERVICES */
};

void ns_process(const char *, char *);
int CheckNick(char *);
void CheckOper(struct Luser *);
void ExpireNicknames(time_t);
void AddFounderChannelToNick(struct NickInfo **, struct ChanInfo *);
void RemoveFounderChannelFromNick(struct NickInfo **, struct ChanInfo *);
struct AccessChannel *AddAccessChannel(struct NickInfo *, struct ChanInfo
			                                       *, struct ChanAccess *);
void DeleteAccessChannel(struct NickInfo *, struct AccessChannel *);
struct NickInfo *FindNick(char *);
struct NickInfo *GetLink(char *);
void DelNick(struct NickInfo *);
void DeleteNick(struct NickInfo *);
int HasFlag(char *, int);
void collide(char *, int);
void release(char *);
void CollisionCheck(time_t);

#ifdef LINKED_NICKNAMES
int IsLinked(struct NickInfo *, struct NickInfo *);
#endif /* LINKED_NICKNAMES */

struct NickInfo *GetMaster(struct NickInfo *);

extern struct NickInfo *nicklist[NICKLIST_MAX];

#endif /* NICKSERVICES */

#endif /* INCLUDED_nickserv_h */
