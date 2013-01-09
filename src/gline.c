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
#include "conf.h"
#include "config.h"
#include "dcc.h"
#include "gline.h"
#include "hybdefs.h"
#include "match.h"
#include "operserv.h"
#include "server.h"
#include "settings.h"
#include "sock.h"

#ifdef ALLOW_GLINES

/*
 * Global - list of glines
 */
struct Gline       *GlineList = NULL;

/*
AddGline() 
  Add gline 'host' to list with 'reason'
*/

void
AddGline(char *host, char *reason, char *who, long when)

{
	struct Gline *tempgline;
	char *tmp;

	tempgline = (struct Gline *) MyMalloc(sizeof(struct Gline));

	if ((tmp = strchr(host, '@')))
	{
		*tmp++ = '\0';
		tempgline->username = MyStrdup(host);
		tempgline->hostname = MyStrdup(tmp);
	}
	else
	{
		tempgline->username = MyStrdup("*");
		tempgline->hostname = MyStrdup(host);
	}

	if (reason)
		tempgline->reason = MyStrdup(reason);
	else
		tempgline->reason = MyStrdup("");

	tempgline->who = MyStrdup(who);

	/* assume when to be in seconds */
	if (!when)
		tempgline->expires = 0;
	else
		tempgline->expires = current_ts + when;

	tempgline->next = GlineList;
	tempgline->prev = NULL;
	if (tempgline->next)
		tempgline->next->prev = tempgline;
	GlineList = tempgline;

	Network->TotalGlines++;
} /* AddGline() */

/*
DeleteGline()
  Remove gptr from gline list
*/

void
DeleteGline(struct Gline *gptr)

{
	if (!gptr)
		return;

	MyFree(gptr->username);
	MyFree(gptr->hostname);
	MyFree(gptr->reason);
	MyFree(gptr->who);

	if (gptr->prev)
		gptr->prev->next = gptr->next;
	else
		GlineList = gptr->next;

	if (gptr->next)
		gptr->next->prev = gptr->prev;

	MyFree(gptr);

	Network->TotalGlines--;
} /* DeleteGline() */

/*
IsGline()
 Return a pointer to gline structure containing the given
username and hostname, if any
*/

struct Gline *
			IsGline(char *username, char *hostname)

{
	struct Gline *temp;

	if (!username || !hostname)
		return (NULL);

	for (temp = GlineList; temp; temp = temp->next)
	{
		if (match(temp->username, username) &&
		        match(temp->hostname, hostname))
			return (temp);
	}

	return (NULL);
} /* IsGline() */

/*
CheckGlined()
  args: struct Luser *lptr
  purpose: determine whether lptr is GLINED from the network;
           if so, issue a KILL for lptr->nick with the reason they are
           GLINED
  return: none
*/

void
CheckGlined(struct Luser *lptr)

{
	struct Gline *tempgline;

	if (!lptr)
		return;

	/*
	 * Make sure we don't kill one of ours
	 */
	if (lptr->server == Me.sptr)
		return;

	if ((tempgline = IsGline(lptr->username, lptr->hostname)))
	{
		toserv(":%s KILL %s :%s!%s (Glined: %s (%s))\r\n",
		       n_OperServ, lptr->nick, Me.name, n_OperServ,
		       tempgline->reason, tempgline->who);

		DeleteClient(lptr);

		if (Me.sptr)
			Me.sptr->numoperkills++;
		Network->TotalOperKills++;

#ifdef STATSERVICES

		if (Network->TotalOperKills > Network->OperKillsT)
			Network->OperKillsT = Network->TotalOperKills;
#endif

#ifdef HYBRID_GLINES

		ExecuteGline(tempgline->username, tempgline->hostname,
		             tempgline->reason);
#endif /* HYBRID_GLINES */

#ifdef HYBRID7_GLINES

		Execute7Gline(tempgline->username, tempgline->hostname,
		              tempgline->reason,
		              tempgline->expires ? (tempgline->expires - current_ts) : 0);
#endif /* HYBRID7_GLINES */

	}
} /* CheckGlined */

#ifdef HYBRID_GLINES

/*
ExecuteGline()
 Attempt to send a global gline with the given parameters.
 
 Now, ircd-hybrid's gline implentation requires three
different opers on three different servers to request
the same gline in order to activate it. As of 08/14/1999,
ircd-hybrid does not check the validity of the operator
nicknames or server names, so we don't even need to
introduce 3 fake servers
*/

void
ExecuteGline(char *username, char *hostname, char *reason)

{
	toserv(":%s GLINE Gliner1 gliner1 gliner1.com pseudo1.org %s %s :%s\r\n",
	       Me.name, username ? username : "*", hostname, reason);

	toserv(":%s GLINE Gliner2 gliner2 gliner2.com pseudo2.org %s %s :%s\r\n",
	       Me.name, username ? username : "*", hostname, reason);

	toserv(":%s GLINE Gliner3 gliner3 gliner3.com pseudo3.org %s %s :%s\r\n",
	       Me.name, username ? username : "*", hostname, reason);
} /* ExecuteGline() */

#endif /* HYBRID_GLINES */

#ifdef HYBRID7_GLINES
/*
 * Execute7Gline()
 * Send a KLINE user host on * :reason (effectively a gline)
 * 
 * :SERVER kline OPERNICK TARGET_SERVER DURATION USER HOST REASON
 */
void Execute7Gline(char *username, char *hostname, char *reason, time_t
                   time)
{
	toserv(":%s KLINE %s %lu %s %s :%s\r\n",
	       n_OperServ, "*", time, username ? username : "*", hostname,
	       reason);
} /* Execute7Gline() */

#endif /* HYBRID7_GLINES */

/*
ExpireGlines()
 Delete any glines that have expired
*/

void
ExpireGlines(time_t unixtime)

{
	struct Gline *tempgline, *prev;

	prev = NULL;
	for (tempgline = GlineList; tempgline; )
	{
		if ((tempgline->expires) && (tempgline->expires <= unixtime))
		{
			SendUmode(OPERUMODE_Y,
			          "*** Expired gline %s@%s [%s]",
			          tempgline->username,
			          tempgline->hostname,
			          tempgline->reason);

			if (prev)
			{
				prev->next = tempgline->next;
				DeleteGline(tempgline);
				tempgline = prev;
			}
			else
			{
				GlineList = tempgline->next;
				DeleteGline(tempgline);
				tempgline = NULL;
			}
		}

		prev = tempgline;

		if (tempgline)
			tempgline = tempgline->next;
		else
			tempgline = GlineList;
	}
} /* ExpireGlines() */

#endif /* ALLOW_GLINES */
