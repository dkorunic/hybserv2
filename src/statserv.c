/*
 * Hybserv2 Services by Hybserv2 team
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 */

#include "stdinc.h"
#include "alloc.h"
#include "client.h"
#include "config.h"
#include "data.h"
#include "err.h"
#include "hash.h"
#include "helpserv.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "mystring.h"
#include "server.h"
#include "settings.h"
#include "sock.h"
#include "statserv.h"
#include "timestr.h"
#include "sprintf_irc.h"

#ifdef  STATSERVICES

aHashEntry  hostTable[HASHCLIENTS];

static void ss_host(struct Luser *, int, char **);
static void ss_domain(struct Luser *, int, char **);
static void ss_server(struct Luser *, int, char **);
static void ss_stats(struct Luser *, int, char **);
static void ss_help(struct Luser *, int, char **);

static void ss_refresh(struct Luser *, int, char **);
static void ss_clearstats(struct Luser *, int, char **);
static void ss_showstats(struct Luser *, int, char **);
static void ss_greplog(struct Luser *, int, char **);
static void ss_showadmins(struct Luser *, int, char **);
static void ss_showopers(struct Luser *, int, char **);

#ifdef SPLIT_INFO
static void ss_splitinfo(struct Luser *, int, char **);
void ss_printsplit(struct Server *, struct Luser *);
#endif

static void ShowServerInfo(struct Server *, struct Luser *, int);

static struct Command statcmds[] =
    {
	    { "STATS", ss_stats, LVL_NONE
	    },
	    { "HELP", ss_help, LVL_NONE },
	    { "SHOWADMINS", ss_showadmins, LVL_NONE },
	    { "SHOWOPERS", ss_showopers, LVL_NONE },
	    { "HOST", ss_host, LVL_ADMIN },
	    { "DOMAIN", ss_domain, LVL_ADMIN },
	    { "SERVER", ss_server, LVL_ADMIN },
	    { "REFRESH", ss_refresh, LVL_ADMIN },
	    { "CLEARSTATS", ss_clearstats, LVL_ADMIN },
	    { "SHOWSTATS", ss_showstats, LVL_NONE },
	    { "GREPLOG",   ss_greplog, LVL_ADMIN },
#ifdef SPLIT_INFO
	    { "SPLIT",    ss_splitinfo, LVL_ADMIN },
#endif

	    { 0, 0, 0 }
    };

long korectdat( long dat1, int dni);
int* get_tg( int year, int *days );
int gnw[] = {31,28,31,30,31,30,31,31,30,31,30,31,31};
int gw[] = {31,29,31,30,31,30,31,31,30,31,30,31,31};

/*
ss_process()
  Process command coming from 'nick' directed towards n_StatServ
*/

void
ss_process(char *nick, char *command)

