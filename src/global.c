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
#include "hash.h"
#include "helpserv.h"
#include "global.h"
#include "log.h"
#include "match.h"
#include "memoserv.h"
#include "misc.h"
#include "mystring.h"
#include "nickserv.h"
#include "operserv.h"
#include "settings.h"
#include "sprintf_irc.h"
#include "server.h"

#ifdef GLOBALSERVICES

static void g_help(struct Luser *, int, char **);
static void g_motd(struct Luser *, int, char **);
static void g_gnote(struct Luser *, int, char **);
static void g_gchannote(struct Luser *, int, char **);

#if defined(NICKSERVICES) && defined(MEMOSERVICES)

static void g_gmemo(struct Luser *, int, char **);

#ifdef CHANNELSERVICES
static void g_gcmemo(struct Luser *, int, char **);
#endif /* CHANNELSERVICES */

#endif /* defined(NICKSERVICES) && defined(MEMOSERVICES) */

static struct Command globalcmds[] =
    {
	    { "HELP", g_help, LVL_NONE
	    },
	    { "MOTD", g_motd, LVL_NONE },
	    { "LOGONNEWS", g_motd, LVL_NONE },

	    { "GNOTE", g_gnote, LVL_ADMIN },
	    { "GNOTICE", g_gnote, LVL_ADMIN },
	    { "GCHANNOTE", g_gchannote, LVL_ADMIN },
	    { "GCNOTE", g_gchannote, LVL_ADMIN },

#if defined(NICKSERVICES) && defined(MEMOSERVICES)

	    { "GMEMO", g_gmemo, LVL_ADMIN
	    },

#ifdef CHANNELSERVICES
	    { "GCMEMO", g_gcmemo, LVL_ADMIN },
#endif

#endif /* defined(NICKSERVICES) && defined(MEMOSERVICES) */

	    { 0, 0, 0 }
    };

/*
gs_process()
  Process command coming from 'nick' directed towards n_Global
*/

void
gs_process(char *nick, char *command)

