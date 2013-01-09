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
#include "channel.h"
#include "config.h"
#include "dcc.h"
#include "hash.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "operserv.h"
#include "server.h"
#include "settings.h"
#include "sock.h"
#include "statserv.h"
#include "sprintf_irc.h"
#include "mystring.h"

static aHashEntry clientTable[HASHCLIENTS];
static aHashEntry channelTable[HASHCHANNELS];
static aHashEntry serverTable[HASHSERVERS];
aHashEntry cloneTable[HASHCLIENTS];

#ifdef STATSERVICES
aHashEntry hostTable[HASHCLIENTS];
#endif

static void WarnClone(char *);
static unsigned int HashNick(const char *);
static unsigned int HashChannel(const char *);
static unsigned int HashServer(const char *);

/*
HashNick()
 Calculate a hash value for 'name'
 
 ** NOTE: This function is originally from ircd-hybrid source.
*/

static unsigned int
HashNick(const char *name)

{
	unsigned int h = 0;
#if 0

	while (*name)
	{
		h = (h << 4) - (h + (unsigned char) ToLower(*name++));
	}

	return (h & (HASHCLIENTS - 1));
#endif

	/* fix broken hash code -kre */
	while (*name)
	{
		h += ToLower(*name);
		name++;
	}
	return h % HASHCLIENTS;

} /* HashNick() */

#ifdef NICKSERVICES

/*
NSHashNick()
 Calculate a hash value for 'name', for NickServ's hash table
*/

unsigned int
NSHashNick(const char *name)

{
	unsigned int h = 0;
#if 0

	while (*name)
	{
		h = (h << 4) - (h + (unsigned char) ToLower(*name++));
	}

	return (h & (NICKLIST_MAX - 1));
#endif

	/* fix broken hash code -kre */
	while (*name)
	{
		h += ToLower(*name);
		name++;
	}

	return h % NICKLIST_MAX;

} /* NSHashNick() */

#endif /* NICKSERVICES */

/*
FindClient()
  Attempts to find client 'name' in hash table
  returns a pointer to client structure containing "name" or NULL
*/

struct Luser *FindClient(const char *name)
{
	struct Luser *tempuser;
	aHashEntry *temphash;
	int hashv;

	if (!name)
		return (NULL);

	hashv = HashNick(name);
	temphash = &clientTable[hashv];

	/*
	 * Got the bucket, now search the chain.
	 */
	for (tempuser = (struct Luser *)temphash->list; tempuser; tempuser =
	            tempuser->hnext)
		if (!irccmp(name, tempuser->nick))
			return(tempuser);

	return NULL;
} /* FindClient() */

/*
ClearHashes()
  Clear client/channel/clone/host hash tables
If (clearstats == 1), clear the StatServ hash tables as well - we
may not want to clear the StatServ hash tables every time we clear
the client/channel hashes, for example if we are reconnecting to a
hub after being squit'd
*/

void
ClearHashes(int clearstats)

{
	memset((void *) clientTable, 0, sizeof(aHashEntry) * HASHCLIENTS);
	memset((void *) channelTable, 0, sizeof(aHashEntry) * HASHCHANNELS);
	memset((void *) serverTable, 0, sizeof(aHashEntry) * HASHSERVERS);
	memset((void *) cloneTable, 0, sizeof(aHashEntry) * HASHCLIENTS);

#ifdef STATSERVICES

	if (clearstats)
		memset((void *) hostTable, 0, sizeof(aHashEntry) * HASHCLIENTS);
#endif
} /* ClearHashes() */

/*
CloneMatch()
 Determine if user1 and user2 are clones - try to match their
user@host (ignoring ~'s)
 Return 1 if the two users are clones, 0 if not
*/

int
CloneMatch(struct Luser *user1, struct Luser *user2)

{
	char *username1,
	*username2;

	if (!user1 || !user2)
		return 0;

	/*
	 * Make sure service pseudo nicks are not flagged as
	 * clones.
	 */
	if ((user1->server == Me.sptr) ||
	        (user2->server == Me.sptr))
		return (0);

	if (user1->username[0] == '~')
		username1 = user1->username + 1;
	else
		username1 = user1->username;

	if (user2->username[0] == '~')
		username2 = user2->username + 1;
	else
		username2 = user2->username;

	if (irccmp(username1, username2) != 0)
		return 0;

	if (irccmp(user1->hostname, user2->hostname) != 0)
		return 0;

	/* both the usernames and hostnames match - must be a clone */
	return 1;
} /* CloneMatch() */

