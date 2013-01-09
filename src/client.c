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
#include "channel.h"
#include "chanserv.h"
#include "client.h"
#include "config.h"
#include "dcc.h"
#include "gline.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "nickserv.h"
#include "operserv.h"
#include "server.h"
#include "settings.h"
#include "statserv.h"
#include "sock.h"
#include "mystring.h"

#if defined NICKSERVICES || defined CHANNELSERVICES

/*
 * Global - linked list of clients
 */
struct Luser *ClientList = NULL;

/*
UpdateUserModes
  args: struct Luser *user, char *modes
  purpose: keep umodes for 'user' updated (also keep oper count
           updated)
  return: none
*/

void
UpdateUserModes(struct Luser *user, char *modes)

{
	int PLUS = 1;
	int umode;
	unsigned int ii;

	if (!modes || !user)
		return;

	for (ii = 0; ii < strlen(modes); ii++)
	{
		if (modes[ii] == '+')
		{
			PLUS = 1;
			continue;
		}
		if (modes[ii] == '-')
		{
			PLUS = 0;
			continue;
		}
		umode = 0;
		if (modes[ii] == 'i')
			umode = UMODE_I;
		if (modes[ii] == 's')
			umode = UMODE_S;
		if (modes[ii] == 'w')
			umode = UMODE_W;
		if (modes[ii] == 'o')
			umode = UMODE_O;
#ifdef DANCER

		if (modes[ii] == 'e')
			if (PLUS)
			{
				struct NickInfo* realptr = FindNick(user->nick);
				if (realptr)
				{
					realptr->flags |= NS_IDENTIFIED;
					RecordCommand("User %s has +e umode, marking as identified",user->nick);
					umode = UMODE_E;
				}
				else
				{
					/* Blech, who is screwing with us? */
					toserv(":%s MODE %s -e\r\n", Me.name, user->nick);
					RecordCommand("User %s has +e umode but is not known to me, setting -e",
					              user->nick);
					umode = 0;
				}
			}
#endif /* DANCER */

		if (!umode)
			continue;

		if (PLUS)
		{
			if ((umode == UMODE_O) && (!IsOperator(user)))
			{
#ifdef STATSERVICES
				char *hostname, *domain;
				struct HostHash *hosth, *domainh;
				time_t currtime = current_ts;
#endif

#ifdef NICKSERVICES

				CheckOper(user);
#endif

				Network->TotalOperators++;

				if (SafeConnect)
					SendUmode(OPERUMODE_O,
					          "*** New Operator: %s (%s@%s) [%s]",
					          user->nick,
					          user->username,
					          user->hostname,
					          user->server ? user->server->name : "*unknown*");

#ifdef STATSERVICES

				if (Network->TotalOperators > Network->MaxOperators)
				{
					Network->MaxOperators = Network->TotalOperators;
					Network->MaxOperators_ts = current_ts;

					if ((Network->MaxOperators % 5) == 0)
					{
						/* inform +y people of new max oper count */
						SendUmode(OPERUMODE_Y,
						          "*** New Max Operator Count: %ld",
						          Network->MaxOperators);
						putlog(LOG2, "New Max Operator Count: %ld",
								Network->MaxOperators);
					}
				}
				if (Network->TotalOperators > Network->MaxOperatorsT)
				{
					Network->MaxOperatorsT = Network->TotalOperators;
					Network->MaxOperatorsT_ts = current_ts;
				}
#endif

				if (user->server)
				{
					user->server->numopers++;

#ifdef STATSERVICES

					if (user->server->numopers > user->server->maxopers)
					{
						user->server->maxopers = user->server->numopers;
						user->server->maxopers_ts = current_ts;
					}
#endif

				}

#ifdef STATSERVICES
				hostname = user->hostname;

				if ((hosth = FindHost(hostname)))
				{
					hosth->curropers++;
					if (hosth->curropers > hosth->maxopers)
					{
						hosth->maxopers = hosth->curropers;
						hosth->maxopers_ts = currtime;
					}
				}

				if ((domain = GetDomain(hostname)))
				{
					if ((domainh = FindDomain(domain)))
					{
						domainh->curropers++;
						if (domainh->curropers > domainh->maxopers)
						{
							domainh->maxopers = domainh->curropers;
							domainh->maxopers_ts = currtime;
						}
					}
				}
#endif /* STATSERVICES */

			}
			user->umodes |= umode;
		}
		else
		{
			if ((umode == UMODE_O) && (IsOperator(user)))
			{
#ifdef STATSERVICES
				char *hostname, *domain;
				struct HostHash *hosth, *domainh;
#endif

				Network->TotalOperators--;
				if (user->server)
					user->server->numopers--;

#ifdef STATSERVICES

				hostname = user->hostname;

				if ((hosth = FindHost(hostname)))
					hosth->curropers--;

				if ((domain = GetDomain(hostname)))
					if ((domainh = FindDomain(domain)))
						domainh->curropers--;
#endif

			}
			user->umodes &= ~umode;
		}
	}
} /* UpdateUserModes() */

