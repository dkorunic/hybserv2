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
#include "conf.h"
#include "config.h"
#include "dcc.h"
#include "flood.h"
#include "global.h"
#include "hash.h"
#include "helpserv.h"
#include "hybdefs.h"
#include "init.h"
#include "jupe.h"
#include "log.h"
#include "match.h"
#include "memoserv.h"
#include "misc.h"
#include "mystring.h"
#include "nickserv.h"
#include "operserv.h"
#include "seenserv.h"
#include "server.h"
#include "settings.h"
#include "sock.h"
#include "statserv.h"
#include "timestr.h"
#include "sprintf_irc.h"

#ifdef STATSERVICES

aHashEntry hostTable[HASHCLIENTS];

#endif /* STATSERVICES */

/*
 * Global - list of network servers
 */
struct Server *ServerList = NULL;
int burst_complete = 0;
time_t most_recent_sjoin=0;
#ifdef SERVICES_FIGHT_FIX
static int ServerCollides = 0;
static time_t ServerCollidesTS = 0;
#endif /* SERVICES_FIGHT_FIX */

static void s_pass(int ac, char **av);
static void s_ping(int ac, char **av);
static void s_server(int ac, char **av);
static void s_nick(int ac, char **av);
static void s_squit(int ac, char **av);
static void s_privmsg(int ac, char **av);
static void s_quit(int ac, char **av);
static void s_kill(int ac, char **av);
static void s_kick(int ac, char **av);
static void s_mode(int ac, char **av);
static void s_part(int ac, char **av);
static void s_error(int ac, char **av);
static void s_whois(int ac, char **av);
static void s_trace(int ac, char **av);
static void s_version(int ac, char **av);
static void s_motd(int ac, char **av);

#ifdef SERVICES_FIGHT_FIX
static void CheckServCollide(struct Server *bad_server);
#endif /* SERVICES_FIGHT_FIX */

#if defined(NICKSERVICES) && defined(CHANNELSERVICES)
static void s_topic(int ac, char **av);
#endif

#ifdef STATSERVICES
static void s_pong(int ac, char **av);
#endif

/*
 * Table of commands the hub will send us
 */
static struct ServCommand servtab[] =
	{
		{ "PASS", s_pass
		},
		{ "PING", s_ping },
		{ "SERVER", s_server },
		{ "NICK", s_nick },
		{ "SQUIT", s_squit },
		{ "PRIVMSG", s_privmsg },
		/*	{ "NOTICE", s_privmsg }, */
		{ "QUIT", s_quit },
		{ "KILL", s_kill },
		{ "KICK", s_kick },
		{ "MODE", s_mode },
		{ "SJOIN", s_sjoin },
		{ "JOIN", s_sjoin },
		{ "PART", s_part },
		{ "ERROR", s_error },
		{ "WHOIS", s_whois },
		{ "TRACE", s_trace },
		{ "VERSION", s_version },
		{ "MOTD", s_motd },

#if defined(NICKSERVICES) && defined(CHANNELSERVICES)

		{ "TOPIC", s_topic
		},
#endif

#ifdef STATSERVICES
		{ "PONG", s_pong },
#endif

		{ 0, 0 }
	};

/*
ProcessInfo()
  args: int arc, char **arv
  purpose: process string sent by hub server (stored in 'arv')
  return: none
*/

void
ProcessInfo(int arc, char **arv)

{
	struct ServCommand *stptr;
	int index;

	if ((arc >= 2) && (*arv[0] == ':'))
		index = 1;
	else
		index = 0;

	for (stptr = servtab; stptr->cmd; stptr++)
	{
		if (!irccmp(arv[index], stptr->cmd))
		{
			(*stptr->func)(arc, arv);
			break;
		}
	}
} /* ProcessInfo() */

/*
AddServer()
  args: int argcnt, char **line
  purpose: add server info contained in 'line'
  return: pointer to newly created server structure
*/

struct Server *
			AddServer(int argcnt, char **line)

{
	struct Server *tempserv;

#ifdef BLOCK_ALLOCATION

	tempserv = (struct Server *) BlockSubAllocate(ServerHeap);
#else

	tempserv = (struct Server *) MyMalloc(sizeof(struct Server));
#endif

	memset(tempserv, 0, sizeof(struct Server));

	/*
	 * check if the server is the hub - if so, the format will
	 * be SERVER server.com .., not :hub SERVER server.com ..
	 */
	if (argcnt == 4)
	{
#ifdef BLOCK_ALLOCATION
		strlcpy(tempserv->name, line[1], SERVERLEN + 1);
#else

		tempserv->name = MyStrdup(line[1]);
#endif

		tempserv->flags |= SERV_MYHUB;
		if (!Me.hub)
			Me.hub = tempserv;

		/*
		 * services won't be in the server list yet, so don't
		 * increment our hub's server count - it will be set
		 * to 1 anyway below
		 */
	}
	else
	{
		if (argcnt < 5)
			return (NULL);

#ifdef BLOCK_ALLOCATION

		strlcpy(tempserv->name, line[2], SERVERLEN + 1);
#else

		tempserv->name = MyStrdup(line[2]);
#endif

		/*
		 * If the server we're adding is services itself,
		 * have "Me.sptr" point to it
		 */
		if (!Me.sptr)
			if (!irccmp(tempserv->name, Me.name))
				Me.sptr = tempserv;

		/*
		 * A new server is being added, increment the server count
		 * of it's hub; if the new server is services itself, don't
		 * increment our hub's server count because it will already
		 * be 1 from when it was added
		 */
		if ((tempserv->uplink = FindServer((line[0][0] == ':') ? line[0] + 1 : line[0])))
		{
			/*
			 * Make sure this server doesn't match services
			 */
			if (tempserv != Me.sptr)
			{
				tempserv->uplink->numservs++;

#ifdef STATSERVICES

				if (tempserv->uplink->numservs > tempserv->uplink->maxservs)
				{
					tempserv->uplink->maxservs = tempserv->uplink->numservs;
					tempserv->uplink->maxservs_ts = current_ts;
				}
#endif

			}
		}

		if (SafeConnect)
			SendUmode(OPERUMODE_L,
					  "*** New Server: %s (hub: %s)",
					  tempserv->name,
					  tempserv->uplink ? tempserv->uplink->name : "*unknown*");
	}

	tempserv->connect_ts = current_ts;
	tempserv->numservs = 1;

#ifdef STATSERVICES

	tempserv->maxservs = 1;
	tempserv->maxservs_ts = tempserv->connect_ts;

#endif /* STATSERVICES */

#ifdef SPLIT_INFO

	tempserv->split_ts = 0;

#endif

	tempserv->next = ServerList;
	tempserv->prev = NULL;
	if (tempserv->next)
		tempserv->next->prev = tempserv;
	ServerList = tempserv;

	(void) HashAddServer(tempserv);

	Network->TotalServers++;
#ifdef STATSERVICES

	if (Network->TotalServers > Network->MaxServers)
	{
		Network->MaxServers = Network->TotalServers;
		Network->MaxServers_ts = current_ts;

		if ((Network->MaxServers % 5) == 0)
		{
			/* notify of new max server count */
			SendUmode(OPERUMODE_Y,
					  "*** New Max Server Count: %ld",
					  Network->MaxServers);
		}
	}
	if (Network->TotalServers > Network->MaxServersT)
	{
		Network->MaxServersT = Network->TotalServers;
		Network->MaxServersT_ts = current_ts;
	}
#endif

	return (tempserv);
} /* AddServer() */

/*
DeleteServer()
  args: struct Server *sptr
  purpose: delete 'sptr' from server list
  return: none
*/

void
DeleteServer(struct Server *sptr)

{
	struct Server *tmpserv;

	if (!sptr)
		return;

#ifdef EXTRA_SPLIT_INFO
	/* This could spawn too much messages, even flood on connects/reconnects
	 * because of severe DoS -kre */
	SendUmode(OPERUMODE_L,
			  "*** Netsplit: %s (hub: %s)",
			  sptr->name,
			  sptr->uplink ? sptr->uplink->name : "*unknown*");
#endif

	/*
	 * Delete server from hash table
	 */
	(void) HashDelServer(sptr);

	if (sptr->uplink)
		sptr->uplink->numservs--;

	/*
	 * go through server list, and check if any leafs had sptr
	 * as their hub, if so, nullify uplink
	 */
	for (tmpserv = ServerList; tmpserv; tmpserv = tmpserv->next)
	{
		if (tmpserv->uplink == sptr)
			tmpserv->uplink = NULL;
	}

#ifndef BLOCK_ALLOCATION
	MyFree(sptr->name);
#endif

	if (sptr->prev)
		sptr->prev->next = sptr->next;
	else
		ServerList = sptr->next;

	if (sptr->next)
		sptr->next->prev = sptr->prev;

#ifdef BLOCK_ALLOCATION

	BlockSubFree(ServerHeap, sptr);

#else

	MyFree(sptr);

#endif /* BLOCK_ALLOCATION */

	Network->TotalServers--;
} /* DeleteServer() */

/*
do_squit()
  SQUIT all servers matching serv
*/

void
do_squit(char *serv, char *reason)

