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
#include "chanserv.h"
#include "client.h"
#include "conf.h"
#include "config.h"
#include "channel.h"
#include "data.h"
#include "dcc.h"
#include "err.h"
#include "helpserv.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "memoserv.h"
#include "misc.h"
#include "mystring.h"
#include "nickserv.h"
#include "operserv.h"
#include "settings.h"
#include "server.h"
#include "sock.h"
#include "timestr.h"
#include "sprintf_irc.h"

#ifdef NICKSERVICES

/*
 * hash containing registered nick info
 */
struct NickInfo *nicklist[NICKLIST_MAX];

static int ChangePass(struct NickInfo *, char *);
static void AddHostToNick(char *, struct NickInfo *);
static struct NickInfo *MakeNick(void);
static void AddNick(struct NickInfo *);
static int OnAccessList(char *, char *, struct NickInfo *);

#ifdef LINKED_NICKNAMES
static int InsertLink(struct NickInfo *hub, struct NickInfo *leaf);
static int DeleteLink(struct NickInfo *nptr, int copyhosts);
#endif /* LINKED_NICKNAMES */

static void n_help(struct Luser *, int, char **);
static void n_register(struct Luser *, int, char **);
static void n_drop(struct Luser *, int, char **);
static void n_identify(struct Luser *, int, char **);
static void n_recoverpass(struct Luser *, int, char **);
static void n_recover(struct Luser *, int, char **);
static void n_release(struct Luser *, int, char **);
static void n_ghost(struct Luser *, int, char **);

static void n_access(struct Luser *, int, char **);
static void n_access_add(struct Luser *, struct NickInfo *, int, char **);
static void n_access_del(struct Luser *, struct NickInfo *, int, char **);
static void n_access_list(struct Luser *, struct NickInfo *, int, char **);

static void n_set(struct Luser *, int, char **);
static void n_set_kill(struct Luser *, struct NickInfo *, int, char **);
static void n_set_automask(struct Luser *, struct NickInfo *, int, char **);
static void n_set_private(struct Luser *, struct NickInfo *, int, char **);
static void n_set_privmsg(struct Luser *, struct NickInfo *, int, char **);
static void n_set_secure(struct Luser *, struct NickInfo *, int, char **);
static void n_set_unsecure(struct Luser *, struct NickInfo *, int, char **);
static void n_set_memos(struct Luser *, struct NickInfo *, int, char **);
static void n_set_notify(struct Luser *, struct NickInfo *, int, char **);
static void n_set_signon(struct Luser *, struct NickInfo *, int, char **);
static void n_set_hide(struct Luser *, struct NickInfo *, int, char **);
static void n_set_password(struct Luser *, struct NickInfo *, int, char **);
static void n_set_phrase(struct Luser *, struct NickInfo *, int, char **);
static void n_set_email(struct Luser *, struct NickInfo *, int, char **);
static void n_set_url(struct Luser *, struct NickInfo *, int, char **);
static void n_set_uin(struct Luser *, struct NickInfo *, int, char **);
static void n_set_gsm(struct Luser *, struct NickInfo *, int, char **);
static void n_set_phone(struct Luser *, struct NickInfo *, int, char **);
#ifdef LINKED_NICKNAMES
static void n_set_master(struct Luser *, struct NickInfo *, int, char **);
#endif /* LINKED_NICKNAMES */

static void n_list(struct Luser *, int, char **);
static void n_info(struct Luser *, int, char **);

#ifdef LINKED_NICKNAMES
static void n_link(struct Luser *, int, char **);
static void n_unlink(struct Luser *, int, char **);
#endif /* LINKED_NICKNAMES */

static void n_fixts(struct Luser *, int, char **);

#ifdef EMPOWERADMINS

static void n_forbid(struct Luser *, int, char **);
static void n_unforbid(struct Luser *, int, char **);
static void n_setpass(struct Luser *, int, char **);
static void n_noexpire(struct Luser *, int, char **);
static void n_clearnoexp(struct Luser *, int, char **);

#ifdef LINKED_NICKNAMES
static void n_showlink(struct Luser *, int, char **);
static void n_droplink(struct Luser *, int, char **);
#endif /* LINKED_NICKNAMES */

static void n_collide(struct Luser *, int, char **);
static void n_flag(struct Luser *, int, char **);

#endif /* EMPOWERADMINS */

/* adding NEVEROP support / CoolCold / */
static void n_set_neverop(struct Luser *, struct NickInfo *, int, char **);

/* adding NOLINK and NOADD support */
static void n_set_nolink(struct Luser *, struct NickInfo *, int, char **);
static void n_set_noadd(struct Luser *, struct NickInfo *, int, char **);

/* main NickServ commands */
static struct Command nickcmds[] =
	{
		{ "HELP", n_help, LVL_NONE },
		{ "REGISTER", n_register, LVL_NONE },
		{ "DROP", n_drop, LVL_NONE },
		{ "IDENTIFY", n_identify, LVL_NONE },
		{ "RECOVERPASS", n_recoverpass, LVL_NONE },
		{ "RECOVER", n_recover, LVL_NONE },
		{ "RELEASE", n_release, LVL_NONE },
		{ "GHOST", n_ghost, LVL_NONE },
		{ "ACCESS", n_access, LVL_IDENT },
		{ "SET", n_set, LVL_IDENT },
		{ "LIST", n_list, LVL_IDENT },
		{ "INFO", n_info, LVL_IDENT },

#ifdef LINKED_NICKNAMES
		{ "LINK", n_link, LVL_IDENT },
		{ "UNLINK", n_unlink, LVL_IDENT },
#endif /* LINKED_NICKNAMES */

#ifdef EMPOWERADMINS

		{ "FORBID", n_forbid, LVL_ADMIN },
		{ "UNFORBID", n_unforbid, LVL_ADMIN },
		{ "SETPASS", n_setpass, LVL_ADMIN },
		{ "CHPASS", n_setpass, LVL_ADMIN },
		{ "NOEXPIRE", n_noexpire, LVL_ADMIN },
		{ "CLEARNOEXP", n_clearnoexp, LVL_ADMIN },
#ifdef LINKED_NICKNAMES
		{ "SHOWLINK", n_showlink, LVL_ADMIN },
		{ "DROPLINK", n_droplink, LVL_ADMIN },
#endif /* LINKED_NICKNAMES */
		{ "COLLIDE", n_collide, LVL_ADMIN },
		{ "FLAG", n_flag, LVL_ADMIN },
		{ "FIXTS", n_fixts, LVL_ADMIN },

#endif /* EMPOWERADMINS */

		{ 0, 0, 0 }
	};

/* sub commands of NickServ ACCESS */
static struct Command accesscmds[] =
	{
		{ "ADD", n_access_add, LVL_NONE
		},
		{ "DEL", n_access_del, LVL_NONE },
		{ "LIST", n_access_list, LVL_NONE },

		{ 0, 0, 0 }
	};

/* sub commands of NickServ SET */
static struct Command setcmds[] =
	{
		{ "KILL", n_set_kill, LVL_NONE
		},
		{ "AUTOMASK", n_set_automask, LVL_NONE },
		{ "PRIVATE", n_set_private, LVL_NONE },
		{ "PRIVMSG", n_set_privmsg, LVL_NONE },
		{ "SECURE", n_set_secure, LVL_NONE },
		{ "UNSECURE", n_set_unsecure, LVL_NONE },
		{ "MEMOS", n_set_memos, LVL_NONE },
		{ "NOTIFY", n_set_notify, LVL_NONE },
		{ "SIGNON", n_set_signon, LVL_NONE },
		{ "HIDE", n_set_hide, LVL_NONE },
		{ "PASSWORD", n_set_password, LVL_NONE },
		{ "PHRASE", n_set_phrase, LVL_NONE },
		{ "EMAIL", n_set_email, LVL_NONE },
		{ "URL", n_set_url, LVL_NONE },
		{ "UIN", n_set_uin, LVL_NONE },
		{ "GSM", n_set_gsm, LVL_NONE },
		{ "PHONE", n_set_phone, LVL_NONE },

#ifdef LINKED_NICKNAMES
		{ "MASTER", n_set_master, LVL_NONE },
#endif

		{ "NEVEROP", n_set_neverop, LVL_NONE },
		{ "NOLINK", n_set_nolink, LVL_NONE },
		{ "NOADD", n_set_noadd, LVL_NONE },
		{ 0, 0, 0 }
	};

/*
ns_process()
  Process command coming from 'nick' directed towards n_NickServ
*/

void ns_process(const char *nick, char *command)
{
	int acnt;
	char **arv;
	struct Command *cptr;
	struct Luser *lptr;
	struct NickInfo *nptr;

	if (!command)
		return;

	lptr = FindClient(nick);
	if (lptr == NULL)
		return;

	if (Network->flags & NET_OFF)
	{
		notice(n_NickServ, lptr->nick,
			   "Services are currently \002disabled\002");
		return;
	}

	acnt = SplitBuf(command, &arv);
	if (acnt == 0)
	{
		MyFree(arv);
		return;
	}

	cptr = GetCommand(nickcmds, arv[0]);

	if (!cptr || (cptr == (struct Command *) -1))
	{
		notice(n_NickServ, lptr->nick,
			   "%s command [%s]",
			   (cptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
			   arv[0]);
		MyFree(arv);
		return;
	}

	/*
	 * Check if the command is for admins only - if so,
	 * check if they match an admin O: line, and are
	 * currently registered with OperServ.
	 */
	if ((cptr->level == LVL_ADMIN) && !(IsValidAdmin(lptr)))
	{
		notice(n_NickServ, lptr->nick, "Unknown command [%s]",
			   arv[0]);
		MyFree(arv);
		return;
	}

	/*
	 * Use FindNick() here instead of GetLink() because
	 * nicknames are identified individually, and using GetLink()
	 * would tell us if the master of the link is identified,
	 * not necessarily this specific nickname
	 */

	nptr = FindNick(lptr->nick);

	if (nptr != NULL && (nptr->flags & NS_FORBID))
	{
		notice(n_NickServ, lptr->nick,
			   "Cannot execute commands for forbidden nicknames");
		MyFree(arv);
		return;
	}

	if (cptr->level != LVL_NONE)
	{
		if (nptr == NULL)
		{
			notice(n_NickServ, lptr->nick,
				   "You must register your nick with \002%s\002 first",
				   n_NickServ);
			notice(n_NickServ, lptr->nick,
				   "Type: \002/MSG %s REGISTER <password> [password recovery phrase]\002 to register your nickname",
				   n_NickServ);
			MyFree(arv);
			return;
		}

		if (!(nptr->flags & NS_IDENTIFIED))
		{
			notice(n_NickServ, lptr->nick,
				   "Password identification is required for [\002%s\002]",
				   cptr->cmd);
			notice(n_NickServ, lptr->nick,
				   "Type \002/MSG %s IDENTIFY <password>\002 and retry",
				   n_NickServ);
			MyFree(arv);
			return;
		}
	}

	/* call cptr->func to execute command */
	(*cptr->func)(lptr, acnt, arv);

	MyFree(arv);

	return;
} /* ns_process() */

/*
ns_loaddata()
  Load NickServ database - return 1 if successful, -1 if not, and -2
if the errors are unrecoverable
*/

int
ns_loaddata()

{
	FILE *fp;
	char line[MAXLINE + 1], **av;
	char *keyword;
	int ac, ret = 1, cnt;
	int islink;
	struct NickInfo *nptr = NULL;

	if (!(fp = fopen(NickServDB, "r")))
	{
		/* NickServ data file doesn't exist */
		return -1;
	}

	cnt = 0;
	islink = 0;

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
			if (ac < 2)
			{
				fatal(1, "%s:%d Invalid database format (FATAL)",
					  NickServDB,
					  cnt);
				ret = -2;
				continue;
			}

			/* check if there is no nickname associated with the data */
			if (!nptr)
			{
				fatal(1, "%s:%d No nickname associated with data",
					  NickServDB,
					  cnt);
				if (ret > 0)
					ret = -1;
				continue;
			}

			keyword = av[0] + 2;
			if (!ircncmp(keyword, "PASS", 4))
			{
				if (!nptr->password)
					nptr->password = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple PASS lines (using first)",
						  NickServDB,
						  cnt,
						  nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp(keyword, "PHRASE", 6))
			{
				if (!nptr->phrase)
					nptr->phrase = MyStrdup(av[1] + 1);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple PHRASE lines (using first)",
						  NickServDB,
						  cnt,
						  nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp(keyword, "HOST", 4) && !islink)
			{
				AddHostToNick(av[1], nptr);
			}
			else if (!ircncmp(keyword, "EMAIL", 5))
			{
				if (!nptr->email)
					nptr->email = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple EMAIL lines (using first)",
						  NickServDB,
						  cnt,
						  nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp(keyword, "URL", 3))
			{
				if (!nptr->url)
					nptr->url = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple URL lines (using first)",
						  NickServDB,
						  cnt,
						  nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp(keyword, "GSM", 3))
			{
				if (!nptr->gsm)
					nptr->gsm = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple GSM lines (using first)",
						  NickServDB, cnt, nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp(keyword, "PHONE", 5))
			{
				if (!nptr->phone)
					nptr->phone = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple PHONE lines (using first)",
						  NickServDB, cnt, nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp(keyword, "FORBIDBY", 8))
			{
				if (!nptr->forbidby)
					nptr->forbidby = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple FORBIDBY lines (using first)",
						  NickServDB, cnt, nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp(keyword, "FORBIDREASON", 12))
			{
				if (!nptr->forbidreason)
					nptr->forbidreason = MyStrdup(av[1] + 1);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple FORBIDREASON lines (using first)",
						  NickServDB, cnt, nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp(keyword, "UIN", 3))
			{
				if (!nptr->UIN)
					nptr->UIN = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple UIN lines (using first)",
						  NickServDB, cnt, nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (LastSeenInfo && !ircncmp(keyword, "LASTUH", 6))
			{
				if (!nptr->lastu && !nptr->lasth)
				{
					nptr->lastu = MyStrdup(av[1]);
					nptr->lasth = MyStrdup(av[2]);
				}
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple LASTUH lines (using first)",
						  NickServDB,
						  cnt,
						  nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (LastSeenInfo && !ircncmp(keyword, "LASTQMSG", 8))
			{
				if (!nptr->lastqmsg)
					nptr->lastqmsg = MyStrdup(av[1] + 1);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple LASTQMSG lines (using first)",
						  NickServDB,
						  cnt,
						  nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}

#ifdef LINKED_NICKNAMES

			else if (!ircncmp(keyword, "LINK", 4))
			{
				if (!nptr->master)
				{
					struct NickInfo *master;

					if (!(master = GetLink(av[1])))
					{
						fatal(1, "%s:%d NickServ entry for [%s] has an unregistered master nickname: %s (FATAL)",
							  NickServDB,
							  cnt,
							  nptr->nick,
							  av[1]);
						ret = -2;
					}
					else
					{
						int    goodlink;

						/*
						 * The master nickname is ok - insert nptr into
						 * nptr->master's link list
						 */
						islink = 1;

						goodlink = InsertLink(master, nptr);
						if (goodlink <= 0)
						{
							fatal(1, "%s:%d InsertLink() failed for nickname [%s]: %s (FATAL)",
								  NickServDB,
								  cnt,
								  nptr->nick,
								  (goodlink == -1) ? "Circular Link" : "Exceeded MaxLinks links");
							ret = -2;
						}
						else
						{
							struct NickHost *hptr;

							/* Delete nptr's access list */
							while (nptr->hosts)
							{
								hptr = nptr->hosts->next;
								MyFree(nptr->hosts->hostmask);
								MyFree(nptr->hosts);
								nptr->hosts = hptr;
							}
						}
					}
				}
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple LINK lines (using first)",
						  NickServDB,
						  cnt,
						  nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}

#endif /* LINKED_NICKNAMES */

#ifdef RECORD_RESTART_TS
			else if (!ircncmp(keyword, "TS", 2))
			{
				if (!nptr->nick_ts)
					nptr->nick_ts = atol(av[1]);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple TS lines (using first)",
						  NickServDB, cnt, nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp(keyword, "LASTSERVER", 10))
			{
				if (!nptr->last_server)
					nptr->last_server = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d NickServ entry for [%s] has multiple LASTSERVER lines (using first)",
						  NickServDB, cnt, nptr->nick);
					if (ret > 0)
						ret = -1;
				}
			}
#endif /* RECORD_RESTART_TS */

		} /* if (!ircncmp("->", keyword, 2)) */
		else
		{
			if (nptr)
			{
				if (!nptr->password && !(nptr->flags & NS_FORBID))
				{
					/* the previous nick didn't have a PASS line */
					fatal(1,
						  "%s:%d No password entry for registered nick [%s] (FATAL)",
						  NickServDB,
						  cnt,
						  nptr->nick);
					ret = -2;
				}

				if (!nptr->hosts && !(nptr->flags & NS_FORBID) && !islink)
				{
					/* the previous nick didn't have a HOST line */
					fatal(1, "%s:%d No hostname entry for registered nick [%s]",
						  NickServDB,
						  cnt,
						  nptr->nick);
					if (ret > 0)
						ret = -1;
				}

				/*
				 * we've come to a new nick entry, so add the last nick to
				 * the list before proceeding
				 */
				/*
				 * Bug. When the nick is in the list already must have the
				 * NS_DELETE flag. There are two workarrounds
				 * possible. The first one: add the newly charged user
				 * regardles if is in the current nicklist. The old nick
				 * will be deleted in ReloadData(). The other: to reset
				 * the NS_DELETE of nptr if the user is already in the nick
				 * list. I rather preffere loading all the database and
				 * throwing away all the current users (that's the meaning
				 * of un RELOAD!) Low memory servers should use the second
				 * method. Heh, it's up to you kre.
				 * -ags
				 */
				/* if (!FindNick(nptr->nick)) */
				AddNick(nptr);

				if (islink)
					islink = 0;
			}

			/*
			 * make sure there are enough args on the line
			 * <nick> <flags> <time created> <last seen>
			 */
			if (ac < 4)
			{
				fatal(1, "%s:%d Invalid database format (FATAL)",
					  NickServDB,
					  cnt);
				ret = -2;
				nptr = NULL;
				continue;
			}
#ifdef STRICT_DATA_CHECK
			/* Check if there already exists that nickname in list. This will
			 * give some overhead, but this will make sure no nicknames are
			 * twice or more times in db. */
			if (FindNick(av[0]))
			{
				fatal(1, "%s:%d NickServ entry [%s] is already in nick list",
					  NickServDB, cnt, av[0]);
				nptr = NULL;
				ret = -1;
			}
			else
			{
#endif
				nptr = MakeNick();
				nptr->nick = MyStrdup(av[0]);
				nptr->flags = atol(av[1]);
				nptr->created = atol(av[2]);
				nptr->lastseen = atol(av[3]);
				nptr->flags &= ~(NS_IDENTIFIED | NS_COLLIDE | NS_NUMERIC | NS_DELETE);
#ifdef STRICT_DATA_CHECK

			}
#endif

		}
		MyFree(av);
	} /* while */

	/*
	 * This is needed because the above loop will only add the nicks
	 * up to, but not including, the last nick in the database, so
	 * we need to add it now - Also, if there is only 1 nick entry
	 * it will get added now
	 */
	if (nptr)
	{
		if ((nptr->password) || (nptr->flags & NS_FORBID))
		{
			if (!nptr->hosts && !(nptr->flags & NS_FORBID) && !islink)
			{
				fatal(1, "%s:%d No hostname entry for registered nick [%s]",
					  NickServDB,
					  cnt,
					  nptr->nick);
				if (ret > 0)
					ret = -1;
			}

			/*
			 * nptr has a password, so it can be added to the table
			 */
			AddNick(nptr);
		}
		else
		{
			if (!nptr->password && !(nptr->flags & NS_FORBID))
			{
				fatal(1, "%s:%d No password entry for registered nick [%s] (FATAL)",
					  NickServDB,
					  cnt,
					  nptr->nick);
				ret = -2;
			}
		}
	} /* if (nptr) */

	fclose(fp);

	return ret;
} /* ns_loaddata */

