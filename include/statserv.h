/*
 * statserv.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_statserv_h
#define INCLUDED_statserv_h

#include "stdinc.h"
#include "config.h"

#ifdef STATSERVICES

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

void ss_process(char *, char *);
void ExpireStats(time_t);
void DoPings(void);
struct HostHash *FindHost(char *);
struct HostHash *FindDomain(char *);
char *GetDomain(char *);

#endif /* STATSERVICES */

#endif /* INCLUDED_statserv_h */