{
	struct Server *tempserv, *prev;

	for (tempserv = ServerList; tempserv; )
	{
		/* squit the server */
		if (match(serv, tempserv->name))
		{
			toserv("SQUIT %s :%s\r\n",
				   tempserv->name, reason);

			prev = tempserv->next;

			DeleteServer(tempserv); /* remove server from list */

			tempserv = prev;
		}
		else
			tempserv = tempserv->next;
	}
} /* do_squit() */

/*
ClearUsers()
  args: none
  purpose: clear all users in linked list
  return: none
*/

void
ClearUsers()

{
	struct Luser *next;

	while (ClientList)
	{
		next = ClientList->next;
		DeleteClient(ClientList);
		ClientList = next;
	}
} /* ClearUsers() */

/*
ClearServs()
  args: none
  purpose: delete all servers in list
  return: none
*/

void
ClearServs()

{
	struct Server *next;

	while (ServerList)
	{
		next = ServerList->next;
		DeleteServer(ServerList);
		ServerList = next;
	}
} /* ClearServs() */

/*
ClearChans()
  args: none
  purpose: delete all channels in list
  return: none
*/

void
ClearChans()

{
	struct Channel *next;

	while (ChannelList)
	{
		next = ChannelList->next;
		DeleteChannel(ChannelList);
		ChannelList = next;
	}
} /* ClearChans() */

/*
s_pass()
*/

static void
s_pass(int ac, char **av)

{
	/* do nothing */
	return;
} /* s_pass() */

/*
s_ping()
 Respond to a ping request
 
 If a server is requesting a PING
  av[0] = "PING"
  av[1] = :servername
 
 If a client is requesting a PING
  av[0] = :nickname
  av[1] = "PING"
  av[2] = nickname
  av[3] = Me.name
*/

static void s_ping(int ac, char **av)
{
	char *who;

	if (ac < 2)
		return;

	if (!irccmp(av[0], "PING"))
		who = av[1];
	else
		who = av[0];

	if (*who == ':')
		who++;

	toserv(":%s PONG %s :%s\r\n", Me.name, Me.name, who);
	burst_complete = 1;
} /* s_ping() */

/*
s_server()
  Process a SERVER statement from hub
 
If hub is identifying itself:
  av[0] = "SERVER"
  av[1] = server name
  av[2] = hop count
  av[3] = description
 
If its another server:
  av[0] = hub name
  av[1] = "SERVER"
  av[2] = server name
  av[3] = hop count
  av[4] = description
*/

static void
s_server(int ac, char **av)

{
	char sendstr[MAXLINE + 1];
	char **line;
	struct Server *tempserv;

	if (ac < 4)
		return;

	/*
	 * make sure av[2] isn't in the server list - one reason it might
	 * be is if services squit'd it and created a fake server
	 * in the connect burst, but the hub would send the server line
	 * to us anyway, before processing our SQUIT
	 */
	if ((tempserv = FindServer(av[2])))
#ifndef SPLIT_INFO

		return;
#else
		/* Yeah, this is server that has already been in server list. Now, we
		* have 2 cases - either this is juped server or is not. If it is,
		* ignore whole split stuff -kre */
	{
		if ((ac == 5)
#ifdef ALLOW_JUPES
				&& !IsJupe(tempserv->name)
#endif /* ALLOW_JUPES */
		   )
		{
			tempserv->uplink = FindServer(av[0] + 1);
			SendUmode(OPERUMODE_Y, "Server %s has connected to %s "
					  "after %s split time",
					  av[2], av[0] + 1, timeago(tempserv->split_ts, 0));
			tempserv->split_ts = 0;
			tempserv->connect_ts = current_ts;

#ifdef RECORD_RESTART_TS
			most_recent_sjoin = current_ts;
#endif

		}
		return;
	}
#endif /* SPLIT_INFO */

	tempserv = AddServer(ac, av);

#ifdef RECORD_RESTART_TS
	most_recent_sjoin = current_ts;
#endif

	if (tempserv && (tempserv == Me.hub))
	{
		/*
		 * it's the SERVER line from the hub in the initial burst
		 */

		/*
		 * get the network server name of hub in case it
		 * doesn't match it's hostname 
		  */
		MyFree(currenthub->realname);
		currenthub->realname = MyStrdup(av[1]);

		/*
		 * add the services (our) server as well
		 */
		ircsprintf(sendstr, ":%s SERVER %s 1 :%s",
				   av[1], Me.name, Me.info);

		SplitBuf(sendstr, &line);

		AddServer(5, line);

		MyFree(line);

		/*
		 * Set our hub's uplink as services (we are the hub of our
		 * uplink)
		 */
		tempserv->uplink = Me.sptr;

		/* initialize service nicks */
		InitServs(NULL);

#ifdef ALLOW_JUPES
		/* Introduce juped nicks/servers to the network */
		InitJupes();
#endif /* ALLOW_JUPES */

	}
	else
	{
#ifdef ALLOW_JUPES
		/* a new server connected to network, check if its juped */
		CheckJuped(av[2]);
#endif /* ALLOW_JUPES */

#ifdef DEBUGMODE

		fprintf(stderr, "Introducing new server %s\n",
				av[2]);
#endif /* DEBUGMODE */

	}
} /* s_server() */

/*
s_nick()
  Process a NICK statement
 
  if new user:
  av[1] = nick name
  av[2] = hop count
  av[3] = TimeStamp
  av[4] = usermodes
  av[5] = username
  av[6] = hostname
  av[7] = server
  av[8] = real name
 
  if nick change:
  av[0] = who
  av[1] = "NICK"
  av[2] = new nick
  av[3] = TS
*/

static void
s_nick(int ac, char **av)