/*
AddHostToNick()
  Add 'host' to list of hosts for 'nickptr'
*/

static void
AddHostToNick(char *host, struct NickInfo *nickptr)

{
	struct NickHost *hptr;

	if (!nickptr || !host)
		return;

	hptr = (struct NickHost *) MyMalloc(sizeof(struct NickHost));
	hptr->hostmask = MyStrdup(host);
	hptr->next = nickptr->hosts;
	nickptr->hosts = hptr;
} /* AddHostToNick() */

#ifdef CHANNELSERVICES

/*
AddFounderChannelToNick()
 Each nick structure contains a list of channels for which they
are registered as a founder (ie: they created the channel). Add
such a channel to 'nptr's list
*/

void
AddFounderChannelToNick(struct NickInfo **nptr, struct ChanInfo *cptr)

{
	struct aChannelPtr *tmp;

	if (cptr == NULL)
		return;

	/* check for duplicates */
	for (tmp = (*nptr)->FounderChannels; tmp != NULL; tmp = tmp->next)
	{
		if (tmp->cptr == cptr)
			return;
	}

	tmp = MyMalloc(sizeof(struct aChannelPtr));
	memset(tmp, 0, sizeof(struct aChannelPtr));

	tmp->cptr = cptr;
	tmp->next = (*nptr)->FounderChannels;
	(*nptr)->FounderChannels = tmp;

	++(*nptr)->fccnt;
} /* AddFounderChannelToNick() */

/*
 * RemoveFounderChannelFromNick()
 *
 * Remove 'cptr' from 'nptr's founder channel list
*/

void RemoveFounderChannelFromNick(struct NickInfo **nptr, struct ChanInfo
								  *cptr)
{
	struct aChannelPtr *tmp = NULL;
	struct aChannelPtr *prev = NULL;

	/* Iterate list of nick's founder channels */
	for (tmp = (*nptr)->FounderChannels; tmp != NULL; )
	{
		/* We have a match! */
		if (tmp->cptr == cptr)
		{
			/* Last time we didn't have a match, so we have a ptr before this
			 * one in case of list relinking */
			if (prev != NULL)
			{
				prev->next = tmp->next;
				MyFree(tmp);
				tmp = prev;
			}
			else
			{
				(*nptr)->FounderChannels = tmp->next;
				MyFree(tmp);
			}

			/* KrisDuv's fix for decreasing number of registered channels */
			--(*nptr)->fccnt;

			/* We can break since there should always be only 1 match */
			break;
		}

		prev = tmp;
		tmp = tmp->next;
	}
} /* RemoveFounderChannelFromNick() */

/*
AddAccessChannel()
 Add an access channel entry for nptr. Return new entry
*/

struct AccessChannel *
			AddAccessChannel(struct NickInfo *nptr, struct ChanInfo *chanptr,
							 struct ChanAccess *accessptr)

{
	struct AccessChannel *acptr;

	for (acptr = nptr->AccessChannels; acptr != NULL; acptr = acptr->next)
	{
		if (acptr->cptr == chanptr)
		{
			if (acptr->accessptr->level < accessptr->level)
				acptr->accessptr->level = accessptr->level;

			return NULL;
		}
	}

	acptr = MyMalloc(sizeof(struct AccessChannel));
	acptr->cptr = chanptr;
	acptr->accessptr = accessptr;

	acptr->prev = NULL;
	acptr->next = nptr->AccessChannels;
	if (acptr->next)
		acptr->next->prev = acptr;
	nptr->AccessChannels = acptr;

	return (acptr);
} /* AddAccessChannel() */

/*
DeleteAccessChannel()
 Delete access channel entry for nptr
*/

void
DeleteAccessChannel(struct NickInfo *nptr, struct AccessChannel *acptr)

{
	assert(nptr && acptr);

	if (acptr->next)
		acptr->next->prev = acptr->prev;
	if (acptr->prev)
		acptr->prev->next = acptr->next;
	else
		nptr->AccessChannels = acptr->next;

	MyFree(acptr);
} /* DeleteAccessChannel() */

#endif /* CHANNELSERVICES */

/*
MakeNick()
 Allocate memory for a NickInfo structure and initialize
its fields
*/

static struct NickInfo *
			MakeNick()

{
	struct NickInfo *nptr;

	nptr = (struct NickInfo *) MyMalloc(sizeof(struct NickInfo));

	memset(nptr, 0, sizeof(struct NickInfo));

	return (nptr);
} /* MakeNick() */

/*
AddNick()
  Add 'nickptr' to nickname table
*/

static void
AddNick(struct NickInfo *nickptr)

{
	unsigned int hashv;

	/*
	 * all the fields of nickptr were filled in already, so just 
	 * insert it in the list
	 */
	hashv = NSHashNick(nickptr->nick);

	nickptr->prev = NULL;
	nickptr->next = nicklist[hashv];
	if (nickptr->next)
		nickptr->next->prev = nickptr;
	nicklist[hashv] = nickptr;

	++Network->TotalNicks;
} /* AddNick() */

/*
DeleteNick()
 Delete structure 'nickptr' - assume calling function handles
list manipulation - setting prev/next variables etc.
*/

void
DeleteNick(struct NickInfo *nickptr)

{
	struct NickHost *htmp = NULL;
	unsigned int hashv = 0;

#ifdef CHANNELSERVICES

	struct ChanInfo *cptr = NULL;
	struct aChannelPtr *ftmp = NULL;
	struct aChannelPtr *ntmp = NULL;
	struct AccessChannel *atmp = NULL;
#endif /* CHANNELSERVICES */

#ifdef MEMOSERVICES

	struct MemoInfo *mi = NULL;
#endif /* MEMOSERVICES */

	if (nickptr == NULL)
		return;

	/* There is little probability for this to happen, but remove the
	 * pseudonick if nick is dropped before the release */
	if (nickptr->flags & NS_RELEASE)
		release(nickptr->nick);

#ifdef LINKED_NICKNAMES

	DeleteLink(nickptr, 0);
#endif /* LINKED_NICKNAMES */

#ifdef MEMOSERVICES
	/* check if the nick had any memos - if so delete them */
	if ((mi = FindMemoList(nickptr->nick)) != NULL)
		DeleteMemoList(mi);
#endif

	hashv = NSHashNick(nickptr->nick);

#ifdef CHANNELSERVICES

	while ((ntmp = nickptr->FounderChannels) != NULL)
	{
		cptr = ntmp->cptr;
		ftmp = ntmp->next;

		/* this way we won't use RemoveFounderChannelFromNick() */
		MyFree(ntmp);

		/* If the channel has a successor, promote them to founder, otherwise
		 * delete the channel */
		if (cptr->successor)
		{
			PromoteSuccessor(cptr);
			--nickptr->fccnt;
		}
		else
		{
			/* Fix by KrisDuv - make ChanServ part if on channel */
			struct Channel *chptr;
			chptr = FindChannel(cptr->name);
			if (IsChannelMember(chptr, Me.csptr))
				cs_part(chptr);

			putlog(LOG2, "%s: Deleting [%s] caused deletion of channel [%s]",
				   n_NickServ, nickptr->nick, cptr->name);
			SendUmode(OPERUMODE_S,
					"%s: Deleting [%s] caused deletion of channel [%s]",
					n_NickServ, nickptr->nick, cptr->name);

			/* And delete channel finally */
			MyFree(cptr->founder);
			cptr->flags |= CS_DELETE;
			/* DeleteChan(cptr); */
		}

		nickptr->FounderChannels = ftmp;
	}

	/*
	 * Before deleting nickptr->AccessChannels, we first want
	 * to parse the list and delete the ChanAccess structures
	 * which point to nickptr - otherwise another person could
	 * register the same nickname and gain instant access to
	 * the channels the old nick had access on.
	 */
	for (atmp = nickptr->AccessChannels; atmp != NULL; atmp = atmp->next)
		DeleteAccess(atmp->cptr, atmp->accessptr);

	while (nickptr->AccessChannels != NULL)
	{
		atmp = nickptr->AccessChannels->next;
		MyFree(nickptr->AccessChannels);
		nickptr->AccessChannels = atmp;
	}
#endif /* CHANNELSERVICES */

	MyFree(nickptr->nick);
	MyFree(nickptr->password);
	MyFree(nickptr->phrase);
	MyFree(nickptr->last_server);

	while (nickptr->hosts != NULL)
	{
		htmp = nickptr->hosts->next;
		MyFree(nickptr->hosts->hostmask);
		MyFree(nickptr->hosts);
		nickptr->hosts = htmp;
	}

	MyFree(nickptr->email);
	MyFree(nickptr->url);
	MyFree(nickptr->gsm);
	MyFree(nickptr->phone);
	MyFree(nickptr->UIN);

	MyFree(nickptr->forbidby);
	MyFree(nickptr->forbidreason);

	MyFree(nickptr->lastu);
	MyFree(nickptr->lasth);
	MyFree(nickptr->lastqmsg);

	if (nickptr->next)
		nickptr->next->prev = nickptr->prev;
	if (nickptr->prev)
		nickptr->prev->next = nickptr->next;
	else
		nicklist[hashv] = nickptr->next;

	MyFree(nickptr);

	--Network->TotalNicks;
} /* DeleteNick() */

/*
HasFlag()
 Determine whether 'nick' has the flag 'flag'
*/

int
HasFlag(char *nick, int flag)

{
	struct NickInfo *master;

	if ((master = GetLink(nick)))
		return (master->flags & flag);

	return (0);
} /* HasFlag() */

/*
ChangePass
  Change password for nptr->nick to 'newpass'
*/

static int
ChangePass(struct NickInfo *nptr, char *newpass)

{
#ifdef CRYPT_PASSWORDS
	char *encr;
#endif

	if (!nptr || !newpass)
		return 0;

	MyFree(nptr->password);

#ifdef CRYPT_PASSWORDS

	/* encrypt it */
	encr = hybcrypt(newpass, NULL);
	assert(encr != 0);

	nptr->password = MyStrdup(encr);

#else

	/* just use plaintext */
	nptr->password = MyStrdup(newpass);

#endif

	return 1;
} /* ChangePass() */

/*
CheckNick()
  If 'nickname' 's host is recognized, return 1; if 'nickname' is
not registered, or does not match a known host, return 0 and
prepare to collide 'nickname'
*/

int
CheckNick(char *nickname)

{
	struct Luser *lptr;
	struct NickInfo *nptr, *realptr;
	int knownhost;

	/* Doh. Wrong usage of names - generally we use nptr for linked
	 * nickname and realptr for master nicknames. Will fix this later on.
	 * -kre */
	realptr = FindNick(nickname);
	nptr = GetMaster(realptr);

	if (!realptr || !nptr)
		return 0; /* nickname is not registered */

	/*
	 * If RECORD_RESTART_TS is defined, the user could possibly be
	 * identified already, if we are rejoining from a netsplit;
	 * if that is the case, don't bother checking hostnames etc
	 */
	if (realptr->flags & NS_IDENTIFIED)
		return 0;

	if (!(lptr = FindClient(nickname)))
		return 0; /* nickname doesn't exist */

	/*
	 * Check if the nick is forbidden
	 */
	if (realptr->flags & NS_FORBID)
	{
		notice(n_NickServ, lptr->nick,
			"This nickname may not be used.  Please choose another.");
		notice(n_NickServ, lptr->nick,
			"If you do not change within one minute, you will be disconnected");
		realptr->flags |= NS_COLLIDE;
		realptr->collide_ts = current_ts + 60;
		return 0;
	}

	/*
	 * We don't need to call OnAccessList() if the nickname
	 * is secure - waste of cpu time
	 */
	if (nptr->flags & NS_SECURE)
		knownhost = 0;
	else
		knownhost = OnAccessList(lptr->username, lptr->hostname, nptr);

	/*
	 * If knownhost is 0, then we know either the nickname is
	 * secure, or their hostname is not on their access list -
	 * in either case, give them a warning
	 */
	if (!knownhost)
	{
		if (NicknameWarn || (nptr->flags & NS_PROTECTED))
		{
			/*
			 * If NicknameWarn is enabled, or if their nickname
			 * is kill protected, give them a warning telling
			 * them to identify
			 */
			notice(n_NickServ, lptr->nick, ERR_NOT_YOUR_NICK);
			notice(n_NickServ, lptr->nick, ERR_NEED_IDENTIFY, n_NickServ);
		}

		if (AllowKillProtection)
		{
			if (AllowKillImmed && (nptr->flags & NS_KILLIMMED))
			{
				if (nptr->flags & NS_SECURE)
				{
					/*
					 * If the nickname is secure, we cannot kill them
					 * immediately, because they will be killed every time
					 * they logon, whether they come from a known host or not.
					 * Give them one minute to identify.
					 */
#if defined FORCE_NICK_CHANGE || defined SVSNICK || defined FORCENICK
					notice(n_NickServ, lptr->nick, ERR_MUST_CHANGE2);
#else

					notice(n_NickServ, lptr->nick, ERR_MUST_CHANGE);
#endif

					realptr->flags |= NS_COLLIDE;
					realptr->collide_ts = current_ts + 60;
				}
				else
				{
					/*
					  * The nickname has kill immediately set, so collide
					  * the nickname right now
					  */
					putlog(LOG1,
						   "%s: immediately killing %s!%s@%s (Nickname Enforcement)",
						   n_NickServ, lptr->nick, lptr->username, lptr->hostname);

					SendUmode(OPERUMODE_S,
							  "%s: immediately killing %s!%s@%s (Nickname Enforcement)",
							  n_NickServ, lptr->nick, lptr->username, lptr->hostname);

					collide(lptr->nick, 1);
					realptr->collide_ts = 0;
				}
			}
			else if (nptr->flags & NS_PROTECTED)
			{
				/*
				 * If the nick is kill protected, give a 1 minute timeout
				 * for them to change
				 */
#if defined FORCE_NICK_CHANGE || defined SVSNICK || defined FORCENICK
				notice(n_NickServ, lptr->nick, ERR_MUST_CHANGE2);
#else

				notice(n_NickServ, lptr->nick, ERR_MUST_CHANGE);
#endif

				realptr->flags |= NS_COLLIDE;
				realptr->collide_ts = current_ts + 60;
			}
		}
		return 0;
	} /* if (!knownhost) */

	if ((knownhost) && (nptr->flags & NS_UNSECURE))
	{	

		MyFree(realptr->lastu);
		realptr->lastu = MyStrdup(lptr->username);
		MyFree(realptr->lasth);
		realptr->lasth = MyStrdup(lptr->hostname);

		/*
		 * They're from a known host, and have UNSECURE set -
		 * mark them as identified
		 */
		realptr->flags |= NS_IDENTIFIED;
	}
	return 1;
} /* CheckNick() */

/*
CheckOper()
  Check if operator 'nickname' has set the OPER flag for their
nickname - we know 'nickname' is already an operator
*/

void
CheckOper(struct Luser *lptr)

{
	struct NickInfo *nptr, *realptr;

	if (!lptr)
		return;

/* -- not needed actually
	if (!IsOper(lptr))
		return;
*/

	if (!(realptr = FindNick(lptr->nick)))
		return;

	if (!(nptr = GetMaster(realptr)))
		return;

	if (IsValidAdmin(lptr) &&
		(realptr->flags & NS_IDENTIFIED) &&
		!(nptr->flags & NS_NOEXPIRE))
	{
		notice(n_NickServ, lptr->nick,
			   "You have not set the NoExpire nickname flag for your nickname");
		notice(n_NickServ, lptr->nick,
			   "Please type \002/MSG %s NOEXPIRE %s ON\002 so your nickname does not expire",
			   n_NickServ, lptr->nick);
	}
} /* CheckOper() */

/*
ExpireNicknames()
 Go through nickname list and delete any nicknames that
have expired.
*/

void
ExpireNicknames(time_t unixtime)

{
	int ii;
	struct NickInfo *nptr, *next;

	for (ii = 0; ii < NICKLIST_MAX; ++ii)
	{
		for (nptr = nicklist[ii]; nptr; nptr = next)
		{
			next = nptr->next;

			if (NickNameExpire)
			{
				long exptime;
				struct NickInfo *mptr;
				
				/* Smart expire? */
				if (NickNameExpireAddTime && NickNameExpireAddPeriod)
					exptime = NickNameExpire + NickNameExpireAddTime * ((unixtime - nptr->created) / NickNameExpireAddPeriod);
				else
					exptime = NickNameExpire;

				/* If the master has NoExpire, slave nicks won't expire, too */
				mptr = GetMaster(nptr);
				if ((!(nptr->flags & (NS_FORBID | NS_NOEXPIRE | NS_IDENTIFIED))) && (!(mptr->flags & NS_NOEXPIRE))
					&& ((unixtime - nptr->lastseen) >= exptime))
				{
					putlog(LOG2,
						   "%s: Expired nickname [%s]",
						   n_NickServ,
						   nptr->nick);

					DeleteNick(nptr);
				}
			} /* if (NickNameExpire) */
		} /* for (nptr = nicklist[ii]; nptr; nptr = next) */
	} /* for (ii = 0; ii < NICKLIST_MAX; ++ii) */
} /* ExpireNicknames() */