/*
AddClient()
  args: char *line;
  purpose: store user info 'line' in next available
           position of user list
  return: pointer to newly allocated Luser structure
*/

struct Luser *
			AddClient(char **line)

{
	char *ch;
	struct Luser *tempuser,
				*newptr;

#ifdef BLOCK_ALLOCATION

	tempuser = (struct Luser *) BlockSubAllocate(ClientHeap);
	if (!tempuser) /* shouldn't happen - but I'm paranoid :) */
		return (NULL);

	memset(tempuser, 0, sizeof(struct Luser));

	strlcpy(tempuser->nick, line[1], NICKLEN + 1);
	strlcpy(tempuser->username, line[5], USERLEN + 1);
	strlcpy(tempuser->hostname, line[6], HOSTLEN + 1);
	strlcpy(tempuser->realname, line[8] + 1, REALLEN + 1);

#else

	tempuser = (struct Luser *) MyMalloc(sizeof(struct Luser));
	memset(tempuser, 0, sizeof(struct Luser));

	tempuser->nick = MyStrdup(line[1]);
	tempuser->username = MyStrdup(line[5]);
	tempuser->hostname = MyStrdup(line[6]);
	tempuser->realname = MyStrdup(line[8] + 1);

#endif /* BLOCK_ALLOCATION */

	tempuser->since = atol(line[3]);

#ifdef NICKSERVICES

	tempuser->nick_ts = tempuser->since;

#if defined SVSNICK || defined FORCENICK
	tempuser->flags &= ~(UMODE_NOFORCENICK);
#endif

#endif /* NICKSERVICES */

	if ((tempuser->server = FindServer(line[7])))
{
		tempuser->server->numusers++;

#ifdef STATSERVICES

		if (tempuser->server->numusers > tempuser->server->maxusers)
		{
			tempuser->server->maxusers = tempuser->server->numusers;
			tempuser->server->maxusers_ts = current_ts;
		}
#endif

	}

	ch = &line[4][1];
	while (*ch)
	{
		switch (*ch)
		{
		case 'i':
		case 'I':
			{
				tempuser->umodes |= UMODE_I;
				break;
			}
		case 's':
		case 'S':
			{
				tempuser->umodes |= UMODE_S;
				break;
			}
		case 'w':
		case 'W':
			{
				tempuser->umodes |= UMODE_W;
				break;
			}
		case 'o':
		case 'O':
			{
				tempuser->umodes |= UMODE_O;

				Network->TotalOperators++;

#ifdef STATSERVICES

				if (Network->TotalOperators > Network->MaxOperators)
				{
					Network->MaxOperators = Network->TotalOperators;
					Network->MaxOperators_ts = current_ts;

					if ((Network->MaxOperators % 5) == 0)
					{
						/* inform +y people of new max oper count */
						SendUmode(OPERUMODE_Y,
						          "*** New Max Operator Count: %ld",
						          Network->MaxOperators);
						putlog(LOG2, "New Max Operator Count: %ld",
								Network->MaxOperators);
					}
				}
				if (Network->TotalOperators > Network->MaxOperatorsT)
				{
					Network->MaxOperatorsT = Network->TotalOperators;
					Network->MaxOperatorsT_ts = current_ts;
				}
#endif

				if (tempuser->server)
				{
					tempuser->server->numopers++;

#ifdef STATSERVICES

					if (tempuser->server->numopers > tempuser->server->maxopers)
					{
						tempuser->server->maxopers = tempuser->server->numopers;
						tempuser->server->maxopers_ts = current_ts;
					}
#endif

				}
				break;
			} /* case 'O' */
#ifdef DANCER
		case 'e':
		case 'E':
			{
				struct NickInfo *realptr;
				realptr = FindNick(tempuser->nick);
				if (realptr)
				{
					realptr->flags |= NS_IDENTIFIED;
					tempuser->umodes |= UMODE_E;
					RecordCommand("User %s has +e umode, marking as identified",
					              tempuser->nick);
				}
				else
				{
					/* Is it one of mine? */
					int mine = 0;
					struct aService *sptr;
					for (sptr = ServiceBots; sptr->name; ++sptr)
						if (!irccmp(tempuser->nick, *(sptr->name)))
						{
							mine = 1;
							break;
						}
					if (!mine)
					{
						/* Blech, who is screwing with us? */
						toserv(":%s MODE %s -e\r\n", Me.name, tempuser->nick);
						RecordCommand("User %s has +e umode but is not known to me, setting -e",
						              tempuser->nick);
					}
				}
				break;
			}
#endif /* DANCER */
		default:
			break;
		} /* switch (*ch) */
		ch++;
	}

	tempuser->next = ClientList;
	tempuser->prev = NULL;
	if (tempuser->next)
		tempuser->next->prev = tempuser;
	ClientList = tempuser;

	/* add client to the hash table */
	newptr = HashAddClient(ClientList, 0);

	Network->TotalUsers++;
#ifdef STATSERVICES

	if (Network->TotalUsers > Network->MaxUsers)
	{
		Network->MaxUsers = Network->TotalUsers;
		Network->MaxUsers_ts = current_ts;

		if ((Network->MaxUsers % 10) == 0)
		{
			/* notify +y people of new max user count */
			SendUmode(OPERUMODE_Y,
			          "*** New Max Client Count: %ld",
			          Network->MaxUsers);
			putlog(LOG2, "New Max Client Count: %ld",
					Network->MaxUsers);
		}
	}
	if (Network->TotalUsers > Network->MaxUsersT)
	{
		Network->MaxUsersT = Network->TotalUsers;
		Network->MaxUsersT_ts = current_ts;
	}
#endif /* STATSERVICES */

#ifdef ALLOW_GLINES
	/*
	 * It's possible the client won't exist anymore, because if the user
	 * is a clone and AutoKillClones is enabled, HashAddClient() would
	 * have already killed the user, in which case newptr will be
	 * NULL - CheckGlined() checks for null pointers
	 */
	CheckGlined(newptr); /* Check if new user is glined */
#endif

	return (tempuser);
} /* AddClient() */