/*
IsClone()
 Attempt to determine if lptr is a clone, by walking down their
bucket in cloneTable[] and matching user@host's
*/

int
IsClone(struct Luser *lptr)

{
	int hashv;
	char uhost[UHOSTLEN + 2];
	struct Luser *tmp;

	if (!lptr)
		return (0);

	/*
	 * Any service nicks, juped nicks, or enforcement nicks
	 * should not be considered clones, even if they
	 * have the same user@host
	 */
	if (lptr->server == Me.sptr)
		return (0);

	ircsprintf(uhost, "%s@%s",
	           (lptr->username[0] == '~') ? lptr->username + 1 : lptr->username,
	           lptr->hostname);

	hashv = HashUhost(uhost);
	for (tmp = cloneTable[hashv].list; tmp; tmp = tmp->cnext)
	{
		if (lptr == tmp)
			continue;

		if (CloneMatch(lptr, tmp))
			return (1);
	}

	/* no clones found */
	return (0);
} /* IsClone() */

/*
WarnClone()
 Send a message to 'nickname' warning them to remove their clones
*/

static void
WarnClone(char *nickname)

{
	if (!nickname || !MaxClonesWarning)
		return;

	notice(n_OperServ, nickname, MaxClonesWarning);
} /* WarnClone() */

/*
HashAddClient()
  Add client 'lptr' to the client hash table, also add lptr's userhost
to the clone hash table
 Return a pointer to the user that was added - do this so we
can return NULL if the user is killed due to AutoKillClones
*/

struct Luser *
			HashAddClient(struct Luser *lptr, int nickchange)