/*
FindNick()
  Return a pointer to registered nick 'nickname'
*/

struct NickInfo *FindNick(char *nickname)
{
	struct NickInfo *nptr;
	unsigned int hashv;

	if (!nickname)
		return (NULL);

	hashv = NSHashNick(nickname);
	for (nptr = nicklist[hashv]; nptr; nptr = nptr->next)
		if (!irccmp(nptr->nick, nickname))
			return (nptr);

	return (NULL);
} /* FindNick() */

/*
GetLink()
 Similar to FindNick(), except return a pointer to nickname's
master if nickname is in a nick link
*/

struct NickInfo *
			GetLink(char *nickname)

{
	struct NickInfo *nptr;

	if (!(nptr = FindNick(nickname)))
		return (NULL);

#ifdef LINKED_NICKNAMES

	if (nptr->master)
		return (nptr->master);
#endif /* LINKED_NICKNAMES */

	return (nptr);
} /* GetLink() */

/*
OnAccessList()
  Return 1 if hostmask is on the list of recognized hosts
for registered nick 'nptr'; 0 if not
*/

static int
OnAccessList(char *username, char *hostname, struct NickInfo *nptr)

{
	struct NickHost *hptr;
	char hostmask[UHOSTLEN + 2];

	if (!username || !hostname || !nptr)
		return 0;

	ircsprintf(hostmask, "%s@%s", username, hostname);

	for (hptr = nptr->hosts; hptr; hptr = hptr->next)
		if (match(hptr->hostmask, hostmask))
			return 1;

	return 0;
} /* OnAccessList() */

/*
collide()
  Send a nick collide for 'nick'
*/

void
collide(char *nick, int dopseudo)

{
	struct Luser *lptr = NULL;
	struct NickInfo *nptr = NULL;

#if defined SVSNICK || defined FORCENICK
	char newnick[NICKLEN + 1];
	long nicknum;
	int base;
	int i, j, cnt = 0;
    struct Luser *fptr = NULL;
#endif

	char **av;
	char sendstr[MAXLINE + 1];

	if (!SafeConnect)
		return;

	if ((lptr = FindClient(nick)) == NULL)
		return;

	if ((nptr = FindNick(nick)) == NULL)
		return;

	/* Do nothing if it's a pseudo nick */
	if ((nptr != NULL) && lptr->server == Me.sptr)
	{
		nptr->flags &= ~(NS_COLLIDE | NS_NUMERIC);
		return;
	}

#if defined SVSNICK || defined FORCENICK
	if (!(lptr->flags & UMODE_NOFORCENICK))
	{
		lptr->flags |= UMODE_NOFORCENICK;
		if (nptr != NULL)
		{
			nptr->flags |= NS_COLLIDE;
			nptr->collide_ts = current_ts + 30;
		}

    /* make a random nickname, give it five tries if already in use */
	  do {

		nicknum = random();
	/* 
	 * calculate how many chars do we need to pad
	 */
#if defined SVSNICK
#if defined SVSNICK_LEN

		base = SVSNICK_LEN;
#else

		base = NICKLEN - strlen(SVSNICK_PREFIX);
#endif
#endif /* SVSNICK */

#if defined FORCENICK
#if defined FORCENICK_LEN

		base = FORCENICK_LEN;
#else

		base = NICKLEN - strlen(FORCENICK_PREFIX);
#endif
#endif /* FORCENICK */

		/* do the padding */
		j = 1;
		for (i = 1; i <= base; ++i)
			j *= 10;
		nicknum %= j;

#ifdef SVSNICK
		ircsprintf(newnick, "%s%ld", SVSNICK_PREFIX, nicknum);
#else /* defined FORCENICK */
		ircsprintf(newnick, "%s%ld", FORCENICK_PREFIX, nicknum);
#endif /* SVSNICK */

        ++cnt;
	  } while ((fptr = FindClient(newnick)) != NULL && (cnt < 5));

      /* OK, we tried... if the random nickname is in use, KILL right away */
        if (fptr) goto killnick;

#ifdef SVSNICK
		toserv(":%s SVSNICK %s %s\r\n", Me.name, lptr->nick, newnick);
#else
		toserv(":%s FORCENICK %s %s\r\n", Me.name, lptr->nick, newnick);
#endif

		/* When using SVSNICK/FORCENICK, we shouldn't delete the existing
		 * real client and allocate a new structure for the pseudo nick,
		 * we'll do it when the server reply with NICK, otherwise we would
		 * cut the bough on which we are sitting. */
		if (dopseudo)
			nptr->flags |= NS_RELEASE;

		return;
	}
#endif /* defined SVSNICK || defined FORCENICK */

killnick:

	/* Sending a server kill will be quieter than an oper
	 * kill since most clients are -k */
	toserv("KILL %s :%s!%s (Nickname Enforcement)\r\n",
		   lptr->nick, Me.name, n_NickServ);

	/* don't add pseudo nickname after KILL */
	/* delete the client and drop the flags */
	if (!dopseudo)
	{
		nptr->flags &= ~(NS_COLLIDE | NS_NUMERIC);
		DeleteClient(lptr);
		return;
	}

	/* normal ghosted nickname, automatically after not identing */
#ifdef DANCER

	ircsprintf(sendstr, "NICK %s 1 1 +i %s %s %s %lu :%s\r\n", lptr->nick,
			   "enforced", Me.name, Me.name, 0xffffffffUL,
			   "Nickname Enforcement");

#else

	ircsprintf(sendstr, "NICK %s 1 %ld +i %s %s %s :%s\r\n",
			   lptr->nick, (long) (lptr->nick_ts - 1), "enforced", Me.name,
			   Me.name, "Nickname Enforcement");

#endif /* DANCER */
	toserv("%s\r\n",sendstr);

	/* erase the old user */
	DeleteClient(lptr);

	/* add the new one */
	SplitBuf(sendstr, &av);
	AddClient(av);
	MyFree(av);

	/* Unfortunately, this makes no guarantees that FORCENICK succeeded.
	 * Please define FALLBACK_TO_KILL if you want to make sure that nick
	 * gets collided for sure */
	/* remove the collide timer, but put a release timer so the pseudo
	 * nick gets removed after NSReleaseTimeout */
	nptr->flags &= ~(NS_COLLIDE | NS_NUMERIC);
	nptr->flags |= NS_RELEASE;
} /* collide() */

/*
release()
  Release the enforced nickname 'nickname'
*/

void
release(char *nickname)

{
	struct Luser *lptr;
	struct NickInfo *nptr;

	if (!(lptr = FindClient(nickname)) || !(nptr = FindNick(nickname)))
		return;

	if ((lptr->server == NULL) || (lptr->server != Me.sptr))
	{
		/* drop the release flag to avoid coming here again */
		nptr->flags &= ~NS_RELEASE;
		return;
	}

	toserv(":%s QUIT :Released\r\n", lptr->nick);
	DeleteClient(lptr);

	nptr->flags &= ~NS_RELEASE;
} /* release() */

/*
CollisionCheck()
 Called from DoTimer() to nick collide anyone who hasn't
identified by the TS given by nptr->collide_ts, or release
nicknames that have passed the release timeout.
*/

void
CollisionCheck(time_t unixtime)

{
	int ii;
	struct NickInfo *nptr;

	for (ii = 0; ii < NICKLIST_MAX; ii++)
	{
		for (nptr = nicklist[ii]; nptr; nptr = nptr->next)
		{
			if (nptr->flags & NS_COLLIDE)
			{
				struct Luser *lptr;

				if ((lptr = FindClient(nptr->nick)))
				{
					if (unixtime >= nptr->collide_ts)
					{
						/*
						 * The current time is 60+ seconds past the
						 * last time they changed their nickname or
						 * connected to the network, and they have not
						 * yet identified - kill them
						 *
						 * Try first sending him 432, if not sucessful then
						 * kill him -harly
						 */
#if defined FORCE_NICK_CHANGE && !defined SVSNICK && !defined FORCENICK
						if (!(nptr->flags & NS_NUMERIC))
						{
							putlog(LOG1,
								   "%s: forcing nick change for %s!%s@%s",
								   n_NickServ, lptr->nick, lptr->username,
								   lptr->hostname);
							SendUmode(OPERUMODE_S,
									  "%s: forcing nick change for %s!%s@%s",
									  n_NickServ, lptr->nick, lptr->username,
									  lptr->hostname);
							toserv(":%s 432 %s %s :Erroneus Nickname\r\n",
								   Me.name, lptr->server, lptr->nick);
							nptr->flags |= NS_NUMERIC;
							nptr->collide_ts = current_ts + 30;
							continue;
						}
#endif

						putlog(LOG1,
							   "%s: killing %s!%s@%s (Nickname Enforcement)",
							   n_NickServ, lptr->nick, lptr->username, lptr->hostname);

						SendUmode(OPERUMODE_S,
								  "%s: killing %s!%s@%s (Nickname Enforcement)",
								  n_NickServ, lptr->nick, lptr->username, lptr->hostname);

						/*
						 * kill the nick and replace with a pseudo nick
						 */
#if defined(SVSNICK) || defined(FORCENICK)
						/* a user joined the network and the nick is registered:
						   collide and do pseudo */
			 if (!(lptr->flags & UMODE_NOFORCENICK))
                           collide(lptr->nick, 1);
                        else
                           /* if we are here, then, for some reason, SVSNICK didn't succeed:
                              this could be after not supplying the password or after RECOVER 
                              _or_ after GHOST */
                           if (nptr->flags & NS_RELEASE)
                             collide(lptr->nick, 1);
                           else collide (lptr->nick, 0);
#else
						collide(lptr->nick, 1);
#endif /* SVSNICK || FORCENICK */
						nptr->collide_ts = 0;

					}
				}
				else
				{
					/*
					 * User must have changed their nick or QUIT -
					 * remove the collide
					 */
					nptr->flags &= ~(NS_COLLIDE | NS_RELEASE | NS_NUMERIC);
					nptr->collide_ts = 0;
				}
			} /* if (nptr->flags & NS_COLLIDE) */
			else if (NSReleaseTimeout && (nptr->flags & NS_RELEASE))
			{
				struct Luser *lptr;

				if ((lptr = FindClient(nptr->nick)))
				{
					/* since we lose 1 minute, let's be accurate and add
					 * it now */
					if ((unixtime - (lptr->nick_ts + 60)) >=
							NSReleaseTimeout)
					{
						putlog(LOG1,
							   "%s: Releasing enforcement pseudo-nick [%s]",
							   n_NickServ, lptr->nick);

						SendUmode(OPERUMODE_S,
								  "%s: Releasing enforcement pseudo-nick [%s]",
								  n_NickServ, lptr->nick);

						/* release the nickname */
						release(lptr->nick);
					}
				}
				else
				{
					/*
					 * Something got messed up, kill the release flag
					 */
					nptr->flags &= ~NS_RELEASE;
				}
			}
		} /* for (nptr = nicklist[ii]; nptr; nptr = nptr->next) */
	} /* for (ii = 0; ii < NICKLIST_MAX; ii++) */
} /* CollisionCheck() */

#ifdef LINKED_NICKNAMES

/*
 * InsertLink()
 * Insert the nick 'leaf' into 'hub's linked nickname list.
 * Return: 1 if successful,
 *		   0 for NULL arguments,
 *		  -1 if a circular link is detected,
 *		  -2 if more than MaxLinks links are formed
 */
static int InsertLink(struct NickInfo *hub, struct NickInfo *leaf)
{
	struct NickInfo *master = NULL, /* new master for link list */
										  *leafmaster = NULL, /* previous master for leaf's list */
														*tmp = NULL;
	int lcnt = 0 ; /* link count */

	if (!hub || !leaf)
		return(0);

	/* discover master */
	if (!hub->master)
		master = hub;
	else
		master = hub->master;

	/* check for circular link */
	for (tmp = master; tmp; tmp = tmp->nextlink)
		if (tmp == leaf)
			return(-1);

	/* setup leaf master */
	if (leaf->master)
	{
		leafmaster = leaf->master;
	}
	else
		leafmaster = leaf;

	/* find out number of linked nicknames in a list */
	if (master->numlinks)
		lcnt += master->numlinks;
	else
		++lcnt; /* master is a standalone nickname */

	/* leaf is in linked list, add linked_count_leaf to linked_count_hub */
	if (leafmaster->numlinks)
		lcnt += leafmaster->numlinks;
	else
		++lcnt;

	/* seems there are too many links, so die instantly */
	if (MaxLinks && (lcnt > MaxLinks))
		return (-2);

#ifdef CHANNELSERVICES

	/*
	 * leafmaster should no longer have a FounderChannels list - add all of
	 * leafmaster's channels to master's channels. There's no point in
	 * reallocating - just assign master's pointer to leafmaster's
	 */
	if (leafmaster->FounderChannels)
	{
		struct aChannelPtr *tmpchan, *nextchan;

		for (tmpchan = leafmaster->FounderChannels; tmpchan; tmpchan =
					nextchan)
		{
			MyFree(tmpchan->cptr->founder);

			/* Add this channel to masters founder list -jared */
			AddFounderChannelToNick(&master, tmpchan->cptr);
			tmpchan->cptr->founder = MyStrdup(master->nick);

			nextchan = tmpchan->next;
			RemoveFounderChannelFromNick(&leafmaster, tmpchan->cptr);
		}
	}

	if (leafmaster->AccessChannels)
	{
		struct AccessChannel *acptr;
		struct AccessChannel *tcptr;

		for (acptr = leafmaster->AccessChannels; acptr != NULL; acptr =
					tcptr)
		{
			/* Add this channel to masters channel access list -jared */
			AddAccess(acptr->cptr, NULL, NULL, master,
					  acptr->accessptr->level, acptr->accessptr->created,
					  acptr->accessptr->last_used, acptr->accessptr->added_by);

			tcptr = acptr->next;
			DeleteAccess(acptr->cptr, acptr->accessptr);
			MyFree(acptr);
		}
	}

	master->fccnt = leafmaster->fccnt;
	leafmaster->FounderChannels = NULL;
	leafmaster->AccessChannels = NULL;
	leafmaster->fccnt = 0;

#endif /* CHANNELSERVICES */

	/* setup new master for leaf master */
	leafmaster->numlinks = 0;
	leafmaster->master = master;

	/* setup masters in whole list */
	for (tmp = leafmaster; tmp->nextlink != NULL; tmp = tmp->nextlink)
		tmp->master = master;

	/* do last master, insert hub's list at the end of leaf's list */
	tmp->master = master;
	tmp->nextlink = hub->nextlink;

	/* we've counted this already */
	master->numlinks = lcnt;

	/* and start list at the leaf's master */
	hub->nextlink = leafmaster;

	return(1);
} /* InsertLink() */

/*
 * DeleteLink()
 *	Remove nptr from it's current link list. If copyhosts == 1, copy
 *	nptr's master's access list
 * 
 * Return: 1 if successful
 *		   0 if NULL pointer
 *		  -1 if nptr is not in a link
 *
 * XXX: We have bugs here. Fix them! -kre
 * started rewriting, however very slowly -kre
 */
static int DeleteLink(struct NickInfo *nptr, int copy)
{
	struct NickInfo *tmp = NULL;
	struct NickInfo *master = NULL;
	struct NickHost *hptr = NULL;

	if (nptr == NULL)
		return(0);

	/* nptr is master but there is NO list! */
	if ((nptr->master == NULL) && (nptr->nextlink == NULL))
		return(-1);

	/* let us find structure -before- nptr */
	if (nptr->master != NULL)
	{
		for (tmp = nptr->master; tmp != NULL; tmp = tmp->nextlink)
			if (tmp->nextlink == nptr)
				break;
		/* we've reached the end, and there was no nptr? now that's kinda
		 * strange */
		if (tmp == NULL)
			return(0);
	}

	/* do relink: before nptr to after nptr */
	if (tmp != NULL)
	{
		master = nptr->master;
		tmp->nextlink = nptr->nextlink;

		/* and make a master from nptr */
		nptr->master = nptr->nextlink = NULL;

	}
	else /* nptr->master is NULL indicating this is master nick */
	{
		/* make nptr->nextlink the new master */
		nptr->nextlink->master = NULL;
		nptr->nextlink->numlinks = nptr->numlinks;
		for (tmp = nptr->nextlink->nextlink; tmp != NULL; tmp = tmp->nextlink)
			tmp->master = nptr->nextlink;

		master = nptr->nextlink;

		/* and yes, declare nptr as nickname which is alone */
		nptr->nextlink = NULL;
	}

	/* also set numlinks to 0 (this is a standalone nickname now) */
	nptr->numlinks = 0;

	--master->numlinks;
	if (master->numlinks == 1)
	{
		master->numlinks = 0;
		master->master = NULL;
	}

	if (copy)
	{
		struct AccessChannel *acptr = NULL;

		/* hosts list */
		for (hptr = master->hosts; hptr != NULL; hptr = hptr->next)
			AddHostToNick(hptr->hostmask, nptr);

		/* access list */
		for (acptr = master->AccessChannels; acptr != NULL;
				acptr = acptr->next)
			AddAccess(acptr->cptr, NULL, NULL, nptr, acptr->accessptr->level,
					  acptr->accessptr->created, acptr->accessptr->last_used,
					  acptr->accessptr->added_by);
	}

	return (1);
} /* DeleteLink() */

/*
 * IsLinked()
 * Determine if nick1 and nick2 are in the same link.
 * Return 1 if yes, 0 if not
 */
int IsLinked(struct NickInfo *nick1, struct NickInfo *nick2)
{
	struct NickInfo *tmp1, *tmp2;

	if (!nick1 || !nick2)
		return (0);

	if (!(tmp1 = nick1->master))
		tmp1 = nick1;

	if (!(tmp2 = nick2->master))
		tmp2 = nick2;

	/* if nick1's master is equal to nick2's master they are linked */
	if (tmp1 == tmp2)
		return(1);

	return(0);
} /* IsLinked() */