{
	struct Luser *lptr,
				*serviceptr;
#ifdef NICKSERVICES
	struct NickInfo *nptr, *newptr;
#endif

#ifdef RECORD_RESTART_TS
	struct Userlist *uptr;
#endif

#ifdef SEENSERVICES
	char oldnick[NICKLEN + 1];
#endif /* SEENSERVICES */

	if (ac == 4)
{
		char  *who;
#if defined(BLOCK_ALLOCATION) || defined(NICKSERVICES)

		char  newnick[NICKLEN + 1];
#endif

		/* nick change, not new user */

		if (av[0][0] == ':')
			who = av[0] + 1;
		else
			who = av[0];

		lptr = FindClient(who);
		if (!lptr)
			return;

		/* Shouldn't happen, but if it's our nick (juped/enforced) and somehow 
           it's unknown for the other side and they are changing nick, ignore. */
		if (lptr->server == Me.sptr)
			return;

#if defined(BLOCK_ALLOCATION) || defined(NICKSERVICES)

		strlcpy(newnick, av[2], sizeof(newnick));
#endif

		SendUmode(OPERUMODE_N,
				  "*** Nick Change: %s -> %s",
				  lptr->nick,
				  av[2]);

#ifdef NICKSERVICES

		nptr = FindNick(lptr->nick);

		/* Update lastseen info for old nickname */
		if (nptr && (nptr->flags & NS_IDENTIFIED))
			nptr->lastseen = current_ts;

		newptr = FindNick(newnick);

		if (newptr)
		{
			if (nptr && (newptr == nptr))
			{
				/* If new nickname is same as old nickname, update connect table
				 * and exit without unregistering. -kre */
#ifdef ADVFLOOD
				updateConnectTable(lptr->username, lptr->hostname);
#endif /* ADVFLOOD */

				return;
			}

			/* Un-Identify the new nickname in case it is registered - it could
			 * possibly be identified from the flags read from the userfile, if
			 * no-one has used this nickname yet. */
			newptr->flags &= ~NS_IDENTIFIED;
		}

#if defined SVSNICK || defined FORCENICK
        lptr->flags &= ~(UMODE_NOFORCENICK);
#endif

		if (nptr)
		{
#if defined SVSNICK || defined FORCENICK
		/* If a pseudo nick must be generated, we should do it here,
		 * because the server's reply with NICK will delete the client's
		 * structure. So we don't touch this in advance in collide(),
		 * otherwise the new allocated structure for the pseudo will be
		 * removed and we'll be unable to find and release the nickname.
		 * Also, we wait for the server reply before creating the
		 * pseudo-nick to avoid possible direct collision. Now, we can
		 * allocate a new structure for our pseudo client. */
			if (nptr->flags & NS_RELEASE)
			{
				char **args;
				char sendstr[MAXLINE + 1];
#ifdef DANCER
				ircsprintf(sendstr, "NICK %s 1 1 +i %s %s %s %lu :%s\r\n",
						lptr->nick, "enforced", Me.name, Me.name,
						0xffffffffUL, "Nickname Enforcement");
#else
				ircsprintf(sendstr, "NICK %s 1 %ld +i %s %s %s :%s\r\n",
						lptr->nick, (long) (lptr->nick_ts - 1),
						"enforced", Me.name, Me.name,
						"Nickname Enforcement");
#endif /* DANCER */
				toserv("%s\r\n", sendstr);
				SplitBuf(sendstr, &args);
				AddClient(args);
				MyFree(args);
				nptr->flags &= ~(NS_COLLIDE | NS_NUMERIC);
			}
#endif /* defined SVSNICK || defined FORCENICK */

#ifdef LINKED_NICKNAMES
			/*
			 * One of the features of linked nicknames is if you identify for
			 * one nick in the link, you are identified for every nick. If
			 * lptr->nick is changing his/her nick to another nickname in the
			 * link (and is currently identified), make sure they get correctly
			 * identified for the new nickname.
			 */
			if (nptr->flags & NS_IDENTIFIED)
			{
				struct NickInfo *tmp;
				if (newptr && IsLinked(nptr, newptr))
				{
					newptr->flags |= NS_IDENTIFIED;


					MyFree(newptr->lastu);
					newptr->lastu = MyStrdup(lptr->username);
					MyFree(newptr->lasth);
					newptr->lasth = MyStrdup(lptr->hostname);
#ifdef DANCER

					toserv(":%s MODE %s +e\r\n", Me.name, newptr->nick);
#endif /* DANCER */

#ifdef RECORD_RESTART_TS
					nptr->nick_ts = 0;
					MyFree(nptr->last_server);
					newptr->nick_ts = atol(av[3] + 1);
					MyFree(newptr->last_server);
					newptr->last_server = MyStrdup(lptr->server->name);
#endif /* RECORD_RESTART_TS */

				}
				tmp = GetMaster(nptr);
				tmp->lastseen = current_ts;
			}
#endif /* LINKED_NICKNAMES */

			/*
			 * Un-Identify the old nickname if it is registered
			 */
			nptr->flags &= ~NS_IDENTIFIED;
		} /* if (nptr) */
#endif /* NICKSERVICES */

		/*
		 * If the user is a generic operator who registered themselves by
		 * sending OperServ a message, remove that flag now - This code is
		 * here for a very special case scenario: If a malicious operator
		 * registers him/herself with OperServ, by sending a generic operator
		 * command, and then changes their nickname to one which has
		 * administrator privileges in hybserv.conf, they will be registered
		 * without having to give the correct password, or even without having
		 * to match the correct hostmask. This code removes their registration
		 * flag to ensure that doesn't happen. Execute this code even if
		 * OpersHaveAccess is turned off, because an operator could have
		 * registered with OperServ, while it was turned on (before a rehash
		 * etc) and the operator could get administrator access that way.
		 */
		if (GetUser(1, lptr->nick, lptr->username, lptr->hostname) ==
				GenericOper)
			lptr->flags &= ~L_OSREGISTERED;

#ifdef RECORD_RESTART_TS
		uptr = GetUser(0, lptr->nick, lptr->username, lptr->hostname);
		if ((uptr != NULL) && (lptr->flags & L_OSREGISTERED))
		{
			MyFree(uptr->last_nick);
			uptr->last_nick = MyStrdup(av[2]);
			uptr->nick_ts = atol(av[3] + 1);
			MyFree(uptr->last_server);
			uptr->last_server = MyStrdup(lptr->server->name);
		}
#endif

#ifdef ALLOW_FUCKOVER
		/* check if old nick was being flooded */
		CheckFuckoverTarget(lptr, av[2]);
#endif

		HashDelClient(lptr, 1);

		/* Erase old nick, and store new nick in it's position */

#ifdef SEENSERVICES
		strlcpy(oldnick, lptr->nick, sizeof(oldnick));
#endif /* SEENSERVICES */

#ifdef BLOCK_ALLOCATION

		strlcpy(lptr->nick, newnick, NICKLEN + 1);
#else

		MyFree(lptr->nick);
		lptr->nick = MyStrdup(av[2]);
#endif /* BLOCK_ALLOCATION */

		HashAddClient(lptr, 1);

#ifdef STATSERVICES

		lptr->numnicks++;
#endif

#ifdef NICKSERVICES
		/*
		 * Update nick_ts, so if the new nick is registered, we know
		 * when to do the KILL if they haven't registered within
		 * 1 minute
		 */
		lptr->nick_ts = atol(av[3] + 1);

		/* Useless check -kre */
#if 0
		/*
		 * check if new nick is stealing someone's nick - but only
		 * if who != av[2], because the user "aba" might change
		 * their nick to "aBa", in which case we don't need to
		 * check
		 */
		if (irccmp(who, av[2]) != 0)
#endif

			CheckNick(av[2]);
#endif /* NICKSERVICES */

#ifdef ALLOW_JUPES
		CheckJuped(av[2]); /* check if new nick is juped */
#endif

#ifdef SEENSERVICES
		es_add(oldnick, lptr->username, lptr->hostname, NULL, current_ts, 2);
#endif /* SEENSERVICES */

#ifdef ADVFLOOD

		updateConnectTable(lptr->username, lptr->hostname);
#endif /* ADVFLOOD */

		return;
	} /* if (ac == 4) */

	/* It is a new user */
	if (ac < 9)
		return;

	/*
	 * Check if someone is nick colliding a service nick
	 */
	if ((serviceptr = GetService(av[1])))
	{
		/*
		 * Make sure it is a valid nick collide before we send a kill. If this
		 * is the initial hub burst, the hub would not have processed our NICK
		 * statement yet, so let TS clean everything up
		 */
		if (IsNickCollide(serviceptr, av))
		{
			struct Luser *bad_lptr;

			toserv(":%s KILL %s :%s\r\n", Me.name, av[1],
				   "Attempt to Nick Collide Services");

			bad_lptr = FindClient(av[1]);

			if (!bad_lptr)
				return;

#ifdef SERVICES_FIGHT_FIX

			/* Check if services are fighting -kre */
			CheckServCollide(bad_lptr->server);

#endif /* SERVICES_FIGHT_FIX */

			DeleteClient(bad_lptr);

			InitServs(serviceptr);

			if (Me.sptr)
				Me.sptr->numservkills++;
			Network->TotalServKills++;

#ifdef STATSERVICES

			if (Network->TotalServKills > Network->ServKillsT)
				Network->ServKillsT = Network->TotalServKills;
#endif

		}

		/*
		 * Even if it is not a valid nick collide, do not
		 * add the user to our user list, since TS will
		 * have the user killed
		 */

		return;
	}

	/*
	 * Send new client connections to opers with a +C usermode
	 * in the format:
	 * nick!user@host (server)
	 */
	if (SafeConnect)
		SendUmode(OPERUMODE_CAPC,
				  "*** Client connection: %s!%s@%s [%s]",
				  av[1], av[5], av[6], av[7]);

	/* Add user to list */
	lptr = AddClient(av);

	if (!lptr || !lptr->server)
	{
		/*
		 * Normally lptr will point to the newly created structure for av[1].
		 * If it is NULL, one possibility is that HashAddClient() killed av[1]
		 * due to AutoKillClones, so go no furthur.
		 *
		 * For now ignore situations when messages come from clients which are
		 * behind masked server, but their originate server in NICK isn't
		 * masked properly -kre
		 */
		return;
	}

#ifdef ALLOW_JUPES
	CheckJuped(av[1]);
#endif

#ifdef NICKSERVICES
	/*
	 * If the nick is registered, and was IDENTIFY'd at the time
	 * the userfile was written, the flags will still show they
	 * were identified, even if hybserv crashed etc, so when a
	 * new nick is introduced, make sure their flags don't contain
	 * NS_IDENTIFIED since theres no way they could have identify'd
	 * yet
	 */
	if ((nptr = FindNick(av[1])))
	{
		nptr->flags &= ~(NS_IDENTIFIED | NS_RELEASE);

#ifdef DANCER
		/* If the user has +e set, mark them as identified */
		if (lptr->umodes & UMODE_E)
			nptr->flags |= NS_IDENTIFIED;
#endif /* DANCER */

#ifdef RECORD_RESTART_TS
		/*
		 * If nick_ts is not 0 then it is the TS of the user when they
		 * last identified as this nick. Since then services must have 
		 * been stopped and started. If the TS match then they're the 
		 * same person
		 */
		if (nptr->nick_ts && (nptr->nick_ts == atol(av[3])))
		{
			if (nptr->last_server &&
					irccmp(nptr->last_server, lptr->server->name))
			{
				RecordCommand("%s: %s!%s@%s failed to identify based on TS - servername changed since SJOIN",
					  n_NickServ, lptr->nick, lptr->username,
					  lptr->hostname);
			}
			else if (most_recent_sjoin + ConnectBurst >= current_ts)
			{
				RecordCommand("%s: %s!%s@%s has been identified based on TS",
							  n_NickServ, lptr->nick, lptr->username,
							  lptr->hostname);
				nptr->flags |= NS_IDENTIFIED;
			}
			else
			{
				RecordCommand("%s: %s!%s@%s failed to identify based on TS - more than ConnectBurst time since SJOIN",
					  n_NickServ, lptr->nick, lptr->username,
					  lptr->hostname);
			}
		}
#endif /* RECORD_RESTART_TS */

	}

	CheckNick(av[1]);

#endif /* NICKSERVICES */

#ifdef RECORD_RESTART_TS
	/*
	 * Check if they were registered with OperServ during a restart,
	 * if so, re-register them
	 */
	uptr = GetUser(0, av[1], av[5], av[6]);
	if ((uptr != NULL) && (uptr->last_nick != NULL) &&
			!irccmp(uptr->last_nick, av[1])
#ifdef OPERNICKIDENT
			&& (nptr) && (nptr->flags & NS_IDENTIFIED)
#ifdef IDENTIFOPER
			&& (nptr->flags & NS_NOEXPIRE) && (lptr->umodes & UMODE_O)
#endif
#endif
	   )
	{
		if (uptr->nick_ts && (uptr->nick_ts == atol(av[3])))
		{
			if (uptr->last_server &&
				irccmp(uptr->last_server, lptr->server->name))
			{
				RecordCommand("%s: %s!%s@%s failed to identify based on TS - servername changed since SJOIN",
					  n_OperServ, lptr->nick, lptr->username,
					  lptr->hostname);
			}
			else if (most_recent_sjoin + ConnectBurst >= current_ts)
			{
				RecordCommand("%s: %s!%s@%s has been identified based on TS",
						  n_OperServ, lptr->nick, lptr->username,
						  lptr->hostname);
				lptr->flags |= L_OSREGISTERED;
			}
			else
			{
				RecordCommand("%s: %s!%s@%s failed to identify based on TS - more than ConnectBurst time since SJOIN",
					  n_OperServ, lptr->nick, lptr->username,
					  lptr->hostname);
			}
		}
	}
#endif /* RECORD_RESTART_TS */

#ifdef GLOBALSERVICES

#ifdef RECORD_RESTART_TS
	if (!(nptr && nptr->flags & NS_IDENTIFIED) &&
			!(lptr->flags & L_OSREGISTERED))
#endif
	if (ircncmp(Me.name, lptr->server->name, strlen(lptr->server->name)))
	{
		/*
		 * Send the motd to the new user
		 */
		if (Network->LogonNewsFile.Contents)
			SendMessageFile(lptr, &Network->LogonNewsFile);
	}

#endif /* GLOBALSERVICES */

} /* s_nick() */