{
	int hashv;
	int foundclone = 0,
	                 killclones = 0;
	struct Luser *tempuser, *temp2;
#ifdef STATSERVICES

	struct HostHash *hosth, *domainh;
	char *hostname, *domain;
	time_t currtime;
	int isip;
#endif

	int notclone; /* if lptr has +e, don't mark them as a clone */
	struct rHost *rhostptr = NULL;
	char uhost[UHOSTLEN + 2];
	struct Userlist *userptr;

	if (!lptr)
		return ((struct Luser *) NULL);

	hashv = HashNick(lptr->nick);
	lptr->hnext = (struct Luser *)clientTable[hashv].list;
	clientTable[hashv].list = (void *)lptr;

	if (nickchange)
	{
		/*
		 * Since it is a nick change, the user was never
		 * removed from the clone or stat hashes - don't add
		 * them again or there will be duplicates
		 */
		return ((struct Luser *) NULL);
	}

	/*
	 * take the ~ out of the ident so in case they aren't running identd,
	 * we'll still pick out the clones
	 */
	if (lptr->username[0] == '~')
	{
		ircsprintf(uhost, "%s@%s", lptr->username + 1, lptr->hostname);

#ifdef STATSERVICES

		Network->NonIdentd++;
#endif

	}
	else
	{
		ircsprintf(uhost, "%s@%s", lptr->username, lptr->hostname);

#ifdef STATSERVICES

		Network->Identd++;
		if (lptr->server)
			lptr->server->numidentd++;
#endif

	}

#ifdef STATSERVICES

	hostname = lptr->hostname;
	currtime = current_ts;

	for (domain = hostname; *domain; domain++)
		;
	while (domain != hostname)
	{
		if (*domain == '.')
			break;
		domain--;
	}
	domain++;

	/*
	 * domain now points to the last segment of the hostname (TLD) -
	 * if its numerical, it must be an IP Address, not a hostname
	 */
	if ((*domain >= '0') && (*domain <= '9'))
		isip = 1;
	else
		isip = 0;

	if (!isip)
	{
		Network->ResHosts++;
		if (lptr->server)
			lptr->server->numreshosts++;
	}

	/* update the domain entry */

	if ((domain = GetDomain(hostname)))
	{
		if (!(domainh = FindDomain(domain)))
		{
			/*
			 * this is the first client from this particular domain, make a
			 * new entry
			 */
			domainh = (struct HostHash *) MyMalloc(sizeof(struct HostHash));
			domainh->hostname = MyStrdup(domain);
			domainh->flags = SS_DOMAIN;
			domainh->currclients = domainh->currunique = domainh->maxclients = domainh->maxunique = 0;
			domainh->lastseen = domainh->maxclients_ts = domainh->maxunique_ts = currtime;
			domainh->curropers = domainh->maxopers = domainh->maxopers_ts = 0;

			domainh->curridentd = 0;

			hashv = HashUhost(domain);
			domainh->hnext = hostTable[hashv].list;
			hostTable[hashv].list = domainh;
		}

		/*
		 * check if there are any clients from the same host - if so,
		 * update maxclient count etc
		 */
		if (!(hosth = FindHost(hostname)))
		{
			/*
			 * this is the first client from this particular host, make a
			 * new entry
			 */
			hosth = (struct HostHash *) MyMalloc(sizeof(struct HostHash));
			hosth->hostname = MyStrdup(hostname);
			hosth->flags = 0;
			hosth->currclients = hosth->currunique = hosth->maxclients = hosth->maxunique = 1;
			hosth->lastseen = hosth->maxclients_ts = hosth->maxunique_ts = currtime;

			if (lptr->username[0] == '~')
				hosth->curridentd = 0;
			else
				hosth->curridentd = 1;

			if (IsOperator(lptr))
			{
				hosth->curropers = hosth->maxopers = 1;
				hosth->maxopers_ts = currtime;

				domainh->curropers++;
				if (domainh->curropers > domainh->maxopers)
				{
					domainh->maxopers = domainh->curropers;
					domainh->maxopers_ts = currtime;
				}
			}
			else
				hosth->curropers = hosth->maxopers = hosth->maxopers_ts = 0;

			hashv = HashUhost(hostname);
			hosth->hnext = hostTable[hashv].list;
			hostTable[hashv].list = hosth;

			/*
			 * its a unique hostname, so increment domain client/unique count
			 */
			domainh->currclients++;
			if (domainh->currclients > domainh->maxclients)
			{
				domainh->maxclients = domainh->currclients;
				domainh->maxclients_ts = currtime;
			}

			if (lptr->username[0] != '~')
				domainh->curridentd++;

			domainh->currunique++;
			if (domainh->currunique > domainh->maxunique)
			{
				domainh->maxunique = domainh->currunique;
				domainh->maxunique_ts = currtime;
			}
		}
		else /* if (!(hosth = FindHost(hostname))) */
		{
			int clonematches = 0;

			/*
			 * hosth points to the structure with the hostname, check if
			 * the new client will break any maxclient/maxunique/maxoper
			 * records
			 */
			hosth->currclients++;
			domainh->currclients++;

			if (lptr->username[0] != '~')
			{
				hosth->curridentd++;
				domainh->curridentd++;
			}

#if 0
			/*
			 * If services is squit'd, the unique count is not reset,
			 * so when it rejoins, the unique count will be higher than
			 * it should be, so when the first client is added, set
			 * the unique count to 0, and it will be incremented in the
			 * below loop
			 */
			if (hosth->currclients == 1)
				hosth->currunique = 0;
#endif

			hosth->lastseen = domainh->lastseen = currtime;
			if (hosth->currclients > hosth->maxclients)
			{
				hosth->maxclients = hosth->currclients;
				hosth->maxclients_ts = currtime;
			}
			if (domainh->currclients > domainh->maxclients)
			{
				domainh->maxclients = domainh->currclients;
				domainh->maxclients_ts = currtime;
			}

			if (IsOperator(lptr))
			{
				hosth->curropers++;
				if (hosth->curropers > hosth->maxopers)
				{
					hosth->maxopers = hosth->curropers;
					hosth->maxopers_ts = currtime;
				}

				domainh->curropers++;
				if (domainh->curropers > domainh->maxopers)
				{
					domainh->maxopers = domainh->curropers;
					domainh->maxopers_ts = currtime;
				}
			}

			/*
			 * go through clone table and find out how many clients
			 * match the new client - if there are any matches, subtract
			 * the number from the current unique client count, otherwise
			 * increment the unique client count by 1
			 */

			hashv = HashUhost(uhost);
			for (tempuser = cloneTable[hashv].list; tempuser; tempuser = tempuser->cnext)
			{
				if (CloneMatch(lptr, tempuser))
					clonematches++;
			}

			if (!clonematches)
			{
				/* lptr is not a clone, increase the unique count by 1 */
				hosth->currunique++;
				if (hosth->currunique > hosth->maxunique)
				{
					hosth->maxunique = hosth->currunique;
					hosth->maxunique_ts = currtime;
				}

				domainh->currunique++;
				if (domainh->currunique > domainh->maxunique)
				{
					domainh->maxunique = domainh->currunique;
					domainh->maxunique_ts = currtime;
				}
			}
			else
				if (clonematches == 1)
				{
					/*
					 * lptr has one clone, decrement unique count
					 */
					hosth->currunique--;
					domainh->currunique--;
				}
		} /* else [if (!(hosth = FindHost(hostname)))] */
	} /* if ((domain = GetDomain(hostname))) */

#endif /* STATSERVICES */

	/* now add them to the clone table */

	hashv = HashUhost(uhost);

	temp2 = cloneTable[hashv].list;

	/*
	 * Make sure we don't treat "e" flag users as clones
	 */
	if (lptr->flags & L_OSREGISTERED)
		userptr = GetUser(1, lptr->nick, lptr->username, lptr->hostname);
	else
		userptr = GetUser(0, lptr->nick, lptr->username, lptr->hostname);

	notclone = CheckAccess(userptr, 'e');

	for (tempuser = cloneTable[hashv].list; tempuser; tempuser = tempuser->cnext)
	{
		if (CloneMatch(lptr, tempuser))
		{
			/* we found a clone */
			lptr->cnext = tempuser->cnext;
			tempuser->cnext = lptr;
			if ((rhostptr = IsRestrictedHost(lptr->username, lptr->hostname)))
			{
				debug("restricted host = [%s@%s], real host = [%s@%s] max conns = [%d]\n",
				      rhostptr->username, rhostptr->hostname,
				      lptr->username, lptr->hostname, rhostptr->hostnum);
			}
			else
			{
				if (notclone)
				{
					foundclone = 0;
				}
				else
				{
					if (lptr->server == Me.sptr)
					{
						/*
						 * lptr's server matches services, so its probably
						 * a service nick or a juped nick
						 */
						foundclone = 0;
					}
					else
						foundclone = 1;
				}
			}

			/*
			 * Send message to opers with a +c usermode about the
			 * clone
			 */
			if (SafeConnect)
				SendUmode(OPERUMODE_C,
				          "*** Clone detected: %s (%s@%s) [%s]",
				          lptr->nick, lptr->username, lptr->hostname,
				          lptr->server ? lptr->server->name : "*unknown*");

			break;
		} /* if (CloneMatch(lptr, tempuser)) */
		else
		{
			if (tempuser->cnext == (struct Luser *) NULL)
			{
				/*
				 * we reached the last member of the bucket without finding
				 * a clone...stick it on the end
				 */
				tempuser->cnext = lptr;
				lptr->cnext = (struct Luser *) NULL;
				break;
			}
		}
		/*
		 * keep a second variable to keep track of previous element, in
		 * case we want to move a bunch to the beginning of the list,
		 * we'll have the previous pointer to redirect past the block
		 * we're removing
		 */

		if (temp2 == (struct Luser *) NULL)
			temp2 = cloneTable[hashv].list;
	}

	if (cloneTable[hashv].list == NULL)
	{
		lptr->cnext = (struct Luser *)cloneTable[hashv].list;
		cloneTable[hashv].list = (void *)lptr;
	}

	if (MaxClones && foundclone)
	{
		int    clcnt = 2; /* number of clones */
		struct Luser  *prev = (struct Luser *) NULL;

		for (tempuser = cloneTable[hashv].list; tempuser; tempuser =
		            tempuser->cnext)
		{
			if (tempuser->cnext == lptr)
			{
				/* go through the table and get a list of all the clones matching
				 * lptr's userhost */
				for (temp2 = lptr->cnext; temp2; temp2 = temp2->cnext)
				{
					if (CloneMatch(lptr, temp2))
						++clcnt;
				}

				if (clcnt < MaxClones)
				{
					return (lptr);
				}

				if (AutoKillClones && (clcnt > MaxClones ))
				{
					killclones = 1;

					putlog(LOG2, "Clones from (%s@%s) killed",
					       lptr->username,
					       lptr->hostname);

					SendUmode(OPERUMODE_C,
					          "*** Killing clones from (%s@%s)",
					          lptr->username,
					          lptr->hostname);
				}
				else
				{
					putlog(LOG2, "Sent clone warning to (%s@%s)",
					       lptr->username,
					       lptr->hostname);

					SendUmode(OPERUMODE_C,
					          "*** Warning clones from (%s@%s)",
					          lptr->username,
					          lptr->hostname);
				}

				/*
				 * now go through the list again, and kill the clones if their
				 * number >= (MaxClones + 1), otherwise, clcnt will equal
				 * MaxClones, so give them a warning
				 */
				prev = lptr;
				for (temp2 = lptr->cnext; temp2; temp2 = temp2->cnext)
				{
					if (CloneMatch(lptr, temp2))
					{
						if (AutoKillClones && (clcnt > MaxClones ))
						{
							toserv(":%s KILL %s :%s!%s (Clones)\r\n",
							       n_OperServ, temp2->nick, Me.name, n_OperServ);
							DeleteClient(temp2);
							temp2 = prev;
						}
						else
							WarnClone(temp2->nick);

						prev = temp2;
					} /* if (CloneMatch(lptr, temp2)) */
				} /* for (temp2 = lptr->cnext; temp2; temp2 = temp2->cnext) */

				break;
			} /* if (tempuser->cnext == lptr) */
		} /* for (tempuser = cloneTable[hashv].list; tempuser; tempuser =
				                                 tempuser->cnext) */

		/*
		 * lptr and tempuser were not killed in the above loop -
		 * kill them now
		 */
		if (AutoKillClones && (clcnt >= (MaxClones + 1)))
		{
			killclones = 1;

			toserv(":%s KILL %s :%s!%s (Clones)\r\n",
			       n_OperServ, lptr->nick, Me.name, n_OperServ);
			DeleteClient(lptr);

			toserv(":%s KILL %s :%s!%s (Clones)\r\n",
			       n_OperServ, tempuser->nick, Me.name, n_OperServ);
			DeleteClient(tempuser);
		}
		else
		{
			WarnClone(lptr->nick);
			WarnClone(tempuser->nick);
		}
	} /* if (MaxClones && foundclone) */

	if (rhostptr)
	{
		int clcnt = 2; /* clone count */

		debug("yippee, clone = [%s][%s@%s]\n", lptr->nick, lptr->username, lptr->hostname);
		for (tempuser = cloneTable[hashv].list; tempuser; tempuser = tempuser->cnext)
		{
			if (tempuser->cnext == lptr)
			{
				/*
				 * we've refound the clone - now see how many clones there are
				 * with this hostname
				 */
				for (temp2 = lptr->cnext; temp2; temp2 = temp2->cnext)
				{
					if (CloneMatch(lptr, temp2))
						clcnt++;
				}

				if (clcnt > rhostptr->hostnum)
				{
					/*
					 * the clone count is over the allowed value in the I: line,
					 * kill lptr->nick
					 */
					debug("clcnt[%d] is over the allowed value [%d]\n",
					      clcnt, rhostptr->hostnum);
					toserv(":%s KILL %s :%s!%s (Restricted hostmask (%d maximum connections allowed))\r\n",
					       n_OperServ, lptr->nick, Me.name, n_OperServ,
					       rhostptr->hostnum);
					DeleteClient(lptr);
				}
				break;
			} /* if (tempuser->cnext == lptr) */
		}
	} /* if (rhostptr) */

	if (killclones)
		return (NULL);
	else
		return (lptr);
} /* HashAddClient() */

