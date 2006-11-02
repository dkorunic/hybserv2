/*
 * Hybserv2 Services by Hybserv2 team
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 * $Id$
 */

#include "stdinc.h"
#include "alloc.h"
#include "client.h"
#include "conf.h"
#include "config.h"
#include "hash.h"
#include "hybdefs.h"
#include "jupe.h"
#include "match.h"
#include "mystring.h"
#include "server.h"
#include "sock.h"
#include "timestr.h"
#include "misc.h"
#include "sprintf_irc.h"

#ifdef ALLOW_JUPES

/*
 * Global - list of jupes
 */
struct Jupe       *JupeList = NULL;
#ifdef JUPEVOTES
struct JupeVote   *VoteList = NULL;
#endif

/*
AddJupe() 
  Add jupe 'name' to list with 'reason'
*/

void
AddJupe(char *name, char *reason, char *who)

{
	struct Jupe *tempjupe;
	unsigned int ii;
	int nickjupe = 1;

	for (ii = 0; ii < strlen(name); ++ii)
	{
		if (IsKWildChar(name[ii]))
		{
			nickjupe = 0;
			break;
		}
	}

	tempjupe = (struct Jupe *) MyMalloc(sizeof(struct Jupe));
	memset(tempjupe, 0, sizeof(struct Jupe));

	tempjupe->name = MyStrdup(name);
	tempjupe->reason = MyStrdup(reason);
	tempjupe->who = MyStrdup(who);
	tempjupe->isnick = nickjupe;

	tempjupe->prev = NULL;
	tempjupe->next = JupeList;
	if (tempjupe->next)
		tempjupe->next->prev = tempjupe;
	JupeList = tempjupe;

	++Network->TotalJupes;
} /* AddJupe() */

/*
DeleteJupe()
  Remove jptr from jupe list
*/

void
DeleteJupe(struct Jupe *jptr)

{
	if (!jptr)
		return;

	MyFree(jptr->name);
	MyFree(jptr->reason);
	MyFree(jptr->who);

	if (jptr->prev)
		jptr->prev->next = jptr->next;
	else
		JupeList = jptr->next;

	if (jptr->next)
		jptr->next->prev = jptr->prev;

	MyFree(jptr);

	Network->TotalJupes--;
} /* DeleteJupe() */

/*
CheckJupes()
 Called after rehash to check if there are any pseudo-servers
which no longer have a J: line. SQUIT any we find
*/

void
CheckJupes()

{
	struct Server *tempserv, *next;

	if (!Me.sptr)
		return;

	for (tempserv = ServerList; tempserv; )
	{
		/*
		 * Make sure tempserv is using us as an uplink, which
		 * would qualify it as a pseudo-server. But also make
		 * sure it's not OUR current hub server - we never
		 * want to squit our hub.
		 */
		if ((tempserv->uplink == Me.sptr) &&
		        (Me.hub != tempserv))
		{
			/*
			 * We found a server who has us as a hub - check if there
			 * is a J: line for it
			 */
			if (!IsJupe(tempserv->name))
			{
				toserv("SQUIT %s :Unjuped\r\n", tempserv->name);

				next = tempserv->next;
				DeleteServer(tempserv);
				tempserv = next;
			}
			else
				tempserv = tempserv->next;
		}
		else
			tempserv = tempserv->next;
	}
} /* CheckJupes() */

/*
DoJupeSquit()
  SQUIT all servers matching serv and create a fake server to take its
place
*/

void
DoJupeSquit(char *serv, char *reason, char *who)

{
	struct Server *tempserv, *prev;
	char sendstr[MAXLINE + 1], **arv;
	int acnt;

	for (tempserv = ServerList; tempserv; )
	{
		if (match(serv, tempserv->name))
		{
			/* squit the server */
			toserv("SQUIT %s :Juped: %s (%s)\r\n", tempserv->name, reason,
			       who);

			prev = tempserv->next;
			DeleteServer(tempserv); /* remove server from list */
			tempserv = prev;
		}
		else
			tempserv = tempserv->next;
	}

	/* add a fake server to replace it */
	ircsprintf(sendstr, ":%s SERVER %s 2 :Juped: %s\r\n", Me.name, serv,
	           reason);

	toserv("%s", sendstr);

	acnt = SplitBuf(sendstr, &arv);
	AddServer(acnt, arv);

	MyFree(arv);
} /* DoJupeSquit() */