{
	int acnt;
	char **arv;
	struct Command *sptr;
	struct Luser *lptr;

	if (!command || !(lptr = FindClient(nick)))
		return;

	if (Network->flags & NET_OFF)
	{
		notice(n_StatServ, lptr->nick,
		       "Services are currently \002disabled\002");
		return;
	}

	acnt = SplitBuf(command, &arv);
	if (acnt == 0)
	{
		MyFree(arv);
		return;
	}

	sptr = GetCommand(statcmds, arv[0]);

	if (!sptr || (sptr == (struct Command *) -1))
	{
		notice(n_StatServ, lptr->nick,
		       "%s command [%s]",
		       (sptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
		       arv[0]);
		MyFree(arv);
		return;
	}

	/*
	 * Check if the command is for admins only - if so,
	 * check if they match an admin O: line.  If they do,
	 * check if they are registered with OperServ,
	 * if so, allow the command
	 */
	if ((sptr->level == LVL_ADMIN) && !(IsValidAdmin(lptr)))
	{
		notice(n_StatServ, lptr->nick, "Unknown command [%s]",
		       arv[0]);
		MyFree(arv);
		return;
	}

	/* call sptr->func to execute command */
	(*sptr->func)(lptr, acnt, arv);

	MyFree(arv);

	return;
} /* ss_process() */

/*
ss_loaddata()
  Load StatServ database - return 1 if successful, -1 if not, and -2
if the errors are unrecoverable
*/

int
ss_loaddata()

{
	FILE *fp;
	char line[MAXLINE + 1], **av;
	char *keyword;
	int ac, ret = 1, cnt;

	if ((fp = fopen(StatServDB, "r")) == NULL)
	{
		/* StatServ data file doesn't exist */
		return (-1);
	}

	cnt = 0;
	/* load data into list */
	while (fgets(line, sizeof(line), fp))
	{
		cnt++;
		ac = SplitBuf(line, &av);
		if (!ac)
		{
			/* probably a blank line */
			MyFree(av);
			continue;
		}

		if (av[0][0] == ';')
		{
			MyFree(av);
			continue;
		}

		if (!ircncmp("->", av[0], 2))
		{
			/*
			 * check if there are enough args
			 */
			if (ac < 3)
			{
				fatal(1, "%s:%d Invalid database format (FATAL)",
				      StatServDB,
				      cnt);
				ret = -2;
				MyFree(av);
				continue;
			}

			keyword = av[0] + 2;
			if (!ircncmp(keyword, "USERS", 5))
			{
				if (Network->TotalUsers <= atol(av[1]))
				{
					Network->MaxUsers = atol(av[1]);
					Network->MaxUsers_ts = atol(av[2]);
				}
			}
			else if (!ircncmp(keyword, "OPERS", 5))
			{
				if (Network->TotalOperators <= atol(av[1]))
				{
					Network->MaxOperators = atol(av[1]);
					Network->MaxOperators_ts = atol(av[2]);
				}
			}
			else if (!ircncmp(keyword, "CHANS", 5))
			{
				if (Network->TotalChannels <= atol(av[1]))
				{
					Network->MaxChannels = atol(av[1]);
					Network->MaxChannels_ts = atol(av[2]);
				}
			}
			else if (!ircncmp(keyword, "SERVS", 5))
			{
				if (Network->TotalServers <= atol(av[1]))
				{
					Network->MaxServers = atol(av[1]);
					Network->MaxServers_ts = atol(av[2]);
				}
			}
		}

		MyFree(av);
	} /* while */

	fclose(fp);

	return (ret);
} /* ss_loaddata */

/*
ExpireStats()
 Delete expired stat entries
*/

void
ExpireStats(time_t unixtime)

{
	int ii;
	struct HostHash *hosth, *hprev;

	if (!StatExpire)
		return;

	for (ii = 0; ii < HASHCLIENTS; ++ii)
	{
		hprev = NULL;
		for (hosth = hostTable[ii].list; hosth; )
		{
			if ((hosth->currclients == 0) &&
			        ((unixtime - hosth->lastseen) >= StatExpire))
			{
				if (hprev)
					hprev->hnext = hosth->hnext;
				else
					hostTable[ii].list = (void *) hosth->hnext;

				putlog(LOG2, "%s: expired %s [%s]",
				       n_StatServ,
				       (hosth->flags & SS_DOMAIN) ? "domain" : "hostname",
				       hosth->hostname);

				MyFree(hosth->hostname);
				MyFree(hosth);

				if (hprev)
					hosth = hprev->hnext;
				else
					hosth = hostTable[ii].list;
			}
			else
			{
				hprev = hosth;
				hosth = hosth->hnext;
			}
		} /* for (hosth = hostTable[ii].list; hosth; ) */
	}
} /* ExpireStats() */

/*
DoPings()
 Ping every server on the network in order to keep track of lag,
etc
*/

void
DoPings()

{
	struct Server *tempserv;
	struct timeval timer;

	GetTime(&timer);

	for (tempserv = ServerList; tempserv; tempserv = tempserv->next)
	{
		/*
		 * Do not ping services itself, or any juped servers
		 * services is hubbing
		 */
		if ((!(tempserv->flags & SERV_MYHUB) && (tempserv->uplink == Me.sptr)) ||
		        (tempserv == Me.sptr))
			continue;

		if (tempserv->lastping_sec || tempserv->lastping_usec)
		{
			/*
			 * We still have not received their reply from the last
			 * time we pinged them - don't ping them again until we
			 * do.
			 */
			continue;
		}

		/*
		 * Record the TS of when we're pinging them
		 */
		tempserv->lastping_sec = timer.tv_sec;
		tempserv->lastping_usec = timer.tv_usec;

		toserv(":%s PING %s :%s\r\n",
		       Me.name, Me.name, tempserv->name);
	}
} /* DoPings() */

/*
FindHost()
  Return pointer to HostHash structure containing 'hostname'
*/

struct HostHash *
			FindHost(char *hostname)

{
	struct HostHash *tmp;
	int hashv;

	if (!hostname)
		return (NULL);

	hashv = HashUhost(hostname);

	for (tmp = hostTable[hashv].list; tmp; tmp = tmp->hnext)
		if (!(tmp->flags & SS_DOMAIN) && (!irccmp(tmp->hostname, hostname)))
			return (tmp);

	return (NULL);
} /* FindHost() */

/*
FindDomain()
  Return pointer to HostHash structure containing 'domain'
*/

struct HostHash *
			FindDomain(char *domain)

{
	struct HostHash *tmp;
	int hashv;

	if (!domain)
		return (NULL);

	hashv = HashUhost(domain);

	for (tmp = hostTable[hashv].list; tmp; tmp = tmp->hnext)
		if ((tmp->flags & SS_DOMAIN) && (!irccmp(tmp->hostname, domain)))
			return (tmp);

	return (NULL);
} /* FindDomain() */

/*
GetDomain()
  Parse 'hostname' and return the last two segments, which correspond
to its domain. ie: if hostname is "aa.bb.cc.dd", return "cc.dd", or
if it is a numerical ip, return "aa.bb"
*/

char *
GetDomain(char *hostname)

{
	static char done[100];
	char *domain, *tmp;
	int dotcnt = 0; /* how many dots there are in the domain */
	int cnt;

	if (!hostname)
		return (NULL);

	/* advance domain to end of hostname */
	for (domain = hostname; *domain; domain++)
		;

	tmp = domain;
	while (tmp != hostname)
	{
		tmp--;
		if (*tmp == '.')
			break;
	}
	tmp++;

	if ((*tmp >= '0') && (*tmp <= '9'))
	{
		/* its a numerical ip */
		done[0] = '\0';
		cnt = 0;
		for (domain = hostname; *domain; domain++)
		{
			done[cnt++] = *domain;
			if (*domain == '.')
			{
				if (!dotcnt)
					dotcnt = 1;
				else
				{
					dotcnt = 2;
					break;
				}
			}
		}
		if (!dotcnt)
			return (NULL);
		if (dotcnt == 1)
			done[cnt] = '\0';
		else
			done[cnt - 1] = '\0';
		return (done);
	}

	while (domain != hostname)
	{
		domain--;
		if (*domain == '.')
		{
			if (!dotcnt)
				dotcnt = 1;
			else
			{
				dotcnt = 2;
				break;
			}
		}
	}

	if ((dotcnt == 2) && !(*(domain + 1) == '.'))
		++domain;
	else if ((dotcnt != 1) || (*domain == '.'))
		return (NULL); /* invalid domain */

	strlcpy(done, domain, sizeof(done));

	return (done);
} /* GetDomain() */

/*
ss_host()
  Give lptr->nick stats for hostname av[1]
*/

static void
ss_host(struct Luser *lptr, int ac, char **av)

{
	struct HostHash *hosth;
	char str[MAXLINE + 1];

	if (ac < 2)
	{
		notice(n_StatServ, lptr->nick,
		       "Syntax: HOST <hostname>");
		notice(n_StatServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_StatServ,
		       "HOST");
		return;
	}

	RecordCommand("%s: %s!%s@%s HOST [%s]",
	              n_StatServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              av[1]);

	if (!(hosth = FindHost(av[1])))
	{
		notice(n_StatServ, lptr->nick,
		       "No hostnames matching [\002%s\002] found",
		       av[1]);
		return;
	}

	notice(n_StatServ, lptr->nick,
	       "Statistics for hostname: \002%s\002",
	       av[1]);
	notice(n_StatServ, lptr->nick,
	       "Current Clients:  %ld (%ld unique)",
	       hosth->currclients,
	       hosth->currunique);
	notice(n_StatServ, lptr->nick,
	       "Current Opers:    %ld",
	       hosth->curropers);

	strlcpy(str, ctime(&hosth->maxclients_ts), sizeof(str));
	str[strlen(str) - 1] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Clients:      %ld on %s",
	       hosth->maxclients,
	       str);

	strlcpy(str, ctime(&hosth->maxunique_ts), sizeof(str));
	str[strlen(str) - 1] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Unique:       %ld on %s",
	       hosth->maxunique,
	       str);

	if (hosth->maxopers_ts)
	{
		strlcpy(str, "on ", sizeof(str));
		strlcat(str, ctime(&hosth->maxopers_ts), sizeof(str));
		str[strlen(str) - 1] = '\0';
	}
	else
		str[0] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Operators:    %ld %s",
	       hosth->maxopers,
	       str);

	notice(n_StatServ, lptr->nick,
	       "Identd Users:     %ld",
	       hosth->curridentd);
	notice(n_StatServ, lptr->nick,
	       "Non-Identd Users: %ld",
	       hosth->currclients - hosth->curridentd);
} /* ss_host() */