/*
HashDelClient()
  Delete client 'lptr' from the client and clone hash tables
*/

int
HashDelClient(struct Luser *lptr, int nickchange)

{
	struct Luser  *tmp, *prev = NULL;
	int  hashv, ret = -1;
	char uhost[UHOSTLEN + 2];
#ifdef STATSERVICES

	struct HostHash *hosth, *domainh;
	char *hostname, *domain;
	int isip;
#endif

	if (!lptr)
		return (0);

	/*
	 * Remove client from the nickname hash
	 */

	hashv = HashNick(lptr->nick);
	for (tmp = (struct Luser *)clientTable[hashv].list; tmp; tmp = tmp->hnext)
	{
		if (tmp == lptr)
		{
			if (prev)
				prev->hnext = tmp->hnext;
			else
				clientTable[hashv].list = (void *)tmp->hnext;
			tmp->hnext = NULL;
			ret = 1;
		}
		prev = tmp;
	}

	if (nickchange)
	{
		/*
		 * Since it is a nick change, we don't need to edit
		 * the stat or clone hash tables, because the client's
		 * user@host can't change
		 */
		return (ret);
	}

	if (lptr->username[0] == '~')
	{
		ircsprintf(uhost, "%s@%s", lptr->username + 1, lptr->hostname);

#ifdef STATSERVICES

		Network->NonIdentd--;
#endif

	}
	else
	{
		ircsprintf(uhost, "%s@%s", lptr->username, lptr->hostname);

#ifdef STATSERVICES

		Network->Identd--;
		if (lptr->server)
			lptr->server->numidentd--;
#endif

	}

#ifdef STATSERVICES

	hostname = lptr->hostname;

	for (domain = hostname; *domain; domain++)
		;
	while (domain != hostname)
	{
		if (*domain == '.')
			break;
		domain--;
	}
	domain++;

	/*
	 * domain now points to the last segment of the hostname (TLD) -
	 * if its numerical, it must be an IP Address, not a hostname
	 */
	if ((*domain >= '0') && (*domain <= '9'))
		isip = 1;
	else
		isip = 0;

	if (!isip)
	{
		Network->ResHosts--;
		if (lptr->server)
			lptr->server->numreshosts--;
	}

	if ((domain = GetDomain(hostname)))
	{
		if ((domainh = FindDomain(domain)))
		{
			if ((hosth = FindHost(hostname)))
			{
				int clonematches = 0;

				hosth->currclients--;
				domainh->currclients--;

				if (lptr->username[0] != '~')
				{
					hosth->curridentd--;
					domainh->curridentd--;
				}

				if (IsOperator(lptr))
				{
					hosth->curropers--;
					domainh->curropers--;
				}

				/*
				 * go through clone table and find out how many clients
				 * match the old client - if there are 2 matches, add 1 to
				 * the unique count since they aren't clones anymore; if there
				 * are no matches, decrement unique client count
				 */

				hashv = HashUhost(uhost);
				for (tmp = (struct Luser *)cloneTable[hashv].list; tmp; tmp = tmp->cnext)
				{
					if (CloneMatch(lptr, tmp))
						clonematches++;
				}

				/*
				 * must decrement clonematches here because lptr itself will
				 * be in cloneTable[], so clonematches will be at least 1,
				 * when in fact it should be 1 lower
				 */
				clonematches--;

				if (!clonematches)
				{
					/* lptr was not a clone, decrement unique count */
					hosth->currunique--;
					domainh->currunique--;
				}
				else
				{
					if (lptr->server != Me.sptr)
						SendUmode(OPERUMODE_E,
						          "*** Clone exit: %s (%s@%s) [%s]",
						          lptr->nick,
						          lptr->username,
						          lptr->hostname,
						          lptr->server ? lptr->server->name : "*unknown*");

					if (clonematches == 1)
					{
						time_t  currtime = current_ts;

						/*
						 * there was only 1 other client who matched lptr, so that
						 * client is unique now, increment unique count
						 */
						hosth->currunique++;
						if (hosth->currunique > hosth->maxunique)
						{
							hosth->maxunique = hosth->currunique;
							hosth->maxunique_ts = currtime;
						}

						domainh->currunique++;
						if (domainh->currunique > domainh->maxunique)
						{
							domainh->maxunique = domainh->currunique;
							domainh->maxunique_ts = currtime;
						}
					}
				} /* else */
			} /* if ((hosth = FindHost(hostname))) */
		} /* if ((domainh = FindDomain(domain))) */
	} /* if ((domain = GetDomain(hostname))) */

#endif /* STATSERVICES */

	/* now remove it from the clone table */

	hashv = HashUhost(uhost);

	prev = (struct Luser *) NULL;
	for (tmp = (struct Luser *)cloneTable[hashv].list; tmp; tmp = tmp->cnext)
	{
		if (tmp == lptr)
		{
			if (prev)
				prev->cnext = tmp->cnext;
			else
				cloneTable[hashv].list = (void *)tmp->cnext;

			tmp->cnext = NULL;
			ret = 1;
		}
		prev = tmp;
	}

	return (ret);
} /* HashDelClient() */