/*
DeleteClient()
  args: char *nick
  purpose: when a user either QUITs the network, or changes their NICK,
           delete the old user from the database.
  return: none         
*/

void
DeleteClient(struct Luser *user)

{
	struct UserChannel *chnext;

#ifdef NICKSERVICES

	struct NickInfo *nptr, *realptr;

#ifdef CHANNELSERVICES

	struct aChannelPtr *fnext;
#endif

#endif /* NICKSERVICES */

	if (user == NULL)
		return;

	SendUmode(OPERUMODE_CAPE,
	          "*** Client exit: %s!%s@%s [%s]",
	          user->nick,
	          user->username,
	          user->hostname,
	          user->server ? user->server->name : "*unknown*");

#ifdef NICKSERVICES

	realptr = FindNick(user->nick);
	nptr = GetMaster(realptr);
	if (nptr && realptr)
	{

		if (LastSeenInfo && (realptr->flags & NS_IDENTIFIED))
		{
			/*
			 * Update last seen user@host info
			 */

			if (realptr->lastu)
				MyFree(realptr->lastu);
			if (realptr->lasth)
				MyFree(realptr->lasth);
			realptr->lastu = MyStrdup(user->username);
			realptr->lasth = MyStrdup(user->hostname);
		}

		/*
		 * they're quitting - unmark them as identified
		 */
		realptr->flags &= ~NS_IDENTIFIED;
	}

#ifdef CHANNELSERVICES
	while (user->founder_channels)
	{
		fnext = user->founder_channels->next;
		RemFounder(user, user->founder_channels->cptr);
		user->founder_channels = fnext;
	}
#endif

#endif /* NICKSERVICES */

#ifdef ALLOW_FUCKOVER
	/* check if user was a target of o_fuckover() */
	CheckFuckoverTarget(user, NULL);
#endif

	if (user->server)
		user->server->numusers--;

	while (user->firstchan)
	{
		chnext = user->firstchan->next;
		RemoveFromChannel(user->firstchan->chptr, user);
		user->firstchan = chnext;
	}

	HashDelClient(user, 0);

	/* keep oper count updated */
	if (user->umodes & UMODE_O)
	{
		Network->TotalOperators--;
		if (user->server)
			user->server->numopers--;
	}

#ifndef BLOCK_ALLOCATION
	MyFree(user->nick);
	MyFree(user->username);
	MyFree(user->hostname);
	MyFree(user->realname);
#endif /* BLOCK_ALLOCATION */

	if (user->prev)
		user->prev->next = user->next;
	else
		ClientList = user->next;

	if (user->next)
		user->next->prev = user->prev;

#ifdef BLOCK_ALLOCATION

	BlockSubFree(ClientHeap, user);

#else

	MyFree(user);

#endif /* BLOCK_ALLOCATION */

	Network->TotalUsers--;
} /* DeleteClient() */