/*
ss_domain()
  Give statistics on domain av[1]
*/

static void
ss_domain(struct Luser *lptr, int ac, char **av)

{
	struct HostHash *hosth;
	char *domain;
	char str[MAXLINE + 1];

	if (ac < 2)
	{
		notice(n_StatServ, lptr->nick,
		       "Syntax: DOMAIN <domain>");
		notice(n_StatServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_StatServ,
		       "DOMAIN");
		return;
	}

	domain = GetDomain(av[1]);

	RecordCommand("%s: %s!%s@%s DOMAIN [%s]",
	              n_StatServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              domain ? domain : av[1]);

	if (!(hosth = FindDomain(domain)))
	{
		notice(n_StatServ, lptr->nick,
		       "No domains matching [\002%s\002] found",
		       domain ? domain : av[1]);
		return;
	}

	notice(n_StatServ, lptr->nick,
	       "Statistics for domain: \002%s\002",
	       domain);
	notice(n_StatServ, lptr->nick,
	       "Current Clients:  %ld (%ld unique)",
	       hosth->currclients,
	       hosth->currunique);
	notice(n_StatServ, lptr->nick,
	       "Current Opers:    %ld",
	       hosth->curropers);

	strlcpy(str, ctime(&hosth->maxclients_ts), sizeof(str));
	str[strlen(str) - 1] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Clients:      %ld on %s",
	       hosth->maxclients,
	       str);

	strlcpy(str, ctime(&hosth->maxunique_ts), sizeof(str));
	str[strlen(str) - 1] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Unique:       %ld on %s",
	       hosth->maxunique,
	       str);

	if (hosth->maxopers_ts)
	{
		strlcpy(str, "on ", sizeof(str));
		strlcat(str, ctime(&hosth->maxopers_ts), sizeof(str));
		str[strlen(str) - 1] = '\0';
	}
	else
		str[0] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Operators:    %ld %s",
	       hosth->maxopers,
	       str);

	notice(n_StatServ, lptr->nick,
	       "Identd Users:     %ld",
	       hosth->curridentd);
	notice(n_StatServ, lptr->nick,
	       "Non-Identd Users: %ld",
	       hosth->currclients - hosth->curridentd);
} /* ss_domain() */