/*
HashUhost()
  Calculate hash value for first several characters of a user@host
  (for clone checking)
*/

int
HashUhost(char *userhost)

{
	unsigned char *hname = (unsigned char *)userhost;
	unsigned int h = 0;
	int i = 30; /* only use first 30 chars of uhost */

#if 0

	while(*hname && --i)
	{
		h = (h << 4) - (h + (unsigned char) ToLower(*hname++));
	}

	return (h & (HASHCLIENTS - 1));
#endif

	while (*hname && --i)
	{
		h += ToLower(*hname);
		hname++;
	}

	return h % HASHCLIENTS;

} /* HashUhost() */

/*
HashChannel()
 Calculate a hash value for the first 30 characters of the channel
name.  Most channels won't have the exact same first 30 characters, and
even if some do, it will be easy to find.
 
 ** NOTE: This function is originally from ircd-hybrid source.
*/

static unsigned int
HashChannel(const char *name)

{
	int i = 30;
	unsigned int h = 0;

#if 0

	while (*name && --i)
	{
		h = (h << 4) - (h + (unsigned char) ToLower(*name++));
	}

	return (h & (HASHCHANNELS - 1));
#endif

	while (*name && --i)
	{
		h += ToLower(*name);
		name++;
	}

	return h % HASHCHANNELS;

} /* HashChannel() */