/*
CheckJuped()
  args: char *name
  purpose: check if 'name' is "juped" (ie, banned from the
           network).  If so, SQUIT/KILL it.
  return: 1 if 'name' matches a jupe, 0 if not
*/

int
CheckJuped(char *name)

{
	struct Jupe *tempjupe;
	struct Server *tempserv;
	char sendstr[MAXLINE + 1], **arv;

	for (tempjupe = JupeList; tempjupe; tempjupe = tempjupe->next)
	{
		if (match(tempjupe->name, name))
		{
			if (tempjupe->isnick)
			{
				struct Luser *lptr;

				if (!(lptr = FindClient(name)))
					return 0;

				/* its a nick jupe, not a server jupe */

#ifdef DANCER

				ircsprintf(sendstr,
				           "NICK %s 1 %ld +i juped juped.com %s %lu :%s\r\n",
				           tempjupe->name,
#ifdef NICKSERVICES
				           (long) (lptr->nick_ts - 1),
#else
				           (long) (lptr->since - 1),
#endif /* NICKSERVICES */
				           Me.name, 0xffffffffL, tempjupe->reason ?
				           tempjupe->reason : "Jupitered Nickname");
#else
				/* collide the nickname */
				ircsprintf(sendstr, "NICK %s 1 %ld +i %s %s %s :%s\r\n",
				           tempjupe->name,
#ifdef NICKSERVICES
				           (long) (lptr->nick_ts - 1),
#else
				(long) (lptr->since - 1),
#endif /* NICKSERVICES */
				           JUPED_USERNAME, JUPED_HOSTNAME, Me.name,
				           tempjupe->reason ? tempjupe->reason :
				           "Jupitered Nickname");
#endif /* DANCER */

				toserv("%s", sendstr);

				DeleteClient(lptr);

				SplitBuf(sendstr, &arv);
				AddClient(arv);
				MyFree(arv);

				if (Me.sptr)
					Me.sptr->numoperkills++;
				Network->TotalOperKills++;
#ifdef STATSERVICES

				if (Network->TotalOperKills > Network->OperKillsT)
					Network->OperKillsT = Network->TotalOperKills;
#endif

			}
			else
			{
				toserv("SQUIT %s :Juped: %s (%s)\r\n", name,
				       tempjupe->reason, tempjupe->who);

				tempserv = FindServer(name);
				DeleteServer(tempserv);

				/* replace it with fake server */
				ircsprintf(sendstr, ":%s SERVER %s 2 :Juped: %s\r\n",
				           Me.name, name, tempjupe->reason);
				toserv("%s", sendstr);
				SplitBuf(sendstr, &arv);

				AddServer(5, arv);
				MyFree(arv);
			}
			return 1;
		}
	}
	return 0;
} /* CheckJuped */

/*
IsJupe()
  Determine if 'hostname' has a J: line, and return a pointer
to the structure if so
*/

struct Jupe *
			IsJupe(char *hostname)

{
	struct Jupe *tempjupe;

	if (!hostname)
		return (NULL);

	for (tempjupe = JupeList; tempjupe; tempjupe = tempjupe->next)
		if (match(tempjupe->name, hostname))
			return (tempjupe);

	return (NULL);
} /* IsJupe() */

/*
InitJupes()
  Called when we successfully connect to a hub - introduce juped
servers and nicknames so they cannot connect
*/

void
InitJupes()

{
	struct Jupe *tmpjupe;
	char sendstr[MAXLINE + 1];
	char **av;

	for (tmpjupe = JupeList; tmpjupe; tmpjupe = tmpjupe->next)
	{
		if (tmpjupe->isnick)
		{
#ifdef DANCER
			ircsprintf(sendstr, "NICK %s 1 1 +i juped juped.com %s :%s\r\n",
			           tmpjupe->name, Me.name, tmpjupe->reason ? tmpjupe->reason :
			           "Jupitered Nickname");
#else
			/* collide the nickname */
			ircsprintf(sendstr, "NICK %s 1 1 +i %s %s %s :%s\r\n",
			           tmpjupe->name, JUPED_USERNAME, JUPED_HOSTNAME, Me.name,
			           tmpjupe->reason ? tmpjupe->reason : "Jupitered Nickname");
#endif /* DANCER */

			toserv("%s", sendstr);

			SplitBuf(sendstr, &av);
			AddClient(av);
			MyFree(av);
		}
		else
		{
			ircsprintf(sendstr, ":%s SERVER %s 2 :Juped: %s", Me.name,
			           tmpjupe->name, tmpjupe->reason);

			toserv(":%s SQUIT %s :%s (%s)\r\n%s\r\n",
			       Me.name,
			       tmpjupe->name,
			       tmpjupe->reason,
			       tmpjupe->who,
			       sendstr);

			SplitBuf(sendstr, &av);
			AddServer(5, av);
			MyFree(av);
		}
	}
} /* InitJupes() */