#endif /* LINKED_NICKNAMES */

/*
 * GetMaster()
 * Return a pointer to nptr's nick link master
 */
struct NickInfo *GetMaster(struct NickInfo *nptr)
{
	if (nptr == NULL)
		return (NULL);

#ifdef LINKED_NICKNAMES

	if (nptr->master)
		return (nptr->master);
#endif

	return (nptr);
} /* GetMaster() */

/* Helper for return-requested-help-item-from-file function */
static void n_help(struct Luser *lptr, int ac, char **av)
{
	if (ac >= 2)
	{
		char  str[MAXLINE + 1];

		if (ac >= 3)
			ircsprintf(str, "%s %s", av[1], av[2]);
		else
		{
			if ((!irccmp(av[1], "ACCESS")) ||
					(!irccmp(av[1], "SET")))
				ircsprintf(str, "%s index", av[1]);
			else
			{
				struct Command *cptr;

				for (cptr = nickcmds; cptr->cmd; cptr++)
					if (!irccmp(av[1], cptr->cmd))
						break;

				if (cptr->cmd)
					if ((cptr->level == LVL_ADMIN) &&
							!(IsValidAdmin(lptr)))
					{
						notice(n_NickServ, lptr->nick,
							   "No help available on \002%s\002",
							   av[1]);
						return;
					}

				ircsprintf(str, "%s", av[1]);
			}
		}

		GiveHelp(n_NickServ, lptr->nick, str, NODCC);
	}
	else
		GiveHelp(n_NickServ, lptr->nick, NULL, NODCC);
} /* n_help() */

/*
n_register()
  Add lptr->nick to nick database with password av[1]
*/

static void
n_register(struct Luser *lptr, int ac, char **av)

{
	char *mask, *prphrase;
	struct NickInfo *nptr;
	time_t currtime;

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick, "Syntax: \002REGISTER\002 <password> [password recovery phrase]");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			   "REGISTER");
		return;
	}

	if ((nptr = FindNick(lptr->nick)))
	{
		notice(n_NickServ, lptr->nick,
			   "The nickname [\002%s\002] is already registered",
			   nptr->nick);
		return;
	}

	currtime = current_ts;

	if (!IsValidAdmin(lptr) &&
			(currtime < (lptr->nickreg_ts + NickRegDelay)))
	{
		notice(n_NickServ, lptr->nick,
			   "Please wait %ld seconds before using REGISTER again",
			   NickRegDelay);
		return;
	}

	nptr = MakeNick();
	nptr->nick = MyStrdup(lptr->nick);

	/* Password recovery phrase for nickname  -bane */
	if (ac < 3)
		prphrase = NULL;
	else
		prphrase = GetString(ac - 2, av + 2);

	/* If prphrase is not NULL and prphrase len is < 8, return. -bane */
	if ((prphrase != NULL) && (strlen(prphrase) < 8))
	{
		notice(n_NickServ, lptr->nick, "Password recovery phrase must be at least 8 characters long, register failed");
		putlog(LOG1, "%s: failed to register with recovery phrase %s!%s@%s",
			   n_NickServ, lptr->nick, lptr->username, lptr->hostname);
		MyFree(prphrase);
		MyFree(nptr->nick);
		MyFree(nptr);
		return;
	}

	if (!ChangePass(nptr, av[1]))
	{
		notice(n_NickServ, lptr->nick,
			   "Register failed - can't set new password");
		putlog(LOG1, "%s: failed to register %s!%s@%s", n_NickServ,
			   lptr->nick, lptr->username, lptr->hostname);
		MyFree(nptr->nick);
		MyFree(nptr);
		return;
	}

	/* Password recovery phrase for nickname  -bane */
	nptr->phrase = prphrase;
	nptr->created = nptr->lastseen = currtime;

	if (LastSeenInfo)
	{
		nptr->lastu = MyStrdup(lptr->username);
		nptr->lasth = MyStrdup(lptr->hostname);
		nptr->lastqmsg = NULL;
	}

	nptr->flags = NS_IDENTIFIED;

	if (NSSetKill)
		nptr->flags |= NS_PROTECTED;
	if (NSSetAutoMask)
		nptr->flags |= NS_AUTOMASK;
	if (NSSetPrivate)
		nptr->flags |= NS_PRIVATE;
	if (NSSetSecure)
		nptr->flags |= NS_SECURE;
	if (NSSetUnSecure)
		nptr->flags |= NS_UNSECURE;
	if (NSSetAllowMemos)
		nptr->flags |= NS_MEMOS;
	if (NSSetMemoSignon)
		nptr->flags |= NS_MEMOSIGNON;
	if (NSSetMemoNotify)
		nptr->flags |= NS_MEMONOTIFY;
	if (NSSetHide)
		nptr->flags |= NS_HIDEALL;
	if (NSSetHideEmail)
		nptr->flags |= NS_HIDEEMAIL;
	if (NSSetHideUrl)
		nptr->flags |= NS_HIDEURL;
	if (NSSetHideQuit)
		nptr->flags |= NS_HIDEQUIT;
	if (NSSetNoLink)
		nptr->flags |= NS_NOLINK;
	if (NSSetNoAdd)
		nptr->flags |= NS_NOADD;

	mask = HostToMask(lptr->username, lptr->hostname);

	AddHostToNick(mask, nptr);

	/* Add nptr to nick table */
	AddNick(nptr);

	lptr->nickreg_ts = currtime;

	notice(n_NickServ, lptr->nick,
		   "Your nickname is now registered under the hostmask [\002%s\002]",
		   mask);
	notice(n_NickServ, lptr->nick,
		   "Your password is [\002%s\002] - Remember this for later use",
		   av[1]);

	if (ac > 2)
		notice(n_NickServ, lptr->nick,
			   "Your password recovery phrase is [\002%s\002] - use it with RECOVERPASS if you ever forget your password",
			   prphrase);

#ifdef DANCER
	/* for ircds that have +e mode -kre */
	toserv(":%s MODE %s +e\r\n", Me.name, lptr->nick);
#endif /* DANCER */

	MyFree(mask);

	RecordCommand("%s: %s!%s@%s REGISTER", n_NickServ, lptr->nick,
				  lptr->username, lptr->hostname);
} /* n_register() */

/*
n_drop()
  Discontinue registered nick 'lptr->nick'
*/

static void
n_drop(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *ni;
	char *dnick;
	struct Userlist *utmp;
	int dropnick = 0;

	if (!(ni = FindNick(lptr->nick)))
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, lptr->nick);
		return;
	}

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
		       "Syntax: \002DROP\002 <password>");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "DROP");
		return;
	}

	/*
	 * Set dnick to lptr->nick instead of ni->nick because
	 * when ni is deleted, dnick will be garbage
	 */
	dnick = lptr->nick;

	/* We now require the password in order to avoid abuse or inattentive
	 * drop, as experience shows. Thus, we also make UNSECURE more
	 * secure... */
	if (pwmatch(ni->password, av[1]))
	{
		/* just a regular user dropping their nick */
		RecordCommand("%s: %s!%s@%s DROP", n_NickServ, lptr->nick,
			  lptr->username, lptr->hostname);

		dropnick = 1;
	}

	if (lptr->flags & L_OSREGISTERED)
		utmp = GetUser(1, lptr->nick, lptr->username, lptr->hostname);
	else
		utmp = GetUser(0, lptr->nick, lptr->username, lptr->hostname);

	if ((dropnick == 0) && IsAdmin(utmp))
	{
#ifdef EMPOWERADMINS
		if (!IsValidAdmin(lptr))
		{
			notice(n_NickServ, lptr->nick, ERR_BAD_PASS);
			notice(n_OperServ, lptr->nick, ERR_NEED_IDENTIFY, n_OperServ);
			RecordCommand("%s: (Unregistered) Administrator %s!%s@%s failed DROP [%s]",
						  n_NickServ, lptr->nick, lptr->username,
						  lptr->hostname, av[1]);
			return;
		}

		if (!(ni = FindNick(av[1])))
		{
			notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, av[1]);
			return;
		}

		dnick = av[1];

		RecordCommand("%s: Administrator %s!%s@%s DROP [%s]", n_NickServ,
					  lptr->nick, lptr->username, lptr->hostname, dnick);

		o_Wallops("DROP from %s!%s@%s for nick [%s]", lptr->nick,
				  lptr->username, lptr->hostname, av[1] );

		dropnick = 1;
#endif

	}

	/* still want to drop, but with no privileges and without correct
	 * password */
	if (dropnick == 0)
	{
		notice(n_NickServ, lptr->nick, ERR_BAD_PASS);
		RecordCommand("%s: %s!%s@%s failed DROP", n_NickServ, lptr->nick,
				lptr->username, lptr->hostname);
		return;
	}

	/* remove the nick from the nicklist table, cause channel drop
	 * flagging, successor promotion, etc. */
	DeleteNick(ni);

#ifdef DANCER
	/* De-identify the user, for ircds that have such mode -kre */
	toserv(":%s MODE %s -e\r\n", Me.name, dnick);
#endif /* DANCER */

	notice(n_NickServ, lptr->nick,
		   "The nickname [\002%s\002] has been dropped", dnick);
} /* n_drop() */

/*
n_identify()
  If successful, lptr->nick is now recognized and can use other
NickServ commands
  If lptr->nick is in a nickname link, the identifying process
works something like this:
  Each individual nick in the link gets their own identified
flag when they identify.  If lptr->nick ever changes his/her nick
to another nickname in the same link, s_nick() will check if
lptr->nick was identified with NickServ.  If so, the new nick
will get auto-identified.  Otherwise the new nickname will have
to identify with NickServ manually. It might be easier and more
efficient to simply set the master of the link as identified
whenever a leaf identifies, but I have chosen not to do this because
a malicious user could wait until a leaf identifies, then change
their nickname to a nickname in the link, and become auto-identified.
Keeping separate identifying flags for each nickname in the link
will prevent this.
*/

static void
n_identify(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *nptr, *realptr;
#ifdef MEMOSERVICES

	struct MemoInfo *mi;
#endif

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002IDENTIFY\002 <password>");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO,
			   n_NickServ,
			   "IDENTIFY");
		return;
	}

	realptr = FindNick(lptr->nick);

	/*
	 * Get master nickname as well for auto-mask purposes
	 */
	nptr = GetMaster(realptr);

	if (!realptr || !nptr)
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED,
			   lptr->nick);
		return;
	}

	if (realptr->flags & NS_IDENTIFIED)
	{
		notice(n_NickServ, lptr->nick,
			   "You have already identified");
		return;
	}

	if (!pwmatch(realptr->password, av[1]))
	{
		notice(n_NickServ, lptr->nick, ERR_BAD_PASS);

		RecordCommand("%s: %s!%s@%s failed IDENTIFY",
					  n_NickServ, lptr->nick, lptr->username, lptr->hostname);

		return;
	}

	RecordCommand("%s: %s!%s@%s IDENTIFY",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname);

	realptr->flags |= NS_IDENTIFIED;
	realptr->flags &= ~(NS_COLLIDE | NS_RELEASE);
	notice(n_NickServ, lptr->nick,
		   "Password accepted - you are now recognized");
#ifdef DANCER

	toserv(":%s MODE %s +e\r\n", Me.name, lptr->nick);
#endif /* DANCER */

	if ((nptr->flags & NS_AUTOMASK) &&
			(!OnAccessList(lptr->username, lptr->hostname, nptr)))
	{
		char *mask = HostToMask(lptr->username, lptr->hostname);

		AddHostToNick(mask, nptr);
		notice(n_NickServ, lptr->nick,
			   "Added hostmask [\002%s\002] to the list of your known hosts",
			   mask);
		MyFree(mask);
	}

	CheckOper(lptr);

	if (LastSeenInfo)
	{
		/*
		 * Update last seen user@host info
		 */
		MyFree(realptr->lastu);
		MyFree(realptr->lasth);
		realptr->lastu = MyStrdup(lptr->username);
		realptr->lasth = MyStrdup(lptr->hostname);
	} /* if (LastSeenInfo) */

	/* I have decided to move here new memo checking code, because it
	 * seems to me more reasonable to have it right after successful
	 * identify, and not every time on signon -kre */
#ifdef MEMOSERVICES
	if (nptr->flags & NS_MEMOSIGNON)
	{
		/* search by master -kre */
		if ((mi = FindMemoList(nptr->nick)))
		{
			if (mi->newmemos)
			{
				notice(n_MemoServ, lptr->nick,
					   "You have \002%d\002 new memo%s",
					   mi->newmemos,
					   (mi->newmemos == 1) ? "" : "s");
				notice(n_MemoServ, lptr->nick,
					   "Type \002/MSG %s LIST\002 to view them",
					   n_MemoServ);
			}
		}
		else
			notice(n_MemoServ, lptr->nick,
				   "You have no new memos");
	}
#endif /* MEMOSERVICES */

	nptr->lastseen = realptr->lastseen = current_ts;

#ifdef RECORD_RESTART_TS
	/* identify only linked, not his master */
	realptr->nick_ts = lptr->nick_ts;
	MyFree(realptr->last_server);
	realptr->last_server = MyStrdup(lptr->server->name);
#endif

} /* n_identify() */

/*
n_recoverpass()
  Change the password for nickname av[1] to av[2] if correct
  assword recovery phrase is entered.  -bane
*/
static void n_recoverpass(struct Luser *lptr, int ac, char **av)
{
	struct NickInfo *nptr;
	char *prphrase;

	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002RECOVERPASS\002 <nick> <newpass> <password recovery phrase>");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			   "RECOVERPASS");
		return;
	}

	if ((nptr = FindNick(av[1])) == NULL)
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, av[1]);
		return;
	}

	/* Get password recovery phrase from input	-bane */
	prphrase = GetString(ac - 3, av + 3);

	/* nick phrase == input phrase */
	if ((nptr->phrase == NULL) || irccmp(nptr->phrase, prphrase))
	{
		notice(n_NickServ, lptr->nick, "Invalid password recovery phrase");
		RecordCommand("%s: %s!%s@%s Invalid RECOVERPASS [%s] ...", n_NickServ,
					  lptr->nick, lptr->username, lptr->hostname, av[1]);

		MyFree(prphrase);
		return;
	}

	if (!ChangePass(nptr, av[2]))
	{
		notice(n_NickServ, lptr->nick, "Password change failed");
		RecordCommand("%s: Failed to change password for [%s] (RECOVERPASS)",
					  n_NickServ, av[1], av[2]);

		MyFree(prphrase);
		return;
	}

	RecordCommand("%s: %s!%s@%s RECOVERPASS [%s] ...", n_NickServ,
				  lptr->nick, lptr->username, lptr->hostname, av[1]);

	/* unidentify if it is identified */
	if (nptr->flags & NS_IDENTIFIED)
	{
		nptr->flags &= ~NS_IDENTIFIED;
		CheckNick(nptr->nick);
	}

	notice(n_NickServ, lptr->nick,
		   "Password for [\002%s\002] has been recovered to [\002%s\002]",
		   av[1], av[2]);

	MyFree(prphrase);
} /* n_recoverpass() */

/*
n_recover()
  Recover nickname av[1] through a collide, and hold it with a
pseudo-nick
*/

static void
n_recover(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *ni;
	int goodcoll;

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002RECOVER <nickname> [password]\002");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO,
			   n_NickServ,
			   "RECOVER");
		return;
	}

	if (!(ni = FindNick(av[1])))
	{
		notice(n_NickServ, lptr->nick,
			   ERR_NOT_REGGED,
			   av[1]);
		return;
	}

	/*
	 * Check if lptr's userhost is on ni's access list - if so check
	 * if ni is SECURE (if it is, lptr->nick MUST supply the correct
	 * password)
	 */
	goodcoll = 0;
	if (OnAccessList(lptr->username, lptr->hostname, ni)
			&& (!(ni->flags & NS_SECURE)))
		goodcoll = 1;
	else
	{
		if (ac >= 3)
			if (pwmatch(ni->password, av[2]))
				goodcoll = 1;
	}

	if (!goodcoll)
	{
		/* Admins may recover any nickname */
		if (IsValidAdmin(lptr))
			goodcoll = 2;
	}

	if (goodcoll)
	{
		struct Luser *lsptr = NULL;

		if (ni->flags & NS_RELEASE)
		{
			notice(n_NickServ, lptr->nick,
				   "The nickname [\002%s\002] has already been recovered",
				   av[1]);
			return;
		}
		if ((lsptr = FindClient(av[1])) == NULL)
		{
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] is not currently online",
				   av[1]);
			return;
		}

		/* Don't allow to collide pseudo nicks, especially jupitered */
		if (lsptr->server == Me.sptr)
		{
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] is a pseudo nickname", av[1]);
			return;
		}

#if defined(SVSNICK) || defined(FORCENICK)
		/* these two lines ensure KILL if SVSNICK/FORCENICK fails */
        	ni->collide_ts = current_ts + 10;
		ni->flags |= NS_COLLIDE;
#endif
		collide(av[1], 1);
		notice(n_NickServ, lptr->nick,
			   "The nickname [\002%s\002] has been recovered",
			   av[1]);
		notice(n_NickServ, lptr->nick,
			   "Type: \002/MSG %s RELEASE %s\002 to release the "
			   "nickname before the timeout",
			   n_NickServ,
			   av[1]);

		RecordCommand("%s: %s%s!%s@%s RECOVER [%s]",
					  n_NickServ,
					  (goodcoll == 2) ? "Administrator " : "",
					  lptr->nick,
					  lptr->username,
					  lptr->hostname,
					  av[1]);
	}
	else
	{
		notice(n_NickServ, lptr->nick,
			   ERR_BAD_PASS);
		RecordCommand("%s: %s!%s@%s failed RECOVER [%s]",
					  n_NickServ,
					  lptr->nick,
					  lptr->username,
					  lptr->hostname,
					  av[1]);
	}
} /* n_recover() */

/*
n_release()
  Release a pseudo nick that is being held after a RECOVER
*/