#if defined(NICKSERVICES) && defined(CHANNELSERVICES)

/*
CSHashChan()
 Calculate a ChanServ channel hash value for 'name'
*/

unsigned int
CSHashChan(const char *name)

{
	int i = 30;
	unsigned int h = 0;

#if 0

	while (*name && --i)
	{
		h = (h << 4) - (h + (unsigned char) ToLower(*name++));
	}

	return (h & (CHANLIST_MAX - 1));
#endif

	while (*name && --i)
	{
		h += ToLower(*name);
		name++;
	}

	return h % CHANLIST_MAX;

} /* CSHashChan() */

#endif /* defined(NICKSERVICES) && defined(CHANNELSERVICES) */

#if defined(NICKSERVICES) && defined(MEMOSERVICES)

/*
MSHashMemo()
 Calculate a MemoServ table hash value for 'name'
*/

unsigned int
MSHashMemo(const char *name)

{
	int i = 30;
	unsigned int h = 0;

	/*
	 * The MemoServ entry may be a channel, so only go up
	 * to thirty characters
	 */

#if 0

	while (*name && --i)
	{
		h = (h << 4) - (h + (unsigned char) ToLower(*name++));
	}

	return (h & (MEMOLIST_MAX - 1));
#endif

	while (*name && --i)
	{
		h += ToLower(*name);
		name++;
	}
	return h % MEMOLIST_MAX;

} /* MSHashMemo() */