/*
ss_server()
 Display server stats for all servers who match <server> and
pass [options]
 
Valid Options:
 -maxusers  -  Limit matches to servers who have less than
               the given number of clients
 -minusers  -  Limit matches to servers who have greater than
               the given number of clients
 -info      -  Show detailed information about each match
 -hub       -  Limit matches to servers using the given hub
*/

static void
ss_server(struct Luser *lptr, int ac, char **av)

{
	struct Server *servptr,
				*hub;
	char argbuf[MAXLINE + 1],
	str[MAXLINE + 1];
	char *target; /* target server */
	int maxusers, /* -maxusers */
	minusers, /* -minusers */
	info;     /* -info */
	int ii,
	alen,
	cnt;

	if (ac < 2)
{
		notice(n_StatServ, lptr->nick,
		       "Syntax: SERVER <server | mask> [options]");
		notice(n_StatServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_StatServ,
		       "SERVER");
		return;
	}

	hub = NULL;
	maxusers = -1;
	minusers = -1;
	target = NULL;
	info = 0;

	/*
	 * Parse their args
	 */
	for (ii = 1; ii < ac; ii++)
	{
		alen = strlen(av[ii]);

		if (!ircncmp(av[ii], "-maxusers", alen))
		{
			if (++ii >= ac)
			{
				notice(n_StatServ, lptr->nick,
				       "No maximum user count given");
				return;
			}

			maxusers = atoi(av[ii]);
		}
		else if (!ircncmp(av[ii], "-minusers", alen))
		{
			if (++ii >= ac)
			{
				notice(n_StatServ, lptr->nick,
				       "No minimum user count given");
				return;
			}

			minusers = atoi(av[ii]);
		}
		else if (!ircncmp(av[ii], "-info", alen))
			info = 1;
		else if (!ircncmp(av[ii], "-hub", alen))
		{
			if (++ii >= ac)
			{
				notice(n_StatServ, lptr->nick,
				       "No hub server given");
				return;
			}

			if (!(hub = FindServer(av[ii])))
			{
				notice(n_StatServ, lptr->nick,
				       "No such server: %s",
				       av[ii]);
				return;
			}
		}
		else
		{
			if (!target)
				target = av[ii];
		}
	}

	if (!target)
	{
		notice(n_StatServ, lptr->nick,
		       "No target server specified");
		return;
	}

	argbuf[0] = '\0';

	if (maxusers >= 0)
	{
		ircsprintf(str, "-maxusers %d ", maxusers);
		strlcat(argbuf, str, sizeof(argbuf));
	}

	if (minusers >= 0)
	{
		ircsprintf(str, "-minusers %d ", minusers);
		strlcat(argbuf, str, sizeof(argbuf));
	}

	if (info)
		strlcat(argbuf, "-info ", sizeof(argbuf));

	if (hub)
	{
		ircsprintf(str, "-hub %s ", hub->name);
		strlcat(argbuf, str, sizeof(argbuf));
	}

	RecordCommand("%s: %s!%s@%s SERVER [%s] %s",
	              n_StatServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              target,
	              argbuf);

	if ((servptr = FindServer(target)))
	{
		ShowServerInfo(servptr, lptr, 1);
		return;
	}

	cnt = 0;
	for (servptr = ServerList; servptr; servptr = servptr->next)
	{
		if (!match(target, servptr->name))
			continue;

		if (((maxusers >= 0) && (servptr->numusers > maxusers)) ||
		        ((minusers >= 0) && (servptr->numusers < minusers)))
			continue;
		else if (hub && (servptr->uplink != hub))
			continue;

		/*
		 * servptr passes all the tests
		 */

		++cnt;

		notice(n_StatServ, lptr->nick,
		       "-----");

		ShowServerInfo(servptr, lptr, info);
	}

	notice(n_StatServ, lptr->nick,
	       "%d match%s found",
	       cnt,
	       (cnt == 1) ? "" : "es");
} /* ss_server() */

/*
ShowServerInfo()
 Display information about servptr to lptr
*/

static void
ShowServerInfo(struct Server *servptr, struct Luser *lptr, int showinfo)