{
	int acnt;
	char **arv;
	struct Command *cptr;
	struct Luser *lptr;

	if (!command || !(lptr = FindClient(nick)))
		return;

	if (Network->flags & NET_OFF)
	{
		notice(n_Global, lptr->nick,
		       "Services are currently \002disabled\002");
		return;
	}

	acnt = SplitBuf(command, &arv);
	if (acnt == 0)
	{
		MyFree(arv);
		return;
	}

	cptr = GetCommand(globalcmds, arv[0]);

	if (!cptr || (cptr == (struct Command *) -1))
	{
		notice(n_Global, lptr->nick,
		       "%s command [%s]",
		       (cptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
		       arv[0]);
		MyFree(arv);
		return;
	}

	/*
	 * Check if the command is for admins only - if so,
	 * check if they match an admin O: line.  If they do,
	 * check if they are EITHER oper'd, or registered with OperServ,
	 * if either of these is true, allow the command
	 */
	if ((cptr->level == LVL_ADMIN) && !(IsValidAdmin(lptr)))
	{
		notice(n_Global, lptr->nick, "Unknown command [%s]",
		       arv[0]);
		MyFree(arv);
		return;
	}

	/* call cptr->func to execute command */
	(*cptr->func)(lptr, acnt, arv);

	MyFree(arv);
} /* gs_process() */

/*
g_help()
 Give lptr help
*/

static void
g_help(struct Luser *lptr, int ac, char **av)

{
	if (ac >= 2)
	{
		char str[MAXLINE + 1];
		struct Command *cptr;

		for (cptr = globalcmds; cptr->cmd; ++cptr)
			if (!irccmp(av[1], cptr->cmd))
				break;

		if (cptr->cmd)
			if ((cptr->level == LVL_ADMIN) &&
			        !(IsValidAdmin(lptr)))
			{
				notice(n_MemoServ, lptr->nick,
				       "No help available on \002%s\002",
				       av[1]);
				return;
			}

		ircsprintf(str, "%s", av[1]);

		GiveHelp(n_Global, lptr->nick, str, NODCC);
	}
	else
		GiveHelp(n_Global, lptr->nick, NULL, NODCC);
} /* g_help() */

/*
g_motd()
 Send lptr->nick the motd (logon news)
*/

static void
g_motd(struct Luser *lptr, int ac, char **av)

{
	RecordCommand("%s: %s!%s@%s MOTD",
	              n_Global,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname);

	if (!Network->LogonNewsFile.Contents)
	{
		notice(n_Global, lptr->nick,
		       "No logon news specified");
		return;
	}

	SendMessageFile(lptr, &Network->LogonNewsFile);
} /* g_motd() */

/*
g_gnote()
 Send a notice to users on the network.
 
Options:
 -all      Send note to all users (default)
 -ops      Send note to all users who have an 'o' flag
 -opers    Send note to all IRC Operators
 -admins   Send note to all users who have an 'a' flag
*/

static void
g_gnote(struct Luser *lptr, int ac, char **av)

{
	int cnt,
	alen,
	bad,
	ii;
	int all,
	ops,
	opers,
	admins;
	char *message;
	char argbuf[MAXLINE + 1];
	char temp[MAXLINE * 2];
	struct Luser *tempuser;
	struct Userlist *userptr;

	if (!GlobalNotices)
	{
		notice(n_Global, lptr->nick,
		       "Global notices are disabled");
		return;
	}

	if (ac < 2)
	{
		notice(n_Global, lptr->nick,
		       "Syntax: \002GNOTE [options] <message>\002");
		return;
	}

	all = 0;
	ops = 0;
	opers = 0;
	admins = 0;
	message = NULL;

	for (cnt = 1; cnt < ac; ++cnt)
	{
		alen = strlen(av[cnt]);

		if (!ircncmp(av[cnt], "-all", alen))
			all = 1;
		else if (!ircncmp(av[cnt], "-ops", alen))
			ops = 1;
		else if (!ircncmp(av[cnt], "-opers", alen))
			opers = 1;
		else if (!ircncmp(av[cnt], "-admins", alen))
			admins = 1;
		else
		{
			message = GetString(ac - cnt, av + cnt);
			break;
		}
	}

	if (!message)
	{
		notice(n_Global, lptr->nick,
		       "No message specified");
		return;
	}

	if (!all && !ops && !opers && !admins)
		all = 1; /* -all is the default */

	argbuf[0] = '\0';

	if (all)
		strlcat(argbuf, "-all ", sizeof(argbuf));

	if (ops)
		strlcat(argbuf, "-ops ", sizeof(argbuf));

	if (opers)
		strlcat(argbuf, "-opers ", sizeof(argbuf));

	if (admins)
		strlcat(argbuf, "-admins ", sizeof(argbuf));

	ircsprintf(temp, "%s%s", argbuf, message);
	strlcpy(argbuf, temp, sizeof(argbuf));

	RecordCommand("%s: %s!%s@%s GNOTE %s",
	              n_Global,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              argbuf);

	cnt = 0;

	for (tempuser = ClientList; tempuser; tempuser = tempuser->next)
	{
		if (FindService(tempuser))
			continue;

		if (!ircncmp(Me.name, tempuser->server->name,
		             strlen(tempuser->server->name)))
			continue;

		bad = 0;

		if (!all)
		{
			if (opers && !IsOperator(tempuser))
				bad = 1;
			else
			{
				if (tempuser->flags & L_OSREGISTERED)
					ii = 1;
				else
					ii = 0;

				userptr = GetUser(ii,
				                  tempuser->nick,
				                  tempuser->username,
				                  tempuser->hostname);

				if (ops && !IsOper(userptr))
					bad = 1;
				else if (admins && !IsAdmin(userptr))
					bad = 1;
			}
		}

		if (bad)
			continue;

		notice(n_Global, tempuser->nick, "%s", message);

		++cnt;
	}

	notice(n_Global, lptr->nick,
	       "Message sent (%d match%s)",
	       cnt,
	       (cnt == 1) ? "" : "es");

	MyFree(message);
} /* g_gnote() */

/*
g_gchannote()
 Send a notice to specified channels
 
Options:
 -mask    Send message to channels matching the given mask
*/

static void
g_gchannote(struct Luser *lptr, int ac, char **av)

{
	int cnt,
	alen;
	char *mask;
	char *message;
	char argbuf[MAXLINE + 1],
	temp[MAXLINE + 1];
	struct Channel *cptr;

	if (!GlobalNotices)
	{
		notice(n_Global, lptr->nick, "Global notices are disabled");
		return;
	}

	if (ac < 2)
	{
		notice(n_Global, lptr->nick,
		       "Syntax: \002GCHANNOTE [options] <message>\002");
		return;
	}

	mask = NULL;
	message = NULL;

	for (cnt = 1; cnt < ac; ++cnt)
	{
		alen = strlen(av[cnt]);

		if (!ircncmp(av[cnt], "-mask", alen))
		{
			if (++cnt >= ac)
			{
				notice(n_Global, lptr->nick,
				       "No mask specified");
				return;
			}

			mask = av[cnt];
		}
		else
		{
			message = GetString(ac - cnt, av + cnt);
			break;
		}
	}

	if (!message)
	{
		notice(n_Global, lptr->nick,
		       "No message specified");
		return;
	}

	*argbuf = '\0';

	if (mask)
		ircsprintf(argbuf, "-mask %s ", mask);

	ircsprintf(temp, "%s%s", argbuf, message);

	strlcpy(argbuf, temp, sizeof(argbuf));

	RecordCommand("%s: %s!%s@%s GCHANNOTE %s",
	              n_Global,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              argbuf);

	cnt = 0;
	for (cptr = ChannelList; cptr; cptr = cptr->next)
	{
		if (mask && !match(mask, cptr->name))
			continue;

		*temp = '-';
		*(temp + 1) = '\0';
		if (cptr->modes & MODE_N)
			strlcat(temp, "n", sizeof(temp));
		if (cptr->modes & MODE_M)
			strlcat(temp, "m", sizeof(temp));

		if (*(temp + 1))
			DoMode(cptr, temp, 0);

		notice(n_Global, cptr->name, "%s", message);
		++cnt;

		if (*(temp + 1))
		{
			*temp = '+';
			DoMode(cptr, temp, 0);
		}
	}

	notice(n_Global, lptr->nick,
	       "Channel message sent (%d match%s)",
	       cnt,
	       (cnt == 1) ? "" : "es");

	MyFree(message);
} /* g_gchannote() */

#if defined(NICKSERVICES) && defined(MEMOSERVICES)

/*
g_gmemo()
 Send a memo to all registered nicks
*/

static void
g_gmemo(struct Luser *lptr, int ac, char **av)

{
	char *text;
	int ii,
	cnt;
	struct NickInfo *nptr;

	if (ac < 2)
	{
		notice(n_Global, lptr->nick,
		       "Syntax: \002GMEMO <memotext>\002");
		return;
	}

	text = GetString(ac - 1, av + 1);

	cnt = 0;
	for (ii = 0; ii < NICKLIST_MAX; ++ii)
	{
		for (nptr = nicklist[ii]; nptr; nptr = nptr->next)
		{
			if (StoreMemo(nptr->nick, text, lptr))
				++cnt;
		}
	}

	notice(n_Global, lptr->nick,
	       "Memo sent to %d nicknames",
	       cnt);

	MyFree(text);
} /* g_gmemo() */

#ifdef CHANNELSERVICES

/*
g_gcmemo()
 Send a memo to all registered channels
*/

static void
g_gcmemo(struct Luser *lptr, int ac, char **av)

{
	char *text;
	int ii,
	cnt;
	struct ChanInfo *cptr;

	if (ac < 2)
	{
		notice(n_Global, lptr->nick,
		       "Syntax: \002GCMEMO <memotext>\002");
		return;
	}

	text = GetString(ac - 1, av + 1);

	cnt = 0;
	for (ii = 0; ii < CHANLIST_MAX; ++ii)
	{
		for (cptr = chanlist[ii]; cptr; cptr = cptr->next)
		{
			if (StoreMemo(cptr->name, text, lptr))
				++cnt;
		}
	}

	notice(n_Global, lptr->nick,
	       "Memo sent to %d channels",
	       cnt);

	MyFree(text);
} /* g_gcmemo() */

#endif /* CHANNELSERVICES */

#endif /* defined(NICKSERVICES) && defined(MEMOSERVICES) */

#endif /* GLOBALSERVICES */