#endif /* defined(NICKSERVICES) && defined(MEMOSERVICES) */

/*
HashAddChan()
  Add channel 'chptr' to the channel hash table
*/

int
HashAddChan(struct Channel *chptr)

{
	int hashv;

	if (!chptr)
		return (0);

	/* calculate the hash value */
	hashv = HashChannel(chptr->name);

	chptr->hnext = (struct Channel *)channelTable[hashv].list;
	channelTable[hashv].list = (void *)chptr;

	return (1);
} /* HashAddChan() */

/*
HashDelChan()
  Delete channel from channel hash table
  Return 1 if successful, 0 if not
*/

int
HashDelChan(struct Channel *chptr)

{
	struct Channel *tmp, *prev;
	int hashv;

	if (!chptr)
		return (0);

	hashv = HashChannel(chptr->name);

	prev = NULL;
	for (tmp = (struct Channel *)channelTable[hashv].list; tmp; tmp = tmp->hnext)
	{
		if (tmp == chptr)
		{
			if (prev)
				prev->hnext = tmp->hnext;
			else
				channelTable[hashv].list = (void *) tmp->hnext;

			tmp->hnext = NULL;
			return (1);
		}
		prev = tmp;
	}

	return (0);
} /* HashDelChan() */

/*
FindChannel()
  Return pointer to channel 'name' in channel hash
*/