static void
n_release(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *ni;
	int goodrel;

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002RELEASE <nickname> [password]\002");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO,
			   n_NickServ,
			   "RELEASE");
		return;
	}

	if (!(ni = FindNick(av[1])))
	{
		notice(n_NickServ, lptr->nick,
			   ERR_NOT_REGGED,
			   av[1]);
		return;
	}

	/*
	 * Check if lptr's userhost is on ni's access list - if so check
	 * if ni is SECURE (if it is, lptr->nick MUST supply the correct
	 * password)
	 */
	goodrel = 0;
	if (OnAccessList(lptr->username, lptr->hostname, ni) &&
			(!(ni->flags & NS_SECURE)))
		goodrel = 1;
	else
	{
		if (ac >= 3)
			if (pwmatch(ni->password, av[2]))
				goodrel = 1;
	}

	if (!goodrel)
	{
		/* Admins who are IDENTIFY'd may release any nickname */
		if (IsValidAdmin(lptr))
			goodrel = 2;
	}

	if (goodrel)
	{
		if (!(ni->flags & NS_RELEASE))
		{
			notice(n_NickServ, lptr->nick,
				   "The nickname [\002%s\002] is not being enforced",
				   av[1]);
			return;
		}

		if (!FindClient(av[1]))
		{
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] is not currently online",
				   av[1]);
			return;
		}
#if defined(SVSNICK) || defined(FORCENICK)
		/* this is if SVSNICK/FORCENICK didn't succeed so the nick
		   is waiting for KILL and making of a pseudo nick */
		if (ni->flags & NS_COLLIDE)
        	{
			notice(n_NickServ, lptr->nick,
				   "Still recovering nickname [\002%s\002], wait a minute...",
				   av[1]);
			return;
        	}
#endif
		release(av[1]);
		notice(n_NickServ, lptr->nick,
			   "The nickname [\002%s\002] has been released",
			   av[1]);

		RecordCommand("%s: %s%s!%s@%s RELEASE [%s]",
					  n_NickServ,
					  (goodrel == 2) ? "Administrator " : "",
					  lptr->nick,
					  lptr->username,
					  lptr->hostname,
					  av[1]);
	}
	else
	{
		notice(n_NickServ, lptr->nick,
			   ERR_BAD_PASS);

		RecordCommand("%s: %s!%s@%s failed RELEASE [%s]",
					  n_NickServ,
					  lptr->nick,
					  lptr->username,
					  lptr->hostname,
					  av[1]);
	}
} /* n_release() */

/*
n_ghost()
  Kill nickname av[1] if av[2] matches lptr's password. (for clients
who have ghosted nicknames)
*/

static void
n_ghost(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *ni;
	int goodcoll;

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002GHOST <nickname> [password]\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "GHOST");
		return;
	}

	if (!(ni = FindNick(av[1])))
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, av[1]);
		return;
	}

	/* Don't allow ghosting self */
	if (!irccmp(lptr->nick, ni->nick))
	{
		return;
	}

	/*
	 * Check if lptr's userhost is on ni's access list - if so check
	 * if ni is SECURE (if it is, lptr->nick MUST supply the correct
	 * password)
	 */
	goodcoll = 0;
	if (OnAccessList(lptr->username, lptr->hostname, ni) &&
			(!(ni->flags & NS_SECURE)))
		goodcoll = 1;
	else
	{
		if (ac >= 3)
			if (pwmatch(ni->password, av[2]))
				goodcoll = 1;
	}

	if (!goodcoll)
	{
		/* Admins may kill any nickname */
		if (IsValidAdmin(lptr))
		{
			goodcoll = 1;
			o_Wallops("Administrative ghost from %s for nick [%s]",
					  lptr->nick, av[1]);
		}
	}

	if (goodcoll)
	{
		struct Luser *gptr = NULL;

		if ((gptr = FindClient(av[1])) == NULL)
		{
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] is not currently online",
				   av[1]);
			return;
		}

		/* Don't allow to ghost pseudo nicknames */
		if (gptr->server == Me.sptr)
		{
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] is a pseudo nickname",
				   av[1]);
			return;
		}

        /* Immediately use KILL for ghosts if issued by an admin, or if the target is identified
           so we can assume the command is coming from the nick owner, otherwise behave like RECOVER,
           but without introducing a pseudo nickname */
#if defined(SVSNICK) || defined(FORCENICK)
        if (IsValidAdmin(lptr) || (ni->flags & NS_IDENTIFIED))
        {
#endif
          toserv("KILL %s :%s!%s (%s (%s!%s@%s))\r\n", gptr->nick, Me.name, n_NickServ,
                (IsValidAdmin(lptr) ? "Administrative Ghost" : "Ghost"), lptr->nick, lptr->username, lptr->hostname);

          DeleteClient(gptr);
          ni->flags &= ~(NS_COLLIDE | NS_RELEASE | NS_NUMERIC);

#if defined(SVSNICK) || defined(FORCENICK)
        } else {
          /* if SVSNICK/FORCENICK don't succeed shortly after the command, use KILL */
          	ni->collide_ts = current_ts + 10;
		ni->flags |= NS_COLLIDE;
		collide(av[1], 0);
        }
#endif
		notice(n_NickServ, lptr->nick, "[\002%s\002] has been killed",
			   av[1]);

		RecordCommand("%s: %s!%s@%s GHOST [%s]", n_NickServ, lptr->nick,
					  lptr->username, lptr->hostname, av[1]);
	}
	else
	{
		notice(n_NickServ, lptr->nick, ERR_BAD_PASS);
		RecordCommand("%s: %s!%s@%s failed GHOST [%s]", n_NickServ,
					  lptr->nick, lptr->username, lptr->hostname, av[1]);
	}
} /* n_ghost() */

/*
n_access()
  Modify the list of known hostmasks for lptr->nick
*/

static void
n_access(struct Luser *lptr, int ac, char **av)

{
	struct Command *cptr;
	struct NickInfo *target;

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002ACCESS {ADD|DEL|LIST} [mask]\002");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO,
			   n_NickServ,
			   "ACCESS");
		return;
	}

	target = NULL;

#ifdef EMPOWERADMINS_MORE

	/*
	 * Allow administrators to specify a nickname to modify
	 */
	if (IsValidAdmin(lptr))
	{
		/*
		 * First, check if av[1] is a valid command. If not, check if it is
		 * a valid nickname. If av[1] is a valid command, process the
		 * command as though lptr is modifying their own access, which they
		 * most likely are. If av[1] is neither a valid command nor a valid
		 * nickname, give them a "Nick not registered" error.
		 */
		cptr = GetCommand(accesscmds, av[1]);
		if (!cptr || (cptr == (struct Command *) -1))
		{
			target = GetLink(av[1]);
			if (!target)
			{
				notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, av[1]);
				return;
			}

			if (ac >= 3)
				cptr = GetCommand(accesscmds, av[2]);
			else
				cptr = NULL;
		}
	}
	else
		cptr = GetCommand(accesscmds, av[1]);

#else

	cptr = GetCommand(accesscmds, av[1]);

#endif /* EMPOWERADMINS_MORE */

	if (cptr && (cptr != (struct Command *) -1))
	{
		/* call the appropriate function */
		(*cptr->func)(lptr, target, ac, av);
	}
	else
	{
		/* the command doesn't exist */
		notice(n_NickServ, lptr->nick,
			   "%s switch [\002%s\002]",
			   (cptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
			   av[1]);
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "ACCESS");
	}
} /* n_access() */

static void
n_access_add(struct Luser *lptr, struct NickInfo *target, int ac, char **av)

{
	struct NickInfo *nptr;
	char user[USERLEN + 1];
	char *mask, *host, *tmp;

	user[0] = '\0';

	if (target)
	{
		if (ac < 4)
		{
			notice(n_NickServ, lptr->nick,
				   "Syntax: \002ACCESS <nickname> ADD <mask>\002");
			notice(n_NickServ, lptr->nick,
				   ERR_MORE_INFO,
				   n_NickServ,
				   "ACCESS ADD");
			return;
		}

		nptr = target;
		mask = av[3];
	}
	else
	{
		if (ac < 3)
		{
			notice(n_NickServ, lptr->nick, "Syntax: \002ACCESS ADD <mask>\002");
			notice(n_NickServ, lptr->nick,
				   ERR_MORE_INFO,
				   n_NickServ,
				   "ACCESS ADD");
			return;
		}

		nptr = GetLink(lptr->nick);
		mask = av[2];
	}

	if (strlen(mask) > UHOSTLEN )
	{
		notice(n_NickServ, lptr->nick, "Hostmask too long!");
		return;
	}

	if (!nptr)
		return;

	tmp = MyStrdup(mask);

	host = strchr(tmp, '@');
	if (host == NULL )
	{
		notice(n_NickServ, lptr->nick, "Invalid hostmask!");
		MyFree(tmp);
		return;
	}
	if (strlen(mask)-strlen(host) > USERLEN - 1)
	{
		notice(n_NickServ, lptr->nick, "Invalid hostmask! Username too long!");
		MyFree(tmp);
		return;
	}

	if (strlen(host))
	{
		strlcpy(user, mask, strlen(mask) - strlen(host) + 1);
	}

	++host;

	if (OnAccessList(user, host, nptr))
	{
		if (target)
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] matches a hostmask already on the Access List for [%s]",
				   mask,
				   target->nick);
		else
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] matches a hostmask already on your Access List",
				   mask);

		MyFree(tmp);
		return;
	}
	else
	{
		AddHostToNick(mask, nptr);

		if (target)
		{
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] has been added to the Access List for [%s]",
				   mask,
				   target->nick);

			RecordCommand("%s: Administrator %s!%s@%s ACCESS %s ADD [%s]",
						  n_NickServ,
						  lptr->nick,
						  lptr->username,
						  lptr->hostname,
						  target->nick,
						  mask);
		}
		else
		{
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] added to your Access List",
				   mask);

			RecordCommand("%s: %s!%s@%s ACCESS ADD [%s]",
						  n_NickServ,
						  lptr->nick,
						  lptr->username,
						  lptr->hostname,
						  mask);
		}
	}

	MyFree(tmp);
} /* n_access_add() */

static void
n_access_del(struct Luser *lptr, struct NickInfo *target, int ac, char **av)

{
	struct NickInfo *nptr;
	struct NickHost *hptr, *prev;
	int found;
	char *mask;

	if (target)
	{
		if (ac < 4)
		{
			notice(n_NickServ, lptr->nick,
				   "Syntax: \002ACCESS <nickname> DEL <mask>\002");
			notice(n_NickServ, lptr->nick,
				   ERR_MORE_INFO,
				   n_NickServ,
				   "ACCESS DEL");
			return;
		}

		nptr = target;
		mask = av[3];
	}
	else
	{
		if (ac < 3)
		{
			notice(n_NickServ, lptr->nick,
				   "Syntax: \002ACCESS DEL <mask>\002");
			notice(n_NickServ, lptr->nick,
				   ERR_MORE_INFO,
				   n_NickServ,
				   "ACCESS DEL");
			return;
		}

		nptr = GetLink(lptr->nick);
		mask = av[2];
	}

	if (!nptr)
		return;

	found = 0;

	prev = NULL;
	for (hptr = nptr->hosts; hptr; )
	{
		if (match(mask, hptr->hostmask))
		{
			found = 1;
			MyFree(hptr->hostmask);

			if (prev)
			{
				prev->next = hptr->next;
				MyFree(hptr);
				hptr = prev;
			}
			else
			{
				nptr->hosts = hptr->next;
				MyFree(hptr);
				hptr = NULL;
			}

			break;
		}

		prev = hptr;
		hptr = hptr->next;
	}

	if (found)
	{
		if (target)
		{
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] removed from the Access List for [%s]",
				   mask,
				   target->nick);

			RecordCommand("%s: Administrator %s!%s@%s ACCESS %s DEL [%s]",
						  n_NickServ,
						  lptr->nick,
						  lptr->username,
						  lptr->hostname,
						  target->nick,
						  mask);
		}
		else
		{
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] removed from your Access List",
				   mask);

			RecordCommand("%s: %s!%s@%s ACCESS DEL [%s]",
						  n_NickServ,
						  lptr->nick,
						  lptr->username,
						  lptr->hostname,
						  mask);
		}
	}
	else
	{
		if (target)
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] was not found on the Acces List for [%s]",
				   mask,
				   target->nick);
		else
			notice(n_NickServ, lptr->nick,
				   "[\002%s\002] was not found on your Access List",
				   mask);
	}
} /* n_access_del() */

static void
n_access_list(struct Luser *lptr, struct NickInfo *target, int ac, char **av)

{
	struct NickInfo *nptr;
	int cnt = 1;
	struct NickHost *hptr;
	char *mask;

	if (target)
		nptr = target;
	else
		nptr = GetLink(lptr->nick);

	if (!nptr)
		return;

	mask = NULL;

	if (target)
	{
		if (ac >= 4)
			mask = av[3];
	}
	else
	{
		if (ac >= 3)
			mask = av[2];
	}

	if (target)
		RecordCommand("%s: Administrator %s!%s@%s ACCESS %s LIST %s",
					  n_NickServ,
					  lptr->nick,
					  lptr->username,
					  lptr->hostname,
					  target->nick,
					  mask ? mask : "");
	else
		RecordCommand("%s: %s!%s@%s ACCESS LIST %s",
					  n_NickServ,
					  lptr->nick,
					  lptr->username,
					  lptr->hostname,
					  mask ? mask : "");

	notice(n_NickServ, lptr->nick,
		   "-- Access List for [\002%s\002] --",
		   nptr->nick);

	for (hptr = nptr->hosts; hptr; hptr = hptr->next)
	{
		if (mask)
			if (match(mask, hptr->hostmask) == 0)
				continue;

		notice(n_NickServ, lptr->nick, "%d) %s",
			   cnt++,
			   hptr->hostmask);
	}

	notice(n_NickServ, lptr->nick, "-- End of list --");
} /* n_access_list() */

/*
n_set()
  Set various options for your nick
*/
static void n_set(struct Luser *lptr, int ac, char **av)
{
	struct Command *cptr = NULL;
	struct NickInfo *nptr = NULL;
	struct NickInfo *sender = NULL;

	if (ac < 3)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SET <nickname> <option> [parameter]\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			   "SET");
		return;
	}

	if ((nptr = FindNick(av[1])) == NULL)
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, av[1]);
		return;
	}

	if (nptr->flags & NS_FORBID)
	{
		notice(n_NickServ, lptr->nick,
			   "Cannot execute commands for forbidden nicknames");
		return;
	}

	sender = FindNick(lptr->nick);

#ifdef LINKED_NICKNAMES
	/* allow administrators to modify users' flags and settings */
	if (!IsLinked(nptr, sender)
#ifdef EMPOWERADMINS_MORE
			&& !IsValidAdmin(lptr)
#endif /* EMPOWERADMINS_MORE */
	   )
	{
		notice(n_NickServ, lptr->nick,
			   "Nickname [\002%s\002] is not linked with [\002%s\002]",
			   nptr->nick, lptr->nick);
		return;
	}
#endif /* LINKED_NICKNAMES */

	cptr = GetCommand(setcmds, av[2]);

	if (cptr && (cptr != (struct Command *) -1))
	{
		/* call the appropriate function */
		(*cptr->func)(lptr, nptr, ac, av);
	}
	else
	{
		/* the command doesn't exist */
		notice(n_NickServ, lptr->nick,
			   "%s switch [\002%s\002]",
			   (cptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
			   av[2]);
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "SET");
		return;
	}
} /* n_set() */

