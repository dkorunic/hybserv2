/*
 * sock.h
 * Copyright (C) 1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_sock_h
#define INCLUDED_sock_h

#ifndef INCLUDED_netinet_in_h
#include <netinet/in.h>         /* struct sockaddr_in */
#define INCLUDED_netinet_in_h
#endif

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>         /* time_t */
#define INCLUDED_sys_types_h
#endif

#ifndef INCLUDED_conf_h
#include "conf.h"         /* struct PortInfo */
#define INCLUDED_conf_h
#endif

#ifndef INCLUDED_config_h
#include "config.h"         /* HIGHTRAFFIC_MODE */
#define INCLUDED_config_h
#endif

/* number of bytes to read from sockets - used to be 8192 */
#define BUFSIZE        16384

/* max parameters the hub can send us */
#define MAXPARAM       15

/*
 * Prototypes
 */

int writesocket(int sockfd, char *str);
void toserv(char *format, ...);
void tosock(int sockfd, char *format, ...);
void SetupVirtualHost();
struct hostent *LookupHostname(char *host, struct in_addr *ip_address);
int ConnectHost(char *hostname, unsigned int port);
int CompleteHubConnection(struct Servlist *hubptr);
void ReadSocketInfo(void);
void DoListen(struct PortInfo *portptr);
int SetNonBlocking(int sockfd);
void SetSocketOptions(int sockfd);
void CycleServers();
void DoBinds();
void signon();

/*
 * External declarations
 */

extern int                      HubSock;
extern char                     *LocalHostName;
extern struct sockaddr_in       LocalAddr;

#ifdef HIGHTRAFFIC_MODE

extern int                      HTM;
extern time_t                   HTM_ts;
extern int                      ReceiveLoad;
extern struct DccUser           *dccnext;

#endif /* HIGHTRAFFIC_MODE */

#endif /* INCLUDED_sock_h */