{
	char str[MAXLINE + 1];

	assert(servptr != 0);
	assert(lptr != 0);

	if (!showinfo)
	{
#ifdef SPLIT_INFO
		if (!servptr->split_ts)
#endif

			notice(n_StatServ, lptr->nick,
			       "%-30s connected: %s, users: %d",
			       servptr->name,
			       timeago(servptr->connect_ts, 0),
			       servptr->numusers);
#ifdef SPLIT_INFO

		else
			notice(n_StatServ, lptr->nick,
			       "%-30s currently in \002netsplit\002 for %s",
			       servptr->name,
			       timeago(servptr->split_ts, 0));
#endif

		return;
	}

#ifdef SPLIT_INFO
	if (!servptr->split_ts)
#endif

		notice(n_StatServ, lptr->nick,
		       "Statistics for server: \002%s\002",
		       servptr->name);
#ifdef SPLIT_INFO

	else
		notice(n_StatServ, lptr->nick,
		       "Last known statistics for server: \002%s\002",
		       servptr->name);
#endif

	notice(n_StatServ, lptr->nick,
	       "Current Clients:       %ld",
	       servptr->numusers);
	notice(n_StatServ, lptr->nick,
	       "Current Opers:         %ld",
	       servptr->numopers);
	notice(n_StatServ, lptr->nick,
	       "Current Servers:       %ld",
	       servptr->numservs);

	if (servptr->maxusers_ts)
	{
		strlcpy(str, "on ", sizeof(str));
		strlcat(str, ctime(&servptr->maxusers_ts), sizeof(str));
		str[strlen(str) - 1] = '\0';
	}
	else
		str[0] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Clients:           %ld %s",
	       servptr->maxusers,
	       str);

	if (servptr->maxopers_ts)
	{
		strlcpy(str, "on ", sizeof(str));
		strlcat(str, ctime(&servptr->maxopers_ts), sizeof(str));
		str[strlen(str) - 1] = '\0';
	}
	else
		str[0] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Opers:             %ld %s",
	       servptr->maxopers,
	       str);

	if (servptr->maxservs_ts)
	{
		strlcpy(str, "on ", sizeof(str));
		strlcat(str, ctime(&servptr->maxservs_ts), sizeof(str));
		str[strlen(str) - 1] = '\0';
	}
	else
		str[0] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Servers:           %ld %s",
	       servptr->maxservs,
	       str);

	notice(n_StatServ, lptr->nick,
	       "Identd Users:          %ld",
	       servptr->numidentd);
	notice(n_StatServ, lptr->nick,
	       "Non-Identd Users:      %ld",
	       servptr->numusers - servptr->numidentd);
	notice(n_StatServ, lptr->nick,
	       "Resolving Hosts:       %ld",
	       servptr->numreshosts);
	notice(n_StatServ, lptr->nick,
	       "Non-Resolving Hosts:   %ld",
	       servptr->numusers - servptr->numreshosts);

	notice(n_StatServ, lptr->nick,
	       "Operator Kills:        %ld",
	       servptr->numoperkills);
	notice(n_StatServ, lptr->nick,
	       "Server Kills:          %ld",
	       servptr->numservkills);

	if (servptr->ping > 0.0)
	{
		notice(n_StatServ, lptr->nick,
		       "Current Ping:          %5.4f seconds",
		       servptr->ping);

		if ((servptr->maxping > 0.0) && servptr->maxping_ts)
		{
			ircsprintf(str, "on %s", ctime(&servptr->maxping_ts));
			str[strlen(str) - 1] = '\0';
			notice(n_StatServ, lptr->nick,
			       "Highest Ping:          %5.4f seconds %s",
			       servptr->maxping,
			       str);
		}

		if ((servptr->minping > 0.0) && servptr->minping_ts)
		{
			ircsprintf(str, "on %s", ctime(&servptr->minping_ts));
			str[strlen(str) - 1] = '\0';
			notice(n_StatServ, lptr->nick,
			       "Lowest Ping:           %5.4f seconds %s",
			       servptr->minping,
			       str);
		}
	}

	if (servptr->uplink)
		notice(n_StatServ, lptr->nick,
		       "Current Hub:           %s (connected for [%s])",
		       servptr->uplink->name,
		       timeago(servptr->connect_ts, 0));
#ifdef SPLIT_INFO

	else
		notice(n_StatServ, lptr->nick,
		       "Currently in \002netsplit\002 for %s",
		       servptr->name,
		       timeago(servptr->connect_ts, 0));
#endif
} /* ShowServerInfo() */

/*
ss_stats()
  Give general network stats
*/

static void
ss_stats(struct Luser *lptr, int ac, char **av)

