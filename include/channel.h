/*
 * channel.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_channel_h
#define INCLUDED_channel_h

#include "stdinc.h"
#include "config.h"
#include "hybdefs.h"

/* Channel flags */
#define CH_VOICED       0x000001 /* user is voiced */
#define CH_OPPED        0x000002 /* user is opped */
#define MODE_O          0x000004 /* someone was +o'd */
#define MODE_V          0x000008 /* someone was +v'd */
#define MODE_L          0x000010 /* channel is +l */
#define MODE_K          0x000020 /* channel is +k */
#define MODE_S          0x000040 /* channel is +s */
#define MODE_P          0x000080 /* channel is +p */
#define MODE_N          0x000100 /* channel is +n */
#define MODE_T          0x000200 /* channel is +t */
#define MODE_M          0x000400 /* channel is +m */
#define MODE_I          0x000800 /* channel is +i */

#ifdef DANCER
# define MODE_C         0x001000 /* channel is +c */
# define MODE_F         0x002000 /* channel is +f */
#endif /* DANCER */

#ifdef HYBRID7_HALFOPS
# define CH_HOPPED      0x001000 /* user is halfopped - Janos */
# define MODE_H         0x002000 /* someone was +h - Janos */
#endif /* HYBRID7_HALFOPS */

struct Luser;

#ifdef GECOSBANS
struct ChannelGecosBan
{
	struct ChannelGecosBan *next, *prev;
	char *who;     /* who set the ban */
	time_t when;   /* when the ban was made */
	char *mask;    /* hostmask of the ban */
};
#endif /* GECOSBANS */

struct ChannelBan
{
	struct ChannelBan *next, *prev;
	char *who;     /* who set the ban */
	time_t when;   /* when the ban was made */
	char *mask;    /* hostmask of the ban */
};

struct Exception
{
	struct Exception *next, *prev;
	char *who;    /* who set the exception */
	time_t when;  /* when it was set */
	char *mask;   /* exception hostmask */
};

#ifdef HYBRID7
struct InviteException
{
	struct InviteException *next, *prev;
	char *who;    /* who set the invite exception */
	time_t when;  /* when it was set */
	char *mask;   /* invite exception hostmask */
};
#endif /* HYBRID7 */

struct ChannelUser
{
	struct ChannelUser *next;
	struct Luser *lptr;   /* pointer to user structure */
	unsigned flags;       /* flags such as opped/voiced */
};

/* Stores info for network channels */
struct Channel
{
	struct Channel *next, *prev, *hnext;

#ifdef BLOCK_ALLOCATION

	char name[CHANNELLEN + 1]; /* channel name */
	char key[KEYLEN + 1];
#ifdef DANCER

	char forward[CHANNELLEN + 1];
#endif /* DANCER */
#else

	char *name;             /* channel name */
	char *key;              /* NULL if no key */
#ifdef DANCER

	char *forward;          /* NULL if no forwarding */
#endif /* DANCER */
#endif /* BLOCK_ALLOCATION */

	int numusers;           /* number of users in the channel */
	int modes;              /* channel modes */
	int limit;              /* 0 if no limit */
	struct ChannelUser *firstuser; /* pointer to first user in channel */
	time_t since;           /* when the channel was created (TS) */
	struct ChannelBan *firstban; /* pointer to first ban */
#ifdef GECOSBANS

	struct ChannelGecosBan *firstgecosban; /* pointer to first gecos field ban*/
#endif /* GECOSBANS */

	struct Exception *exceptlist; /* pointer to first ban exception */
#ifdef HYBRID7

	struct InviteException *inviteexceptlist; /* ptr to first invite
		                                               exception - Janos */
#endif /* HYBRID7 */

	/*
	 * flood_ts[0] is the TS of the first time a *Serv was kicked;
	 * flood_ts[1] is the TS of the last time
	 */
	time_t flood_ts[2];
	int floodcnt;           /* how many times a *Serv was kicked/deoped */
};

#ifdef GECOSBANS
void AddGecosBan(const char *, struct Channel *, const char *);
void DeleteGecosBan(struct Channel *, const char *);
#endif /* GECOSBANS */

void AddBan(const char *, struct Channel *, const char *);
void DeleteBan(struct Channel *, const char *);
void AddException(const char *, struct Channel *, const char *);
void DeleteException(struct Channel *, const char *);

#ifdef HYBRID7
void AddInviteException(const char *, struct Channel *, const char *);
void DeleteInviteException(struct Channel *, const char *);
#endif /* HYBRID7 */

struct Channel *AddChannel(char **, const int, char **);
void DeleteChannel(struct Channel *);
void AddToChannel(struct Channel *, char *);
void RemoveNickFromChannel(const char *, char *);
void RemoveFromChannel(struct Channel *, struct Luser *);
void SetChannelMode(struct Channel *, const int, const int, const struct
		Luser *);
void UpdateChanModes(struct Luser *, const char *, struct Channel *,
		char *);
struct UserChannel *FindChannelByUser(const struct Luser *, const struct
		Channel *);
struct ChannelUser *FindUserByChannel(const struct Channel *, const struct
		Luser *);
void DoMode(struct Channel *, char *, const int);
void SetModes(const char *, const int, const char, struct Channel *, const
		char *);
void KickBan(const int, const char *, struct Channel *, const char
		*, const char *);
struct Channel *IsChan(const char *);
int IsChannelMember(const struct Channel *, const struct Luser *);
int IsChannelVoice(const struct Channel *, const struct Luser *);
int IsChannelOp(const struct Channel *, const struct Luser *);
#ifdef HYBRID7
int IsChannelHOp(const struct Channel *chptr, const struct Luser *lptr);
#endif /* HYBRID7 */

struct ChannelBan *MatchBan(const struct Channel *, const char *);
struct ChannelBan *FindBan(const struct Channel *, const char *);
struct Exception *MatchException(const struct Channel *, const char *);
struct Exception *FindException(const struct Channel *, const char *);
#ifdef HYBRID7
struct InviteException *MatchInviteException(const struct Channel *, const
		char *);
struct InviteException *FindInviteException(const struct Channel *, const
		char *);
#endif /* HYBRID7 */

#ifdef GECOSBANS
struct ChannelGecosBan *MatchGecosBan(const struct Channel *, const char *);
struct ChannelGecosBan *FindGecosBan(const struct Channel *, const char *);
#endif /* GECOSBANS */

extern struct Channel *ChannelList;

#endif /* INCLUDED_channel_h */