/*
s_privmsg()
  Process a PRIVMSG statement
  av[0] = who
  av[1] = "PRIVMSG"
  av[2] = destination
  av[3] = actual message
*/

static void
s_privmsg(int ac, char **av)

{
	char *who, /* nick who gave privmsg */
	  *command, /* contains the /msg that 'who' gave */
	  *tmp;
	int issecured = 0;
	struct Luser *lptr, *serviceptr = NULL;
#ifdef PUBCOMMANDS
	int proceedpub = 0;
#endif

	if (ac < 4)
		return;

	if (av[0][0] == ':')
		who = av[0] + 1;
	else
		who = av[0];

	if (!(lptr = FindClient(who)))
		return;

	/* remove the leading ":" from the command string
	   I moved this code from below for smoother source tweak -kre */
	command = av[3] + 1;

	if (RestrictedAccess)
  {
		/*
		 * Check if the user has an "o" flag - if not, don't allow
		 * the command
		 */
		if (!IsOper(GetUser(0, lptr->nick, lptr->username, lptr->hostname)))
			return;
	}

#ifndef EXTREMEDEBUG
	if (IgnoreList)
	{
		char chkstr[MAXLINE + 1];

		if (lptr)
		{
			ircsprintf(chkstr, "%s!%s@%s", lptr->nick, lptr->username,
					   lptr->hostname);
			if (OnIgnoreList(chkstr))
				return;
		}
	}
#endif /* !EXTREMEDEBUG */

	if ((tmp = strchr(av[2], '@')))
	{
		if (!irccmp(tmp + 1, Me.name))
			*tmp = '\0';
		issecured = 1;
	}
	else
		issecured = 0;

	serviceptr = GetService(av[2]);

/* 
 If PUBCOMMANDS is defined, we need to match this pub commands first and
 if any command matched, check flood then process, so matching is here /
 CoolCold /
*/
#ifdef PUBCOMMANDS
	/* match, then flood check, then proceed */
	if (!serviceptr)
	{
#ifdef CHANNELSERVICES
		if (match("!OP*", command))
		{
			proceedpub = CS_PUB_OP;
		}
		else if (match("!DEOP*", command)) 
		{
			proceedpub = CS_PUB_DEOP;
		}
		else
#ifdef SEENSERVICES
		if (match("!SEENNICK *", command))
		{
			proceedpub = SS_PUB_SEENNICK;
		}
		else if (match("!SEEN *", command))
		{
			proceedpub = SS_PUB_SEEN;
		}
#else
		{
		}
#endif
	}
#endif
#endif

#ifdef PUBCOMMANDS
	if (FloodProtection && (serviceptr || proceedpub))
#else
	if (FloodProtection && serviceptr)
#endif
	{
		if (!IsValidAdmin(lptr))
		{
			lptr->messages++;
			if (!lptr->msgs_ts[0])
				lptr->msgs_ts[0] = current_ts;
			else
			{
				lptr->msgs_ts[1] = current_ts;
				if (lptr->messages == FloodCount)
				{
					lptr->messages = 0;
					if ((lptr->msgs_ts[1] - lptr->msgs_ts[0]) <= FloodTime)
					{
						char *mask = HostToMask(lptr->username,
								lptr->hostname);

						/*
						 * they sent too many messages in too little time
						 */
						lptr->flood_trigger++;
						if (lptr->flood_trigger >= IgnoreOffenses)
						{
							AddIgnore(mask, 0);
							notice(n_OperServ, lptr->nick,
								   "Maximum flood offenses reached, you are on permanent ignore");
							putlog(LOG2,
								   "PRIVMSG flood from %s!%s@%s (permanent ignore)",
								   lptr->nick,
								   lptr->username,
								   lptr->hostname);

							SendUmode(OPERUMODE_Y,
									  "*** Detected flood from %s!%s@%s (permanent ignore)",
									  lptr->nick,
									  lptr->username,
									  lptr->hostname);
						}
						else
						{
							AddIgnore(mask, IgnoreTime);
							notice(n_OperServ, lptr->nick,
								   "Received %d+ messages in <= %d seconds, you are being ignored for %s",
								   FloodCount,
								   FloodTime,
								   timeago(IgnoreTime, 3));
							putlog(LOG2,
								   "PRIVMSG flood from %s!%s@%s (%s ignore)",
								   lptr->nick,
								   lptr->username,
								   lptr->hostname,
								   timeago(IgnoreTime, 2));

							SendUmode(OPERUMODE_Y,
									  "*** Detected flood from %s!%s@%s (%s ignore)",
									  lptr->nick,
									  lptr->username,
									  lptr->hostname,
									  timeago(IgnoreTime, 2));
						}

						MyFree(mask);
						return;
					}
					else
					{
						lptr->messages = 1;
						lptr->msgs_ts[0] = current_ts;
					}
				}
				else
				{
					if ((lptr->msgs_ts[1] - lptr->msgs_ts[0]) > FloodTime)
					{
						/*
						 * FloodTime seconds has passed since their last
						 * message, reset everything
						 */
						lptr->messages = 1;
						lptr->msgs_ts[0] = current_ts;
					}
				}
			}
		} /* if (!IsValidAdmin(lptr)) */
	} /* if (FloodProtection && GetService(av[2])) */

	if (!serviceptr && (*command == '\001'))
	{
		struct Channel *chptr;

		chptr = FindChannel(av[2]);
		if (IsChannelMember(chptr, Me.osptr))
		{
			/* its a ctcp request to a channel that n_OperServ is on */
			onctcp(who, n_OperServ, command);
		}

#if defined(NICKSERVICES) && defined(CHANNELSERVICES)
		if (IsChannelMember(chptr, Me.csptr))
		{
			/* its a ctcp request to a channel that n_ChanServ is on */
			onctcp(who, n_ChanServ, command);
		}
#endif /* defined(NICKSERVICES) && defined(CHANNELSERVICES) */

		return;
	}

	if (serviceptr && (command[0] == '\001'))
	{
		/* process ctcp request */
		onctcp(who, serviceptr->nick, command);
		return;
	}

#ifdef PUBCOMMANDS 
	if (proceedpub) /* let's check did we match some pub command or not /
					   CoolCold / */
	{
		/* now create common part of all public commands */
		struct Channel *chptr;
		struct ChanInfo *ci = NULL;
		char **tmpargv = NULL;
		char *pubcommand;
		int acnt, i;
		char tmpcommand[MAXLINE + 1];

		if (!(chptr = FindChannel(av[2])))
			return;

		if (!((ci = FindChan(chptr->name)) && (ci->flags & CS_PUBCOMMANDS)))
			return;

		pubcommand = MyStrdup(command + 1);
		acnt = SplitBuf(pubcommand, &tmpargv);

		switch (proceedpub)
		{
			case CS_PUB_OP:
				if (!IsChannelMember(chptr, Me.csptr))
				{
					MyFree(tmpargv);
					MyFree(pubcommand);
					return;
				}
				
				if (acnt > 1)
				{
					ircsprintf(tmpcommand, "OP %s", chptr->name);
					for (i = 1; i < acnt; ++i)
					{
						strlcat(tmpcommand, " ", sizeof(tmpcommand));
						strlcat(tmpcommand, tmpargv[i],
								sizeof(tmpcommand));
					}

				}
				else
					ircsprintf(tmpcommand, "OP %s %s", chptr->name,
						lptr->nick);
				cs_process(who, tmpcommand);
				break;

			case CS_PUB_DEOP:
				if (!IsChannelMember(chptr, Me.csptr))
				{
					MyFree(tmpargv);
					MyFree(pubcommand);
					return;
				}
				
				if (acnt > 1)
				{
					ircsprintf(tmpcommand, "OP %s", chptr->name);
					for (i = 1; i < acnt; ++i)
					{
						strlcat(tmpcommand, " -", sizeof(tmpcommand));
						strlcat(tmpcommand, tmpargv[i],
								sizeof(tmpcommand));
					}

				}
				else
					ircsprintf(tmpcommand, "OP %s -%s", chptr->name,
						lptr->nick);
				cs_process(who, tmpcommand);
				break;

#ifdef SEENSERVICES
			case SS_PUB_SEEN:
			case SS_PUB_SEENNICK:
				if (!IsChannelMember(chptr, Me.esptr))
				{
					MyFree(tmpargv);
					MyFree(pubcommand);
					return;
				}
				
				strlcpy(tmpcommand, command + 1, sizeof(tmpcommand));
				es_process(who, tmpcommand);
				break;
#endif /* SEENSERVICES */
		}
		
		MyFree(tmpargv);
		MyFree(pubcommand);

		return;
	}
#endif
	
	if (SecureMessaging && !issecured && serviceptr)
	{
		notice(serviceptr->nick, lptr->nick,
				"Please use secure messaging (example: \002/%s <command>\002 or \002/msg %s@%s\002)",
				serviceptr->nick, serviceptr->nick, Me.name);
		return;
	}

	if (serviceptr == Me.osptr)
		os_process(who, command, NODCC);

#ifdef NICKSERVICES

	else if (serviceptr == Me.nsptr)
		ns_process(who, command);

#ifdef CHANNELSERVICES

	else if (serviceptr == Me.csptr)
		cs_process(who, command);
#endif

#ifdef MEMOSERVICES

	else if (serviceptr == Me.msptr)
		ms_process(who, command);
#endif

#endif /* NICKSERVICES */

#ifdef STATSERVICES

	else if (serviceptr == Me.ssptr)
		ss_process(who, command);

#endif /* STATSERVICES */

#ifdef HELPSERVICES

	else if (serviceptr == Me.hsptr)
		hs_process(who, command);

#endif /* HELPSERVICES */

#ifdef GLOBALSERVICES

	else if (serviceptr == Me.gsptr)
		gs_process(who, command);

#endif /* GLOBALSERVICES */

#ifdef SEENSERVICES

	else if (serviceptr == Me.esptr)
		es_process(who, command);

#endif /* SEENSERVICES */

} /* s_privmsg() */