#ifdef JUPEVOTES
/*
 * AddVote()
 * adds a vote, returns -1 if they already voted, or 0
 * if it's added
 */
int AddVote(char *name, char *who)
{
	struct JupeVote *tempvote, *votematch = NULL;
	int exists = 0;
	int i;
	time_t unixtime = current_ts;

	/* check if someone voted for this server already */
	for (tempvote = VoteList; tempvote; tempvote = tempvote->next)
		if (match(name, tempvote->name))
		{
			exists = 1;
			for (i = 0; i < JUPEVOTES; i++)
				/* see if they already voted for this server */
				if (tempvote->who[i] && match(who, tempvote->who[i]))
					return -1;
			votematch = tempvote;
		}

	if(exists) /* add vote to total */
	{
		votematch->who[votematch->count] = MyStrdup(who);
		votematch->count++;
		votematch->lasttime = unixtime;
	}
	else /* add new vote to linked list */
	{
		tempvote = (struct JupeVote *) MyMalloc(sizeof(struct JupeVote));
		memset(tempvote, 0, sizeof(struct JupeVote));

		tempvote->name = MyStrdup(name);
		tempvote->who[0] = MyStrdup(who);
		tempvote->count = 1;
		tempvote->lasttime = unixtime;

		tempvote->prev = NULL;
		tempvote->next = VoteList;
		if (tempvote->next)
			tempvote->next->prev = tempvote;
		VoteList = tempvote;
	}
	return 0;
}

/*
 * CountVotes(char *name)
 * returns how many votes a server has
 */

int CountVotes(char *name)
{
	struct JupeVote *tempvote;

	for (tempvote = VoteList; tempvote; tempvote = tempvote->next)
		if (match(name, tempvote->name))
			return tempvote->count;

	debug("CountVotes: ERROR!? NO votes found for %s",
	      name);
	return 0;
}

/* DeleteVote()
 * delete vote matching the name given
 */

void DeleteVote(char *name)
{
	struct JupeVote *tempvote;
	int i;

	for (tempvote = VoteList; tempvote; tempvote = tempvote->next)
	{
		if (match(name, tempvote->name))
		{
			MyFree(tempvote->name);
			for (i = 0; i < JUPEVOTES; i++)
				if(tempvote->who[i])
					MyFree(tempvote->who[i]);

			if (tempvote->prev)
				tempvote->prev->next = tempvote->next;
			else
				VoteList = tempvote->next;

			if (tempvote->next)
				tempvote->next->prev = tempvote->prev;

			MyFree(tempvote);
			return;
		}
	}
}

/* ExpireVotes()
 * expires any votes that are over 10 mins old,
 * called every minute by DoTimer()
 */

void ExpireVotes(time_t unixtime)
{
	struct JupeVote *tempvote;
	int i;

	for (tempvote = VoteList; tempvote; tempvote = tempvote->next)
	{
		if ((unixtime - tempvote->lasttime) > 600)
		{
			debug("ExpireVotes: expiring %s",
			      tempvote->name);

			MyFree(tempvote->name);
			for (i = 0; i < JUPEVOTES; i++)
				if(tempvote->who[i])
					MyFree(tempvote->who[i]);

			if (tempvote->prev)
				tempvote->prev->next = tempvote->next;
			else
				VoteList = tempvote->next;

			if (tempvote->next)
				tempvote->next->prev = tempvote->prev;

			MyFree(tempvote);
			return;
		}
	}
}
#endif /* JUPEVOTES */

#endif /* ALLOW_JUPES */