struct Channel *
			FindChannel(const char *name)

{
	int hashv;
	struct Channel *tempchan;
	aHashEntry *temphash;

	if (!name)
		return ((struct Channel *) NULL);

	hashv = HashChannel(name);
	temphash = &channelTable[hashv];

	for (tempchan = (struct Channel *)temphash->list; tempchan; tempchan = tempchan->hnext)
		if (!irccmp(name, tempchan->name))
			return (tempchan);

	return ((struct Channel *) NULL);
} /* FindChannel() */

/*
HashServer()
 Returns a hash value based on 'name'
*/

static unsigned int
HashServer(const char *name)

{
	unsigned int h = 0;
#if 0

	while (*name)
	{
		h = (h << 4) - (h + (unsigned char) ToLower(*name++));
	}

	return (h & (HASHSERVERS - 1));
#endif

	/* fix broken hash code -kre */
	while (*name)
	{
		h += ToLower(*name);
		name++;
	}
	return h % HASHSERVERS;

} /* HashServer() */

struct Server *
			FindServer(const char *name)

{
	struct Server *tempserv;

	assert(name != NULL);

	/* We can't just calculate the hash value for <name> and match
	 * all servers from that part of the hash table, because
	 * masked versions of <name> can have other hash values that
	 * the unmasked <name>.
	 * -adx
	 */

	for (tempserv = serverTable[HashServer(name)].list; tempserv;
	        tempserv = tempserv->hnext)
		if (!irccmp(tempserv->name, name))
			return (tempserv);

	if ('*' == *name || '.' == *name)
		return NULL;

	{
		char buffer[HOSTLEN + 1];
		char *s = buffer - 1;

		buffer[HOSTLEN] = '\0';
		strlcpy(buffer, name, sizeof(buffer));

		while ((s = strchr(s + 2, '.')) != NULL)
		{
			*--s = '*';
			for (tempserv = serverTable[HashServer(s)].list; tempserv;
			        tempserv = tempserv->hnext)
				if (!irccmp(tempserv->name, s))
					return (tempserv);
		}
	}

	return (NULL);
} /* FindServer() */

/*
HashAddServer()
  Add server 'sptr' to the channel hash table
*/

int
HashAddServer(struct Server *sptr)

{
	int hashv;

	if (!sptr)
		return (0);

	/* calculate the hash value */
	hashv = HashServer(sptr->name);

	sptr->hnext = (struct Server *) serverTable[hashv].list;
	serverTable[hashv].list = (void *)sptr;

	return (1);
} /* HashAddServer() */

/*
HashDelServer()
  Delete server from hash table
  Return 1 if successful, 0 if not
*/

int
HashDelServer(struct Server *sptr)

{
	struct Server *tmp, *prev;
	int hashv;

	if (!sptr)
		return (0);

	hashv = HashServer(sptr->name);

	prev = NULL;
	for (tmp = (struct Server *)serverTable[hashv].list; tmp; tmp = tmp->hnext)
	{
		if (tmp == sptr)
		{
			if (prev)
				prev->hnext = tmp->hnext;
			else
				serverTable[hashv].list = (void *) tmp->hnext;

			tmp->hnext = NULL;
			return (1);
		}
		prev = tmp;
	}

	return (0);
} /* HashDelServer() */
