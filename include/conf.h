/*
 * conf.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_conf_h
#define INCLUDED_conf_h

#include "stdinc.h"
#include "config.h"

/* userlist privileges/flags */
#define PRIV_ADMIN      0x0001  /* can use administrator commands */
#define PRIV_OPER       0x0002  /* can use operator commands */
#define PRIV_JUPE       0x0004  /* can use jupe/unjupe commands */
#define PRIV_GLINE      0x0008  /* can use gline/ungline commands */
#define PRIV_CHAT       0x0010  /* can use dcc chat service */
#define PRIV_FRIEND     0x0020  /* has "friend" privileges */
#define PRIV_EXCEPTION  0x0040  /* protected user */
#define PRIV_SADMIN     0x0080  /* services administrator */

/* monitored channel flags */
#define CHAN_DELETE      0x001  /* to delete channels after a rehash */

/* server list flags */
#define SL_DELETE     (1 << 0) /* to delete servers after a rehash */
#define SL_CONNECT    (1 << 1) /* currently connecting to this server */

#define SetHubConnect(x)    ((x)->flags |= SL_CONNECT)
#define IsHubConnect(x)     ((x)->flags &  SL_CONNECT)
#define ClearHubConnect(x)  ((x)->flags &= ~SL_CONNECT)

/* Port flags */
#define PRT_TCM         1 /* accept only tcm bot connections */
#define PRT_USERS       2 /* accept only user connections */
#define PRT_DELETE      3 /* marked for deletion in a rehash */

/* Solaris does not provide this by default. */
#ifndef INADDR_NONE
# define INADDR_NONE ((unsigned long)-1)
#endif

struct Luser;
struct Server;

struct MyInfo
{
	char *name;     /* services' server name */
	char *info;     /* services' info line */
	char *admin;    /* administrative info */
	struct Server *sptr; /* pointer to services entry in server list */
	struct Server *hub;  /* pointer to services' current hub server */
	struct Luser *osptr;   /* pointer to OperServ client structure */

#ifdef NICKSERVICES

	struct Luser *nsptr;   /* pointer to NickServ */
#ifdef CHANNELSERVICES

	struct Luser *csptr;   /* pointer to ChanServ */
#endif /* CHANNELSERVICES */
#ifdef MEMOSERVICES

	struct Luser *msptr;   /* pointer to MemoServ */
#endif /* MEMOSERVICES */
#endif /* NICKSERVICES */
#ifdef STATSERVICES

	struct Luser *ssptr;   /* pointer to StatServ */
#endif /* STATSERVICES */
#ifdef HELPSERVICES

	struct Luser *hsptr;   /* pointer to HelpServ */
#endif /* HELPSERVICES */
#ifdef GLOBALSERVICES

	struct Luser *gsptr;   /* pointer to Global(Serv) */
#endif /* GLOBALSERVICES */
#ifdef SEENSERVICES

	struct Luser *esptr;   /* pointer to SeenServ */
#endif /* SEENSERVICES */
};

/* Stores server S: lines from hybserv.conf */
struct Servlist
{
	struct Servlist *next;
	char *hostname;       /* hostname of hub server */
	char *password;       /* password for connection */
	char *realname;       /* network name of hub (if differnet from hostname) */
	int port;             /* port for connection */
	time_t connect_ts;    /* time we connected to it */
	int sockfd;           /* socket fd (must match HubSock) */
	unsigned int flags;
};

/* Stores O: lines from hybserv.conf */
struct Userlist
{
	struct Userlist *next;
	char *nick;        /* nickname */
	char *username;    /* username of user */
	char *hostname;    /* hostname of user */
	char *password;    /* password user must .identify with */
	int flags;         /* privileges user has */
	long umodes;       /* user modes set */

#ifdef RECORD_RESTART_TS
	/* Record TS of nicks so that when services are restarted
	 * they won't have to reidentify */
	time_t nick_ts;
	char *last_nick;
    char *last_server;
#endif
};

/* Stores C: lines from hybserv.conf */
struct Chanlist
{
	struct Chanlist *next;
	char *name;  /* name of channel */
	int flags;   /* to mark CHAN_DELETE during rehash */
};

struct Botlist
{
	struct Botlist *next;
	char *name;     /* nickname of tcm */
	char *username; /* username of tcm */
	char *hostname; /* hostname of tcm */
	char *password; /* password for tcm */
	int port;       /* port for connection */
};

/* Stores I: line info from hybserv.conf */
struct rHost
{
	struct rHost *next;
	char *username; /* username of restricted hostmask */
	char *hostname; /* hostname of restricted hostmask */
	int hostnum;    /* number of connections allowed from hostmask */
#ifdef ADVFLOOD

	int banhost;
#endif /* ADVFLOOD */
};

/* Stores P: line info from hybserv.conf */
struct PortInfo
{
	struct PortInfo *next;
	int port;      /* port to listen on */
	char *host;    /* hostmask to allow */
	int socket;    /* socket file descriptor */
	int type;      /* type of connections to accept */
	int tries;     /* how many times we've tried to bind the port */
};

#if defined AUTO_ROUTING && defined SPLIT_INFO
/* Stores M: lines from hybserv.conf */
struct cHost
{
	struct cHost *next;
	long re_time; /* reconnect activation time (max value of split_ts) */
	char *hub,    /* hub that leaf should be connected to*/
	*leaf;      /* leaf */
	int port;     /* reconnect port */
};
#endif

int Rehash(void);
void ParseConf(const char *, const int);
void LoadConfig(void);
struct Chanlist *IsChannel(const char *);
struct Servlist *IsServLine(const char *, const char *);
struct PortInfo *IsListening(const int);
struct PortInfo *IsPort(const int);
struct Botlist *IsBot(const char *);
void AddMyChan(const char *);
void CheckChans(void);
int IsProtectedHost(const char *, const char *);
struct Userlist *GetUser(const int, const char *, const char *,
		const char *);
int CheckAccess(const struct Userlist *, const char);
struct rHost *IsRestrictedHost(const char *, const char *);

extern struct Userlist *UserList;
extern struct Servlist *ServList;
extern struct Chanlist *ChanList;
extern struct Botlist *BotList;
extern struct Botlist *RemoteBots;
extern struct PortInfo *PortList;
extern struct rHost *rHostList;
extern struct Userlist *GenericOper;
extern struct Servlist *currenthub;
extern struct cHost *cHostList;
extern int HubCount;

#endif /* INCLUDED_conf_h */
