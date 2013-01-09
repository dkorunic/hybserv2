/*
 * hybdefs.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_hybdefs_h
#define INCLUDED_hybdefs_h

#include "stdinc.h"
#include "config.h"
#include "motd.h"

#define   MAXLINE    512  /* don't change this */
#define   BUFSIZE  16364  /* don't change this - number of bytes to read
from sockets */
#define   MAXPARAM    20  /* don't change this - max parameters the hub
can send us */
#define   REALLEN     50  /* ircd value for max server info length */
#define   USERLEN     10  /* username length */
#define   HOSTLEN     63  /* hostname length */
#define   KEYLEN      23  /* channel key length */
#define   SERVERLEN   63  /* server hostname length */
#define   TIMELEN     50  /* internal time strings */
#define   UHOSTLEN    (USERLEN + HOSTLEN)
#define   MAXUSERLEN  (NICKLEN + USERLEN + HOSTLEN + 2)

/* Command execution levels */
#define LVL_NONE        0 /* anyone can execute */
#define LVL_IDENT       1 /* must be identified with NickServ */
#define LVL_OPER        2 /* must have "o" privs to execute */
#define LVL_ADMIN       3 /* must be an admin to execute */

/* Network flags */
#define NET_OFF         0x0001 /* Services are deactivated */

/* SOMAXCONN */
#define HYBSERV_SOMAXCONN 25

struct Server;

struct NetworkInfo
{
	float TotalUsers;     /* current user count */
	float TotalServers;   /* current server count */
	float TotalOperators; /* current operator count */
	float TotalChannels;  /* current channel count */
	float TotalOperKills; /* operator kills seen since startup */
	float TotalServKills; /* server kills seen since startup */

	int TotalJupes;   /* number of jupes */
	int TotalGlines;  /* number of glines */
	int TotalConns;   /* number of dcc connections */
	int MyChans;      /* number of monitored channels */
	int flags;

#ifdef GLOBALSERVICES

	struct MessageFile LogonNewsFile;
#endif /* GLOBALSERVICES */

	long RecvB; /* total bytes received */

	/* total bytes received 10 seconds ago - so we can check if we should
	 * enter high-traffic mode */
	long CheckRecvB;

	/* This is set to the current RecvB, so 10 seconds from now we can set
	 * CheckRecvB to LastRecvB, and update LastRecvB to the current RecvB
	 * again */
	long LastRecvB;

	long SendB;                   /* total bytes sent */
	struct Server *firstserv;     /* ptr to first server in list */

#ifdef NICKSERVICES

	int TotalNicks; /* number of registered nicknames */

#ifdef CHANNELSERVICES

	int TotalChans; /* number of registered channels */
#endif

#ifdef MEMOSERVICES

	int TotalMemos; /* number of memos */
#endif

#endif /* NICKSERVICES */

#ifdef STATSERVICES

	long MaxUsers;           /* max users seen */
	time_t MaxUsers_ts;
	long MaxServers;         /* max servers seen */
	time_t MaxServers_ts;
	long MaxOperators;       /* max operators seen */
	time_t MaxOperators_ts;
	long MaxChannels;        /* max channels seen */
	time_t MaxChannels_ts;
	long Identd;             /* number of clients running identd */
	long NonIdentd;          /* number of clients not running identd */
	long ResHosts;           /* number of clients with resolving hosts */
	long MaxUsersT;          /* max users seen today */
	time_t MaxUsersT_ts;
	long MaxServersT;        /* max servers seen today */
	time_t MaxServersT_ts;
	long MaxOperatorsT;      /* max operators seen today */
	time_t MaxOperatorsT_ts;
	long MaxChannelsT;       /* max channels seen today */
	time_t MaxChannelsT_ts;
	long OperKillsT;         /* oper kills seen today */
	long ServKillsT;         /* server kills seen today */
#endif /* STATSERVICES */
};

extern struct NetworkInfo *Network;
extern struct MyInfo Me;
extern int SafeConnect;
extern char hVersion[];
extern time_t TimeStarted;
extern long gmt_offset;
extern time_t current_ts;
#ifdef RECORD_RESTART_TS
extern time_t most_recent_sjoin;
#endif

#endif /* INCLUDED_hybdefs_h */