{
	float avgops, avguc, avgus;
	struct tm *tmp_tm;
	char str[MAXLINE + 1], tmp[MAXLINE + 1];
	char **tav;
	time_t currtime;

	RecordCommand("%s: %s!%s@%s STATS",
	              n_StatServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname);

	avgops = Network->TotalOperators / Network->TotalServers;
	if (Network->TotalChannels > 0.0)
		avguc = Network->TotalUsers / Network->TotalChannels;
	else
		avguc = 0;
	avgus = Network->TotalUsers / Network->TotalServers;

	notice(n_StatServ, lptr->nick,
	       "Current Users:              %1.0f (avg. %1.2f users per server)",
	       Network->TotalUsers,
	       avgus);
	notice(n_StatServ, lptr->nick,
	       "Current Operators:          %1.0f (avg. %1.2f operators per server)",
	       Network->TotalOperators,
	       avgops);
	notice(n_StatServ, lptr->nick,
	       "Current Channels:           %1.0f (avg. %1.2f users per channel)",
	       Network->TotalChannels,
	       avguc);
	notice(n_StatServ, lptr->nick,
	       "Current Servers:            %1.0f",
	       Network->TotalServers);

	strlcpy(str, ctime(&Network->MaxUsers_ts), sizeof(str));
	str[strlen(str) - 1] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Users:                  %ld on %s",
	       Network->MaxUsers,
	       str);

	if (Network->MaxOperators_ts)
	{
		strlcpy(str, "on ", sizeof(str));
		strlcat(str, ctime(&Network->MaxOperators_ts), sizeof(str));
		str[strlen(str) - 1] = '\0';
	}
	else
		str[0] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Operators:              %ld %s",
	       Network->MaxOperators,
	       str);

	if (Network->MaxChannels_ts)
	{
		strlcpy(str, "on ", sizeof(str));
		strlcat(str, ctime(&Network->MaxChannels_ts), sizeof(str));
		str[strlen(str) - 1] = '\0';
	}
	else
		str[0] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Channels:               %ld %s",
	       Network->MaxChannels,
	       str);

	strlcpy(str, ctime(&Network->MaxServers_ts), sizeof(str));
	str[strlen(str) - 1] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Servers:                %ld on %s",
	       Network->MaxServers,
	       str);

	notice(n_StatServ, lptr->nick,
	       "Identd Users:               %ld",
	       Network->Identd);
	notice(n_StatServ, lptr->nick,
	       "Non-Identd Users:           %ld",
	       Network->NonIdentd);
	notice(n_StatServ, lptr->nick,
	       "Resolving Host Users:       %ld",
	       Network->ResHosts);
	notice(n_StatServ, lptr->nick,
	       "Non-Resolving Host Users:   %ld",
	       (long) Network->TotalUsers - Network->ResHosts);

	currtime = current_ts;
	strlcpy(tmp, ctime(&currtime), sizeof(tmp));
	SplitBuf(tmp, &tav);
	ircsprintf(str, "%s %s %s, %s", tav[0], tav[1], tav[2], tav[4]);
	notice(n_StatServ, lptr->nick,
	       "-- \002So far today:\002 (%s) --",
	       str);
	MyFree(tav);

	if (Network->MaxUsersT_ts)
	{
		tmp_tm = localtime(&Network->MaxUsersT_ts);
		ircsprintf(str, "at %d:%02d:%02d", tmp_tm->tm_hour, tmp_tm->tm_min,
		           tmp_tm->tm_sec);
	}
	else
		str[0] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Users:                  %ld %s",
	       Network->MaxUsersT,
	       str);

	if (Network->MaxOperatorsT_ts)
	{
		tmp_tm = localtime(&Network->MaxOperatorsT_ts);
		ircsprintf(str, "at %d:%02d:%02d", tmp_tm->tm_hour, tmp_tm->tm_min,
		           tmp_tm->tm_sec);
	}
	else
		str[0] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Operators:              %ld %s",
	       Network->MaxOperatorsT,
	       str);

	if (Network->MaxChannelsT_ts)
	{
		tmp_tm = localtime(&Network->MaxChannelsT_ts);
		ircsprintf(str, "at %d:%02d:%02d", tmp_tm->tm_hour, tmp_tm->tm_min,
		           tmp_tm->tm_sec);
	}
	else
		str[0] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Channels:               %ld %s",
	       Network->MaxChannelsT,
	       str);

	if (Network->MaxServersT_ts)
	{
		tmp_tm = localtime(&Network->MaxServersT_ts);
		ircsprintf(str, "at %d:%02d:%02d", tmp_tm->tm_hour, tmp_tm->tm_min,
		           tmp_tm->tm_sec);
	}
	else
		str[0] = '\0';
	notice(n_StatServ, lptr->nick,
	       "Max Servers:                %ld %s",
	       Network->MaxServersT,
	       str);

	notice(n_StatServ, lptr->nick,
	       "Operator Kills:             %ld",
	       Network->OperKillsT);
	notice(n_StatServ, lptr->nick,
	       "Server Kills:               %ld",
	       Network->ServKillsT);
} /* ss_stats() */

/*
ss_help()
  Give lptr help on command av[1]
*/

static void
ss_help(struct Luser *lptr, int ac, char **av)

{
	if (ac >= 2)
	{
		char str[MAXLINE + 1];
		struct Command *sptr;

		for (sptr = statcmds; sptr->cmd; sptr++)
			if (!irccmp(av[1], sptr->cmd))
				break;

		if (sptr->cmd)
			if ((sptr->level == LVL_ADMIN) &&
			        !(IsValidAdmin(lptr)))
			{
				notice(n_StatServ, lptr->nick,
				       "No help available on \002%s\002",
				       av[1]);
				return;
			}

		ircsprintf(str, "%s", av[1]);

		GiveHelp(n_StatServ, lptr->nick, str, NODCC);
	}
	else
		GiveHelp(n_StatServ, lptr->nick, NULL, NODCC);
} /* ss_help() */

/*
ss_refresh()
 Re-ping all servers on the network in order to update lag
information
*/

static void
ss_refresh(struct Luser *lptr, int ac, char **av)

{
	if (!LagDetect)
	{
		notice(n_StatServ, lptr->nick,
		       "Lag Detection is disabled");
		return;
	}

	RecordCommand("%s: %s!%s@%s REFRESH",
	              n_StatServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname);

	notice(n_StatServ, lptr->nick,
	       "Updating server lag information");

	DoPings();
} /* ss_refresh() */

