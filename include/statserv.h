/*
 * statserv.h
 * HybServ2 Services by HybServ2 team
 *
 * $Id$
 */

#ifndef INCLUDED_statserv_h
#define INCLUDED_statserv_h

#ifndef INCLUDED_config_h
#include "config.h"        /* STATSERVICES */
#define INCLUDED_config_h
#endif

#ifdef STATSERVICES

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>        /* time_t */
#define INCLUDED_sys_types_h
#endif

/* StatServ flags */
#define SS_DOMAIN        0x00000001 /* struct contains a domain, not host */

/* Stores info for clients' hostnames */
struct HostHash

{
  struct HostHash *hnext;
  char *hostname;

  time_t lastseen;        /* last time someone had this host */
  long currclients;       /* current clients from this host */
  long curropers;         /* current opers from this host */
  long currunique;        /* current unique clients from this host */
  long curridentd;        /* current clients running identd */

  long maxclients;        /* max clients seen from this host */
  time_t maxclients_ts;   /* time max clients were seen */
  long maxunique;         /* max unique (not cloned) clients from this host */
  time_t maxunique_ts;    /* time unique clients were seen */
  long maxopers;          /* max opers seen from this host */
  time_t maxopers_ts;     /* time max opers were seen */

  int flags;              /* is it a hostname or domain? */
};

/*
 * Prototypes
 */

void ss_process(char *nick, char *command);
void ExpireStats(time_t unixtime);
void DoPings();
struct HostHash *FindHost(char *hostname);
struct HostHash *FindDomain(char *domain);
char *GetDomain(char *hostname);

#endif /* STATSERVICES */

#endif /* INCLUDED_statserv_h */