static void n_set_kill(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Kill Protection for [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_PROTECTED) ? "ON" :
			   ((nptr->flags & NS_KILLIMMED) ? "IMMED" : "OFF"));
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s KILL %s", n_NickServ, lptr->nick,
				  lptr->username, lptr->hostname, nptr->nick,
				  StrToupper(av[3]));

	if (!irccmp(av[3], "ON"))
	{
		nptr->flags &= ~NS_KILLIMMED;
		if (AllowKillProtection)
		{
			nptr->flags |= NS_PROTECTED;
			notice(n_NickServ, lptr->nick,
				   "Kill Protection for [\002%s\002] is now [\002ON\002]",
				   nptr->nick);
			return;
		}
		notice(n_NickServ, lptr->nick,
			   "Kill Protection is disabled on this network");
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		nptr->flags &= ~(NS_PROTECTED | NS_KILLIMMED);
		notice(n_NickServ, lptr->nick,
			   "Kill Protection for [\002%s\002] is now [\002OFF\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[3], "IMMED"))
	{
		if (AllowKillImmed)
		{
			nptr->flags &= ~NS_PROTECTED;
			nptr->flags |= NS_KILLIMMED;
			notice(n_NickServ, lptr->nick,
				   "Immediate Kill Protection for [\002%s\002] is now [\002ON\002]",
				   nptr->nick);
		}
		else
		{
			notice(n_NickServ, lptr->nick,
				   "The [\002IMMED\002] option is disabled on this network");
		}

		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> KILL {ON|OFF|IMMED}\002");
	notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "SET KILL");
} /* n_set_kill() */

static void n_set_automask(struct Luser *lptr, struct NickInfo *nptr, int ac,
						   char **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "AutoMask for [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_AUTOMASK) ? "ON" : "OFF");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s AUTOMASK %s", n_NickServ,
				  lptr->nick, lptr->username, lptr->hostname, nptr->nick,
				  StrToupper(av[3]));

	if (!irccmp(av[3], "ON"))
	{
		nptr->flags |= NS_AUTOMASK;
		notice(n_NickServ, lptr->nick,
			   "AutoMask for [\002%s\002] is now [\002ON\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		nptr->flags &= ~NS_AUTOMASK;
		notice(n_NickServ, lptr->nick,
			   "AutoMask for [\002%s\002] is now [\002OFF\002]",
			   nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> AUTOMASK {ON|OFF}\002");
	notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
		   "SET AUTOMASK");
} /* n_set_automask() */

static void n_set_neverop(struct Luser *lptr, struct NickInfo *nptr, int ac,
						   char **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "NeverOP for [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_NEVEROP) ? "ON" : "OFF");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s NEVEROP %s", n_NickServ,
				  lptr->nick, lptr->username, lptr->hostname, nptr->nick,
				  StrToupper(av[3]));

	if (!irccmp(av[3], "ON"))
	{
		nptr->flags |= NS_NEVEROP;
		notice(n_NickServ, lptr->nick,
			   "NeverOP for [\002%s\002] is now [\002ON\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		nptr->flags &= ~NS_NEVEROP;
		notice(n_NickServ, lptr->nick,
			   "NeverOP for [\002%s\002] is now [\002OFF\002]",
			   nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> NEVEROP {ON|OFF}\002");
	notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
		   "SET NEVEROP");
} /* n_set_neverop() */


static void n_set_private(struct Luser *lptr, struct NickInfo *nptr, int ac, char
						  **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Privacy for [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_PRIVATE) ? "ON" : "OFF");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s PRIVATE %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, StrToupper(av[3]));

	if (!irccmp(av[3], "ON"))
	{
		nptr->flags |= NS_PRIVATE;
		notice(n_NickServ, lptr->nick,
			   "Privacy for [\002%s\002] is now [\002ON\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		nptr->flags &= ~NS_PRIVATE;
		notice(n_NickServ, lptr->nick,
			   "Privacy for [\002%s\002] is now [\002OFF\002]",
			   nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> PRIVATE {ON|OFF}\002");
	notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
		   "SET PRIVATE");
} /* n_set_private() */


static void n_set_privmsg(struct Luser *lptr, struct NickInfo *nptr, int ac, char
						  **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Private messaging for [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_PRIVMSG) ? "ON (PRIVMSG)" : "OFF (NOTICE)");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s PRIVMSG %s", n_NickServ, lptr->nick,
				  lptr->username, lptr->hostname, nptr->nick, StrToupper(av[3]));

	if (!irccmp(av[3], "ON"))
	{
		nptr->flags |= NS_PRIVMSG;
		notice(n_NickServ, lptr->nick,
			   "Private messaging for [\002%s\002] is now [\002ON (PRIVMSG)\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		nptr->flags &= ~NS_PRIVMSG;
		notice(n_NickServ, lptr->nick,
			   "Private messaging for [\002%s\002] is now [\002OFF (NOTICE)\002]",
			   nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> PRIVMSG {ON|OFF}\002");
	notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
		   "SET PRIVMSG");
} /* n_set_privmsg() */

static void n_set_secure(struct Luser *lptr, struct NickInfo *nptr, int ac, char
						 **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Security for [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_SECURE) ? "ON" : "OFF");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s SECURE %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, StrToupper(av[3]));

	if (!irccmp(av[3], "ON"))
	{
		nptr->flags |= NS_SECURE;
		notice(n_NickServ, lptr->nick,
			   "Security for [\002%s\002] is now [\002ON\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		nptr->flags &= ~NS_SECURE;
		notice(n_NickServ, lptr->nick,
			   "Security for [\002%s\002] is now [\002OFF\002]",
			   nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> SECURE {ON|OFF}\002");
	notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "SET SECURE");
} /* n_set_secure() */

static void n_set_unsecure(struct Luser *lptr, struct NickInfo *nptr, int ac,
						   char **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "UnSecure for [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_UNSECURE) ? "ON" : "OFF");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s UNSECURE %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, StrToupper(av[3]));

	if (!irccmp(av[3], "ON"))
	{
		nptr->flags |= NS_UNSECURE;
		notice(n_NickServ, lptr->nick,
			   "UnSecure for [\002%s\002] is now [\002ON\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		nptr->flags &= ~NS_UNSECURE;
		notice(n_NickServ, lptr->nick,
			   "UnSecure for [\002%s\002] is now [\002OFF\002]",
			   nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> UNSECURE {ON|OFF}\002");
	notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
		   "SET UNSECURE");
} /* n_set_unsecure() */

static void n_set_memos(struct Luser *lptr, struct NickInfo *nptr, int ac, char
						**av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "AllowMemos for [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_MEMOS) ? "ON" : "OFF");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s MEMOS %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, StrToupper(av[3]));

	if (!irccmp(av[3], "ON"))
	{
		nptr->flags |= NS_MEMOS;
		notice(n_NickServ, lptr->nick,
			   "AllowMemos for [\002%s\002] is now [\002ON\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		nptr->flags &= ~NS_MEMOS;
		notice(n_NickServ, lptr->nick,
			   "AllowMemos for [\002%s\002] is now [\002OFF\002]",
			   nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> MEMOS {ON|OFF}\002");
	notice(n_NickServ, lptr->nick,
		   ERR_MORE_INFO, n_NickServ, "SET MEMOS");
} /* n_set_memos() */

static void n_set_notify(struct Luser *lptr, struct NickInfo *nptr, int ac, char
						 **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Notification of new memos for [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_MEMONOTIFY) ? "ON" : "OFF");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s NOTIFY %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, StrToupper(av[3]));

	if (!irccmp(av[3], "ON"))
	{
		nptr->flags |= NS_MEMONOTIFY;
		notice(n_NickServ, lptr->nick,
			   "Notification of new memos for [\002%s\002] is now [\002ON\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		nptr->flags &= ~NS_MEMONOTIFY;
		notice(n_NickServ, lptr->nick,
			   "Notification of new memos for [\002%s\002] is now [\002OFF\002]",
			   nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> NOTIFY {ON|OFF}\002");
	notice(n_NickServ, lptr->nick,
		   ERR_MORE_INFO, n_NickServ, "SET NOTIFY");
} /* n_set_notify() */

static void n_set_signon(struct Luser *lptr, struct NickInfo *nptr, int ac, char
						 **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Notification of memos on signon for [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_MEMOSIGNON) ? "ON" : "OFF");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s SIGNON %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, StrToupper(av[3]));

	if (!irccmp(av[3], "ON"))
	{
		nptr->flags |= NS_MEMOSIGNON;
		notice(n_NickServ, lptr->nick,
			   "Notification of memos on signon for [\002%s\002] is now [\002ON\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		nptr->flags &= ~NS_MEMOSIGNON;
		notice(n_NickServ, lptr->nick,
			   "Notification of memos on signon for [\002%s\002] is now [\002OFF\002]",
			   nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> SIGNON {ON|OFF}\002");
	notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "SET SIGNON");
} /* n_set_signon() */

static void n_set_hide(struct Luser *lptr, struct NickInfo *nptr, int ac, char
					   **av)
{
	char str[MAXLINE + 1];
	int flag = 0;

	/* Check arguments first .. */
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SET <nickname> HIDE {ALL|EMAIL|URL|QUIT} {ON|OFF}\002",
			   nptr->nick);
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			   "SET HIDE");
		return;
	}

	strlcpy(str, StrToupper(av[3]), sizeof(str));

	RecordCommand("%s: %s!%s@%s SET %s HIDE %s %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, str,
				  (ac < 5) ? "" : StrToupper(av[4]));

	if (!ircncmp(av[3], "ALL", strlen(av[2])))
	{
		flag = NS_HIDEALL;
		strlcpy(str, "Hide Info", sizeof(str));
	}
	else if (!ircncmp(av[3], "EMAIL", strlen(av[2])))
	{
		flag = NS_HIDEEMAIL;
		strlcpy(str, "Hide Email", sizeof(str));
	}
	else if (!ircncmp(av[3], "URL", strlen(av[2])))
	{
		flag = NS_HIDEURL;
		strlcpy(str, "Hide Url", sizeof(str));
	}
	else if (!ircncmp(av[3], "QUIT", strlen(av[2])))
	{
		flag = NS_HIDEQUIT;
		strlcpy(str, "Hide Quit", sizeof(str));
	}
	else if (!ircncmp(av[3], "ADDR", strlen(av[2])))
	{
		flag = NS_HIDEADDR;
		strlcpy(str, "Hide Address", sizeof(str));
	}

	/* order this properly */
	if (!flag)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SET <nickname> HIDE {ALL|EMAIL|URL|QUIT} {ON|OFF}\002",
			   nptr->nick);
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			   "SET HIDE");
		return;
	}

	if (ac < 5)
	{
		notice(n_NickServ, lptr->nick,
			   "%s for [\002%s\002] is [\002%s\002]",
			   str, nptr->nick,
			   (nptr->flags & flag) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[4], "ON"))
	{
		nptr->flags |= flag;
		notice(n_NickServ, lptr->nick,
			   "%s for [\002%s\002] is now [\002ON\002]",
			   str, nptr->nick);
		return;
	}

	if (!irccmp(av[4], "OFF"))
	{
		nptr->flags &= ~flag;
		notice(n_NickServ, lptr->nick,
			   "%s for [\002%s\002] is now [\002OFF\002]",
			   str, nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> HIDE {ALL|EMAIL|URL|QUIT} {ON|OFF}\002");
	notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
		   "SET HIDE");
} /* n_set_hide() */

static void n_set_password(struct Luser *lptr, struct NickInfo *nptr, int ac,
						   char **av)
{
	/* The user must supply the current password! */
	if (ac < 5)
	{
		notice(n_NickServ, lptr->nick,
				"Syntax: \002SET <nickname> PASSWORD <oldpass> <newpass>\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			   "SET PASSWORD");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s PASSWORD",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick);

	if (!pwmatch(nptr->password, av[3]))
	{
		notice(n_NickServ, lptr->nick, ERR_BAD_PASS);
		return;
	}

	if (!ChangePass(nptr, av[4]))
	{
		notice(n_NickServ, lptr->nick,
			   "Password change for [\002%s\002] failed",
			   nptr->nick);
		return;
	}

	notice(n_NickServ, lptr->nick,
		   "Password for [\002%s\002] is now [\002%s\002]",
		   nptr->nick, av[4]);
} /* n_set_password() */

/* n_set_phrase()  -bane */
static void n_set_phrase(struct Luser *lptr, struct NickInfo *nptr, int
						 ac, char **av)
{
	char *prphrase;

	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SET <nickname> PHRASE <newphrase>\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			   "SET PHRASE");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s PHRASE ...",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick);

	/* Get password recovery phrase from input	-bane */
	prphrase = GetString(ac - 3, av + 3);

	if (strlen(prphrase) < 8)
	{
		notice(n_NickServ, lptr->nick,
			   "Password recovery phrase must be at least 8 characters long");

		MyFree(prphrase);
		return;
	}

	MyFree(nptr->phrase);
	nptr->phrase = prphrase;

	notice(n_NickServ, lptr->nick,
		   "Password recovery phrase for [\002%s\002] is now [\002%s\002]",
		   nptr->nick, prphrase);
} /* n_set_phrase() */

static void n_set_email(struct Luser *lptr, struct NickInfo *nptr, int ac, char
						**av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SET <nickname> EMAIL <email address|->\002");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO, n_NickServ, "SET EMAIL");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s EMAIL %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, av[3]);

	MyFree(nptr->email);

	if (!irccmp(av[3], "-"))
	{
		nptr->email = NULL;
		notice(n_NickServ, lptr->nick,
			   "Email address for [\002%s\002] has been cleared",
			   nptr->nick);
		return;
	}

	nptr->email = MyStrdup(av[3]);

	notice(n_NickServ, lptr->nick,
		   "Email address for [\002%s\002] is now [\002%s\002]",
		   nptr->nick, nptr->email);
} /* n_set_email() */

static void n_set_url(struct Luser *lptr, struct NickInfo *nptr, int ac, char
					  **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SET <nickname> URL <url|->\002");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO, n_NickServ, "SET URL");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s URL %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, av[3]);

	MyFree(nptr->url);

	if (!irccmp(av[3], "-"))
	{
		nptr->url = NULL;
		notice(n_NickServ, lptr->nick,
			   "URL for [\002%s\002] has been cleared",
			   nptr->nick);
		return;
	}

	nptr->url = MyStrdup(av[3]);

	notice(n_NickServ, lptr->nick,
		   "URL for [\002%s\002] is now [\002%s\002]",
		   nptr->nick, nptr->url);
} /* n_set_url() */

static void n_set_uin(struct Luser *lptr, struct NickInfo *nptr, int ac, char
					  **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SET <nickname> UIN <uin|->\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO,
			   n_NickServ, "SET UIN");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s UIN %s", n_NickServ, lptr->nick,
				  lptr->username, lptr->hostname, nptr->nick, av[3]);

	MyFree(nptr->UIN);

	if (!irccmp(av[3], "-"))
	{
		nptr->UIN = NULL;
		notice(n_NickServ, lptr->nick,
			   "UIN for [\002%s\002] has been cleared",
			   nptr->nick);
		return;
	}

	nptr->UIN = MyStrdup(av[3]);

	notice(n_NickServ, lptr->nick,
		   "UIN for [\002%s\002] is now [\002%s\002]",
		   nptr->nick, nptr->UIN);
} /* n_set_uin() */

static void n_set_gsm(struct Luser *lptr, struct NickInfo *nptr, int ac, char
					  **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SET <nickname> GSM <GSM number|->\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			   "SET GSM");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s GSM %s", n_NickServ, lptr->nick,
				  lptr->username, lptr->hostname, nptr->nick, av[3]);

	MyFree(nptr->gsm);

	if (!irccmp(av[3], "-"))
	{
		nptr->gsm = NULL;
		notice(n_NickServ, lptr->nick,
			   "GSM number for [\002%s\002] has been cleared",
			   nptr->nick);
		return;
	}

	nptr->gsm = MyStrdup(av[3]);

	notice(n_NickServ, lptr->nick,
		   "GSM number for [\002%s\002] is now [\002%s\002]",
		   nptr->nick, nptr->gsm);
} /* n_set_gsm() */

/* Set phone number for nick. Code taken from IrcBg and modified -kre */
static void n_set_phone(struct Luser *lptr, struct NickInfo *nptr, int ac, char
						**av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SET PHONE <phone number|->\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			   "SET PHONE");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s PHONE %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, av[3]);

	MyFree(nptr->phone);

	if (!irccmp(av[3], "-"))
	{
		nptr->phone = NULL;
		notice(n_NickServ, lptr->nick,
			   "Phone number for [\002%s\002] has been cleared",
			   nptr->nick);
		return;
	}

	nptr->phone = MyStrdup(av[3]);

	notice(n_NickServ, lptr->nick,
		   "Phone number for [\002%s\002] is now [\002%s\002]",
		   nptr->nick, nptr->phone);
} /* n_set_phone() */

#ifdef LINKED_NICKNAMES
/*
n_set_master()
 Set the master nickname for lptr's nick link to av[1]
*/
static void n_set_master(struct Luser *lptr, struct NickInfo *nptr, int ac, char
						 **av)
{
	struct NickInfo *newmaster, *origmaster;
	struct NickHost *hptr;

	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SET <nickname> MASTER <nickname>\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			   "SET MASTER");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s MASTER %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, av[3]);

	if (!(newmaster = FindNick(av[3])))
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, av[3]);
		return;
	}

#ifdef LINKED_NICKNAMES
	if (!IsLinked(newmaster, nptr))
	{
		notice(n_NickServ, lptr->nick,
			   "The nickname [\002%s\002] is not linked with [\002%s\002]",
			   newmaster->nick, nptr->nick);
		return;
	}
#endif /* LINKED_NICKNAMES */

	if ((nptr->master == newmaster) ||
			(!nptr->master && (nptr == newmaster)))
	{
		notice(n_NickServ, lptr->nick,
			   "The nickname [\002%s\002] is already the master for [\002%s\002]",
			   newmaster->nick, nptr->nick);
		return;
	}

	if (nptr->master)
		origmaster = nptr->master;
	else
		origmaster = nptr;

	DeleteLink(newmaster, 1);
	InsertLink(newmaster, origmaster);

	/*
	 * Delete origmaster's access list, since newmaster
	 * has it now
	 */
	while (origmaster->hosts)
	{
		hptr = origmaster->hosts->next;
		MyFree(origmaster->hosts->hostmask);
		MyFree(origmaster->hosts);
		origmaster->hosts = hptr;
	}

	notice(n_NickServ, lptr->nick,
		   "The master nickname for [\002%s\002] is now [\002%s\002]",
		   nptr->nick, newmaster->nick);
} /* n_set_master() */
#endif /* LINKED_NICKNAMES */

/*
n_list()
  Display list of nicknames matching av[1]
*/

static void
n_list(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *temp;
	int IsAnAdmin;
	int ii, cnt,
	match_flags = 0, /* flags to match */
				  mcnt, /* total matches found */
				  acnt; /* total matches - private nicks */

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002LIST <pattern> [options]\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO,
			   n_NickServ, "LIST");
		return;
	}

	RecordCommand("%s: %s!%s@%s LIST %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname, av[1]);

	if (IsValidAdmin(lptr))
		IsAnAdmin = 1;
	else
		IsAnAdmin = 0;

	for (cnt = 2; cnt < ac; cnt++)
	{
		if (!ircncmp(av[cnt], "-forbid", strlen(av[cnt])))
			match_flags |= NS_FORBID;
		else if (!ircncmp(av[cnt], "-online", strlen(av[cnt])))
			match_flags |= NS_IDENTIFIED;
		else if (!ircncmp(av[cnt], "-private", strlen(av[cnt])))
			match_flags |= NS_PRIVATE;
		else if (!ircncmp(av[cnt], "-protect", strlen(av[cnt])))
			match_flags |= NS_PROTECTED;
		else if (!ircncmp(av[cnt], "-noexpire", strlen(av[cnt])))
			match_flags |= NS_NOEXPIRE;
	}

	acnt = mcnt = 0;
	notice(n_NickServ, lptr->nick,
		   "-- Listing nicknames matching [\002%s\002] --",
		   av[1]);

	for (ii = 0; ii < NICKLIST_MAX; ++ii)
	{
		for (temp = nicklist[ii]; temp; temp = temp->next)
		{
			if (match(av[1], temp->nick))
			{
				if (match_flags && !(temp->flags & match_flags))
					continue;

				++mcnt;

				if ((IsAnAdmin) || !(temp->flags & NS_PRIVATE))
				{
					char str[20];

					++acnt;
					if (temp->flags & NS_FORBID)
						strlcpy(str, "<< FORBIDDEN >>", sizeof(str));
					else if ((temp->flags & NS_IDENTIFIED) && FindClient(temp->nick))
						strlcpy(str, "<< ONLINE >>", sizeof(str));
					else if (temp->flags & NS_PRIVATE)
						strlcpy(str, "<< PRIVATE >>", sizeof(str));
					else if (temp->flags & NS_PROTECTED)
						strlcpy(str, "<< PROTECTED >>", sizeof(str));
					else if (temp->flags & NS_KILLIMMED)
						strlcpy(str, "<< IMMEDKILL >>", sizeof(str));
					else if (temp->flags & NS_NOEXPIRE)
						strlcpy(str, "<< NOEXPIRE >>", sizeof(str));
					else
						str[0] = '\0';

					notice(n_NickServ, lptr->nick,
						   "%-10s %15s created %s ago",
						   temp->nick, str, timeago(temp->created, 1));
				}
			} /* if (match(av[1], temp->nick)) */
		} /* for (temp = nicklist[ii]; temp; temp = temp->next) */
	} /* for (ii = 0; ii < NICKLIST_MAX; ++ii) */

	notice(n_NickServ, lptr->nick,
		   "-- End of list (%d/%d matches shown) --",
		   acnt,
		   mcnt);
} /* n_list() */