/*
s_squit()
 Process an SQUIT statement
  av[0] = "SQUIT"
  av[1] = server name
  av[2] = reason
 
  or:
 
  av[0] = who
  av[1] = "SQUIT"
  av[2] = server name
  av[3] = reason
*/

static void
s_squit(int ac, char **av)

{
	struct Server *sptr;
#ifdef SPLIT_INFO

	struct Server *tmpserv;
#endif

#ifdef ALLOW_JUPES
	struct Jupe *tmpjupe;
#endif

	if (ac == 3)
		sptr = FindServer(av[1]);
	else if (ac == 4)
		sptr = FindServer(av[2]);
	else
		sptr = NULL;

#ifdef ALLOW_JUPES
	/* if manual squitting a juped server, get it back in! */
	/* XXX: refactor this back into jupe.c function -kre */
	if ((tmpjupe = IsJupe(av[ac-2])))
	{
	    /* it's our fake server, just get it back in (already hash-added) 
	     */
	    if (sptr)
	      toserv(":%s SERVER %s 2 :Juped: %s\r\n", Me.name,
		     sptr->name, tmpjupe->reason);

	    /* it's a remote server quiting (most probably after SQUIT in jupe.c),
	     * which is to be juped
	     */
	    else
	      FakeServer(av[ac-2], tmpjupe->reason);

		return;
	}
#endif
	/* If we defined more info on split, we will not delete
	 * non-intentionally splitted servers from hash -kre */

#ifndef SPLIT_INFO

	DeleteServer(sptr); /* delete server from list */
#else

	/* Iterate all server list. Enter split_ts for both splitted hub and his
	* leaves. NULLify uplinks for splitted leaves if there are any -kre */
	for (tmpserv=ServerList; tmpserv; tmpserv=tmpserv->next)
	{
		if (tmpserv->uplink==sptr || tmpserv==sptr)
		{

#ifdef EXTRA_SPLIT_INFO
			/* This could spawn too much messages -kre */
			SendUmode(OPERUMODE_L,
					  "*** Netsplit: %s (hub: %s)", tmpserv->name,
					  tmpserv->uplink ? tmpserv->uplink->name : "*unknown*");
#endif

			tmpserv->uplink = NULL;
			tmpserv->split_ts = current_ts;
			tmpserv->connect_ts = 0;
		}
	}
#endif
} /* s_squit() */

/*
s_quit()
  Process a QUIT statement
  av[0] = who
  av[1] = "QUIT"
  av[2] = message
*/

static void
s_quit(int ac, char **av)

{
	struct Luser *lptr;
#ifdef NICKSERVICES

	struct NickInfo *nptr;
#endif

	if (ac < 3)
		return;

	if (av[0][0] == ':')
		lptr = FindClient(av[0] + 1);
	else
		lptr = FindClient(av[0]);

	if (!lptr)
	{
		putlog(LOG1,
			   "DEBUG: s_quit(): Unable to locate client [%s]",
			   av[0] + 1);
		return;
	}

#ifdef NICKSERVICES
	if (LastSeenInfo && (nptr = FindNick(lptr->nick)))
	{
		if (nptr->flags & NS_IDENTIFIED)
		{
			struct NickInfo *tmp;

			MyFree(nptr->lastqmsg);
			nptr->lastqmsg = MyStrdup(av[2] + 1);
			tmp = GetMaster(nptr);
			tmp->lastseen = nptr->lastseen = current_ts;

#ifdef RECORD_RESTART_TS
			/* Invalidating nick_ts on a QUIT only if it was set within
			 * the last 5 minutes (+10% slack), i.e. the maximum
			 * inter-server TS differential that hybrid will allow
			 * -Brian Brazil */
			if (current_ts - 330 < nptr->nick_ts)
			{
				nptr->nick_ts = 0;
				MyFree(nptr->last_server);
			}
#endif /* RECORD_RESTART_TS */
		}
	}
#endif /* NICKSERVICES */

#ifdef SEENSERVICES
	es_add(lptr->nick, lptr->username, lptr->hostname, av[2] + 1,
		   current_ts, 1);
#endif /* SEENSERVICES */

#ifdef ADVFLOOD
#if 0

	if (lptr->server != Me.sptr)
#endif

		updateConnectTable(lptr->username, lptr->hostname);
#endif /* ADVFLOOD */

	DeleteClient(lptr); /* delete user */
} /* s_quit() */

/*
s_kill()
  Process a KILL statement
  av[0] = who
  av[1] = "KILL"
  av[2] = victim
  av[3] = message
*/

static void
s_kill(int ac, char **av)

{
	struct Luser *kuser,
				*lptr,
				*serviceptr;
	struct Server *sptr;
	char *who;
	struct NickInfo *nptr;

	if (ac < 4)
		return;

	kuser = FindClient(av[2]);
	if (!kuser)
		return;

	DeleteClient(kuser);

	if (av[0][0] == ':')
		who = av[0] + 1;
	else
		who = av[0];

	lptr = FindClient(who);

	if (lptr)
	{
		SendUmode(OPERUMODE_CAPO,
				  "*** Operator Kill: [%s] by %s!%s@%s (%s)",
				  av[2],
				  lptr->nick,
				  lptr->username,
				  lptr->hostname,
				  av[3] + 1);
	}
	else
	{
		SendUmode(OPERUMODE_CAPS,
				  "*** Server Kill: [%s] by %s (%s)",
				  av[2],
				  who,
				  av[3] + 1);
	}

#if defined NICKSERVICES && defined RECORD_RESTART_TS
	if ((nptr = FindNick(av[2])))
	{
		nptr->nick_ts = 0;
		MyFree(nptr->last_server);
	}
#endif /* NICKSERVICES && RECORD_RESTART_TS */

	if (match("*.*", who))
	{
		sptr = FindServer(who);
		if (sptr)
			sptr->numservkills++;
		Network->TotalServKills++;
#ifdef STATSERVICES

		if (Network->TotalServKills > Network->ServKillsT)
			Network->ServKillsT = Network->TotalServKills;
#endif

	}
	else
	{
		if (lptr)
		{
			if (lptr->server)
				++lptr->server->numoperkills;

#ifdef STATSERVICES

			++lptr->numkills;
#endif

		}

		++Network->TotalOperKills;
#ifdef STATSERVICES

		if (Network->TotalOperKills > Network->OperKillsT)
			Network->OperKillsT = Network->TotalOperKills;
#endif

	}

	if ((serviceptr = GetService(av[2])))
	{
		/* a service nick was killed - reintroduce them to the network */

		/*
		 * NOTE: serviceptr should ONLY be used for it's pointer value
		 * now, since the Luser structure it points to has been
		 * deleted by DeleteClient()
		 */

		if (lptr)
		{
			putlog(LOG1, "%s was killed by %s!%s@%s, re-initializing", av[2],
				   lptr->nick, lptr->username, lptr->hostname);

#ifdef SERVICES_FIGHT_FIX

			CheckServCollide(lptr->server);
#endif /* SERVICES_FIGHT_FIX */

		}
		else
		{
			putlog(LOG1,
				   "%s was killed by %s (nick collide), re-initializing",
				   av[2], who);

			toserv(":%s KILL %s :%s\r\n", Me.name, av[2],
				   "Attempt to Nick Collide Services");

			if (Me.sptr)
				++Me.sptr->numservkills;
			++Network->TotalServKills;
#ifdef STATSERVICES

			if (Network->TotalServKills > Network->ServKillsT)
				Network->ServKillsT = Network->TotalServKills;
#endif

		}

		/* re-introduce the killed *Serv to the network */
		InitServs(serviceptr);

		/*
		 * We can't do: if (serviceptr == Me.osptr) here, because
		 * if OperServ was killed, InitServs() would have given
		 * Me.osptr a new value, where serviceptr would be
		 * the old value
		 */
		if (!irccmp(av[2], n_OperServ))
		{
			CheckChans();
		}

#if defined(NICKSERVICES) && defined(CHANNELSERVICES)

		/*
		 * If ChanServ was killed, rejoin all it's channels
		 */
		else if (!irccmp(av[2], n_ChanServ))
			cs_RejoinChannels();

#endif

	}
} /* s_kill() */