/*
IsRegistered()
  args: struct Luser *lptr, int sockfd
  purpose: check if 'lptr' has registered with OperServ
  return: 1 if nick has registered, 0 if not
*/

int
IsRegistered(struct Luser *lptr, int sockfd)

{
	struct DccUser *dccptr;

	if (lptr)
		if (lptr->flags & L_OSREGISTERED)
			return (1);

	if (sockfd != NODCC)
	{
		if ((dccptr = IsDccSock(sockfd)))
			if (!IsDccPending(dccptr))
				return (1);

#if 0
		/* all we have left to work with is the nickname - *sigh* */
		if ((dccptr = IsOnDcc(nick)))
			if (!IsDccPending(dccptr))
				return (1);
#endif

	}

	return (0);
} /* IsRegistered() */

/*
GetNick()
 Take a nickname arguement of the format: @+nickname, @nickname,
or +nickname and return just "nickname"
*/

char *GetNick(char *nickname)
{
	char *final;

	if (!nickname)
		return (NULL);

	final = nickname;
	if (IsNickPrefix(*final))
	{
		if (IsNickPrefix(*(++final)))
			++final;
	}

	return (final);
} /* GetNick() */

/*
IsOperator()
  args: struct Luser *user
  purpose: determine if 'user' has an 'o' umode (oper flags)
  return: 1 if 'user' is an oper, 0 if not
*/

int
IsOperator(struct Luser *user)

{
	if (!user)
		return (0);

	return (user->umodes & UMODE_O);

} /* IsOperator() */

/*
IsValidAdmin()
  Returns 1 if lptr matches an administrator O: line AND is
currently registered with OperServ
*/

int
IsValidAdmin(struct Luser *lptr)

{
	if (!lptr)
		return 0;

	if (!(lptr->flags & L_OSREGISTERED))
		return 0;

	/*
	 * Give the arguement 0 to GetUser() indicating we must match
	 * hostnames and not just nicknames - for better security
	 */
	if (IsAdmin(GetUser(0, lptr->nick, lptr->username, lptr->hostname)))
		return 1;

	return 0;
} /* IsValidAdmin() */

/*
 * IsValidServicesAdmin()
 * Returns 1 if lptr is an identified Services Administrator
 */
int IsValidServicesAdmin(struct Luser *lptr)
{
	if (!lptr)
		return 0;
	
	if (!(lptr->flags & L_OSREGISTERED))
		return 0;

	if (IsServicesAdmin(GetUser(0, lptr->nick, lptr->username, lptr->hostname)))
		return 1;

	return 0;
} /* IsValidServicesAdmin() */

/*
IsNickCollide()
 Determine if 'servptr' is being nick collided
A nickname collision is successful if the following conditions
are met:
 
  1) Both user@host's match, and the connecting nick's TS is
     newer than our *Serv's TS
  2) The user@host's do NOT match, and the connecting nick's
     TS is older than our *Serv's TS
  3) The two TS's are equal, in which case both are killed
 
Returns: 1 if successful collision
         0 if not
*/

int
IsNickCollide(struct Luser *servptr, char **av)

{
	int sameuh;
	time_t newts;
	char *user;

	assert(servptr != 0);
	assert(av != 0);

	newts = (time_t) atol(av[3]);

	if (newts == servptr->since)
	{
		return 1;
	}
	else
	{
		if (*av[5] == '~')
			user = av[5] + 1;
		else
			user = av[5];

		sameuh = (!irccmp(user, servptr->username) &&
		          !irccmp(av[6], servptr->hostname));

		if (newts > servptr->since)
		{
			if (sameuh)
				return 1;
		}
		else
		{
			if (!sameuh)
				return 1;
		}
	}

	return 0;
} /* IsNickCollide() */

#endif