/*
n_info()
  Display some information about nickname av[1]
*/

static void
n_info(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *nptr, *realptr, *tmpnick;
	int online = 0, isadmin = 0, isowner = 0;

	/* find about nick who issued the request */
	if (!(tmpnick = FindNick(lptr->nick)))
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, lptr->nick);
		return;
	}

	/* if there is less than 2 arguments, target is requester itself */
	if (ac < 2)
		realptr = tmpnick;
	else
		/* no, target is av[1] */
		if (!(realptr = FindNick(av[1])))
		{
			notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, av[1]);
			return;
		}

	if (!(nptr = GetMaster(realptr)))
		return;

#if defined EMPOWERADMINS || defined EMPOWERADMINS_MORE
	if (NotifyOpers)
	{
		struct Luser *ptr = FindClient(realptr->nick);
		if ((ptr != NULL) && (ptr->flags & L_OSREGISTERED) &&
				irccmp(realptr->nick, lptr->nick))
		{
			notice(n_NickServ, realptr->nick,
					"%s is doing INFO on you", lptr->nick);
		}
	}
#endif

	RecordCommand("%s: %s!%s@%s INFO %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  realptr->nick);

	isadmin = IsValidAdmin(lptr);
	isowner = ((nptr == GetMaster(tmpnick)) &&
			   (tmpnick->flags & NS_IDENTIFIED));

	if (((nptr->flags & NS_PRIVATE) || (nptr->flags & NS_FORBID)) &&
			!isowner
#ifdef EMPOWERADMINS
			&& !isadmin)
#else

	   )
#endif
	{
		notice(n_NickServ, lptr->nick,
			   "The nickname [\002%s\002] is private",
			   realptr->nick);
		RecordCommand("%s: %s!%s@%s failed INFO %s",
					  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
					  realptr->nick);
		return;
	}

	if (FindClient(realptr->nick))
		if (realptr->flags & NS_IDENTIFIED)
			online = 1;

	notice(n_NickServ, lptr->nick,
			"           Nickname: %s %s",
			realptr->nick,
			online ? "<< ONLINE >>" : "");

	notice(n_NickServ, lptr->nick,
			"         Registered: %s ago",
			timeago(realptr->created, 1));

	if ((isadmin || isowner) && NickNameExpire)
	{
		long exptime;
		if (NickNameExpireAddTime && NickNameExpireAddPeriod)
			exptime = NickNameExpire + NickNameExpireAddTime * ((current_ts - realptr->created) / NickNameExpireAddPeriod);
		else
			exptime = NickNameExpire;

		notice(n_NickServ, lptr->nick,
			"        Expire Time: %s",
			timeago(exptime, 3));
	}

	if (realptr->lastseen && !online)
		notice(n_NickServ, lptr->nick,
			"          Last Seen: %s ago",
			timeago(realptr->lastseen, 1));

	if (!(nptr->flags & NS_HIDEALL)
			|| isadmin || isowner)
	{
		char buf[MAXLINE + 1];

		if (LastSeenInfo)
		{
			if (!(nptr->flags & NS_HIDEADDR) || isadmin || isowner)
				if (!online && realptr->lastu && realptr->lasth)
					notice(n_NickServ, lptr->nick,
						   "  Last Seen Address: %s@%s",
						   realptr->lastu, realptr->lasth);

			if (!(nptr->flags & NS_HIDEQUIT) || isadmin || isowner)
				if (realptr->lastqmsg)
					notice(n_NickServ, lptr->nick,
						   " Last Seen Quit Msg: %s",
						   realptr->lastqmsg);
		} /* if (LastSeenInfo) */

		if (!(nptr->flags & NS_HIDEEMAIL) || isadmin || isowner)
			if (realptr->email)
				notice(n_NickServ, lptr->nick,
					"      Email Address: %s", realptr->email);

		if (!(nptr->flags & NS_HIDEURL) || isadmin || isowner)
			if (realptr->url)
				notice(n_NickServ, lptr->nick,
					"                URL: %s", realptr->url);

		if (nptr->UIN)
			notice(n_NickServ, lptr->nick,
				"                UIN: %s", nptr->UIN);

		if (nptr->gsm)
			notice(n_NickServ, lptr->nick,
				"                GSM: %s", nptr->gsm);

		if (nptr->phone)
			notice(n_NickServ, lptr->nick,
				"              Phone: %s", nptr->phone);

		buf[0] = '\0';
		if (AllowKillProtection)
		{
			if (AllowKillImmed && (nptr->flags & NS_KILLIMMED))
				strlcat(buf, "Immediate Kill Protection, ", sizeof(buf));
			else
				if (nptr->flags & NS_PROTECTED)
					strlcat(buf, "Kill Protection, ", sizeof(buf));
		}
		if (realptr->flags & NS_NOEXPIRE)  /* show the specific flag, not the master's one */
			strlcat(buf, "NoExpire, ", sizeof(buf));
		if (nptr->flags & NS_AUTOMASK)
			strlcat(buf, "AutoMask, ", sizeof(buf));
		if (nptr->flags & NS_PRIVATE)
			strlcat(buf, "Private, ", sizeof(buf));
		if (nptr->flags & NS_FORBID)
			strlcat(buf, "Forbidden, ", sizeof(buf));
		if (nptr->flags & NS_SECURE)
			strlcat(buf, "Secure, ", sizeof(buf));
		if (nptr->flags & NS_UNSECURE)
			strlcat(buf, "UnSecure, ", sizeof(buf));
		if (nptr->flags & NS_HIDEALL)
			strlcat(buf, "Hidden, ", sizeof(buf));
		if (nptr->flags & NS_MEMOS)
			strlcat(buf, "AllowMemos, ", sizeof(buf));
		if (nptr->flags & NS_MEMONOTIFY)
			strlcat(buf, "MemoNotify, ", sizeof(buf));
		if (nptr->flags & NS_MEMOSIGNON)
			strlcat(buf, "MemoSignon, ", sizeof(buf));
		if (nptr->flags & NS_NOREGISTER)
			strlcat(buf, "NoRegister, ", sizeof(buf));
		if (nptr->flags & NS_NOCHANOPS)
			strlcat(buf, "NoChannelOps, ", sizeof(buf));
		if (nptr->flags & NS_NOLINK)
			strlcat(buf, "NoLink, ", sizeof(buf));
       if (nptr->flags & NS_NOADD)
           strlcat(buf, "NoAdd, ", sizeof(buf));
		if (nptr->flags & NS_NEVEROP)
			strlcat(buf, "NeverOp, ", sizeof(buf));


		if (*buf)
		{
			buf[strlen(buf) - 2] = '\0';
			notice(n_NickServ, lptr->nick,
				   "   Nickname Options: %s",
				   buf);
		}
	}

	if (isadmin)
	{
		if (nptr->flags & NS_FORBID)
		{
			if (nptr->forbidby)
				notice(n_NickServ, lptr->nick, "          Forbid by: %s",
					   nptr->forbidby);
			if (nptr->forbidreason)
				notice(n_NickServ, lptr->nick, "      Forbid reason: %s",
					   nptr->forbidreason);
		}
	}

	if (isadmin || isowner)
	{
		int cnt;

		/*
		 * Show administrators all nicknames in this user's
		 * link, and also all channels this user has registered
		 */

#ifdef LINKED_NICKNAMES

		if (nptr->master || nptr->nextlink)
		{
			struct NickInfo *tmp;

			cnt = 0;

			notice(n_NickServ, lptr->nick,
				"   Linked Nicknames (first is master):");

			for (tmp = GetMaster(nptr); tmp; tmp = tmp->nextlink)
			{
				notice(n_NickServ, lptr->nick,
					"                     %d) %s", ++cnt, tmp->nick);
			}
		}

#endif /* LINKED_NICKNAMES */

#ifdef CHANNELSERVICES

		/*
		 * ChanServDB makes all founder entries the nickname of
		 * the LINK master - so make sure we test the founder
		 * nickname against the link master's
		 */
		if (nptr->FounderChannels)
		{
			struct aChannelPtr *tmpchan;

			notice(n_NickServ, lptr->nick, "Channels Registered:");

			cnt = 0;
			for (tmpchan = nptr->FounderChannels; tmpchan; tmpchan =
						tmpchan->next)
			{
				notice(n_NickServ, lptr->nick, "                     %d) %s",
					   ++cnt, tmpchan->cptr->name);
			}
		}

		if (nptr->AccessChannels)
		{
			struct AccessChannel *acptr;

			notice(n_NickServ, lptr->nick,
				   "Access Channels:");

			cnt = 0;
			for (acptr = nptr->AccessChannels; acptr; acptr = acptr->next)
			{
				notice(n_NickServ, lptr->nick,
					"                     %d) (%-3d) %s",
					++cnt, acptr->accessptr->level, acptr->cptr->name);
			}
		}

#endif /* CHANNELSERVICES */

	} /* if (isadmin || isowner) */
} /* n_info() */

#ifdef LINKED_NICKNAMES

/*
n_link()
 Link lptr's nickname to av[1]. Make sure lptr supplied the correct
password for the nickname av[1], which will be stored in av[2]
*/

static void
n_link(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *target, /* target nickname */
				*nptr, /* lptr's nick structure */
				*mptr; /* nptr's master   -kamen */
	int badlink,
	ret;

	if (ac < 3)
{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002LINK <nickname> <password>\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "LINK");
		return;
	}

	target = FindNick(av[1]);

	if (target == NULL)
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, av[1]);
		return;
	}

	badlink = 0;

	if (!(nptr = FindNick(lptr->nick)))
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, lptr->nick);
		badlink = 1;
	}
	else if (nptr->flags & NS_FORBID)
	{
		notice(n_NickServ, lptr->nick,
			   "The nickname [\002%s\002] is forbidden",
			   target->nick);
		badlink = 1;
	}
	/* NoLink support   -kamen */
	else if ((mptr = GetMaster(target)) && (mptr->flags & NS_NOLINK))
	{
		notice(n_NickServ, lptr->nick,
			   "The nickname [\002%s\002] does not accept links",
			   target->nick);
		badlink = 1;
	}
	else if (!irccmp(target->nick, lptr->nick))
	{
		notice(n_NickServ, lptr->nick,
			   "You cannot link to your current nickname");
		badlink = 1;
	}
	else if (!pwmatch(target->password, av[2]))
	{
		notice(n_NickServ, lptr->nick,
			   ERR_BAD_PASS);
		badlink = 1;
	}


	if (badlink)
	{
		RecordCommand("%s: %s!%s@%s failed LINK %s",
					  n_NickServ, lptr->nick, lptr->username,
					  lptr->hostname, target->nick);
		return;
	}

	RecordCommand("%s: %s!%s@%s LINK %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname, target->nick);

	if ((ret = InsertLink(target, nptr)) > 0)
	{
		struct NickHost *htmp;

		/*
		 * Successful link - delete lptr's (nptr's) access list,
		 * also copy nptr's memo list to target's list
		 */
		while (nptr->hosts)
		{
			htmp = nptr->hosts->next;
			MyFree(nptr->hosts->hostmask);
			MyFree(nptr->hosts);
			nptr->hosts = htmp;
		}

#ifdef MEMOSERVICES
		MoveMemos(nptr, target);
#endif /* MEMOSERVICES */

		target->lastseen = nptr->lastseen;

		notice(n_NickServ, lptr->nick,
			   "Your nickname is now linked to [\002%s\002]",
			   target->nick);
	}
	else if (ret == 0)
	{
		notice(n_NickServ, lptr->nick,
			   "Link failed");
		putlog(LOG1, "%s: n_link: InsertLink() failed for %s (hub: %s)",
			   n_NickServ,
			   lptr->nick,
			   target->nick);
	}
	else if (ret == (-1))
	{
		notice(n_NickServ, lptr->nick,
			   "Link failed: circular link detected");
	}
	else if (ret == (-2))
	{
		notice(n_NickServ, lptr->nick,
			   "Number of links cannot exceed %d",
			   MaxLinks);
	}
} /* n_link() */

/*
n_unlink()
 Unlink lptr->nick from its current nick link. Make a copy of it's
master's access list for it.
 If av[1]/av[2] supplied, unlink nickname av[1] from its nick link
(making sure password av[2] matches up)
*/

static void
n_unlink(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *nptr;
	int unlink;

	if (ac == 2)
	{
		/*
		 * They specified a nickname to unlink, but no password
		 */
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002UNLINK [nickname [password]]\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "UNLINK");
		return;
	}

	if (ac < 3)
		nptr = FindNick(lptr->nick);
	else
		nptr = FindNick(av[1]);

	if (!nptr)
	{
		notice(n_NickServ, lptr->nick,
			   ERR_NOT_REGGED,
			   (ac < 3) ? lptr->nick : av[1]);
		return;
	}

	if (ac >= 3)
	{
		if (!pwmatch(nptr->password, av[2]))
		{
			notice(n_NickServ, lptr->nick,
				   ERR_BAD_PASS);
			RecordCommand("%s: %s!%s@%s failed UNLINK %s",
						  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
						  nptr->nick);
			return;
		}
	}

	RecordCommand("%s: %s!%s@%s UNLINK %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname, nptr->nick);

	unlink = DeleteLink(nptr, 1);

	if (unlink > 0)
	{
		notice(n_NickServ, lptr->nick,
			   "The nickname [\002%s\002] has been unlinked",
			   nptr->nick);
	}
	else if (unlink == 0)
	{
		notice(n_NickServ, lptr->nick,
			   "DeleteLink() reported errors, unlink failed");
		putlog(LOG1, "%s: UNLINK failed: DeleteLink() contained errors",
			   n_NickServ);
	}
	else if (unlink == -1)
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_LINKED, nptr->nick);
	}
} /* n_unlink() */

#endif /* LINKED_NICKNAMES */

#ifdef EMPOWERADMINS

/*
n_forbid()
  Create forbidden nickname av[1] (assume permission to exec
this command has already been checked)
*/

static void
n_forbid(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *nptr, *realptr, *tmp;
	char sendstr[MAXLINE + 1];

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002FORBID <nickname> [reason]\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "FORBID");
		return;
	}

	RecordCommand("%s: %s!%s@%s FORBID [%s]", n_NickServ, lptr->nick,
				  lptr->username, lptr->hostname, av[1]);

	o_Wallops("FORBID from %s for nick [%s]", lptr->nick, av[1]);

	nptr = FindNick(av[1]);
	realptr = GetMaster(nptr);
	ircsprintf(sendstr, "%s!%s@%s", lptr->nick, lptr->username,
			   lptr->hostname);

	if (nptr == NULL)
	{
		/* nick is not registered - do so now; add nick to nick table */
		nptr = MakeNick();
		nptr->nick = MyStrdup(av[1]);
		nptr->created = current_ts;
		nptr->flags |= NS_FORBID;
		nptr->forbidby = MyStrdup(sendstr);
		if (ac < 3)
			nptr->forbidreason = NULL;
		else
			nptr->forbidreason = GetString(ac - 2, av + 2);
		AddNick(nptr);
	}
	else
	{
#if 0
		/* We should be backwards compatibile, so we'll check the master.
		 * Although in the future this should fallback only to nptr -kre
		 * */
		if (realptr->flags & NS_FORBID)
		{
			notice(n_NickServ, lptr->nick,
				   "The nickname [\002%s\002] is already forbidden",
				   av[1]);
			return;
		}
#endif

#ifdef LINKED_NICKNAMES
		for (tmp = realptr; tmp != NULL; tmp = tmp->nextlink)
#else
		tmp = realptr;
#endif /* LINKED_NICKNAMES */
		{
			tmp->flags |= NS_FORBID;
			tmp->flags &= ~NS_IDENTIFIED;

			MyFree(tmp->forbidby);
			MyFree(tmp->forbidreason);

			tmp->forbidby = MyStrdup(sendstr);
			if (ac < 3)
				tmp->forbidreason = NULL;
			else
				tmp->forbidreason = GetString(ac - 2, av + 2);

			notice(n_NickServ, lptr->nick,
				   "The nickname [\002%s\002] is now forbidden",
				   tmp->nick);
			CheckNick(tmp->nick);
		}
	}
} /* n_forbid() */

/*
 * n_unforbid()
 * Remove forbid flag on nickname av[1]
 * (assume permission to exec this command has already been checked)
 * -kre
 */
static void
n_unforbid(struct Luser *lptr, int ac, char **av)
{
	struct NickInfo *nptr, *realptr, *tmp;

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002UNFORBID <nickname>\002");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO, n_NickServ, "UNFORBID");
		return;
	}

	nptr = FindNick(av[1]);
	realptr = GetMaster(nptr);

	if (nptr == NULL)
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, av[1]);
		return;
	}

	RecordCommand("%s: %s!%s@%s UNFORBID [%s]",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  av[1]);

	o_Wallops("UNFORBID from %s for nick [%s]", lptr->nick, av[1]);

	if (!nptr->password)
	{
		/* It is made from AddNick() in forbid() code - it is empty, so it is
		 * safe to delete it -kre */
		DeleteNick(nptr);

		notice(n_NickServ, lptr->nick,
			   "The nickname [\002%s\002] has been dropped", av[1]);
	}
	else
	{
#ifdef LINKED_NICKNAMES
		for (tmp = realptr; tmp != NULL; tmp = tmp->nextlink)
#else
		tmp = realptr;
#endif /* LINKED_NICKNAMES */
		{
			/* other flags will be set from CheckNick() code */
			tmp->flags &= ~(NS_FORBID | NS_IDENTIFIED | NS_COLLIDE);

			MyFree(tmp->forbidby);
			MyFree(tmp->forbidreason);

			notice(n_NickServ, lptr->nick,
				   "Nickname [\002%s\002] is now unforbidden", tmp->nick);
			CheckNick(tmp->nick);
		}
	}
} /* n_unforbid() */

/*
n_setpass()
  Change the password for nickname av[1] to av[2]
*/