/*
s_kick()
  Process a KICK statement
  av[0] = who
  av[1] = "KICK"
  av[2] = channel
  av[3] = nickname
  av[4] = reason
*/

static void
s_kick(int ac, char **av)

{
	char *who;
	struct Luser *lptr,
				*kptr;
	struct Channel *chptr;

	if (ac < 5)
		return;

	if (!(kptr = FindClient(av[3])))
		return;

	if (!(chptr = FindChannel(av[2])))
		return;

	if (av[0][0] == ':')
		who = av[0] + 1;
	else
		who = av[0];

	lptr = FindClient(who);

	RemoveFromChannel(chptr, kptr);

	if (FindService(kptr))
		FloodCheck(chptr, lptr, kptr, 1);

#ifdef STATSERVICES

	if (lptr)
		lptr->numkicks++;
#endif

	if (lptr)
{
		putlog(LOG3, "%s: %s!%s@%s kicked %s [%s]",
			   av[2],
			   lptr->nick,
			   lptr->username,
			   lptr->hostname,
			   av[3],
			   av[4] + 1);

		SendUmode(OPERUMODE_K,
				  "*** %s: Kick [%s] by %s!%s@%s (%s)",
				  av[2],
				  av[3],
				  lptr->nick,
				  lptr->username,
				  lptr->hostname,
				  av[4] + 1);
	}
	else
	{
		putlog(LOG3, "%s: %s kicked %s [%s]",
			   av[2],
			   who,
			   av[3],
			   av[4] + 1);

		SendUmode(OPERUMODE_K,
				  "*** %s: Kick [%s] by %s (%s)",
				  av[2],
				  av[3],
				  who,
				  av[4] + 1);
	}
} /* s_kick() */

/*
s_mode()
  Process MODE statement
  av[0] = who
  av[1] = "MODE"
  av[2] = channel
  av[3] = modes
  av[4]- = optional args
*/

static void
s_mode(int ac, char **av)

{
	char *who,
	*modes;
	struct Luser *lptr;
	struct Channel *chptr;

	if (ac < 4)
		return;

	if (av[0][0] == ':')
		who = av[0] + 1;
	else
		who = av[0];
	lptr = FindClient(who);

	modes = GetString(ac - 3, av + 3);

	if (!modes)
		return;

	if (!IsChanPrefix(*av[2]) && lptr)
	{
		/* it was a umode change, not channel mode */
		UpdateUserModes(lptr, (*modes == ':') ? modes + 1 : modes);
		MyFree(modes);
		return;
	}

	if (!(chptr = FindChannel(av[2])))
	{
		MyFree(modes);
		return;
	}

	/* update modes field in chan structure */
	UpdateChanModes(lptr, who, chptr, modes);

	MyFree(modes);
} /* s_mode() */

/*
s_sjoin()
  Process an SJOIN statement
  av[0] = server name
  av[1] = "SJOIN"
  av[2] = channel TS
  av[3] = channel
  av[4] = channel modes
  av[5] = nicks
*/

void
s_sjoin(int ac, char **av)

