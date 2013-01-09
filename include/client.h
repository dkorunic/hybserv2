/*
 * client.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_client_h
#define INCLUDED_client_h

#include "stdinc.h"
#include "config.h"
#include "hybdefs.h"

/* Luser flags */
#define L_OSREGISTERED  0x0001 /* user is identified with OperServ */

/* user mode flags */
#define UMODE_O          0x0001 /* IRC Operator */
#define UMODE_I          0x0002 /* Invisible */
#define UMODE_W          0x0004 /* Wallops */
#define UMODE_S          0x0008 /* Server Notices */
#ifdef DANCER
# define UMODE_E         0x0010 /* Identified umode */
#endif /* DANCER */
#define UMODE_NOFORCENICK          0x0020 /* Doesn't support forcenick */

struct UserChannel
{
	struct UserChannel *next;
	int flags; /* channel flags - opped/voiced etc */
	struct Channel *chptr; /* pointer to channel */
};

#if defined NICKSERVICES && defined CHANNELSERVICES

struct aChannelPtr
{
	struct aChannelPtr *next;
	struct ChanInfo *cptr;
};

#endif /* NICKSERVICES && CHANNELSERVICES */

/* User structure */
struct Luser
{
	struct Luser *next, *prev, *hnext, *cnext;

#ifdef BLOCK_ALLOCATION
	/*
	 * When BLOCK_ALLOCATION is enabled, we don't want to have to malloc()
	 * space for nick,userhost,realname etc, so have it preallocated
	 */
	char nick[NICKLEN + 1];     /* nickname */
	char username[USERLEN + 1]; /* username */
	char hostname[HOSTLEN + 1]; /* hostname */
	char realname[REALLEN + 1]; /* realname */
#else

	char *nick;
	char *username;
	char *hostname;
	char *realname;
#endif /* BLOCK_ALLOCATION */

	int umodes;                    /* global usermodes they have */
	struct Server *server;         /* pointer to server they're on */
	struct UserChannel *firstchan; /* pointer to list of channels */
	time_t since;                  /* when they connected to the network */

#ifdef NICKSERVICES

	time_t nick_ts;      /* time of their last nick change */
	time_t nickreg_ts;   /* time they last registered a nickname */
#ifdef CHANNELSERVICES
	/* list of channels user has identified for */
	struct aChannelPtr *founder_channels;
#endif /* CHANNELSERVICES */
#endif /* NICKSERVICES */

	/*
	 * msgs_ts[0] is the timestamp of the first message they send services;
	 * msgs_ts[1] is the timestamp of the last message they send
	 */
	time_t msgs_ts[2];
	int messages;        /* number of messages they've sent */
	int flood_trigger;   /* how many times they triggered flood protection */

	int flags;

#ifdef STATSERVICES

	long  numops;      /* how many times they +o'd someone */
	long  numdops;     /* how many times they -o'd someone */
	long  numvoices;   /* how many times they +v'd someone */
	long  numdvoices;  /* how many times they -v'd someone */
	long  numnicks;    /* how many times they've changed their nickname */
	long  numkicks;    /* how many times they've kicked someone */
	long  numkills;    /* how many times they've killed someone */
	long  numhops;     /* how many times they +h'd someone */
	long  numdhops;    /* how many times they -h'd someone */
#endif /* STATSERVICES */
};

void UpdateUserModes(struct Luser *, char *);
struct Luser *AddClient(char **);
void DeleteClient(struct Luser *);
char *GetNick(char *);
int IsRegistered(struct Luser *, int);
int IsOperator(struct Luser *);
int IsValidAdmin(struct Luser *);
int IsValidServicesAdmin(struct Luser *lptr);
int IsNickCollide(struct Luser *, char **);

extern struct Luser *ClientList;

#endif /* INCLUDED_client_h */