static void
n_setpass(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *nptr;

	if (ac < 3)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SETPASS <nickname> <password>\002");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO,
			   n_NickServ,
			   "SETPASS");
		return;
	}

	RecordCommand("%s: %s!%s@%s SETPASS [%s] ...",
				  n_NickServ,
				  lptr->nick,
				  lptr->username,
				  lptr->hostname,
				  av[1]);

	o_Wallops("SETPASS from %s!%s@%s for nick [%s]",
			  lptr->nick, lptr->username, lptr->hostname,
			  av[1] );

	if (!(nptr = FindNick(av[1])))
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED,
			   av[1]);
		return;
	}

	if (!ChangePass(nptr, av[2]))
	{
		notice(n_NickServ, lptr->nick,
			   "Password change failed");
		RecordCommand("%s: Failed to change password for [%s] to [%s] (SETPASS)",
					  n_NickServ,
					  av[1],
					  av[2]);
		return;
	}

	/* unidentify if it is identified */
	if (nptr->flags & NS_IDENTIFIED)
	{
		nptr->flags &= ~NS_IDENTIFIED;
		CheckNick(nptr->nick);
	}

	notice(n_NickServ, lptr->nick,
		   "Password for [\002%s\002] has been changed to [\002%s\002]", av[1],
		   av[2]);
} /* n_setpass() */

/*
n_noexpire()
 Mark nickname av[1] never to expire
*/
static void n_noexpire(struct Luser *lptr, int ac, char **av)
{
	struct NickInfo *nptr;

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002NOEXPIRE <nickname> {ON|OFF}\002");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			   "NOEXPIRE");
		return;
	}

	RecordCommand("%s: %s!%s@%s NOEXPIRE [%s]",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  av[1], (ac < 3) ? "" : StrToupper(av[2]));

	if (!(nptr = FindNick(av[1])))
	{
		notice(n_NickServ, lptr->nick, ERR_NOT_REGGED, av[1]);
		return;
	}

	if (ac < 3)
	{
		notice(n_NickServ, lptr->nick,
			   "NoExpire for nickname [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_NOEXPIRE) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[2], "ON"))
	{
		nptr->flags |= NS_NOEXPIRE;
		notice(n_NickServ, lptr->nick,
			   "NoExpire for [\002%s\002] is now [\002ON\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[2], "OFF"))
	{
		nptr->lastseen = current_ts;
		nptr->flags &= ~NS_NOEXPIRE;
		notice(n_NickServ, lptr->nick,
			   "NoExpire for [\002%s\002] is now [\002OFF\002]",
			   nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002NOEXPIRE <nickname> {ON|OFF}\002");
	notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
		   "NOEXPIRE");
} /* n_noexpire() */

/*
 * Clear all noexpire flags for all users. Code taken from IrcBg and
 * modified. -kre
 */
void n_clearnoexp(struct Luser *lptr, int ac, char **av)
{
	int ii;
	struct NickInfo *nptr;
	time_t currtime;

	if (ac > 1)
	{
		notice(n_NickServ, lptr->nick,
			"Syntax: CLEARNOEXP");
		notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
			"CLEARNOEXP");
		return;
	}

	if (!IsValidServicesAdmin(lptr))
	{
		notice(n_NickServ, lptr->nick,
			"You must IDENTIFY as a Services Administrator to use this command");
		return;
	}
	
	RecordCommand("%s: %s!%s@%s CLEARNOEXP",
			n_NickServ, lptr->nick, lptr->username, lptr->hostname);

	currtime = current_ts;
	
	for (ii = 0; ii < NICKLIST_MAX; ++ii)
		for (nptr = nicklist[ii]; nptr; nptr = nptr->next)
			if (nptr->flags & NS_NOEXPIRE)
			{
				nptr->lastseen = currtime;
				nptr->flags &= ~NS_NOEXPIRE;
			}

	notice(n_NickServ, lptr->nick,
		"All noexpire flags for nicknames have been cleared.");

} /* n_clearnoexp() */

#ifdef LINKED_NICKNAMES
/*
n_showlink()
 Show nicknames linked to av[1]
*/

static void
n_showlink(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *nptr, *tmp, *start;

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002SHOWLINK <nickname>\002");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO,
			   n_NickServ,
			   "SHOWLINK");
		return;
	}

	RecordCommand("%s: %s!%s@%s SHOWLINK [%s]",
				  n_NickServ,
				  lptr->nick,
				  lptr->username,
				  lptr->hostname,
				  av[1]);

	if (!(nptr = GetLink(av[1])))
	{
		notice(n_NickServ, lptr->nick,
			   ERR_NOT_REGGED,
			   av[1]);
		return;
	}

	if (!nptr->master && !nptr->nextlink)
	{
		notice(n_NickServ, lptr->nick,
			   ERR_NOT_LINKED,
			   nptr->nick);
		return;
	}

	notice(n_NickServ, lptr->nick,
		   "-- Listing Nickname Link for [\002%s\002] --",
		   nptr->nick);

	notice(n_NickServ, lptr->nick,
		   "\002%s\002 << MASTER >>",
		   nptr->master ? nptr->master->nick : nptr->nick);

	if (nptr->master)
		start = nptr->master->nextlink;
	else
		start = nptr->nextlink;

	for (tmp = start; tmp; tmp = tmp->nextlink)
	{
		notice(n_NickServ, lptr->nick,
			   "  %s",
			   tmp->nick);

	}

	notice(n_NickServ, lptr->nick,
		   "-- End of list (%d nicknames in link) --",
		   nptr->master ? nptr->master->numlinks : nptr->numlinks);
} /* n_showlink() */

/*
n_droplink()
 Drop every nickname in the link av[1] is in.
*/

static void
n_droplink(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *nptr, *tmp;

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002DROPLINK <nickname>\002");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO,
			   n_NickServ,
			   "DROPLINK");
		return;
	}

	RecordCommand("%s: %s!%s@%s DROPLINK [%s]",
				  n_NickServ,
				  lptr->nick,
				  lptr->username,
				  lptr->hostname,
				  av[1]);

	if (!(nptr = GetLink(av[1])))
	{
		notice(n_NickServ, lptr->nick,
			   ERR_NOT_REGGED,
			   av[1]);
		return;
	}

	if (!nptr->master && !nptr->nextlink)
	{
		notice(n_NickServ, lptr->nick,
			   ERR_NOT_LINKED,
			   nptr->nick);
		return;
	}

	while (nptr)
	{
		tmp = nptr->nextlink;
		DeleteNick(nptr);
		nptr = tmp;
	}

	notice(n_NickServ, lptr->nick,
		   "Link containing [\002%s\002] has been dropped",
		   av[1]);
} /* n_droplink() */

#endif /* LINKED_NICKNAMES */

/*
n_collide()
 Perform various tasks on the current nick collide list
 
Options:
 -list	 - list nicknames to be collided
 -halt	 - stop nick collision for targets
 -set	 - set nick collision for target
 -setnow - set nick collision for target immediately
*/

static void
n_collide(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *nptr;
	int ii, cnt;
	int list, /* 1 if -list given */
	halt, /* 1 if -halt given */
	set
		, /* 1 if -set given */
		setnow; /* 1 if -setnow given */
	char *target;
	char argbuf[MAXLINE + 1];

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002COLLIDE <pattern> [options]\002");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO,
			   n_NickServ,
			   "COLLIDE");
		return;
	}

	target = NULL;
	list = halt = set
					  = setnow = 0;

	for (cnt = 1; cnt < ac; cnt++)
	{
		if (!ircncmp(av[cnt], "-list", strlen(av[cnt])))
			list = 1;
		else if (!ircncmp(av[cnt], "-halt", strlen(av[cnt])))
			halt = 1;
		else if (!ircncmp(av[cnt], "-set", strlen(av[cnt])))
			set
				= 1;
		else if (!ircncmp(av[cnt], "-setnow", strlen(av[cnt])))
			setnow = 1;
		else
		{
			if (!target)
				target = av[cnt];
		}
	}

	if (!target)
	{
		notice(n_NickServ, lptr->nick,
			   "No pattern specified");
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002COLLIDE <pattern> [options]\002");
		return;
	}

	if (ac < 3)
	{
		/*
		 * They didn't give any options - default to -set
		 */
		set
			= 1;
	}

	ircsprintf(argbuf, "[%s] ", target);

	if (list)
		strlcat(argbuf, "-list ", sizeof(argbuf));

	if (halt)
		strlcat(argbuf, "-halt ", sizeof(argbuf));

	if (set
	   )
		strlcat(argbuf, "-set ", sizeof(argbuf));

	if (setnow)
		strlcat(argbuf, "-setnow ", sizeof(argbuf));

	RecordCommand("%s: %s!%s@%s COLLIDE %s",
				  n_NickServ,
				  lptr->nick,
				  lptr->username,
				  lptr->hostname,
				  argbuf);

	if (set
			|| setnow)
	{
		nptr = FindNick(target);

		if (!nptr)
		{
			notice(n_NickServ, lptr->nick,
				   ERR_NOT_REGGED,
				   target);
			return;
		}

		if (set)
		{
			nptr->flags |= NS_COLLIDE;
			nptr->collide_ts = current_ts + 60;
			notice(n_NickServ, lptr->nick,
				   "The nickname [\002%s\002] has been marked for collision",
				   nptr->nick);
#if defined FORCE_NICK_CHANGE || defined SVSNICK || defined FORCENICK

			notice(n_NickServ, nptr->nick, ERR_MUST_CHANGE2);
#else

			notice(n_NickServ, nptr->nick, ERR_MUST_CHANGE);
#endif

		}
		else /* setnow */
		{
			collide(nptr->nick, 1);
			notice(n_NickServ, lptr->nick,
				   "The nickname [\002%s\002] has been collided",
				   nptr->nick);
		}

		return;
	} /* if (set || setnow) */

	if (list)
		notice(n_NickServ, lptr->nick,
			   "-- Listing collisions matching [\002%s\002] --",
			   target);

	cnt = 0;
	for (ii = 0; ii < NICKLIST_MAX; ++ii)
	{
		for (nptr = nicklist[ii]; nptr; nptr = nptr->next)
		{
			if (nptr->flags & NS_COLLIDE)
			{
				if (match(target, nptr->nick))
				{
					cnt++;
					if (halt)
						nptr->flags &= ~NS_COLLIDE;

					if (list)
					{
						notice(n_NickServ, lptr->nick,
							   "%-15s",
							   nptr->nick);
					}
				}
			} /* if (nptr->flags & NS_COLLIDE) */
		}
	}

	if (list)
	{
		notice(n_NickServ, lptr->nick,
			   "-- End of list (%d match%s found) --",
			   cnt,
			   (cnt == 1) ? "" : "es");
	}
	else
	{
		notice(n_NickServ, lptr->nick,
			   "%d match%s found",
			   cnt,
			   (cnt == 1) ? "" : "es");
	}
} /* n_collide() */

/*
n_flag()
 Gives a registered nickname various flags which will limit
their privileges
 
Available flags:
-noregister		Prevents them from registering a channel
-nochanops		Will not op them on a channel if they're an AOP
-clear			Clear all current flags
*/

static void
n_flag(struct Luser *lptr, int ac, char **av)

{
	struct NickInfo *nptr, *realptr;
	char buf[MAXLINE + 1];
	char pstr[MAXLINE + 1];
	int ii;

	if (ac < 2)
	{
		notice(n_NickServ, lptr->nick,
			   "Syntax: \002FLAG <nickname> [options]\002");
		notice(n_NickServ, lptr->nick,
			   ERR_MORE_INFO,
			   n_NickServ,
			   "FLAG");
		return;
	}

	if (!(realptr = FindNick(av[1])))
	{
		notice(n_NickServ, lptr->nick,
			   ERR_NOT_REGGED,
			   av[1]);
		return;
	}

	if (!(nptr = GetMaster(realptr)))
		return;

	buf[0] = '\0';
	pstr[0] = '\0';

	if (ac < 3)
	{
		RecordCommand("%s: %s!%s@%s FLAG [%s]",
					  n_NickServ,
					  lptr->nick,
					  lptr->username,
					  lptr->hostname,
					  realptr->nick);

		/*
		 * They gave no options, just display a list of
		 * nptr's current flags
		 */
		if (nptr->flags & NS_NOREGISTER)
			strlcat(buf, "NoRegister, ", sizeof(buf));
		if (nptr->flags & NS_NOCHANOPS)
			strlcat(buf, "NoChannelOps, ", sizeof(buf));

		if (*buf)
		{
			buf[strlen(buf) - 2] = '\0';
			notice(n_NickServ, lptr->nick,
				   "The flags for [\002%s\002] are: %s",
				   realptr->nick,
				   buf);
		}
		else
		{
			notice(n_NickServ, lptr->nick,
				   "The nickname [\002%s\002] has no current flags",
				   realptr->nick);
		}

		return;
	}

	/*
	 * Parse the options they gave
	 */
	for (ii = 2; ii < ac; ++ii)
	{
		if (!ircncmp(av[ii], "-noregister", strlen(av[ii])))
		{
			nptr->flags |= NS_NOREGISTER;
			strlcat(buf, "NoRegister, ", sizeof(buf));
			strlcat(pstr, "-noregister ", sizeof(pstr));
		}
		else if (!ircncmp(av[ii], "-nochanops", strlen(av[ii])))
		{
			nptr->flags |= NS_NOCHANOPS;
			strlcat(buf, "NoChannelOps, ", sizeof(buf));
			strlcat(pstr, "-nochanops ", sizeof(pstr));
		}
		else if (!ircncmp(av[ii], "-clear", strlen(av[ii])))
		{
			nptr->flags &= ~(NS_NOREGISTER | NS_NOCHANOPS);
			notice(n_NickServ, lptr->nick,
				   "The flags for [\002%s\002] have been cleared",
				   realptr->nick);
			buf[0] = '\0';
			strlcat(pstr, "-clear ", sizeof(pstr));

			break;
		}
		else
		{
			notice(n_NickServ, lptr->nick,
				   "Invalid flag: %s",
				   av[ii]);
		}
	}

	if (*buf)
	{
		buf[strlen(buf) - 2] = '\0';
		notice(n_NickServ, lptr->nick,
			   "The flags (%s) have been added to [\002%s\002]",
			   buf,
			   realptr->nick);
	}

	RecordCommand("%s: %s!%s@%s FLAG [%s] %s",
				  n_NickServ,
				  lptr->nick,
				  lptr->username,
				  lptr->hostname,
				  realptr->nick,
				  pstr);
} /* n_flag() */

#endif /* EMPOWERADMINS */

/*
 */
static void n_fixts(struct Luser *lptr, int ac, char **av)
{
	int tsdelta = 0;
	time_t now = 0;
	char dMsg[] = "Detected nickname \002%s\002 with TS %d "
				  "below TS_MAX_DELTA %d";
	struct Luser *ouser = NULL;

	if (ac < 2)
		tsdelta = MaxTSDelta;
	else
		tsdelta = atoi(av[1]);

	now = current_ts;

	/* Be paranoid */
	if (tsdelta <= 0)
	{
		notice(n_NickServ, lptr->nick,
			   "Wrong TS_MAX_DELTA specified, using default of 8w");
		tsdelta = 4838400; /* 8 weeks */
	}

	for (ouser = ClientList; ouser; ouser = ouser->next)
	{
		if ((now - ouser->since) >= tsdelta)
		{
			SendUmode(OPERUMODE_Y, dMsg, ouser->nick, ouser->since, tsdelta);
			notice(n_NickServ, lptr->nick, dMsg, ouser->nick, ouser->since,
				   tsdelta);
			putlog(LOG1, "%s: Bogus TS nickname: [%s] (TS=%d)",
				   n_NickServ, ouser->nick, ouser->since);

			collide(ouser->nick, 0);
			notice(n_NickServ, lptr->nick,
				   "The nickname [\002%s\002] has been collided",
				   ouser->nick);
		}
	}
}

/*
 * Disables the LINK option for the nickname    -kamen
 */
static void n_set_nolink(struct Luser *lptr, struct NickInfo *nptr, int
		ac, char **av)
{
	if (ac < 4)
	{
		notice(n_NickServ, lptr->nick,
			   "Disable linking to [\002%s\002] is [\002%s\002]",
			   nptr->nick,
			   (nptr->flags & NS_NOLINK) ? "ON" : "OFF");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET %s NOLINK %s",
				  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
				  nptr->nick, StrToupper(av[3]));

	if (!irccmp(av[3], "ON"))
	{
		nptr->flags |= NS_NOLINK;
		notice(n_NickServ, lptr->nick,
			   "Disable linking to [\002%s\002] is now [\002ON\002]",
			   nptr->nick);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		nptr->flags &= ~NS_NOLINK;
		notice(n_NickServ, lptr->nick,
			   "Disable linking to [\002%s\002] is now [\002OFF\002]",
			   nptr->nick);
		return;
	}

	/* user gave an unknown param */
	notice(n_NickServ, lptr->nick,
		   "Syntax: \002SET <nickname> NOLINK {ON|OFF}\002");
	notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "SET NOLINK");
} /* n_set_nolink() */


/*
 * Prevents a nickname from being added to channel access lists
 */
static void n_set_noadd(struct Luser *lptr, struct NickInfo *nptr, int
                ac, char **av)
{
        if (ac < 4)
        {
                notice(n_NickServ, lptr->nick,
                           "Disable adding to channel lists is [\002%s\002]",
                           nptr->nick,
                           (nptr->flags & NS_NOADD) ? "ON" : "OFF");
                return;
        }

        RecordCommand("%s: %s!%s@%s SET %s NOADD %s",
                                  n_NickServ, lptr->nick, lptr->username, lptr->hostname,
                                  nptr->nick, StrToupper(av[3]));

        if (!irccmp(av[3], "ON"))
        {
                nptr->flags |= NS_NOADD;
                notice(n_NickServ, lptr->nick,
                           "Disable adding to channel lists is now [\002ON\002]",
                           nptr->nick);
                return;
        }

        if (!irccmp(av[3], "OFF"))
        {
                nptr->flags &= ~NS_NOADD;
                notice(n_NickServ, lptr->nick,
                           "Disable adding to channel lists is now [\002OFF\002]",
                           nptr->nick);
                return;
        }

        /* user gave an unknown param */
        notice(n_NickServ, lptr->nick,
                   "Syntax: \002SET <nickname> NOADD {ON|OFF}\002");
        notice(n_NickServ, lptr->nick, ERR_MORE_INFO, n_NickServ, "SET NOADD");
} /* n_set_noadd() */

#endif /* NICKSERVICES */
