/*
 * server.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_server_h
#define INCLUDED_server_h

#include "stdinc.h"
#include "config.h"

/* Server structure flags */
#define SERV_MYHUB      0x0001 /* services' current hub server */

/* Defining public commands for services */
#ifdef PUBCOMMANDS
# define CS_PUB_OP 1
# define CS_PUB_DEOP 2
# define SS_PUB_SEEN 3
# define SS_PUB_SEENNICK 4
#endif

/* Stores info for servers on network */
struct Server
{
	struct Server *next, *prev, *hnext;

#ifdef BLOCK_ALLOCATION

	char name[SERVERLEN + 1]; /* name of server */
#else

	char *name;
#endif /* BLOCK_ALLOCATION */

	long numusers;       /* number of users on server */
	long numopers;       /* number of operators on server */
	long numoperkills;   /* number of operator kills */
	long numservkills;   /* number of server kills */
	long numservs;       /* current servers linked */
	struct Server *uplink; /* hub for this server */
	time_t connect_ts;   /* time server has been connected to its hub */
	long flags;

#ifdef STATSERVICES

	long maxusers;        /* max users seen on this server */
	time_t maxusers_ts;   /* when max users were seen */
	long maxopers;        /* max opers seen on this server */
	time_t maxopers_ts;   /* when max opers were seen */
	long maxservs;        /* max servers seen from this server */
	time_t maxservs_ts;   /* when max servs were seen */
	long numidentd;       /* number of identd clients on server */
	long numreshosts;     /* number of clients with resolving hosts */
	float maxping;        /* maximum ping received from this server */
	time_t maxping_ts;    /* when max ping was received */
	float minping;        /* minimum ping received from this server */
	time_t minping_ts;    /* when min ping was received */
	float ping;           /* current ping time received from this server */
	long lastping_sec;    /* TS of last time we pinged them */
	long lastping_usec;   /* microseconds corresponding to lastping_sec */
#endif /* STATSERVICES */

#ifdef SPLIT_INFO
	/* This could depend on STATSERVICES, but it can actually function for
	 * itself by SendUmode(). -kre */
	time_t split_ts;      /* TS of last known SERVER introducing */
#endif
};

struct ServCommand
{
	char *cmd; /* holds command */
	void (*func)(); /* function to call */
};

void ProcessInfo(int, char **);
struct Server *AddServer(int, char **);
void DeleteServer(struct Server *);
void do_squit(char *, char *);
void s_sjoin(int, char **);
void ServNotice(char *, char *, int);
void ClearUsers(void);
void ClearServs(void);
void ClearChans(void);

extern struct Server *ServerList;
extern int burst_complete;

#endif /* INCLUDED_server_h */
