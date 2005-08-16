/*
 * sock.h
 * Hybserv2 Services by Hybserv2 team
 *
 * $Id$
 */

#ifndef INCLUDED_sock_h
#define INCLUDED_sock_h

#include "stdinc.h"
#include "config.h"
#include "conf.h"

/* number of bytes to read from sockets - used to be 8192 */
#define BUFSIZE        16384
/* max parameters the hub can send us */
#define MAXPARAM       15

struct Servlist;

int writesocket(int, char *);
void toserv(char *, ...);
void tosock(int, char *, ...);
void SetupVirtualHost(void);
struct hostent *LookupHostname(char *, struct in_addr *);
int ConnectHost(char *, unsigned int);
int CompleteHubConnection(struct Servlist *);
void ReadSocketInfo(void);
void DoListen(struct PortInfo *);
int SetNonBlocking(int);
void SetSocketOptions(int);
void CycleServers(void);
void DoBinds(void);
void signon(void);

extern int HubSock;
extern char *LocalHostName;
extern struct sockaddr_in LocalAddr;
extern char buffer[];
extern int paramc;
extern char *nextparam;
extern char spill[];
extern int offset;

#ifdef HIGHTRAFFIC_MODE
extern int HTM;
extern time_t HTM_ts;
extern int ReceiveLoad;
extern struct DccUser *dccnext;
#endif /* HIGHTRAFFIC_MODE */

extern int read_socket_done;

#endif /* INCLUDED_sock_h */
