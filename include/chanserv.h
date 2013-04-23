/*
 * chanserv.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_chanserv_h
#define INCLUDED_chanserv_h

#include "stdinc.h"
#include "config.h"
#include "hash.h"

#ifdef CHANNELSERVICES

/* ChanServ flags */
#define CS_PRIVATE      0x00000001 /* channel won't show up in LIST */
#define CS_TOPICLOCK    0x00000002 /* must be changed via SET TOPIC */
#define CS_SECURE       0x00000004 /* channel is secure */
#define CS_SECUREOPS    0x00000008 /* only aop/sop/founders can be opped */
#define CS_SUSPENDED    0x00000010 /* channel is suspended - NOT IN USE */
#define CS_FORBID       0x00000020 /* channel is forbidden */
#define CS_RESTRICTED   0x00000040 /* channel is restricted */
#define CS_FORGET       0x00000080 /* channel is forgotten */
#define CS_DELETE       0x00000100 /* delete after a RELOAD */
#define CS_NOEXPIRE     0x00000200 /* never expires */
#define CS_GUARD        0x00000400 /* have ChanServ join the channel */
#define CS_SPLITOPS     0x00000800 /* let people keep ops from splits */
#define CS_VERBOSE      0x00001000 /* notify chanops for access changes */
#define CS_EXPIREBANS   0x00002000 /* expire bans after EXPIRETIME */
#define CS_SEENSERV     0x00004000 /* channel is served by SeenServ */
#ifdef PUBCOMMANDS
# define CS_PUBCOMMANDS 0x01000000 /* allow public commands in channel */
#endif

/* access_lvl[] indices */
#if defined HYBRID7 && defined HYBRID7_HALFOPS
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
#endif /* HYBRID7 && HYBRID7_HALFOPS */

struct Luser;
struct Channel;

struct ChanAccess
{
	struct ChanAccess *next, *prev;
	int level;               /* privs mask has */
	char *hostmask;          /* hostmask that has access */
	struct NickInfo *nptr;   /* nickname */

	/*
	 * pointer to corresponding AccessChannel structure on nptr's
	 * AccessChannels list - this way, when we delete a ChanAccess
	 * structure, we don't have to loop through all of nptr's access
	 * channels to find the corresponding pointer.
	 */
	struct AccessChannel *acptr;
	time_t created; /* time when this entry was added */
	time_t last_used; /* last time the person joined the channel while
		                       identified */
	char *added_by;          /* who added this entry */
};

struct AutoKick
{
	struct AutoKick *next;
	char *hostmask; /* mask to autokick */
	char *reason;   /* reason for autokick */
	long expires; /* AKICK expiration */
};

struct ChanInfo
{
	struct ChanInfo *next, *prev;
	char *name;                   /* channel name */
	char *founder;                /* founder nick (must be registered) */
	time_t last_founder_active;   /* last time the founder joined/left */
	char *successor;              /* successor nick (must be registered) */
	time_t last_successor_active; /* last time the founder joined/left */
	char *password;               /* founder password */
	char *topic;                  /* NULL if no topic lock */
	long limit;                   /* 0 if no limit */
	char *key;                    /* NULL if no key */
#ifdef DANCER

	char *forward;                /* NULL if no forward target */
#endif /* DANCER */

	int modes_on;                 /* modes to enforce */
	int modes_off;                /* modes to enforce off */
	struct ChanAccess *access;    /* access list */
	int akickcnt;                 /* number of akicks */
	struct AutoKick *akick;       /* autokick list */
	long expirebans;              /* bans expiration time */
	char *entrymsg;               /* msg to send to users upon entry to
									 channel */
	char *email;                  /* email address of channel */
	char *url;                    /* url of channel */
	char *comment;                /* channel comment line */

	char *forbidby;               /* who did a forbid on channel */
	char *forbidreason;           /* and an optional reason */

	/* list of users who have founder access */
	struct f_users
	{
		struct f_users *next;
		struct Luser *lptr;
	}
	*founders;

	time_t created;               /* timestamp when it was registered */
	time_t lastused;              /* for expiration purposes */
	int *access_lvl;              /* customized access levels for this channel */
	long flags;                   /* channel flags */
};

void cs_process(char *, char *);
void cs_join(struct ChanInfo *);
void cs_join_ts_minus_1(struct ChanInfo *);
void cs_part(struct Channel *);
void cs_CheckOp(struct Channel *, struct ChanInfo *, char *);
void cs_CheckJoin(struct Channel *, struct ChanInfo *, char *);
void cs_CheckSjoin(struct Channel *, struct ChanInfo *, int, char **,
                   int);
void cs_CheckModes(struct Luser *, struct ChanInfo *, int, int, struct
                   Luser *);
void cs_CheckTopic(char *, char *);
int cs_ShouldBeOnChan(struct ChanInfo *);
void cs_RejoinChannels(void);
void PromoteSuccessor(struct ChanInfo *);
void ExpireChannels(time_t);
void ExpireBans(time_t);
void ExpireAkicks(time_t);

#ifndef HYBRID_ONLY
void CheckEmptyChans();
#endif /* !HYBRID_ONLY */

struct ChanInfo *FindChan(char *);
void DeleteChan(struct ChanInfo *);
void RemFounder(struct Luser *, struct ChanInfo *);
void DeleteAccess(struct ChanInfo *, struct ChanAccess *);
int HasAccess(struct ChanInfo *, struct Luser *, int);
void SetDefaultALVL(struct ChanInfo *);
void c_clear_all(struct Luser *, struct NickInfo *, int, char **);
int IsFounder(struct Luser *, struct ChanInfo *);
int AddAccess(struct ChanInfo *, struct Luser *, char *,
              struct NickInfo *, int, time_t, time_t, char *);

extern struct ChanInfo *chanlist[CHANLIST_MAX];
extern struct Channel *ChannelList;

#endif /* CHANNELSERVICES */

#endif /* INCLUDED_chanserv_h */