/*
ss_clearstats()
 Clear host/domain statistics
Valid options:
 -domain   - Delete all domain entries
 -host     - Delete all hostname entries
 -all      - Delete both domain and hostname entries (default)
*/

static void
ss_clearstats(struct Luser *lptr, int ac, char **av)

{
	int ii, bad;
	struct HostHash *hosth, *hnext;
	char buf[MAXLINE + 1],
	buf2[MAXLINE + 1];
	int domain,
	host;

	domain = 0;
	host = 0;

	if (ac >= 2)
	{
		for (ii = 1; ii < ac; ii++)
		{
			if (!ircncmp(av[ii], "-domain", strlen(av[ii])))
				domain = 1;
			else if (!ircncmp(av[ii], "-host", strlen(av[ii])))
				host = 1;
			else if (!ircncmp(av[ii], "-all", strlen(av[ii])))
				host = domain = 1;
		}
	}
	else
	{
		/*
		 * They didn't give an option - delete everything by
		 * default
		 */
		host = domain = 1;
	}

	buf[0] = '\0';
	buf2[0] = '\0';

	if (domain && host)
	{
		strlcat(buf, "-all", sizeof(buf));
		strlcat(buf2, "All", sizeof(buf2));
	}
	else
	{
		if (domain)
		{
			strlcat(buf, "-domain", sizeof(buf));
			strlcat(buf2, "Domains", sizeof(buf2));
		}

		if (host)
		{
			strlcat(buf, "-host", sizeof(buf));
			strlcat(buf2, "Hostnames", sizeof(buf2));
		}
	}

	RecordCommand("%s: %s!%s@%s CLEARSTATS %s",
	              n_StatServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              buf);

	for (ii = 0; ii < HASHCLIENTS; ii++)
	{
		bad = 0;
		hosth = hostTable[ii].list;
		while (hosth)
		{
			if (!domain && (hosth->flags & SS_DOMAIN))
				bad = 1;
			if (!host && !(hosth->flags & SS_DOMAIN))
				bad = 1;

			if (bad)
			{
				bad = 0;
				hosth = hosth->hnext;
				continue;
			}

			hnext = hosth->hnext;
			MyFree(hosth->hostname);
			MyFree(hosth);

			/*
			 * Make sure we set hostTable[ii].list to hnext as well
			 * or it will not be set to NULL at the end of the list,
			 * which could cause big problems
			 */
			hostTable[ii].list = hosth = hnext;
		}
	}

	notice(n_StatServ, lptr->nick,
	       "Statistics cleared: %s",
	       buf2);
} /* ss_clearstats() */

/*
ss_showstats()
 Show nick/chan/memo statistics
*/

static void
ss_showstats(struct Luser *lptr, int ac, char **av)

{
	RecordCommand("%s: %s!%s@%s SHOWSTATS",
	              n_StatServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname);

#ifdef NICKSERVICES

	notice(n_StatServ, lptr->nick,
	       "Registered Nicknames: %d",
	       Network->TotalNicks);

#ifdef CHANNELSERVICES

	notice(n_StatServ, lptr->nick,
	       "Registered Channels:  %d",
	       Network->TotalChans);

#endif /* CHANNELSERVICES */

#ifdef MEMOSERVICES

	notice(n_StatServ, lptr->nick,
	       "Total Memos:          %d",
	       Network->TotalMemos);

#endif /* MEMOSERVICES */

#endif /* NICKSERVICES */
} /* ss_showstats() */



/*
 * static void ss_greplog() 
 * 
 * just proccess user request for grep of logs and start thread for grep
 * also we make some checks here for data validity  
 */

