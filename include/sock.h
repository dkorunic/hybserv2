/*
 * sock.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_sock_h
#define INCLUDED_sock_h

#include "stdinc.h"
#include "config.h"
#include "conf.h"

struct Servlist;

int writesocket(int, char *);
void toserv(char *, ...);
void tosock(int, char *, ...);
void SetupVirtualHost(void);
struct addrinfo *LookupHostname(const char *);
char *LookupAddress(struct sockaddr *, socklen_t);
char *ConvertHostname(struct sockaddr *, socklen_t);
int IgnoreErrno(int);
unsigned short int GetPort(struct sockaddr *);
void SetPort(struct sockaddr *, unsigned short int);
int ConnectHost(const char *, unsigned int);
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
extern struct sockaddr_storage LocalAddr;
extern socklen_t LocalAddrSize;
extern char buffer[];
extern int paramc;
extern char *nextparam;
extern char spill[];
extern int offset;

#ifdef HIGHTRAFFIC_MODE
extern int HTM;
extern time_t HTM_ts;
extern int ReceiveLoad;
#endif /* HIGHTRAFFIC_MODE */

extern struct DccUser *dccnext;
extern int read_socket_done;

#endif /* INCLUDED_sock_h */
