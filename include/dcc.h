/*
 * dcc.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_dcc_h
#define INCLUDED_dcc_h

#include "stdinc.h"
#include "config.h"
#include "hybdefs.h"

/* socket flags */
#define NOSOCKET     (-2) /* unused socket in portlist */
#define SOCK_UNUSED  (1 << 0) /* unused socket */
#define SOCK_TCMBOT  (1 << 1) /* socket is a tcm bot connection */
#define SOCK_BOTHUB  (1 << 2) /* tcm bot is the hub */
#define SOCK_PENDING (1 << 3) /* has the tcm sent the password yet? */
#define SOCK_DCC     (1 << 4) /* socket being used for dcc connection */
#define SOCK_EOF     (1 << 5) /* socket EOF'd during write() */
#define SOCK_NEEDID  (1 << 6) /* need ident reply */
#define SOCK_WRID    (1 << 7) /* need to request ident */
#define SOCK_CONNECT (1 << 8) /* connection has been activated */

/* DccUser flag macros */
#define SetDccConnect(x)   ((x)->flags |= SOCK_CONNECT)
#define IsDccConnect(x)    ((x)->flags &  SOCK_CONNECT)
#define ClearDccConnect(x) ((x)->flags &= ~SOCK_CONNECT)
#define SetDccPending(x)   ((x)->flags |= SOCK_PENDING)
#define IsDccPending(x)    ((x)->flags &  SOCK_PENDING)
#define ClearDccPending(x) ((x)->flags &= ~SOCK_PENDING)

struct PortInfo;
struct Botlist;

/* info for dcc/tcm connections */
struct DccUser
{
	struct DccUser *next, *prev;
	int socket;       /* socket file descriptor */
	int authfd;       /* ident file descriptor */
	int port;         /* remote port */
	char *nick;       /* nickname */
	char *username;   /* username */
	char *hostname;   /* hostname */
	int flags;        /* socket flags */
	char spill[MAXLINE * 2];
	int offset;

	/* time of their last message - for telnet clients who haven't entered
	 * their password yet, time that they connected */
	time_t idle;
};

void BroadcastDcc(int, char *, ...);
void SendUmode(int, char *, ...);
void SendDccMessage(struct DccUser *, char *);
void ConnectClient(struct PortInfo *);
int GreetDccUser(struct DccUser *);
void TelnetGreet(struct DccUser *);
void ExpireIdent(time_t);
void ExpireTelnet(time_t);
void CheckEOF(void);
struct Userlist *DccGetUser(struct DccUser *);
int telnet_check(int, char *);
void readauth(struct DccUser *);
void writeauth(struct DccUser *);
void onctcp(char *, char *, char *);
int ConnectToTCM(char *, struct Botlist *);
void LinkBots(void);
void CloseConnection(struct DccUser *);
void DeleteDccClient(struct DccUser *);
struct Botlist *GoodTCM(struct DccUser *);
struct DccUser *GetBot(char *);
struct DccUser *IsDccSock(int);
struct DccUser *IsOnDcc(char *);
void DccProcess(struct DccUser *, char *);
int BotProcess(struct DccUser *, char *);
void SendMotd(int);
void ServReboot(void);

extern struct DccUser *connections;
#ifdef ADMININFO
extern struct Luser *ClientList;
#endif /* ADMININFO */

#endif /* INCLUDED_dcc_h */