static void
ss_greplog(struct Luser *lptr, int ac, char **av )
{
	int             day;
	char            buf[MAXLINE + 1];
	char            date[10];
	FILE *          lf;
	char            grep_log_filename[MAXLINE + 1];
	int             i;
	struct tm *     tm;
	int             iCounter;

	if (ac < 3)
	{
		notice(n_StatServ,lptr->nick,
		       "Syntax: \002GREPLOG <service> <pattern> [days]\002");
		return;
	}

	if (ac > 3 && av[3] != NULL)
		day = atoi(av[3]);
	else
		day = 0;

	if (day < 0)
	{
		notice(n_StatServ, lptr->nick, "Day count should be positive.");
		return;
	}

	if (!GetService(av[1]))
	{
		notice(n_StatServ, lptr->nick, "Invalid service!");
		return;
	}

	iCounter = 0;
	tm = localtime(&current_ts);

	RecordCommand("%s: %s!%s@%s GREPLOG [%s %s] %s", n_StatServ, lptr->nick,
			lptr->username, lptr->hostname,
			av[1], av[2], (ac >= 4) ? av[3] : "");

	ircsprintf(date, "%4.4d%2.2d%2.2d",
	           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

	for (i = 0; i <= day; i++)
	{
		if (i == 0)
			ircsprintf(grep_log_filename, "%s/%s", LogPath, LogFile);
		else
			ircsprintf(grep_log_filename, "%s/%s.%8.8ld", LogPath, LogFile,
			           korectdat(atol(date), -i));

		if ((lf = fopen(grep_log_filename, "r")) == NULL)
		{
			notice(n_StatServ, lptr->nick, "No log file: %s",
			       grep_log_filename);
			continue;
		}
		notice(n_StatServ, lptr->nick,
		       "Searching for [%s] with service [%s] for [%d] days in file [%s]",
		       av[2], av[1], day, grep_log_filename);

		while (fgets(buf, sizeof(buf), lf))
		{
			if ((ircncmp(buf + 25, av[1], strlen(av[1])) == 0)
			        && match(av[2], buf + 25 + strlen(av[1]) + 2))
			{
				iCounter ++;
				notice(n_StatServ, lptr->nick,
				       "[%d] ... %s", iCounter, buf);
			}
		}
		fclose(lf);
	}
	notice(n_StatServ, lptr->nick, "End of search." );

	return;
} /* o_greplog */

/*
 * long korectdate()
 *
 * get date before / or after dat1 with dni days
 */
long korectdat( long dat1, int dni)
{
	int g, m, d;
	long olddays, newdays, days;
	int *tg;
	int i, days_in_year;

	g = dat1 / 10000;
	if ( g <= 99 )
		g += 1900;
	m = ( dat1 % 10000 ) / 100;
	d = dat1 % 100;

	olddays = (long)(--g) * 365L;
	olddays += ( g / 4 - g / 100 + g / 400 );

	tg = get_tg( ++g, &days_in_year );
	for( i = 0; i < m - 1; ++i )
		olddays += tg[i];
	olddays += d;

	newdays = olddays + dni;

	for( g = 1, days = 0; days < newdays; ++g )
	{
		tg = get_tg( g, &days_in_year );
		days += days_in_year;
	}
	days -= days_in_year;
	g -= 1;

	for( m = 0; days + tg[m] < newdays; ++m )
		days += tg[m];

	d = newdays - days;
	return( (long)g * 10000L + (long)( m + 1 ) * 100 + d );
}

int* get_tg( int year, int *days )
{
	*days = 365;
	if( year % 4 )
		return( gnw );
	if( !( year % 100 ) && ( year % 400 ) )
		return( gnw );
	*days = 366;
	return gw;
}

#ifdef SPLIT_INFO
/* Accepts these parameters:
 * a) no parameters - to display split data for all servers currently in
 *    netsplit
 * b) av[1] - to display split data for current server
 * -kre
 */
static void ss_splitinfo(struct Luser *lptr, int ac, char **av)
{
	struct Server *tmpserv;
	char sMsg[] = "%-30s currently in \002netsplit\002 for %s";
	int issplit = 0;

	if (ac < 2)
	{
		for (tmpserv = ServerList; tmpserv; tmpserv = tmpserv->next)
			if (tmpserv->split_ts)
			{
				notice(n_StatServ, lptr->nick, sMsg, tmpserv->name,
				       timeago(tmpserv->split_ts, 0));
				issplit = 1;
			}
	}
	else
	{
		if ((tmpserv = FindServer(av[1])))
		{
			if (tmpserv->split_ts)
			{
				notice(n_StatServ, lptr->nick, sMsg, tmpserv->name,
				       timeago(tmpserv->split_ts, 0));
				issplit = 1;
			}
		}
		else
			notice(n_StatServ, lptr->nick,
			       "Invalid server %s!", av[1]);
	}

	if (!issplit)
		notice(n_StatServ, lptr->nick,
		       "No split for specified server or no active splits");
}

#endif /* SPLIT_INFO */


/*
 * Show services adminstrators. Code from IrcBg, slightly modified. -kre
 */
static void ss_showadmins(struct Luser *lptr, int ac, char **av)
{
	int iCnt = 0;
	struct Luser *tempuser;

	RecordCommand("%s: %s!%s@%s SHOWADMINS",
	              n_StatServ, lptr->nick, lptr->username, lptr->hostname);

	notice(n_StatServ, lptr->nick, "Currently online services admins");
	notice(n_StatServ, lptr->nick, "--------------------------------");

	for (tempuser = ClientList; tempuser; tempuser = tempuser->next)
	{
		if (IsValidAdmin(tempuser))
			notice(n_StatServ, lptr->nick , "[%d] %s", ++iCnt,tempuser->nick );
	}

	notice(n_StatServ, lptr->nick, "--------------------------------");
	notice(n_StatServ, lptr->nick, " %d admins online.", iCnt);
}

/*
 * Show operators. Code from IrcBg, slightly modified. -kre
 */
static void ss_showopers(struct Luser *lptr, int ac, char **av)
{
	int iCnt = 0;
	struct Luser *tempuser;

	RecordCommand("%s: %s!%s@%s SHOWOPERS",
	              n_StatServ, lptr->nick, lptr->username, lptr->hostname);

	notice(n_StatServ, lptr->nick, "Currently online irc operators");
	notice(n_StatServ, lptr->nick, "-------------------------------");

	for (tempuser = ClientList; tempuser; tempuser = tempuser->next)
	{
		if (IsOperator(tempuser))
			notice(n_StatServ, lptr->nick , "[%d] %s", ++iCnt,tempuser->nick );
	}

	notice(n_StatServ, lptr->nick, "-------------------------------");
	notice(n_StatServ, lptr->nick, " %d operators online.", iCnt);
}

#endif /* STATSERVICES */