{
	char sendstr[MAXLINE + 1];
	char modes[MAXLINE + 1];
	char **line;
	char **nicks; /* array of SJOINing nicknames */
	char *oldnick;
	struct Channel *cptr, *oldptr;
	int ncnt; /* number of SJOINing nicks */
	int mcnt;
	int chanserv_deopped = 0, operserv_deopped = 0, seenserv_devoiced = 0;

#if defined(NICKSERVICES) && defined(CHANNELSERVICES)

	struct ChanInfo *ci = NULL;
	char *currnick;
	int ii;
#endif

	if (!irccmp(av[1], "JOIN"))
	{
		char *chan;

		if (ac < 3)
			return;

		chan = (*av[2] == ':') ? av[2] + 1 : av[2];

		/* kludge for older ircds that don't use SJOIN */
		ircsprintf(sendstr, ":%s SJOIN %ld %s + :%s",
				   currenthub->realname, (long) current_ts, chan, av[0]);

		SplitBuf(sendstr, &line);
		SplitBuf(av[0], &nicks);

		cptr = AddChannel(line, 1, nicks);

		/* If the channel has a C: line, have OperServ join it */
		if (cptr)
			if (IsChannel(cptr->name) && !IsChannelMember(cptr, Me.osptr))
				os_join(cptr);

#if defined(NICKSERVICES) && defined(CHANNELSERVICES)

#ifdef HYBRID_ONLY

		if (cptr)
		{
			ci = FindChan(chan);
			if ((cptr->numusers <= 1) || cs_ShouldBeOnChan(ci))
				cs_join(ci);
		}

#else

		if (cptr)
		{
			if ((ci = FindChan(chan)) && !IsChannelMember(cptr, Me.csptr) &&
					!(ci->flags & CS_FORGET))
				cs_join(ci);
		}

#endif /* HYBRID_ONLY */

#endif

		MyFree(line);
		MyFree(nicks);

		return;
	}

	/*
	 * It is an SJOIN
	 */

	if (ac < 6)
		return;

	mcnt = 5; /* default position for channel nicks, if no limit/key */
	strlcpy(modes, av[4], sizeof(modes));

	/*
	 * names list *should* start with a :, if it doesn't, there's a
	 * limit and/or key 
	 */
	if (av[mcnt][0] != ':')
	{
		strlcat(modes, " ", sizeof(modes));
		strlcat(modes, av[mcnt], sizeof(modes));
		mcnt++;
		if (av[mcnt][0] != ':')
		{
			strlcat(modes, " ", sizeof(modes));
			strlcat(modes, av[mcnt], sizeof(modes));
			mcnt++;
		}
	}

	ncnt = SplitBuf(av[mcnt] + 1, &nicks);

	/* Safety check - if we got no nicknames, that is non-valid blank SJOIN
	 * and we should silently ignore it kre */
	if (!ncnt)
	{
		MyFree(nicks);
		return;
	}

	/*
	 * when a user joins the channel "0", they get parted from all their
	 * channels
	 */
	if (*av[3] == '0')
	{
		struct Luser *lptr;
		struct UserChannel *tempc;

		if (!(lptr = FindClient(nicks[0])))
		{
			MyFree(nicks);
			return;
		}

		while (lptr->firstchan)
		{
			tempc = lptr->firstchan->next;
			RemoveFromChannel(lptr->firstchan->chptr, lptr);
			lptr->firstchan = tempc;
		}

		MyFree(nicks);
		return;
	} /* if (*av[3] == '0') */

	oldptr = FindChannel(av[3]);
	oldnick = GetNick(nicks[0]);
	assert(oldnick != NULL); /* We should always know at least _first_
								nickname from list, since there would be
								no sjoin otherwise -kre */

	if (SafeConnect)
	{
		if (!oldptr)
		{
			/*
			 * It is a new channel - notify all +j users
			 */
			SendUmode(OPERUMODE_J,
					  "*** New channel: %s (created by %s)",
					  av[3],
					  oldnick);
		} /* if (!oldptr) */
		else
			SendUmode(OPERUMODE_J,
					  "*** Channel join: %s (%s)",
					  oldnick,
					  oldptr->name);
	}

	cptr = AddChannel(av, ncnt, nicks);

	if (!cptr)
	{
		MyFree(nicks);
		return; /* should never happen */
	}

	/*
	 * If oldptr is valid, it was an existing channel,
	 * so check TSora stuff to keep synched.
	 * Otherwise, it's a new channel, so check if
	 * OperServ should be monitoring it.
	 */

	if (oldptr)
	{
		if (atol(av[2]) < cptr->since)
		{
			struct ChannelUser *tempuser;
			struct UserChannel *tempchan;
			struct ChannelBan *nextban;
#ifdef HYBRID7

			struct InviteException *nextinvex;
#endif /* HYBRID7 */

			struct Exception *nextex;

#ifdef GECOSBANS

			struct ChannelGecosBan *nextgecosban;
#endif /* GECOSBANS */

			/*
			 * if the TS given in the SJOIN is less than the recorded 
			 * channelTS, everyone in the channel except the SJOIN'ing 
			 * nick(s) is deoped/devoiced, all previous modes are replaced 
			 * with the new modes, and all previous bans are unset
			 */

			cptr->since = atol(av[2]); /* keep the older TS */
			/* keep older modes */
			cptr->modes = 0;
			UpdateChanModes(0, av[0] + 1, cptr, modes);

			/* clear all bans */
			while (cptr->firstban)
			{
				nextban = cptr->firstban->next;
				if (cptr->firstban->who)
					MyFree(cptr->firstban->who);
				MyFree(cptr->firstban->mask);
				MyFree(cptr->firstban);
				cptr->firstban = nextban;
			}

			/* clear all exceptions */
			while (cptr->exceptlist)
			{
				nextex = cptr->exceptlist->next;
				if (cptr->exceptlist->who)
					MyFree(cptr->exceptlist->who);
				MyFree(cptr->exceptlist->mask);
				MyFree(cptr->exceptlist);
				cptr->exceptlist = nextex;
			}

#ifdef HYBRID7
			/* clear all inviteexceptions */
			while (cptr->inviteexceptlist)
			{
				nextinvex = cptr->inviteexceptlist->next;
				if (cptr->inviteexceptlist->who)
					MyFree(cptr->inviteexceptlist->who);
				MyFree(cptr->inviteexceptlist->mask);
				MyFree(cptr->inviteexceptlist);
				cptr->inviteexceptlist = nextinvex;
			}
#endif /* HYBRID7 */

#ifdef GECOSBANS
			/* clear all gecos bans */
			while (cptr->firstgecosban)
			{
				nextgecosban = cptr->firstgecosban->next;
				if (cptr->firstgecosban->who)
					MyFree(cptr->firstgecosban->who);
				MyFree(cptr->firstgecosban->mask);
				MyFree(cptr->firstgecosban);
				cptr->firstgecosban = nextgecosban;
			}
#endif /* GECOSBANS */

			for (tempuser = cptr->firstuser; tempuser; tempuser = tempuser->next)
			{
				/* don't deop the nicks who just SJOIN'd */
				if (IsInNickArray(ncnt, nicks, tempuser->lptr->nick))
					continue;

#if defined(CHANNELSERVICES)

				if (tempuser->flags & CH_OPPED)
				{
					/* never set ChanServ/OperServ as deopped.. -adx */
					if (tempuser->lptr == Me.csptr)
						chanserv_deopped = 1;
					else if (tempuser->lptr == Me.osptr)
						operserv_deopped = 1;
					else
					{
						tempuser->flags &= ~CH_OPPED;
						tempchan = FindChannelByUser(tempuser->lptr, cptr);
						if (tempchan)
							tempchan->flags &= ~CH_OPPED;
					}
				}
#endif
#ifdef HYBRID7_HALFOPS
				/* Yeps, do same for halfops -Janus */
				if (tempuser->flags & CH_HOPPED)
				{
					tempuser->flags &= ~CH_HOPPED;
					tempchan = FindChannelByUser(tempuser->lptr, cptr);
					if (tempchan)
						tempchan->flags &= ~CH_HOPPED;
				}
#endif /* HYBRID7_HALFOPS */
				if (tempuser->flags & CH_VOICED)
				{
#ifdef SEENSERVICES
				  if (tempuser->lptr == Me.esptr)
				    seenserv_devoiced = 1;
		  
				  else
				    {
#endif
					tempuser->flags &= ~CH_VOICED;
					tempchan = FindChannelByUser(tempuser->lptr, cptr);
					if (tempchan)
						tempchan->flags &= ~CH_VOICED;
#ifdef SEENSERVICES
				    }
#endif
				}
			} /* for (tempuser .. ) */

			if (operserv_deopped)
			{
				/* n_OperServ was deoped in the sjoin, must reop it */
#ifdef SAVE_TS
				os_part(cptr);
				os_join(cptr);
#else

				toserv(":%s MODE %s +o %s\r\n",
					   Me.name, cptr->name, n_OperServ);
#endif

				putlog(LOG2, "%s: %s attempted to deop %s", cptr->name,
					   av[0] + 1, n_OperServ);
			}

#if defined(NICKSERVICES) && defined(CHANNELSERVICES)
			if (chanserv_deopped)
			{
				/* n_ChanServ was deoped in the sjoin, must reop it */
#ifdef SAVE_TS
				cs_part(cptr);
				cs_join(FindChan(cptr->name));
#else

				toserv(":%s MODE %s +o %s\r\n",
					   Me.name, cptr->name, n_ChanServ);
#endif

				putlog(LOG2, "%s: %s attempted to deop %s",
					   cptr->name,
					   av[0] + 1,
					   n_ChanServ);
			}
#endif
#ifdef SEENSERVICES
                        if (seenserv_devoiced)
			  {
			    /* n_SeenServ was devoiced in the sjoin, must revoice it */
#ifdef SAVE_TS
			    ss_part(cptr);
			    ss_join(cptr);
#else

			    toserv(":%s MODE %s +v %s\r\n",
				   Me.name, cptr->name, n_SeenServ);
#endif

			    putlog(LOG2, "%s: %s attempted to devoice %s", cptr->name,
				   av[0] + 1, n_SeenServ);
			  }
#endif /* SEENSERVICES */

		} /* if (atol(av[2]) < cptr->since) */
	} /* if (oldptr) */
	else
	{
		/*
		 * It is a new channel - check if OperServ should be
		 * monitoring it
		 */

		if (IsChannel(cptr->name) && !IsChannelMember(cptr, Me.osptr))
			os_join(cptr);

	} /* !oldptr */

#if defined NICKSERVICES && defined CHANNELSERVICES

	ci = FindChan(cptr->name);

	if (ci)
	{
		/*
		 * Call cs_CheckSjoin() to check if any of the SJOINing
		 * nicks should be deopped. If oldptr is NULL,
		 * the channel did not exist before, so pass an arguement
		 * of 1 to cs_CheckSjoin()'s "newchan" parameter indicating
		 * the channel is brand new.
		 */
		cs_CheckSjoin(cptr, ci, ncnt, nicks, oldptr ? 0 : 1);

		for (ii = 0; ii < ncnt; ii++)
		{
			currnick = GetNick(nicks[ii]);
			if (!currnick)
				continue;

			/*
			 * Check if nick is on the AKICK list for chname
			 */
			cs_CheckJoin(cptr, ci, currnick);

			/*
			 * Check if nick has AutoOp access on channel, if so, op them
			 */
			cs_CheckOp(cptr, ci, currnick);
		}
	} /* if (ci) */
#endif /* defined(NICKSERVICES) && defined(CHANNELSERVICES) */

	MyFree(nicks);
} /* s_sjoin() */

/*
s_part()
  Process a PART statement
  av[0] = who
  av[1] = "PART"
  av[2] = channel
*/

static void
s_part(int ac, char **av)

{
	char *who;

	if (ac < 3)
		return;

	if (av[0][0] == ':')
		who = av[0] + 1;
	else
		who = av[0];

	RemoveFromChannel(FindChannel(av[2]), FindClient(who));
} /* s_part() */

/*
s_error()
  Log the error that the hub sends
*/

static void
s_error(int ac, char **av)

{
	char *serror;

	if (ac >= 3)
		serror = av[2] + 1;
	else if (ac >= 2)
		serror = av[1] + 1;
	else
		serror = "";

	putlog(LOG1, "Server Error: %s", serror);
} /* s_error() */

/*
s_whois()
  Reply to a remote whois query
 
  av[0] = who
  av[1] = "WHOIS"
  av[2] = nickname
  av[3] = nickname
*/

static void
s_whois(int ac, char **av)

{
	char *who, *target, *isoper;
	struct Luser *serviceptr;

	if (ac < 4)
		return;

	if (av[0][0] == ':')
		who = av[0] + 1;
	else
		who = av[0];

	if (av[3][0] == ':')
		target = av[3] + 1;
	else
		target = av[3];

	if (!(serviceptr = GetService(target)))
		return;

	SendUmode(OPERUMODE_Y,
			  "*** Remote whois [%s] requested by %s",
			  serviceptr->nick,
			  who);

	isoper = strchr(ServiceUmodes, 'o');

	toserv(":%s 311 %s %s %s %s * :%s\r\n",
		   Me.name, who, serviceptr->nick, serviceptr->username, Me.name,
		   serviceptr->realname);

	toserv(":%s 312 %s %s %s :%s\r\n",
		   Me.name, who, serviceptr->nick, Me.name, Me.info);

	if (isoper)
	{
		toserv(":%s 313 %s %s :is Network Service daemon\r\n",
			   Me.name, who, serviceptr->nick);
	}

	toserv(":%s 317 %s %s 0 %ld :seconds idle, signon time\r\n",
		   Me.name, who, serviceptr->nick, TimeStarted);

	toserv(":%s 318 %s %s :End of /WHOIS list.\r\n",
		   Me.name, who, serviceptr->nick);
} /* s_whois() */

