/*
 * dcc.h
 * HybServ2 Services by HybServ2 team
 *
 * $Id$
 */

#ifndef INCLUDED_dcc_h
#define INCLUDED_dcc_h

#ifndef INCLUDED_hybdefs_h
#include "hybdefs.h"         /* MAXLINE needed -kre */
#define INCLUDED_hybdefs_h
#endif

#ifndef INCLUDED_sys_time_h
#include <sys/time.h>         /* time_t */
#define INCLUDED_sys_time_h
#endif

struct Luser;
struct PortInfo;
struct Botlist;

/* socket flags */
#define NOSOCKET (-2) /* unused socket in portlist */

#define SOCK_UNUSED  (1 << 0) /* unused socket */
#define SOCK_TCMBOT  (1 << 1) /* socket is a tcm bot connection */
#define SOCK_BOTHUB  (1 << 2) /* tcm bot is the hub */
#define SOCK_PENDING (1 << 3) /* has the tcm sent the password yet? */
#define SOCK_DCC     (1 << 4) /* socket being used for dcc connection */
#define SOCK_EOF     (1 << 5) /* socket EOF'd during write() */
#define SOCK_NEEDID  (1 << 6) /* need ident reply */
#define SOCK_WRID    (1 << 7) /* need to request ident */
#define SOCK_CONNECT (1 << 8) /* connection has been activated */

/*
 * DccUser flag macros
 */

#define SetDccConnect(x)   ((x)->flags |= SOCK_CONNECT)
#define IsDccConnect(x)    ((x)->flags &  SOCK_CONNECT)
#define ClearDccConnect(x) ((x)->flags &= ~SOCK_CONNECT)

#define SetDccPending(x)   ((x)->flags |= SOCK_PENDING)
#define IsDccPending(x)    ((x)->flags &  SOCK_PENDING)
#define ClearDccPending(x) ((x)->flags &= ~SOCK_PENDING)

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

  /*
   * time of their last message - for telnet clients who haven't
   * entered their password yet, time that they connected
   */
  time_t idle;
};

/*
 * Prototypes
 */

void BroadcastDcc(int, char *, ...);
void SendUmode(int, char *, ...);

void SendDccMessage(struct DccUser *dccptr, char *msg);
void ConnectClient(struct PortInfo *portptr);
int GreetDccUser(struct DccUser *dccptr);
void TelnetGreet(struct DccUser *dccptr);
void ExpireIdent(time_t unixtime);
void ExpireTelnet(time_t unixtime);
void CheckEOF();
struct Userlist *DccGetUser(struct DccUser *dccptr);
int telnet_check(int, char *);
void readauth(struct DccUser *dccptr);
void writeauth(struct DccUser *dccptr);
void onctcp(char *, char *, char *);
int ConnectToTCM(char *, struct Botlist *bptr);
void LinkBots();
void CloseConnection(struct DccUser *dccptr);
void DeleteDccClient(struct DccUser *dccptr);
struct Botlist *GoodTCM(struct DccUser *ptr);
struct DccUser *GetBot(char *nick);
struct DccUser *IsDccSock(int socket);
struct DccUser *IsOnDcc(char *nick);
void DccProcess(struct DccUser *dccptr, char *line);
void BotProcess(struct DccUser *bptr, char *line);
void SendMotd(int socket);
void ServReboot();

/*
 * External declarations
 */

extern struct DccUser *connections;

#ifdef ADMININFO
extern struct Luser *ClientList;
#endif /* ADMININFO */

#endif /* INCLUDED_dcc_h */