/*
s_trace()
  Reply to a remote trace query
 
  av[0] = who
  av[1] = "TRACE"
  av[2] = server name
*/

static void
s_trace(int ac, char **av)

{
	char *who, *isoper;
	struct aService *sptr;

	if (ac < 3)
		return;

	if (irccmp(av[2] + 1, Me.name) != 0)
		return;

	isoper = strchr(ServiceUmodes, 'o');

	if (av[0][0] == ':')
		who = av[0] + 1;
	else
		who = av[0];

	SendUmode(OPERUMODE_Y,
			  "*** Remote trace query requested by %s",
			  who);

	for (sptr = ServiceBots; sptr->name; ++sptr)
	{
		toserv(":%s %d %s %s %d :%s[%s@%s]\r\n",
			   Me.name,
			   isoper ? 204 : 205,
			   who,
			   isoper ? "Oper" : "User",
			   isoper ? 5 : 1,
			   *(sptr->name),
			   *(sptr->ident),
			   Me.name);
	}

	toserv(":%s 206 %s Serv 10 %1.0fS %1.0fC %s :AutoConn.!*@%s\r\n",
		   Me.name,
		   who,
		   Network->TotalServers,
		   Network->TotalUsers,
		   currenthub->realname ? currenthub->realname : currenthub->hostname,
		   Me.name);

	toserv(":%s 262 %s %s :End of TRACE\r\n",
		   Me.name,
		   who,
		   Me.name);
} /* s_trace() */

/*
s_version()
  Reply to a remote version query
 
  av[0] = who
  av[1] = "VERSION"
  av[2] = server name
*/

static void
s_version(int ac, char **av)

{
	char *who;

	if (ac < 3)
		return;

	if (irccmp(Me.name, (av[2][0] == ':') ? av[2] + 1 : av[2]) != 0)
		return;

	if (av[0][0] == ':')
		who = av[0] + 1;
	else
		who = av[0];

	SendUmode(OPERUMODE_Y,
			  "*** Remote version query requested by %s",
			  who);

	toserv(":%s 351 %s Hybserv2-%s. %s :TS3\r\n",
		   Me.name, who, hVersion, Me.name);
} /* s_version() */

/*
s_motd()
  Send global motd to av[0]
 
  av[0] = who
  av[1] = "MOTD"
  av[2] = server name
*/

static void
s_motd(int ac, char **av)

{
	char *ch;
	char *who, *final;
	char line[MAXLINE + 1];
	FILE *fp;

	if (ac < 3)
		return;

	if (irccmp(Me.name, (av[2][0] == ':') ? av[2] + 1 : av[2]) != 0)
		return;

	if (av[0][0] == ':')
		who = av[0] + 1;
	else
		who = av[0];

	SendUmode(OPERUMODE_Y,
			  "*** Remote motd query requested by %s",
			  who);

	if (!(fp = fopen(MotdFile, "r")))
	{
		toserv(":%s 422 %s :MOTD File is missing\r\n",
			   Me.name,
			   who);
		return;
	}

	toserv(":%s 375 %s :- %s Message of the Day -\r\n",
		   Me.name,
		   who,
		   Me.name);

	while (fgets(line, sizeof(line), fp))
	{
		if ((ch = strchr(line, '\n')) != NULL)
			*ch = '\0';

		if (*line == '\0')
		{
			toserv(":%s 372 %s :- \r\n", Me.name, who);
			continue;
		}
		else
		{
			final = Substitute(NULL, line, NODCC);
			if (final && (final != (char *) -1))
			{
				toserv(":%s 372 %s :- %s\r\n", Me.name, who, final);
				MyFree(final);
			}
		}
	}

	toserv(":%s 376 %s :End of /MOTD command\r\n", Me.name, who);

	fclose(fp);
} /* s_motd() */

#if defined(NICKSERVICES) && defined(CHANNELSERVICES)

/*
s_topic()
  Check if topic change conflicts with a locked topic on channel
av[2]
 
  av[0] = who
  av[1] = "TOPIC"
  av[2] = channel
  av[3]- = new topic
*/

static void
s_topic(int ac, char **av)

{
	if (ac < 4)
		return;

	SendUmode(OPERUMODE_T,
			  "*** %s: Topic by %s (%s)",
			  av[2],
			  av[0] + 1,
			  av[3] + 1);

	cs_CheckTopic(av[0] + 1, av[2]);
} /* s_topic() */

#endif

#ifdef STATSERVICES

/*
s_pong()
 Process a server PONG
 
  av[0] = :servername
  av[1] = "PONG"
  av[2] = servername
  av[3] = Me.name
*/

static void
s_pong(int ac, char **av)

{
	struct timeval pong_t;
	time_t currts;
	long deltasec, deltausec;
	struct Server *servptr;
	char *who;

	who = av[0];
	if (*who == ':')
		who++;

	if (!(servptr = FindServer(who)))
		return; /* server doesn't exist */

	/*
	 * If servptr->lastping_* is 0, we never pinged them, so
	 * they shouldn't be PONGing us
	 */
	if (!servptr->lastping_sec || !servptr->lastping_usec)
		return;

	GetTime(&pong_t);
	currts = current_ts;

	/*
	 * Now set delta variables to the delta given by
	 * pong_t->tv_{u}sec - servptr->lastping_{u}sec.
	 * That will represent how much time has passed since we
	 * sent the ping requests.
	 */
	deltasec = pong_t.tv_sec - servptr->lastping_sec;
	deltausec = pong_t.tv_usec - servptr->lastping_usec;

	/*
	 * Finally, set the current ping variable to the float number:
	 * "deltasec.deltausec".
	 * Of course, we must divide the microseconds by one million
	 * since a microsecond is 10^-6 seconds.
	 */
	servptr->ping = (float) (deltasec + (deltausec / 1000000.0));

	/*
	 * Now determine if this current ping reply is the highest
	 * or lowest we've received from this server.
	 */
	if (servptr->ping > servptr->maxping)
	{
		servptr->maxping = servptr->ping;
		servptr->maxping_ts = currts;
	}

	if ((servptr->minping == 0.0) || (servptr->ping < servptr->minping))
	{
		servptr->minping = servptr->ping;
		servptr->minping_ts = currts;
	}

	/*
	 * Now reset the lastping fields so we can ping the server
	 * again in the future.
	 */
	servptr->lastping_sec = servptr->lastping_usec = 0;

	if (MaxPing)
	{
		/*
		 * Make sure servptr is our current hub. Our hub's uplink
		 * field will ALWAYS match services, even though we are
		 * not the actual hub of our hub.
		 */
		if ((servptr->uplink == Me.sptr) && (servptr->ping > (float) MaxPing))
		{
			/*
			 * servptr matches our current hub server, and it's ping
			 * reply is greater than MaxPing - attempt to switch
			 * hubs - make sure there's at least 1 other hub to
			 * switch to.
			 */
			if (HubCount > 1)
			{
				SendUmode(OPERUMODE_Y,
						  "*** Ping response time from hub (%s) has exceeded [%s], rerouting",
						  servptr->name,
						  timeago(MaxPing, 3));

				toserv(":%s ERROR :Rerouting (lag: %5.4f seconds)\r\n",
					   Me.name, servptr->ping);
				toserv(":%s QUIT\r\n", Me.name);

				close(HubSock);
				ClearUsers();
				ClearChans();
				ClearServs();
				ClearHashes(0);

				HubSock = NOSOCKET;

				/*
				 * Now, try to cycle through server list for a new
				 * hub
				 */
				CycleServers();
			}
		}
	} /* if (MaxPing) */
} /* s_pong() */

#endif /* STATSERVICES */

#ifdef SERVICES_FIGHT_FIX
/* This is preliminary version of the code, I will work on this later. It
 * will probably have jupe/etc.. options -kre */
static void CheckServCollide(struct Server *bad_server)
{

	/* Check if those two options are set at all */
	if (MaxServerCollides && MinServerCollidesDelta)
	{
		/* If this is first collide TS */
		if (!ServerCollidesTS)
		{
			ServerCollidesTS = current_ts;
			return;
		}

		/* Check if collides went over top */
		if (ServerCollides >= MaxServerCollides)
		{
			/* When was first collide? */
			if (current_ts - ServerCollidesTS < MinServerCollidesDelta)
			{
				char note1[] = "Detected services fight from %s";
				char *reason = MyMalloc(sizeof(note1) + sizeof(bad_server->name));
				ircsprintf(reason, note1, bad_server->name);

				SendUmode(OPERUMODE_Y, "%s", reason);
				DoShutdown(NULL, reason);
			}
			else
			{
				/* We had some collides but they are slower than expected, so
				 * reset counter and timestamp */
				ServerCollides = 0;
				ServerCollidesTS = current_ts;
				return;
			}
		}

		ServerCollides++;
	}
}
#endif /* SERVICES_FIGHT_FIX */
