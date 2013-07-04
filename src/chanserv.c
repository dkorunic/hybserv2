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
#include "data.h"
#include "conf.h"
#include "config.h"
#include "dcc.h"
#include "err.h"
#include "hash.h"
#include "helpserv.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "memoserv.h"
#include "misc.h"
#include "mystring.h"
#include "nickserv.h"
#include "operserv.h"
#include "server.h"
#include "seenserv.h"
#include "settings.h"
#include "sock.h"
#include "timestr.h"
#include "sprintf_irc.h"

#if defined NICKSERVICES && defined CHANNELSERVICES

/* hash table of registered channels */
struct ChanInfo *chanlist[CHANLIST_MAX];

static void cs_joinchan(struct ChanInfo *);
static void cs_CheckChan(struct ChanInfo *, struct Channel *);
static void cs_SetTopic(struct Channel *, char *);
static int ChangeChanPass(struct ChanInfo *, char *);
static void AddChan(struct ChanInfo *);
static struct ChanInfo *MakeChan(void);
static void AddFounder(struct Luser *, struct ChanInfo *);
static int DelAccess(struct ChanInfo *, struct Luser *, char *,
                     struct NickInfo *);
static int AddAkick(struct ChanInfo *, struct Luser *, char *, char *, long);
static int DelAkick(struct ChanInfo *, char *);
/* static int IsFounder(struct Luser *, struct ChanInfo *); */
static int GetAccess(struct ChanInfo *, struct Luser *);
static struct ChanAccess *OnAccessList(struct ChanInfo *, char *,
			                                       struct NickInfo *);
static struct AutoKick *OnAkickList(struct ChanInfo *, char *);
static void chanopsnotice(struct Channel *cptr, char* msg);

/* default access levels for new channels */
static int DefaultAccess[CA_SIZE] = {
                                        -1,          /* CA_AUTODEOP */
                                        5,           /* CA_AUTOVOICE */
                                        8,           /* CA_CMDVOICE */
                                        5,           /* CA_ACCESS */
                                        5,           /* CA_CMDINVITE */
#ifdef HYBRID7_HALFOPS
                                        /* Default access for halfop and cmdhalfop -Janos */
                                        8,           /* CA_AUTOHALFOP */
                                        10,          /* CA_CMDHALFOP */
#endif /* HYBRID7_HALFOPS */
                                        10,          /* CA_AUTOOP */
                                        10,          /* CA_CMDOP */
#ifndef HYBRID7
                                        10,          /* CA_CMDUNBAN */
#else
                                        8,           /* CA_CMDUNBAN */
#endif /* HYBRID7 */
                                        15,          /* CA_AKICK */
                                        20,          /* CA_CMDCLEAR */
                                        25,          /* CA_SET */
                                        40,          /* CA_SUPEROP */
                                        50           /* CA_FOUNDER */
                                    };

static void c_help(struct Luser *, struct NickInfo *, int, char **);
static void c_register(struct Luser *, struct NickInfo *, int, char **);
static void c_drop(struct Luser *, struct NickInfo *, int, char **);
static void c_identify(struct Luser *, struct NickInfo *, int, char **);

static void c_access(struct Luser *, struct NickInfo *, int, char **);
static void c_access_add(struct Luser *, struct NickInfo *, int, char **);
static void c_access_del(struct Luser *, struct NickInfo *, int, char **);
static void c_access_list(struct Luser *, struct NickInfo *, int, char **);

static void c_akick(struct Luser *, struct NickInfo *, int, char **);
static void c_akick_add(struct Luser *, struct NickInfo *, int, char **);
static void c_akick_del(struct Luser *, struct NickInfo *, int, char **);
static void c_akick_list(struct Luser *, struct NickInfo *, int, char **);

static void c_delme(struct Luser *lptr, struct NickInfo *, int ac, char **av);

static void c_list(struct Luser *, struct NickInfo *, int, char **);
static void c_level(struct Luser *, struct NickInfo *, int, char **);

static void c_set(struct Luser *, struct NickInfo *, int, char **);
static void c_set_topiclock(struct Luser *, struct NickInfo *, int, char **);
static void c_set_private(struct Luser *, struct NickInfo *, int, char **);
static void c_set_verbose(struct Luser *, struct NickInfo *, int, char **);
static void c_set_secure(struct Luser *, struct NickInfo *, int, char **);
static void c_set_secureops(struct Luser *, struct NickInfo *, int, char **);
static void c_set_splitops(struct Luser *, struct NickInfo *, int, char **);
static void c_set_restricted(struct Luser *, struct NickInfo *, int, char **);
static void c_set_forget(struct Luser *, struct NickInfo *, int, char **);
static void c_set_guard(struct Luser *, struct NickInfo *, int, char **);
static void c_set_password(struct Luser *, struct NickInfo *, int, char **);
static void c_set_founder(struct Luser *, struct NickInfo *, int, char **);
static void c_set_successor(struct Luser *, struct NickInfo *, int, char **);
static void c_set_mlock(struct Luser *, struct NickInfo *, int, char **);
static void c_set_topic(struct Luser *, struct NickInfo *, int, char **);
static void c_set_entrymsg(struct Luser *, struct NickInfo *, int, char **);
static void c_set_email(struct Luser *, struct NickInfo *, int, char **);
static void c_set_url(struct Luser *, struct NickInfo *, int, char **);
static void c_set_expirebans(struct Luser *, struct NickInfo *, int, char **);
static void c_set_comment(struct Luser *, struct NickInfo *, int, char **);
#ifdef PUBCOMMANDS
static void c_set_pubcommands(struct Luser *, struct NickInfo *, int, char **);
#endif

static void c_modes(struct Luser *, struct NickInfo *, int, char **);
static void c_cycle(struct Luser *, struct NickInfo *, int, char **);
static void c_invite(struct Luser *, struct NickInfo *, int, char **);
static void c_op(struct Luser *, struct NickInfo *, int, char **);
#ifdef HYBRID7_HALFOPS
static void c_hop(struct Luser *, struct NickInfo *, int, char **);
#endif /* HYBRID7_HALFOPS */
static void c_voice(struct Luser *, struct NickInfo *, int, char **);
static void c_unban(struct Luser *, struct NickInfo *, int, char **);
static void c_info(struct Luser *, struct NickInfo *, int, char **);

static void c_clear(struct Luser *, struct NickInfo *, int, char **);
static void c_clear_ops(struct Luser *, struct NickInfo *, int, char **);
#ifdef HYBRID7_HALFOPS
static void c_clear_hops(struct Luser *, struct NickInfo *, int, char **);
#endif /* HYBRID7_HALFOPS */
static void c_clear_voices(struct Luser *, struct NickInfo *, int, char **);
static void c_clear_modes(struct Luser *, struct NickInfo *, int, char **);
#ifdef GECOSBANS
static void c_clear_gecos_bans(struct Luser *, struct NickInfo *, int, char **);
#endif /* GECOSBANS */
static void c_clear_bans(struct Luser *, struct NickInfo *, int, char **);
static void c_clear_users(struct Luser *, struct NickInfo *, int, char **);
/* static void c_clear_all(struct Luser *, struct NickInfo *, int, char **); */

#ifdef EMPOWERADMINS
static void c_forbid(struct Luser *, struct NickInfo *, int, char **);
static void c_unforbid(struct Luser *, struct NickInfo *, int, char **);
static void c_setpass(struct Luser *, struct NickInfo *, int, char **);
static void c_status(struct Luser *, struct NickInfo *, int, char **);
static void c_forget(struct Luser *, struct NickInfo *, int, char **);
static void c_noexpire(struct Luser *, struct NickInfo *, int, char **);
static void c_clearnoexp(struct Luser *, struct NickInfo *, int, char
                            **);
static void c_fixts(struct Luser *, struct NickInfo *, int, char **);
static void c_resetlevels(struct Luser *, struct NickInfo *, int, char
                          **);
#endif /* EMPOWERADMINS */

/* main ChanServ commands */
static struct Command chancmds[] =
    {
	    { "HELP", c_help, LVL_NONE
	    },
	    { "REGISTER", c_register, LVL_IDENT },
	    { "DROP", c_drop, LVL_IDENT },
	    { "IDENTIFY", c_identify, LVL_IDENT },
	    { "ACCESS", c_access, LVL_IDENT },
	    { "AKICK", c_akick, LVL_IDENT },
	    { "LIST", c_list, LVL_NONE },
	    { "LEVEL", c_level, LVL_IDENT },
	    { "SET", c_set, LVL_IDENT },
	    { "CYCLE", c_cycle, LVL_IDENT },
	    { "MODES", c_modes, LVL_IDENT },
	    { "INVITE", c_invite, LVL_IDENT },
	    { "OP", c_op, LVL_IDENT },
#ifdef HYBRID7_HALFOPS
	    { "HALFOP", c_hop, LVL_IDENT },
#endif /* HYBRID7_HALFOPS */
	    { "VOICE", c_voice, LVL_IDENT },
	    { "UNBAN", c_unban, LVL_IDENT },
	    { "INFO", c_info, LVL_NONE },
	    { "CLEAR", c_clear, LVL_IDENT },
	    { "FIXTS" , c_fixts, LVL_ADMIN },
	    { "DELME", c_delme, LVL_IDENT },
#ifdef EMPOWERADMINS
	    { "FORBID", c_forbid, LVL_ADMIN },
	    { "UNFORBID", c_unforbid, LVL_ADMIN },
	    { "SETPASS", c_setpass, LVL_ADMIN },
	    { "STATUS", c_status, LVL_ADMIN },
	    { "FORGET", c_forget, LVL_ADMIN },
	    { "NOEXPIRE", c_noexpire, LVL_ADMIN },
	    { "CLEARNOEXP", c_clearnoexp, LVL_ADMIN },
	    { "RESETLEVELS", c_resetlevels, LVL_ADMIN },
#endif
	    { 0, 0, 0 }
    };

/* sub-commands for ChanServ ACCESS */
static struct Command accesscmds[] =
    {
	    { "ADD", c_access_add, LVL_NONE
	    },
	    { "DEL", c_access_del, LVL_NONE },
	    { "LIST", c_access_list, LVL_NONE },
	    { 0, 0, 0 }
    };

/* sub-commands for ChanServ AKICK */
static struct Command akickcmds[] =
    {
	    { "ADD", c_akick_add, LVL_NONE },
	    { "DEL", c_akick_del, LVL_NONE },
	    { "LIST", c_akick_list, LVL_NONE },
	    { 0, 0, 0 }
    };

/* sub-commands for ChanServ SET */
static struct Command setcmds[] =
    {
	    { "TOPICLOCK", c_set_topiclock, LVL_NONE
	    },
	    { "TLOCK", c_set_topiclock, LVL_NONE },
	    { "PRIVATE", c_set_private, LVL_NONE },
	    { "VERBOSE", c_set_verbose, LVL_NONE },
	    { "SECURE", c_set_secure, LVL_NONE },
	    { "SECUREOPS", c_set_secureops, LVL_NONE },
	    { "SPLITOPS", c_set_splitops, LVL_NONE },
	    { "RESTRICTED", c_set_restricted, LVL_NONE },
	    { "FORGET", c_set_forget, LVL_NONE },
	    { "GUARD", c_set_guard, LVL_NONE },
	    { "PASSWORD", c_set_password, LVL_NONE },
	    { "NEWPASS", c_set_password, LVL_NONE },
	    { "FOUNDER", c_set_founder, LVL_NONE },
	    { "SUCCESSOR", c_set_successor, LVL_NONE },
	    { "MLOCK", c_set_mlock, LVL_NONE },
	    { "MODELOCK", c_set_mlock, LVL_NONE },
	    { "TOPIC", c_set_topic, LVL_NONE },
	    { "ENTRYMSG", c_set_entrymsg, LVL_NONE },
	    { "ONJOIN", c_set_entrymsg, LVL_NONE },
	    { "EMAIL", c_set_email, LVL_NONE },
	    { "MAIL", c_set_email, LVL_NONE },
	    { "URL", c_set_url, LVL_NONE },
	    { "WEBSITE", c_set_url, LVL_NONE },
	    { "EXPIREBANS", c_set_expirebans, LVL_NONE },
#ifdef PUBCOMMANDS
	    { "PUBCOMMANDS", c_set_pubcommands, LVL_NONE },
#endif
	    { "COMMENT", c_set_comment, LVL_NONE },
	    { 0, 0, 0 }
    };

/* sub-commands for ChanServ CLEAR */
static struct Command clearcmds[] =
    {
	    { "OPS", c_clear_ops, LVL_NONE
	    },
#ifdef HYBRID7_HALFOPS
	    /* Allow clear halfops for hybrid7, too -Janos */
	    { "HALFOPS", c_clear_hops, LVL_NONE },
#endif /* HYBRID7_HALFOPS */
	    { "VOICES", c_clear_voices, LVL_NONE },
	    { "MODES", c_clear_modes, LVL_NONE },
	    { "BANS", c_clear_bans, LVL_NONE },
#ifdef GECOSBANS
	    { "GECOSBANS", c_clear_gecos_bans, LVL_NONE },
#endif /* GECOSBANS */
	    { "USERS", c_clear_users, LVL_NONE },
	    { "ALL", c_clear_all, LVL_NONE },
	    { 0, 0, 0 }
    };

typedef struct
{
	int level;
	char *cmd;
	char *desc;
}
AccessInfo;

static AccessInfo accessinfo[] = {
                                     { CA_AUTODEOP, "AUTODEOP", "Automatic deop/devoice" },
                                     { CA_AUTOVOICE, "AUTOVOICE", "Automatic voice" },
                                     { CA_CMDVOICE, "CMDVOICE", "Use of command VOICE" },
                                     { CA_ACCESS, "ACCESS", "Allow ACCESS modification" },
                                     { CA_CMDINVITE, "CMDINVITE", "Use of command INVITE" },
#ifdef HYBRID7_HALFOPS
                                     /* Halfop help indices -Janos */
                                     { CA_AUTOHALFOP, "AUTOHALFOP", "Automatic halfop"},
                                     { CA_CMDHALFOP, "CMDHALFOP", "Use of command HALFOP"},
#endif /* HYBRID7_HALFOPS */
                                     { CA_AUTOOP, "AUTOOP", "Automatic op" },
                                     { CA_CMDOP, "CMDOP", "Use of command OP" },
                                     { CA_CMDUNBAN, "CMDUNBAN", "Use of command UNBAN" },
                                     { CA_AKICK, "AUTOKICK", "Allow AKICK modification" },
                                     { CA_CMDCLEAR, "CMDCLEAR", "Use of command CLEAR" },
                                     { CA_SET, "SET", "Modify channel SETs" },
                                     { CA_SUPEROP, "SUPEROP", "All of the above" },
                                     { CA_FOUNDER, "FOUNDER", "Full access to the channel" },
                                     { 0, 0, 0 }
                                 };

/*
cs_process()
  Process command coming from 'nick' directed towards n_ChanServ
*/
void cs_process(char *nick, char *command)
{
	int acnt;
	char **arv;
	struct Command *cptr;
	struct Luser *lptr;
	struct NickInfo *nptr;
	struct ChanInfo *chptr;

	if (!command || !(lptr = FindClient(nick)))
		return;

	if (Network->flags & NET_OFF)
	{
		notice(n_ChanServ, lptr->nick,
		       "Services are currently \002disabled\002");
		return;
	}

	acnt = SplitBuf(command, &arv);
	if (acnt == 0)
	{
		MyFree(arv);
		return;
	}

	cptr = GetCommand(chancmds, arv[0]);

	if (!cptr || (cptr == (struct Command *) -1))
	{
		notice(n_ChanServ, lptr->nick,
		       "%s command [%s]",
		       (cptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
		       arv[0]);
		MyFree(arv);
		return;
	}

	nptr = FindNick(lptr->nick);

	if ((nptr == NULL) &&
	        (cptr->level != LVL_NONE))
	{
		/* the command requires a registered nickname */

		notice(n_ChanServ, lptr->nick, "Your nickname is not registered");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_NickServ,
		       "REGISTER");
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
		notice(n_ChanServ, lptr->nick, "Unknown command [%s]", arv[0]);
		MyFree(arv);
		return;
	}

	if (nptr)
	{
		if (nptr->flags & NS_FORBID)
		{
			notice(n_ChanServ, lptr->nick,
			       "Cannot execute commands for forbidden nicknames");
			MyFree(arv);
			return;
		}

		if (cptr->level != LVL_NONE)
		{
			if (!(nptr->flags & NS_IDENTIFIED))
			{
				notice(n_ChanServ, lptr->nick,
				       "Password identification is required for [\002%s\002]",
				       cptr->cmd);
				notice(n_ChanServ, lptr->nick,
				       "Type \002/MSG %s IDENTIFY <password>\002 and retry",
				       n_NickServ);
				MyFree(arv);
				return;
			}
		}
	} /* if (nptr) */

	if ((chptr = FindChan(acnt >= 2 ? arv[1] : NULL)))
	{
		/* Complain only if it not admin-level command -kre */
		if (!IsValidAdmin(lptr))
		{
			if (chptr->flags & CS_FORBID)
			{
				notice(n_ChanServ, lptr->nick,
				       "Cannot execute commands for forbidden channels");
				MyFree(arv);
				return;
			}
			else
				if (chptr->flags & CS_FORGET)
				{
					notice(n_ChanServ, lptr->nick,
					       "Cannot execute commands for forgotten channels");
					MyFree(arv);
					return;
				}
		}
	}

	/* call cptr->func to execute command */
	(*cptr->func)(lptr, nptr, acnt, arv);

	MyFree(arv);

	return;
} /* cs_process() */

/*
cs_loaddata()
  Load ChanServ database - return 1 if successful, -1 if not, and -2
if the errors are unrecoverable
*/

int
cs_loaddata(void)
{
	FILE *fp;
	char line[MAXLINE + 1], **av;
	char *keyword;
	int ac, ret = 1, cnt;
	struct ChanInfo *cptr = NULL;

	if (!(fp = fopen(ChanServDB, "r")))
	{
		/* ChanServ data file doesn't exist */
		return -1;
	}

	cnt = 0;
	/* load data into list */
	while (fgets(line, sizeof(line), fp))
	{
		++cnt;

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
				      ChanServDB,
				      cnt);
				ret = -2;
				MyFree(av);
				continue;
			}

			/* check if there is no channel associated with data */
			if (!cptr)
			{
				fatal(1, "%s:%d No channel associated with data",
				      ChanServDB,
				      cnt);
				if (ret > 0)
					ret = -1;
				MyFree(av);
				continue;
			}

			keyword = av[0] + 2;
			if (!ircncmp("PASS", keyword, 4))
			{
				if (!cptr->password)
					cptr->password = MyStrdup(av[1]);
				else
				{
					fatal(1,
					      "%s:%d ChanServ entry for [%s] has multiple PASS lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("FNDR", keyword, 4))
			{
				if (!cptr->founder && !(cptr->flags & (CS_FORBID | CS_FORGET)))
				{
					struct NickInfo *ni = FindNick(av[1]);
					struct NickInfo *master;

					if (!ni)
					{
						fatal(1, "%s:%d Founder nickname [%s] for channel [%s] is not registered (FATAL)",
						      ChanServDB,
						      cnt,
						      av[1],
						      cptr->name);
						ret = -2;
					}
					else if ((master = GetMaster(ni)))
					{
						cptr->founder = MyStrdup(master->nick);
						AddFounderChannelToNick(&master, cptr);
					}
					else
					{
						fatal(1, "%s:%d Unable to find master nickname for founder [%s] on channel [%s] (FATAL)",
						      ChanServDB,
						      cnt,
						      ni->nick,
						      cptr->name);
						ret = -2;
					}
					if (ac > 2)
						cptr->last_founder_active = atol(av[2]);
				}
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple FNDR lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp(keyword, "SUCCESSOR", 9))
			{
				if (!cptr->successor)
				{
					struct NickInfo  *ni = FindNick(av[1]);

					if (!ni)
					{
						fatal(1, "%s:%d Successor nickname [%s] for channel [%s] is not registered (ignoring)",
						      ChanServDB,
						      cnt,
						      av[1],
						      cptr->name);
						if (ret > 0)
							ret = -1;
					}
					else
						cptr->successor = MyStrdup(ni->nick);
				}
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple SUCCESSOR lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
				if (ac > 2)
					cptr->last_successor_active = atol(av[2]);
			}
			else if (!ircncmp("ACCESS", keyword, 6))
			{
				if (ac < 3)
				{
					fatal(1, "%s:%d Invalid database format (FATAL)",
					      ChanServDB,
					      cnt);
					ret = -2;
				}
				else
				{
					struct NickInfo *nptr;
					time_t created = 0, last_used = 0;

					if (ac > 4)
					{
						created = atol(av[3]);
						last_used = atol(av[4]);
					}

					if ((nptr = FindNick(av[1])))
						AddAccess(cptr, NULL, NULL, nptr, atoi(av[2]),
						          created, last_used, ((ac > 5) ? av[5]: "*Unknown*"));
					else
						AddAccess(cptr, NULL, av[1], NULL, atoi(av[2]),
						          created, last_used, ((ac > 5) ? av[5]: "*Unknown*"));
				}
			}
			else if (!ircncmp("AKICK", keyword, 5))
			{
				if (ac < 3)
				{
					fatal(1, "%s:%d Invalid database format (FATAL)",
					      ChanServDB,
					      cnt);
					ret = -2;
				}
				else
				{
					if (ac == 4)
						AddAkick(cptr, NULL, av[2], av[3] + 1, atol(av[1]));
					else
						if (ac == 3)
							AddAkick(cptr, NULL, av[1], av[2] + 1, 0);
				}
			}
			else if (!ircncmp("ALVL", keyword, 4))
			{
				/* channel access levels */
				if (ac != (CA_SIZE + 1))
				{
#ifndef HYBRID7
					fatal(1, "%s:%d Invalid database format (FATAL)",
					      ChanServDB,
					      cnt);
					ret = -2;
#else

					SetDefaultALVL(cptr);
					fatal(1, "%s:%d No access level list for "
						  "registered channel [%s] (using default)",
						  ChanServDB, cnt, cptr->name);
#endif /* HYBRID7 */

				}
				else if (!cptr->access_lvl)
				{
					int ii;

					cptr->access_lvl = (int *) MyMalloc(sizeof(int) * CA_SIZE);
					for (ii = 0; ii < CA_SIZE; ii++)
						cptr->access_lvl[ii] = atoi(av[ii + 1]);
				}
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple ALVL lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("FORBIDBY", keyword, 8))
			{
				if (!cptr->forbidby)
					cptr->forbidby = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple FORBIDBY lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("FORBIDREASON", keyword, 12))
			{
				if (!cptr->forbidreason)
					cptr->forbidreason = MyStrdup(av[1] + 1);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple FORBIDREASON lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("TOPIC", keyword, 5))
			{
				if (!cptr->topic)
					cptr->topic = MyStrdup(av[1] + 1);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple TOPIC lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("LIMIT", keyword, 5))
			{
				if (!cptr->limit)
					cptr->limit = atoi(av[1]);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple LIMIT lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("KEY", keyword, 3))
			{
				if (!cptr->key)
					cptr->key = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple KEY lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("EXPIREBANS", keyword, 10))
			{
				if (!cptr->expirebans)
					cptr->expirebans = atol(av[1]);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple EXPIREBANS lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
#ifdef DANCER
			else if (!strncasecmp("FORWARD", keyword, 3))
			{
				if (!cptr->forward)
					cptr->forward = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple FORWARD lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
#endif /* DANCER */
			else if (!ircncmp("MON", keyword, 3))
			{
				if (!cptr->modes_on)
					cptr->modes_on = atoi(av[1]);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple MON lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("MOFF", keyword, 4))
			{
				if (!cptr->modes_off)
					cptr->modes_off = atoi(av[1]);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple MOFF lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("ENTRYMSG", keyword, 8))
			{
				if (!cptr->entrymsg)
					cptr->entrymsg = MyStrdup(av[1] + 1);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple ENTRYMSG lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("EMAIL", keyword, 5))
			{
				if (!cptr->email)
					cptr->email = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple EMAIL lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("URL", keyword, 3))
			{
				if (!cptr->url)
					cptr->url = MyStrdup(av[1]);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple URL lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
			else if (!ircncmp("COMMENT", keyword, 7))
			{
				if (!cptr->comment)
					cptr->comment = MyStrdup(av[1] + 1);
				else
				{
					fatal(1, "%s:%d ChanServ entry for [%s] has multiple COMMENT lines (using first)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}
			}
		}
		else
		{
			if (cptr)
			{
				if (!cptr->access_lvl)
				{
					SetDefaultALVL(cptr);

					fatal(1,
					      "%s:%d No access level list for registered channel [%s] (using default)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					if (ret > 0)
						ret = -1;
				}

				if (!cptr->password && !(cptr->flags & (CS_FORBID | CS_FORGET)))
				{
					/* the previous channel didn't have a PASS line */
					fatal(1, "%s:%d No founder password entry for registered channel [%s] (FATAL)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					ret = -2;
				}

				if (!cptr->founder && !(cptr->flags & (CS_FORBID | CS_FORGET)))
				{
					/* the previous channel didn't have a FNDR line */
					fatal(1, "%s:%d No founder nickname entry for registered channel [%s] (FATAL)",
					      ChanServDB,
					      cnt,
					      cptr->name);
					ret = -2;
				}

				/*
				 * we've come to a new channel entry, so add the last chan
				 * to the list before proceeding
				 */
				AddChan(cptr);
			} /* if (cptr) */

			/*
			 * make sure there are enough args on the line
			 * <channel> <flags> <time created> <last used>
			 */
			if (ac < 4)
			{
				fatal(1, "%s:%d Invalid database format (FATAL)",
				      ChanServDB,
				      cnt);
				ret = -2;
				cptr = NULL;
				MyFree(av);
				continue;
			}

			if (av[0][0] != '#')
			{
				fatal(1, "%s:%d Invalid channel name [%s] (skipping)",
				      ChanServDB,
				      cnt,
				      av[0]);
				if (ret > 0)
					ret = -1;
				cptr = NULL;
				MyFree(av);
				continue;
			}

			cptr = MakeChan();
			cptr->name = MyStrdup(av[0]);
			cptr->flags = atoi(av[1]);
			cptr->created = atoi(av[2]);
			cptr->lastused = atoi(av[3]);
			cptr->flags &= ~CS_DELETE;
		}

		MyFree(av);
	} /* while */

	if (cptr)
	{
		if (!cptr->access_lvl && !(cptr->flags & (CS_FORBID | CS_FORGET)))
		{
			SetDefaultALVL(cptr);

			fatal(1,
			      "%s:%d No access level list for registered channel [%s] (using default)",
			      ChanServDB,
			      cnt,
			      cptr->name);
			if (ret > 0)
				ret = -1;
		}

		if ((cptr->password && cptr->founder) ||
		        (cptr->flags & (CS_FORBID | CS_FORGET)))
		{
			/*
			 * cptr has a founder and password, so it can be added to
			 * the table
			 */
			AddChan(cptr);
		}
		else
		{
			if (!cptr->password && !(cptr->flags & (CS_FORBID | CS_FORGET)))
			{
				fatal(1, "%s:%d No founder password entry for registered channel [%s] (FATAL)",
				      ChanServDB,
				      cnt,
				      cptr->name);
				ret = -2;
			}

			if (!cptr->founder && !(cptr->flags & (CS_FORBID | CS_FORGET)))
			{
				fatal(1, "%s:%d No founder nickname entry for registered channel [%s] (FATAL)",
				      ChanServDB,
				      cnt,
				      cptr->name);
				ret = -2;
			}
		}
	} /* if (cptr) */

	fclose(fp);

	return (ret);
} /* cs_loaddata */

/*
cs_join()
  Have ChanServ join a channel
*/

void
cs_join(struct ChanInfo *chanptr)

{
	int goodjoin;
	char sendstr[MAXLINE + 1], **av;
	struct Channel *chptr;
	time_t chants;

	if (!chanptr)
		return;

	if (cs_ShouldBeOnChan(chanptr))
		goodjoin = 1;
	else
		goodjoin = 0;

	chptr = FindChannel(chanptr->name);

	if (goodjoin)
	{
		/*
		 * chptr might be null, so make sure we check before
		 * using it
		 */
		if (chptr)
			chants = chptr->since;
		else
			chants = current_ts;

		ircsprintf(sendstr, ":%s SJOIN %ld %s + :@%s\r\n", Me.name,
		           (long) chants, chanptr->name, n_ChanServ);
		toserv("%s", sendstr);

		SplitBuf(sendstr, &av);

		/*
		 * The previous call to FindChannel() may have
		 * been null, so reassign chptr to the channel
		 * structure if the channel was just created.
		 * This way, we will always pass a valid pointer
		 * to cs_CheckChan()
		 */
		chptr = AddChannel(av, 0, (char **) NULL);

		MyFree(av);
	}

	/* Check that the right ops are opped etc */
	cs_CheckChan(chanptr, chptr);
} /* cs_join() */

/*
cs_joinchan()
 Similar to cs_join, except don't call cs_CheckChan() on the
channel.
*/

static void
cs_joinchan(struct ChanInfo *chanptr)

{
	char sendstr[MAXLINE + 1], **av;
	struct Channel *chptr;

	chptr = FindChannel(chanptr->name);

	ircsprintf(sendstr, ":%s SJOIN %ld %s + :@%s\r\n",
	           Me.name, chptr ? (long) chptr->since : (long) current_ts,
	           chanptr->name, n_ChanServ);
	toserv("%s", sendstr);

	SplitBuf(sendstr, &av);
	AddChannel(av, 0, (char **) NULL);
	MyFree(av);
} /* cs_joinchan() */

/*
cs_join_ts_minus_1()
  Have ChanServ join a channel with TS - 1
*/

void
cs_join_ts_minus_1(struct ChanInfo *chanptr)

{
	char sendstr[MAXLINE + 1], **av;
	struct Channel *cptr = FindChannel(chanptr->name);
	int ac;

	ircsprintf(sendstr,
	           ":%s SJOIN %ld %s + :@%s\r\n",
	           Me.name, cptr ? (long) (cptr->since - 1) : (long) current_ts,
	           cptr->name, n_ChanServ);
	toserv("%s", sendstr);

	ac = SplitBuf(sendstr, &av);
	s_sjoin(ac, av);

	MyFree(av);

	/* Check that the right ops are opped etc */
	cs_CheckChan(chanptr, cptr);
} /* cs_join_ts_minus_1() */

/*
cs_part()
  Have ChanServ part 'chptr'
*/

void
cs_part(struct Channel *chptr)

{
	if (!chptr)
		return;

	toserv(":%s PART %s\r\n", n_ChanServ, chptr->name);
	RemoveFromChannel(chptr, Me.csptr);
} /* cs_part() */

/*
cs_CheckChan()
  Called right after ChanServ joins a channel, to check that the right
modes get set, the right people are opped, etc
*/

static void
cs_CheckChan(struct ChanInfo *cptr, struct Channel *chptr)

{
	struct ChannelUser *tempu;
	char modes[MAXLINE + 1]; /* mlock modes to set */

	if (!cptr || !chptr)
		return;

	if (cptr->flags & CS_FORBID)
	{
		char knicks[MAXLINE + 1]; /* nicks to kick */
		int jj = 1;
		knicks[0] = '\0';

		for (tempu = chptr->firstuser; tempu; tempu = tempu->next)
		{
			if (FindService(tempu->lptr))
				continue;

			if (jj * (NICKLEN + 1) >= sizeof(knicks))
			{
				SetModes(n_ChanServ, 0, 'o', chptr, knicks);
				knicks[0] = '\0';
				jj = 1;
			}

			strlcat(knicks, tempu->lptr->nick, sizeof(knicks));
			strlcat(knicks, " ", sizeof(knicks));
			++jj;
		}

		SetModes(n_ChanServ, 0, 'o', chptr, knicks);

		/*
		 * Have ChanServ join the channel to enforce the +i
		 */
		if (!IsChannelMember(chptr, Me.csptr))
			cs_joinchan(cptr);

		toserv(":%s MODE %s +i\r\n", n_ChanServ, cptr->name);
		UpdateChanModes(Me.csptr, n_ChanServ, chptr, "+i");
		KickBan(0, n_ChanServ, chptr, knicks, "Forbidden Channel");

		return;
	}

	/* cptr->lastused = current_ts; */

	if ((cptr->flags & CS_SECUREOPS) || (cptr->flags & CS_RESTRICTED))
	{
		/* SECUREOPS is set - deop all non autoops */
		char dopnicks[MAXLINE + 1];
		int jj = 1;
		dopnicks[0] = '\0';

		for (tempu = chptr->firstuser; tempu; tempu = tempu->next)
		{
			if (FindService(tempu->lptr))
				continue;

			if ((tempu->flags & CH_OPPED) &&
			        !HasAccess(cptr, tempu->lptr, CA_AUTOOP))
			{
				if (jj * (NICKLEN + 1) >= sizeof(dopnicks))
				{
					SetModes(n_ChanServ, 0, 'o', chptr, dopnicks);
					dopnicks[0] = '\0';
					jj = 1;
				}

				strlcat(dopnicks, tempu->lptr->nick, sizeof(dopnicks));
				strlcat(dopnicks, " ", sizeof(dopnicks));
				++jj;
			}
		}

		SetModes(n_ChanServ, 0, 'o', chptr, dopnicks);
	}

	if ((cptr->flags & CS_RESTRICTED))
	{
		/* channel is restricted - kickban all non-ops */
		char kbnicks[MAXLINE + 1];
		int jj = 1;
		kbnicks[0] = '\0';

		for (tempu = chptr->firstuser; tempu; tempu = tempu->next)
		{
			if (FindService(tempu->lptr))
				continue;

#ifdef EMPOWERADMINS_MORE
			if (IsValidAdmin(tempu->lptr))
				continue;
#endif

			if (!HasAccess(cptr, tempu->lptr, CA_CMDOP))
			{

				if (jj * (NICKLEN + 1) >= sizeof(kbnicks))
				{
					KickBan(1, n_ChanServ, chptr, kbnicks,
							"Restricted Channel");
					kbnicks[0] = '\0';
					jj = 1;
				}

				strlcat(kbnicks, tempu->lptr->nick, sizeof(kbnicks));
				strlcat(kbnicks, " ", sizeof(kbnicks));
				++jj;
			}
		}

		/*
		 * Have ChanServ join the channel to enforce the bans
		 */
		if (!IsChannelMember(chptr, Me.csptr))
			cs_joinchan(cptr);

		KickBan(1, n_ChanServ, chptr, kbnicks, "Restricted Channel");
	}

	strlcpy(modes, "+", sizeof(modes));
#ifdef DANCER

	if (cptr->modes_on || cptr->key || cptr->limit || cptr->forward)
#else

	if (cptr->modes_on || cptr->key || cptr->limit)
#endif /* DANCER */

	{
		if ((cptr->modes_on & MODE_S) &&
		        !(chptr->modes & MODE_S))
			strlcat(modes, "s", sizeof(modes));
		if ((cptr->modes_on & MODE_P) &&
		        !(chptr->modes & MODE_P))
			strlcat(modes, "p", sizeof(modes));
		if ((cptr->modes_on & MODE_N) &&
		        !(chptr->modes & MODE_N))
			strlcat(modes, "n", sizeof(modes));
		if ((cptr->modes_on & MODE_T) &&
		        !(chptr->modes & MODE_T))
			strlcat(modes, "t", sizeof(modes));
		if ((cptr->modes_on & MODE_M) &&
		        !(chptr->modes & MODE_M))
			strlcat(modes, "m", sizeof(modes));
#ifdef DANCER

		if ((cptr->modes_on & MODE_C) &&
		        !(chptr->modes & MODE_C))
			strlcat(modes, "c", sizeof(modes));
#endif /* DANCER */

		if ((cptr->modes_on & MODE_I) &&
		        !(chptr->modes & MODE_I))
			strlcat(modes, "i", sizeof(modes));
		if (cptr->limit)
			strlcat(modes, "l", sizeof(modes));
		if (cptr->key)
			strlcat(modes, "k", sizeof(modes));
#ifdef DANCER

		if (cptr->forward)
			strlcat(modes, "f", sizeof(modes));
#endif /* DANCER */ /* DANCER */

		if (cptr->limit)
		{
			char temp[MAXLINE + 1];

			ircsprintf(temp, "%s %ld", modes, cptr->limit);
			strlcpy(modes, temp, sizeof(modes));
		}
		if (cptr->key)
		{
			char temp[MAXLINE + 1];

			if (chptr->key[0] != '\0')
			{
				ircsprintf(temp, "-k %s", chptr->key);
				toserv(":%s MODE %s %s\r\n", n_ChanServ, chptr->name, temp);
				UpdateChanModes(Me.csptr, n_ChanServ, chptr, temp);
			}
			ircsprintf(temp, "%s %s", modes, cptr->key);
			strlcpy(modes, temp, sizeof(modes));
		}
#ifdef DANCER
		if (cptr->forward)
		{
			char temp[MAXLINE + 1];

			ircsprintf(temp, "%s %s", modes, cptr->forward);
			strlcpy(modes, temp, sizeof(modes));
		}
#endif /* DANCER */
	}
	if (modes[1])
	{
		toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
		UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
	}

	strlcpy(modes, "-", sizeof(modes));
	if (cptr->modes_off)
	{
		if ((cptr->modes_off & MODE_S) &&
		        (chptr->modes & MODE_S))
			strlcat(modes, "s", sizeof(modes));
		if ((cptr->modes_off & MODE_P) &&
		        (chptr->modes & MODE_P))
			strlcat(modes, "p", sizeof(modes));
		if ((cptr->modes_off & MODE_N) &&
		        (chptr->modes & MODE_N))
			strlcat(modes, "n", sizeof(modes));
		if ((cptr->modes_off & MODE_T) &&
		        (chptr->modes & MODE_T))
			strlcat(modes, "t", sizeof(modes));
		if ((cptr->modes_off & MODE_M) &&
		        (chptr->modes & MODE_M))
			strlcat(modes, "m", sizeof(modes));
#ifdef DANCER

		if ((cptr->modes_off & MODE_C) &&
		        (chptr->modes & MODE_C))
			strlcat(modes, "c", sizeof(modes));
#endif /* DANCER */

		if ((cptr->modes_off & MODE_I) &&
		        (chptr->modes & MODE_I))
			strlcat(modes, "i", sizeof(modes));
		if ((cptr->modes_off & MODE_L) &&
		        (chptr->limit))
			strlcat(modes, "l", sizeof(modes));
		if ((cptr->modes_off & MODE_K) &&
		        (chptr->key[0] != '\0'))
		{
			strlcat(modes, "k ", sizeof(modes));
			strlcat(modes, chptr->key, sizeof(modes));
		}
#ifdef DANCER
		if ((cptr->modes_off & MODE_F) &&
		        (chptr->forward))
		{
			strlcat(modes, "f ", sizeof(modes));
			strlcat(modes, chptr->forward, sizeof(modes));
		}
#endif /* DANCER */

	}
	if (modes[1])
	{
		toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
		UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
	}

	cs_SetTopic(FindChannel(cptr->name), cptr->topic);
} /* cs_CheckChan() */

/*
cs_SetTopic()
 Set the topic on the given channel to 'topic'
*/

static void
cs_SetTopic(struct Channel *chanptr, char *topic)

{
	struct ChanInfo *cptr = NULL;

	if (!chanptr || !topic)
		return;

	cptr = FindChan(chanptr->name);

	if (cs_ShouldBeOnChan(cptr))
	{
		/*
		 * ChanServ should already be on the channel - just set
		 * topic normally
		 */
		toserv(":%s TOPIC %s :%s\r\n", n_ChanServ, chanptr->name, topic);
	}
	else
	{
		/*
		 * Hybrid won't accept a TOPIC from a user unless they are
		 * on the channel - have ChanServ join and leave.
		 *
		 * Modifications to be sure all fits in linebuf of ircd. -kre
		 * However +ins supports topic burst -Janos
		 * It won't help if topiclen > ircdbuflen, that was original bug.
		 * Anyway, it is dealt with c_topic() code, too. :-) -kre
		 */
		toserv(":%s SJOIN %ld %s + :@%s\r\n", Me.name, chanptr->since,
		       chanptr->name, n_ChanServ);
		toserv(":%s TOPIC %s :%s\r\n", n_ChanServ, chanptr->name, topic);
		toserv(":%s PART %s\r\n", n_ChanServ, chanptr->name);
	}
} /* cs_SetTopic() */

/*
cs_CheckModes()
  Called after a mode change, to check if the mode is in conflict
with the mlock modes etc
*/

void
cs_CheckModes(struct Luser *source, struct ChanInfo *cptr,
              int isminus, int mode, struct Luser *lptr)

{
	int slev; /* user level for source */
	char modes[MAXLINE + 1];
	struct Channel *chptr;

	if (!cptr || !source)
		return;

	if ((cptr->flags & (CS_FORBID | CS_FORGET)))
		return;

	/*
	 * Allow OperServ and ChanServ to set any mode
	 */
	if ((source == Me.csptr) || (source == Me.osptr))
		return;

	/* founders can set any mode */
	if (IsFounder(source, cptr))
		return;

	if (!(chptr = FindChannel(cptr->name)))
		return;

	slev = GetAccess(cptr, source);

	if (mode == MODE_O)
	{
		int    ulev; /* user level for (de)opped nick */

		if (!lptr)
			return;

		ulev = GetAccess(cptr, lptr);
		if (isminus)
		{
			/*
			 * don't let someone deop a user with a greater user level,
			 * but allow a user to deop him/herself
			 */
			if ((lptr != source) &&
			        HasAccess(cptr, lptr, CA_AUTOOP) &&
			        (ulev >= slev))
			{
				if (HasFlag(lptr->nick, NS_NOCHANOPS))
					return;

				ircsprintf(modes, "+o %s", lptr->nick);
				toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
				UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
			}
		}
		else /* mode +o */
		{
			if (((cptr->flags & CS_SECUREOPS) &&
			        !HasAccess(cptr, lptr, CA_AUTOOP)) ||
			        (GetAccess(cptr, lptr) == cptr->access_lvl[CA_AUTODEOP]))
			{
				ircsprintf(modes, "-o %s", lptr->nick);
				toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
				UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
			}
			else if (HasFlag(lptr->nick, NS_NOCHANOPS))
			{
				ircsprintf(modes, "-o %s", lptr->nick);
				toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
				UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);

				notice(n_ChanServ, lptr->nick,
				       "You are not permitted to have channel op privileges");
				SendUmode(OPERUMODE_Y,
				          "Flagged user %s!%s@%s was opped on channel [%s] by %s",
				          lptr->nick, lptr->username, lptr->hostname, cptr->name,
				          source->nick);
			}
		}
		return;
	} /* if (mode == MODE_O) */

	if (mode == MODE_V)
	{
		if (!isminus)
		{
			/* autodeop people aren't allowed voice status either */
			if (GetAccess(cptr, lptr) == cptr->access_lvl[CA_AUTODEOP])
			{
				ircsprintf(modes, "-v %s", lptr->nick);
				toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
				UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
			}
		}

		return;
	} /* if (mode == MODE_V) */

#ifdef HYBRID7_HALFOPS
	/* Properly handle autodeop and halfop -Janos */
	if (mode == MODE_H)
	{
		if (!isminus)
		{
			/* Autodeop people aren't allowed halfop status either */
			if (GetAccess(cptr, lptr) == cptr->access_lvl[CA_AUTODEOP])
			{
				ircsprintf(modes, "-h %s", lptr->nick);
				toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
				UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
			}
		}
	} /* if (mode == MODE_H) */
#endif /* HYBRID7_HALFOPS */

	/*
	 * Check if the mode conflicts with any enforced modes for the
	 * channel
	 */
	if (cptr->modes_on && isminus)
	{
		modes[0] = '\0';
		if ((mode == MODE_S) &&
		        (cptr->modes_on & MODE_S))
#ifndef HYBRID

			ircsprintf(modes, "+s-p");
#else
			/* In hybrid +p and +s has another meaning -Janos */
			ircsprintf(modes, "+s");
#endif /* HYBRID7 */

		if ((mode == MODE_P) &&
		        (cptr->modes_on & MODE_P))
#ifndef HYBRID

			ircsprintf(modes, "+p-s");
#else
			/* In hybrid +p and +s has another meaning -Janos */
			ircsprintf(modes, "+p");
#endif /* HYBRID7 */

		if ((mode == MODE_N) &&
		        (cptr->modes_on & MODE_N))
			ircsprintf(modes, "+n");
		if ((mode == MODE_T) &&
		        (cptr->modes_on & MODE_T))
			ircsprintf(modes, "+t");
		if ((mode == MODE_M) &&
		        (cptr->modes_on & MODE_M))
			ircsprintf(modes, "+m");
#ifdef DANCER

		if ((mode == MODE_C) &&
		        (cptr->modes_on & MODE_C))
			ircsprintf(modes, "+c");
#endif /* DANCER */

		if ((mode == MODE_I) &&
		        (cptr->modes_on & MODE_I))
			ircsprintf(modes, "+i");

		if (modes[0])
		{
			toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
			UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
		}
	}
	if ((mode == MODE_L) &&
	        (cptr->limit))
	{
		ircsprintf(modes, "+l %ld", cptr->limit);
		toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
		UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
	}
	if ((mode == MODE_K) &&
	        (cptr->key))
	{
		if (!isminus)
		{
			ircsprintf(modes, "-k %s", chptr->key);
			toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
			UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
		}
		ircsprintf(modes, "+k %s", cptr->key);
		toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
		UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
	}
#ifdef DANCER
	if ((mode == MODE_F) && (cptr->forward))
	{
		if (!isminus)
		{
			ircsprintf(modes, "-f %s", chptr->forward);
			toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
			UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
		}
		ircsprintf(modes, "+f %s", cptr->forward);
		toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
		UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
	}
#endif /* DANCER */

	if (cptr->modes_off && !isminus)
	{
		modes[0] = '\0';
		if ((mode == MODE_S) &&
		        (cptr->modes_off & MODE_S))
			ircsprintf(modes, "-s");
		if ((mode == MODE_P) &&
		        (cptr->modes_off & MODE_P))
			ircsprintf(modes, "-p");
		if ((mode == MODE_N) &&
		        (cptr->modes_off & MODE_N))
			ircsprintf(modes, "-n");
		if ((mode == MODE_T) &&
		        (cptr->modes_off & MODE_T))
			ircsprintf(modes, "-t");
		if ((mode == MODE_M) &&
		        (cptr->modes_off & MODE_M))
			ircsprintf(modes, "-m");
#ifdef DANCER

		if ((mode == MODE_C) &&
		        (cptr->modes_off & MODE_C))
			ircsprintf(modes, "-c");
#endif /* DANCER */

		if ((mode == MODE_I) &&
		        (cptr->modes_off & MODE_I))
			ircsprintf(modes, "-i");
		if ((mode == MODE_L) &&
		        (cptr->modes_off & MODE_L))
			ircsprintf(modes, "-l");
		if ((mode == MODE_K) &&
		        (cptr->modes_off & MODE_K))
		{
			if (chptr->key[0] != '\0')
				ircsprintf(modes, "-k %s", chptr->key);
		}
#ifdef DANCER
		if ((mode == MODE_F) &&
		        (cptr->modes_off & MODE_F))
		{
			if (chptr->forward)
				ircsprintf(modes, "-f %s", chptr->forward);
		}
#endif /* DANCER */

		if (modes[0])
		{
			toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
			UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
		}
	}
} /* cs_CheckModes() */

/*
cs_CheckTopic()
 Called when a topic is changed - if topiclock is enabled, change
it back
*/

void
cs_CheckTopic(char *who, char *channel)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(channel)))
		return;

	if (cptr->flags & CS_TOPICLOCK)
	{
		if (IsFounder(FindClient(who), cptr))
			return;

		if (cptr->topic)
			cs_SetTopic(FindChannel(cptr->name), cptr->topic);
		else
			cs_SetTopic(FindChannel(cptr->name), "");
	}
} /* cs_CheckTopic() */

/*
cs_CheckOp()
  Check if nick has AutoOp access on 'chanptr', if so
op nick
*/

void
cs_CheckOp(struct Channel *chanptr, struct ChanInfo *cptr, char *nick)

{
	struct ChannelUser *tempuser;
	char modes[MAXLINE + 1];

	if (!chanptr || !cptr || !nick)
		return;

	/*
	 * If the user is flagged to be denied channel op
	 * privileges, do nothing
	 */
	
	/* If user has NEVEROP flag do nothing too / CoolCold / */
	if (HasFlag(nick, NS_NOCHANOPS) || HasFlag(nick, NS_NEVEROP))
		return;

	/*
	 * If ChanServ should be on the channel, but isn't, don't
	 * do anything
	 */
	if (cs_ShouldBeOnChan(cptr) && !IsChannelOp(chanptr, Me.csptr))
		return;

	if (!(tempuser = FindUserByChannel(chanptr, FindClient(nick))))
		return;

	if (tempuser->flags & CH_OPPED)
	{
		/*
		 * They're already opped on the channel - we don't need to do anything
		 */
		return;
	}

	/* XXX: isn't working as it should */
#if 0
	if (HasAccess(cptr, tempuser->lptr, CA_AUTODEOP))
	{
		/* If user has autodeop access, do nothing
		 * -- OnGeboren -- */
		return;
	}
#endif

	if (HasAccess(cptr, tempuser->lptr, CA_AUTOOP))
	{
		ircsprintf(modes, "+o %s", tempuser->lptr->nick);
		toserv(":%s MODE %s %s\r\n", n_ChanServ, chanptr->name, modes);
		UpdateChanModes(Me.csptr, n_ChanServ, chanptr, modes);
	}
#ifdef HYBRID7_HALFOPS
	/* Add autohalfop -Janos */
	else if (!(tempuser->flags & CH_HOPPED) &&
	         HasAccess(cptr, tempuser->lptr, CA_AUTOHALFOP))
	{
		ircsprintf(modes, "+h %s", tempuser->lptr->nick);
		toserv(":%s MODE %s %s\r\n", n_ChanServ, chanptr->name, modes);
		UpdateChanModes(Me.csptr, n_ChanServ, chanptr, modes);
	}
#endif /* HYBRID7_HALFOPS */
	else if (!(tempuser->flags & CH_VOICED) &&
	         HasAccess(cptr, tempuser->lptr, CA_AUTOVOICE))
	{
		ircsprintf(modes, "+v %s", tempuser->lptr->nick);
		toserv(":%s MODE %s %s\r\n", n_ChanServ, chanptr->name, modes);
		UpdateChanModes(Me.csptr, n_ChanServ, chanptr, modes);
	}
} /* cs_CheckOp() */

/*
cs_CheckJoin()
  Check if 'nick' is on the AutoKick list for chanptr; if not, send
the entry message to 'nickname' (if there is one)
*/

void
cs_CheckJoin(struct Channel *chanptr, struct ChanInfo *cptr, char *nickname)

{
	struct Luser *lptr;
	struct NickInfo *nptr;
	struct AutoKick *ak;
	char chkstr[MAXLINE + 1];
	char modes[MAXLINE + 1];
	char *nick;

	if (!chanptr || !cptr || !nickname)
		return;

	nick = nickname;
	if (IsNickPrefix(*nick))
	{
		if (IsNickPrefix(*(++nick)))
			++nick;
	}

	if (!(lptr = FindClient(nick)))
		return;

	if (FindService(lptr))
		return;

	/*
	 * If the channel is restricted, and lptr just created it,
	 * cs_CheckChan(), called from cs_CheckSjoin() would have
	 * already kickbanned lptr, in which case they would no
	 * longer be on the channel, but s_sjoin() would not know
	 * that - catch it here.
	 */
	if (!IsChannelMember(chanptr, lptr))
		return;

	/* someone joined - update lastused time */
	/* cptr->lastused = current_ts; */

#ifndef HYBRID_ONLY
	/*
	 * Check if ChanServ has joined the channel yet - this might
	 * be the initial SJOIN burst in which case we can't do anything
	 */
	if (!IsChannelMember(chanptr, Me.csptr))
		return;
#endif /* !HYBRID_ONLY */

	if (cptr->flags & CS_FORBID)
	{
		/*
		 * Normally, we would never get here since ChanServ would have
		 * kicked everyone out of the forbidden channel when services was
		 * first started.  However, if an administrator uses OMODE -i on the
		 * channel, they could conceivably join again, so rekick them out
		 */

		if (!IsChannelMember(chanptr, Me.csptr))
			cs_joinchan(cptr);

		toserv(":%s MODE %s +is\r\n", n_ChanServ, cptr->name);
		UpdateChanModes(Me.csptr, n_ChanServ, chanptr, "+is");
		KickBan(0, n_ChanServ, chanptr, lptr->nick, "Forbidden Channel");
		return;
	}

	ircsprintf(chkstr, "%s!%s@%s", lptr->nick, lptr->username,
	           lptr->hostname);

	if ((ak = OnAkickList(cptr, chkstr)))
	{
		/*
		 * Don't kick founders who have identified - even if they match an
		 * AKICK entry
		 */
		if (IsFounder(lptr, cptr))
			return;

		if (chanptr->numusers == 1)
		{
			/*
			 * This user is the only one on the channel - if we kick him
			 * out, he could just rejoin since the channel would have been
			 * dropped, have ChanServ join the channel for a few minutes
			 */
			cs_joinchan(cptr);
		}

		ircsprintf(modes, "+b %s", ak->hostmask);
		toserv(":%s MODE %s %s\r\n", n_ChanServ, chanptr->name, modes);
		UpdateChanModes(Me.csptr, n_ChanServ, chanptr, modes);

		toserv(":%s KICK %s %s :%s\r\n", n_ChanServ, chanptr->name,
		       lptr->nick, ak->reason ? ak->reason : "");
		RemoveFromChannel(chanptr, lptr);
	}
	else if ((cptr->flags & CS_RESTRICTED) &&
	         !HasAccess(cptr, lptr, CA_AUTOOP)
#ifdef EMPOWERADMINS_MORE
	         && !IsValidAdmin(lptr)
#endif
	        )
	{
		char *mask = HostToMask(lptr->username, lptr->hostname);

		if (!IsChannelMember(chanptr, Me.csptr))
			cs_joinchan(cptr);

		ircsprintf(modes, "+b *!%s", mask);

		toserv(":%s MODE %s %s\r\n", n_ChanServ, chanptr->name, modes);

		UpdateChanModes(Me.csptr, n_ChanServ, chanptr, modes);

		MyFree(mask);

		toserv(":%s KICK %s %s :%s\r\n",
		       n_ChanServ, chanptr->name, lptr->nick, "Restricted Channel");

		RemoveFromChannel(chanptr, lptr);
	}
	else if (cptr->entrymsg && burst_complete) /* Don't send entry messages
				                                                                    during initial burst */
	{
		/*
		 * Send cptr->entrymsg to lptr->nick in the form of a NOTICE
		 */
		notice(n_ChanServ, lptr->nick, "[%s] %s", chanptr->name,
		       cptr->entrymsg);
	}

	/* Is this the founder? (not somebody with founder _access_, but the
	 * actual founder nick) */
	if ((nptr = FindNick(lptr->nick)) && (nptr->flags & NS_IDENTIFIED))
	{
		if (cptr->founder && irccmp(lptr->nick, cptr->founder) == 0)
			/* That's the founder joining. Update activity timer */
			cptr->lastused = cptr->last_founder_active = current_ts;
		if (cptr->successor && irccmp(lptr->nick, cptr->successor) == 0)
			cptr->lastused = cptr->last_successor_active = current_ts;
	}
} /* cs_CheckJoin() */

/*
cs_CheckSjoin()
 Called from s_sjoin() to check if the SJOINing nicks are
allowed to have operator status on "chptr->name"
 
nickcnt : number of SJOINing nicks
nicks   : array of SJOINing nicks
newchan : 1 if a newly created channel
*/

void
cs_CheckSjoin(struct Channel *chptr, struct ChanInfo *cptr,
              int nickcnt, char **nicks, int newchan)

{
	struct Luser *nlptr;
	char dnicks[MAXLINE + 1]; /* nicks to deop */
	char *currnick;
	int ii, jj = 1; /* looping */

	if (!chptr || !cptr || !nicks)
		return;

	if (cptr->flags & CS_FORGET)
		return;

#ifndef HYBRID_ONLY

	if (IsChannelMember(chptr, Me.csptr))
	{
		/*
		 * ChanServ is already on the channel so it is not a brand new
		 * channel - we don't have to worry about the new user(s) who joined
		 */
		return;
	}

#endif /* !HYBRID_ONLY */

	/*
	 * This is where ChanServ will join the channel if its
	 * registered.  We just got the TS, so we know how to get
	 * ChanServ opped. The thing is, ChanServ won't join any
	 * registered channels which are empty on startup, since
	 * there won't be an SJOIN associated with it; thus the
	 * moment someone joins such a channel, an SJOIN will
	 * be sent, causing ChanServ to join (theres little point in
	 * ChanServ sitting in an empty channel anyway)
	 */

#ifdef HYBRID_ONLY

	/*
	 * Only call cs_join() if the channel is brand new,
	 * or ChanServ will be changing the topic or setting
	 * mlock modes everytime someone joins.
	 */
	if (newchan)
	{
		cs_join(cptr);

#ifdef SEENSERVICES
		if (cptr->flags & CS_SEENSERV)
			ss_join(chptr);
#endif /* SEENSERVICES */
	}

#else

	cs_join(cptr);

#ifdef SEENSERVICES
	if (cptr->flags & CS_SEENSERV)
		ss_join(chptr);
#endif /* SEENSERVICES */
#endif /* HYBRID_ONLY */

	/*
	 * If SPLITOPS is on, don't worry about anything else,
	 * just let everyone get opped - toot 26/6/2000
	 */
	if (cptr->flags & CS_SPLITOPS)
	{
		return;
	}

	/*
	 * First, if AllowAccessIfSOp is enabled, check if there
	 * is a SuperOp currently opped on the channel. If there
	 * is, return because we don't need to deop anyone in
	 * that case. If the channel has RESTRICTED or SECUREOPS
	 * enabled, we deop non-autops regardless of having
	 * a SuperOp on the channel.
	 */
	if (AllowAccessIfSOp &&
	        !(cptr->flags & CS_RESTRICTED) &&
	        !(cptr->flags & CS_SECUREOPS))
	{
		struct ChannelUser *tempu;

		for (tempu = chptr->firstuser; tempu; tempu = tempu->next)
		{
			if (HasAccess(cptr, tempu->lptr, CA_SUPEROP))
			{
				/*
				 * There is at least 1 SuperOp opped on the channel
				 * and SECUREOPS/RESTRICTED is off, so allow the 
				 * other ops to stay opped, regardless of being
				 * an Aop or higher
				 */
				return;
			}
		}
	} /* if (AllowAccessIfSOp) */

	/*
	  * deop the nick(s) who created the channel if they
	  * aren't an autoop or higher
	  */
	dnicks[0] = '\0';
	jj = 1;

	for (ii = 0; ii < nickcnt; ii++)
	{
		currnick = GetNick(nicks[ii]);
		nlptr = FindClient(currnick);

		if (!nlptr)
			continue;

		if (FindService(nlptr))
			continue;

		if (!IsChannelOp(chptr, nlptr))
			continue;

		if (!HasFlag(nlptr->nick, NS_NOCHANOPS) &&
		        HasAccess(cptr, nlptr, CA_AUTOOP))
			continue;

		if (jj * (NICKLEN + 1) >= sizeof(dnicks))
		{
			SetModes(n_ChanServ, 0, 'o', chptr, dnicks);
			dnicks[0] = '\0';
			jj = 1;
		}

		strlcat(dnicks, nlptr->nick, sizeof(dnicks));
		strlcat(dnicks, " ", sizeof(dnicks));
		++jj;

		/*
		 * Send them a notice so they know why they're being
		 * deoped
		 */
		if (GiveNotice)
		{
			notice(n_ChanServ, nlptr->nick,
			       "You do not have AutoOp access to [%s]", cptr->name);
		}
	}

	/*
	 * Deop the non-autoops
	 */
	SetModes(n_ChanServ, 0, 'o', chptr, dnicks);
} /* cs_CheckSjoin() */

/*
cs_ShouldBeOnChan()
 Determine whether ChanServ should be on the channel 'cptr'.
If so, return 1, otherwise 0
*/

int
cs_ShouldBeOnChan(struct ChanInfo *cptr)

{
	if (!cptr)
		return (0);

#ifndef HYBRID_ONLY

	/*
	 * Since HYBRID_ONLY is not defined, ChanServ should be on
	 * every single monitored channel
	 */
	return (1);

#endif /* !HYBRID_ONLY */

	/*
	 * Now, HYBRID_ONLY is defined, so generally ChanServ should NOT
	 * be on the channel. However, if the channel has SET GUARD
	 * enabled, ChanServ should be monitoring the channel.
	 */
	if (AllowGuardChannel)
		if (cptr->flags & CS_GUARD)
			return (1);

	return (0);
} /* cs_ShouldBeOnChan() */

/*
cs_RejoinChannels()
 Have ChanServ rejoin all channel's it should be in
*/

void
cs_RejoinChannels()

{
	int ii;
	struct ChanInfo *cptr;

	for (ii = 0; ii < CHANLIST_MAX; ii++)
	{
		for (cptr = chanlist[ii]; cptr; cptr = cptr->next)
		{
			if (cptr->flags & CS_FORGET)
				continue;
			if (!cs_ShouldBeOnChan(cptr))
				continue;
			if (!FindChannel(cptr->name))
				continue;

			cs_join(cptr);
		}
	}
} /* cs_RejoinChannels() */

/*
PromoteSuccessor()
 Promote the successor on cptr to founder
*/

void
PromoteSuccessor(struct ChanInfo *cptr)

{
	assert(cptr && cptr->successor);

	putlog(LOG2,
	       "%s: promoting successor [%s] to founder on [%s]",
	       n_ChanServ,
	       cptr->successor,
	       cptr->name);

	MyFree(cptr->founder);

	cptr->founder = cptr->successor;
	cptr->last_founder_active = cptr->last_successor_active;
	cptr->successor = NULL;
	cptr->last_successor_active = 0;
} /* PromoteSuccessor() */

/*
MakeChan()
 Create a ChanInfo structure and return it
*/

static struct ChanInfo *
			MakeChan()

{
	struct ChanInfo *chanptr;

	chanptr = (struct ChanInfo *) MyMalloc(sizeof(struct ChanInfo));

	memset(chanptr, 0, sizeof(struct ChanInfo));

	return (chanptr);
} /* MakeChan() */

/*
ExpireChannels()
 Delete any channels that have expired.
*/

void
ExpireChannels(time_t unixtime)

{
	int ii;
	struct ChanInfo *cptr, *next;

	if (!ChannelExpire)
		return;

	for (ii = 0; ii < CHANLIST_MAX; ++ii)
	{
		for (cptr = chanlist[ii]; cptr; cptr = next)
		{
			struct Channel *chptr;
			struct ChannelUser *tempuser;

			next = cptr->next;

			/* find if anyone is still idling in the channel */
			chptr = FindChannel(cptr->name);
			if (chptr && chptr->numusers)
			{
				for (tempuser = chptr->firstuser; tempuser; tempuser =
						tempuser->next)
				{
					if (GetAccess(cptr, tempuser->lptr))
						break;
				}
			}

			if ((!(cptr->flags & (CS_FORBID | CS_FORGET | CS_NOEXPIRE))) &&
			        ((unixtime - cptr->lastused) >= ChannelExpire))
			{
				putlog(LOG2,
				       "%s: Expired channel [%s]",
				       n_ChanServ,
				       cptr->name);

				DeleteChan(cptr);
			}
		} /* for (cptr = chanlist[ii]; cptr; cptr = next) */
	}
} /* ExpireChannels() */

#ifndef HYBRID_ONLY

/*
CheckEmptyChans()
 Part any channels in which ChanServ is the only user
 or part if it shouldn't be on channel
*/

void
CheckEmptyChans()

{
	struct UserChannel *tempc = NULL;
	struct UserChannel *temp = NULL;

	if (Me.csptr)
	{
		for (tempc = Me.csptr->firstchan; tempc != NULL; )
		{
			if ((tempc->chptr->numusers == 1) ||
				(tempc->chptr->numusers == 2 &&
				 IsChannelMember(tempc->chptr, Me.esptr)) ||
			        !cs_ShouldBeOnChan(FindChan(tempc->chptr->name)))
              
			{
				/* ChanServ is the only client on the channel, part */
				SendUmode(OPERUMODE_Y, "%s: Parting channel [%s]",
				          n_ChanServ, tempc->chptr->name);

				temp = tempc->next;
#ifdef SEENSERVICES
				ss_part(tempc->chptr);
#endif /* SEENSERVICES */
				cs_part(tempc->chptr);
				tempc = temp;
			}
			else
				tempc = tempc->next;
		}
	}
} /* CheckEmptyChans() */

#endif /* !HYBRID_ONLY */

/*
FindChan()
  Return a pointer to registered channel 'channel'
*/

struct ChanInfo *
			FindChan(char *channel)

{
	struct ChanInfo *cptr;
	unsigned int hashv;

	if (!channel)
		return (NULL);

	hashv = CSHashChan(channel);
	for (cptr = chanlist[hashv]; cptr; cptr = cptr->next)
		if (!irccmp(cptr->name, channel))
			return (cptr);

	return (NULL);
} /* FindChan() */

/*
AddFounder()
  Add founder 'lptr' to 'chanptr'
*/

static void
AddFounder(struct Luser *lptr, struct ChanInfo *chanptr)

{
	struct aChannelPtr *fcptr;
	struct f_users *fuptr;

	if (!lptr || !chanptr)
		return;

	fcptr = (struct aChannelPtr *) MyMalloc(sizeof(struct aChannelPtr));
	fcptr->cptr = chanptr;
	fcptr->next = lptr->founder_channels;
	lptr->founder_channels = fcptr;

	fuptr = (struct f_users *) MyMalloc(sizeof(struct f_users));
	fuptr->lptr = lptr;
	fuptr->next = chanptr->founders;
	chanptr->founders = fuptr;
} /* AddFounder() */

/*
RemFounder()
  Remove lptr->nick from the list of channel founders for 'cptr'
*/

void
RemFounder(struct Luser *lptr, struct ChanInfo *cptr)

{
	struct aChannelPtr *fcptr, *cprev;
	struct f_users *fuptr, *uprev;

	if (!cptr || !lptr)
		return;

	cprev = NULL;
	for (fcptr = lptr->founder_channels; fcptr != NULL; )
	{
		if (fcptr->cptr == cptr)
		{
			if (cprev)
			{
				cprev->next = fcptr->next;
				MyFree(fcptr);
				fcptr = cprev;
			}
			else
			{
				lptr->founder_channels = fcptr->next;
				MyFree(fcptr);
			}

			/*
			 * cptr should occur only once in the founder list -
			 * break after we find it
			 */
			break;
		}

		cprev = fcptr;
		fcptr = fcptr->next;
	}

	uprev = NULL;
	for (fuptr = cptr->founders; fuptr != NULL; )
	{
		if (fuptr->lptr == lptr)
		{
			if (uprev)
			{
				uprev->next = fuptr->next;
				MyFree(fuptr);
				fuptr = uprev;
			}
			else
			{
				cptr->founders = fuptr->next;
				MyFree(fuptr);
			}

			break;
		}

		uprev = fuptr;
		fuptr = fuptr->next;
	}
} /* RemFounder() */

/*
ChangeChanPass()
  Change channel founder password for cptr->name to 'newpass'
*/

static int
ChangeChanPass(struct ChanInfo *cptr, char *newpass)

{
#ifdef CRYPT_PASSWORDS
	char  *encr;
#endif

	if (!cptr || !newpass)
		return 0;

	if (!cptr->password)
	{
		/*
		 * The password hasn't been set yet, so we're probably registering the
		 * channel right now, thus we need to make our own salt.
		 *
		 * Use hybcrypt() for that, which will properly salt and encrypt
		 * string, possibly with MD5 if it is setup in UseMD5 -kre
		 */
#ifdef CRYPT_PASSWORDS

		encr = hybcrypt(newpass, NULL);
		assert(encr != 0);

		cptr->password = MyStrdup(encr);

#else

		/* just use plaintext */
		cptr->password = MyStrdup(newpass);

#endif

	}
	else
	{
		/* the password is being changed */
		/* Use new hybcrypt routine instead -kre */

#ifdef CRYPT_PASSWORDS

		encr = hybcrypt(newpass, cptr->password);
		assert(encr != 0);

		MyFree(cptr->password);
		cptr->password = MyStrdup(encr);

#else

		MyFree(cptr->password);
		cptr->password = MyStrdup(newpass);

#endif

	} /* else */

	return 1;
} /* ChangeChanPass() */

/*
AddChan()
  Add 'chanptr' to chanlist table
*/

static void
AddChan(struct ChanInfo *chanptr)

{
	unsigned int hashv;

	/*
	 * all the fields of chanptr were filled in already, so just 
	 * insert it in the list
	 */
	hashv = CSHashChan(chanptr->name);

	chanptr->prev = NULL;
	chanptr->next = chanlist[hashv];
	if (chanptr->next)
		chanptr->next->prev = chanptr;

	chanlist[hashv] = chanptr;

	++Network->TotalChans;
} /* AddChan() */

/*
DeleteChan()
 Deletes channel 'chanptr'
*/

void
DeleteChan(struct ChanInfo *chanptr)

{
	struct NickInfo *nptr;
	struct AutoKick *ak;
	struct ChanAccess *ca;
	struct Channel *chptr;
	struct f_users *fdrs;
	int hashv;

#ifdef MEMOSERVICES

	struct MemoInfo *mi;
#endif

	if (chanptr == NULL)
		return;

	hashv = CSHashChan(chanptr->name);

#ifdef MEMOSERVICES
	/* check if the chan had any memos - if so delete them */
	if ((mi = FindMemoList(chanptr->name)))
		DeleteMemoList(mi);
#endif

	MyFree(chanptr->password);
	MyFree(chanptr->topic);
	MyFree(chanptr->key);
#ifdef DANCER

	MyFree(chanptr->forward);
#endif /* DANCER */

	/* leave channel if needed */
	chptr = FindChannel(chanptr->name);
	if (chptr != NULL)
	{
		if (IsChannelMember(chptr, Me.csptr))
			cs_part(chptr);
#ifdef SEENSERVICES
		ss_part(chptr);
#endif /* SEENSERVICES */
	}

	while (chanptr->akick)
	{
		ak = chanptr->akick->next;
		MyFree(chanptr->akick->hostmask);
		MyFree(chanptr->akick->reason);
		MyFree(chanptr->akick);
		chanptr->akick = ak;
	}

	while (chanptr->access)
	{
		ca = chanptr->access->next;

		/*
		 * If there is an nptr entry in this access node, we must
		 * delete it's corresponding AccessChannel entry.
		 */
		if (chanptr->access->nptr && chanptr->access->acptr)
			DeleteAccessChannel(chanptr->access->nptr, chanptr->access->acptr);

		MyFree(chanptr->access->hostmask);
		MyFree(chanptr->access->added_by);
		MyFree(chanptr->access);
		chanptr->access = ca;
	}

	while (chanptr->founders)
	{
		fdrs = chanptr->founders->next;
		RemFounder(chanptr->founders->lptr, chanptr);
		chanptr->founders = fdrs;
	}

	MyFree(chanptr->name);

	if (chanptr->founder)
	{
		if (!(chanptr->flags & (CS_FORBID | CS_FORGET)) &&
		        (nptr = GetLink(chanptr->founder)))
			RemoveFounderChannelFromNick(&nptr, chanptr);

		MyFree(chanptr->founder);
	}

	MyFree(chanptr->successor);
	MyFree(chanptr->email);
	MyFree(chanptr->url);
	MyFree(chanptr->entrymsg);
	MyFree(chanptr->access_lvl);
	MyFree(chanptr->comment);

	MyFree(chanptr->forbidby);
	MyFree(chanptr->forbidreason);

	if (chanptr->next)
		chanptr->next->prev = chanptr->prev;
	if (chanptr->prev)
		chanptr->prev->next = chanptr->next;
	else
		chanlist[hashv] = chanptr->next;

	MyFree(chanptr);

	--Network->TotalChans;
} /* DeleteChan() */

/*
 * AddAccess()
 * Add 'mask' or 'nptr' to the access list for 'chanptr' with 'level'
 * Return 1 if successful, 0 if not
 *
 * rewrote this to use master nicknames for access inheritance -kre
 */
int AddAccess(struct ChanInfo *chanptr, struct Luser *lptr, char
              *mask, struct NickInfo *nptr, int level, time_t created, time_t
              last_used, char *added_by)
{
	struct ChanAccess *ptr;
	struct NickInfo *master_nptr;

	if (!chanptr || (!mask && !nptr))
		return 0;

	/* get master */
	master_nptr = GetMaster(nptr);

	if ((ptr = OnAccessList(chanptr, mask, master_nptr)))
	{
		/* 'mask' is already on the access list - just change the level */
		if (lptr)
			if (GetAccess(chanptr, lptr) <= ptr->level)
				return 0;

		/* change added_by */
		MyFree(ptr->added_by);
		ptr->added_by = MyStrdup(added_by);

		ptr->level = level;
		return 1;
	}

	ptr = (struct ChanAccess *) MyMalloc(sizeof(struct ChanAccess));
	memset(ptr, 0, sizeof(struct ChanAccess));

	ptr->created = created;
	ptr->last_used = last_used;
	ptr->added_by = MyStrdup(added_by);

	if (master_nptr)
	{
		ptr->nptr = master_nptr;
		ptr->hostmask = NULL;

		/* We also want the NickInfo structure to keep records of what
		 * channels it has access on, so if we ever delete the nick, we will
		 * know where to remove it's access. */
		ptr->acptr = AddAccessChannel(master_nptr, chanptr, ptr);

		if (ptr->acptr == NULL)
		{
			MyFree(ptr);
			return 0;
		}
	}
	else
		ptr->hostmask = MyStrdup(mask);

	ptr->level = level;

	ptr->prev = NULL;
	ptr->next = chanptr->access;
	if (ptr->next)
		ptr->next->prev = ptr;

	chanptr->access = ptr;

	return 1;
} /* AddAccess() */

/*
 * DelAccess()
 * Delete 'mask' from cptr's access list
 * Return -1 if 'mask' has a higher user level than lptr, otherwise
 * the number of matches
 *
 * rewrote using master nicks -kre
 */
static int DelAccess(struct ChanInfo *cptr, struct Luser *lptr, char
                     *mask, struct NickInfo *nptr)
{
	struct ChanAccess *temp = NULL, *tempnext;
	struct NickInfo *master_nptr = NULL;
	int ret = 0, cnt = 0, ulev;
	int found = 0;

	if (!cptr || !lptr || (!mask && !nptr))
		return 0;

	if (!(cptr->access))
		return 0;

	master_nptr = GetMaster(nptr);

	ulev = GetAccess(cptr, lptr);

	for (temp = cptr->access; temp != NULL; temp = tempnext)
	{
		/* be sure we have a valid reference -kre */
		tempnext = temp->next;

		found = 0;

		if (master_nptr && temp->nptr && (master_nptr == temp->nptr))
			found = 1;
		else
			if (mask && temp->hostmask)
				if (match(mask, temp->hostmask))
					found = 1;

		if (found)
		{
			/* don't let lptr->nick delete a hostmask that has a >= level
			 * than it does, except if it is founder */
			if (temp->level >= ulev && !IsFounder(lptr, cptr))
			{
				ret = -1;
				continue;
			}

			++cnt;

			/* master_nptr will have an AccessChannel entry which is set to
			 * 'temp', so it must be deleted */
			if (master_nptr && temp->acptr)
				DeleteAccessChannel(master_nptr, temp->acptr);

			DeleteAccess(cptr, temp);

		} /* if (found) */
	} /* for .. */

	if (cnt > 0)
		return (cnt);

	return (ret);
} /* DelAccess() */

/*
 * DeleteAccess()
 * Free the ChanAccess structure 'ptr'
 */
void DeleteAccess(struct ChanInfo *cptr, struct ChanAccess *ptr)
{
	assert(cptr);
	assert(ptr);

	if (ptr->next)
		ptr->next->prev = ptr->prev;
	if (ptr->prev)
		ptr->prev->next = ptr->next;
	else
		cptr->access = ptr->next;

	MyFree(ptr->hostmask);
	MyFree(ptr->added_by);
	MyFree(ptr);
} /* DeleteAccess() */

/*
 * AddAkick()
 * Add 'mask' to 'chanptr' 's autokick list
 */
static int AddAkick(struct ChanInfo *chanptr, struct Luser *lptr, char
                    *mask, char *reason, long when)
{
	struct AutoKick *ptr;
#if defined HYBRID || defined HYBRID7

	char *tmpmask;
#endif

	if (!chanptr || !mask)
		return 0;

	/* require at least 1 minute (expire resolution) -kre */
	if (when && (when - current_ts < 60))
		return 0;

#if defined HYBRID || defined HYBRID7
	/* Fix bogus masks (in fact, they are only in hybrid bogus -kre */
	if ((tmpmask = strchr(mask, '!')) && (tmpmask - mask > NICKLEN))
		return 0;
#endif

	if (lptr)
	{
		struct ChanAccess *ca;
		int found = 0;

		/* Check if lptr is trying to add an akick that matches a
		 * level higher than their own */
		for (ca = chanptr->access; ca; ca = ca->next)
		{
			if (ca->hostmask)
				if (match(mask, ca->hostmask))
					found = 1;

			if (ca->nptr)
			{
				struct NickHost *hptr;
				char chkstr[MAXLINE + 1];

				for (hptr = ca->nptr->hosts; hptr; hptr = hptr->next)
				{
					if (strchr(hptr->hostmask, '!'))
						strlcpy(chkstr, hptr->hostmask, sizeof(chkstr));
					else
						ircsprintf(chkstr, "*!%s", hptr->hostmask);

					if (match(mask, chkstr))
					{
						found = 1;
						break;
					}
				}
			}

			if (found)
			{
				if (ca->level >= GetAccess(chanptr, lptr))
					return 0;
				found = 0;
			}
		}
	}

	ptr = (struct AutoKick *) MyMalloc(sizeof(struct AutoKick));
	ptr->hostmask = MyStrdup(mask);
	if (reason)
		ptr->reason = MyStrdup(reason);
	else
		ptr->reason = NULL;

	ptr->expires = when;

	ptr->next = chanptr->akick;
	chanptr->akick = ptr;
	chanptr->akickcnt++;

	return 1;
} /* AddAkick() */

/*
DelAkick()
  Delete 'mask' from cptr's akick list; return number of matches
*/

static int
DelAkick(struct ChanInfo *cptr, char *mask)

{
	struct AutoKick *temp, *prev, *next;
	int cnt;

	if (!cptr || !mask)
		return 0;

	if (!cptr->akick)
		return 0;

	cnt = 0;

	prev = NULL;
	for (temp = cptr->akick; temp; temp = next)
	{
		next = temp->next;

		if (match(mask, temp->hostmask))
		{
			++cnt;

			MyFree(temp->hostmask);
			if (temp->reason)
				MyFree(temp->reason);

			if (prev)
			{
				prev->next = temp->next;
				MyFree(temp);
				temp = prev;
			}
			else
			{
				cptr->akick = temp->next;
				MyFree(temp);
				/* temp = NULL; */
			}
		}

		prev = temp;
	}

	cptr->akickcnt -= cnt;

	return (cnt);
} /* DelAkick() */

/*
IsFounder()
  Determine if lptr->nick has identified as a founder for 'cptr'
*/

int
IsFounder(struct Luser *lptr, struct ChanInfo *cptr)

{
	struct f_users *temp;

#ifdef EMPOWERADMINS_MORE
	if (IsValidAdmin(lptr))
		return 1;
#endif

	for (temp = cptr->founders; temp; temp = temp->next)
		if (temp->lptr == lptr)
			return 1;

	return 0;
} /* IsFounder() */

/*
 * GetAccess()
 * Return highest access level 'lptr' has on 'cptr'
 */
static int GetAccess(struct ChanInfo *cptr, struct Luser *lptr)
{
	struct ChanAccess *ca;
	struct Channel *chptr;
	char chkstr[MAXLINE + 1];
	int level = -9999;
	struct NickInfo *nptr;
	int found;

	if (!cptr || !lptr)
		return 0;

	if (cptr->flags & (CS_FORBID | CS_FORGET))
		return 0;

	if (IsFounder(lptr, cptr))
		return (cptr->access_lvl[CA_FOUNDER]);

	ircsprintf(chkstr, "%s!%s@%s", lptr->nick, lptr->username,
	           lptr->hostname);

	if ((nptr = FindNick(lptr->nick)))
	{
		/* If nptr has not yet identified, don't give them access */
		if (!(nptr->flags & NS_IDENTIFIED))
		{
			if (cptr->flags & CS_SECURE)
				/* If the channel is secured, they MUST be identified to get
				 * access - return 0 */
				return(0);

			if (!AllowAccessIfSOp)
				nptr = NULL;
			else if (!IsChannelOp(FindChannel(cptr->name), lptr))
				nptr = NULL;
			else
			{
				/*
				 * They're opped on the channel, and AllowAccessIfSOp is
				 * enabled, so allow them access. This will only work during
				 * a netjoin, so ChanServ doesn't wrongfully deop whole
				 * channels. If a channel op tries to use this feature to
				 * get access without identifying, cs_process() will stop
				 * them cold.
				 */
			}
		}
	}
	else if (cptr->flags & CS_SECURE)
	{
		/* The channel is secured, and they do not have a registered
		 * nickname - return 0 */
		return (0);
	}

	for (ca = cptr->access; ca; ca = ca->next)
	{
		found = 0;
		if (nptr && ca->nptr)
		{
			if (nptr == ca->nptr)
				found = 1;
			else
			{
#ifdef LINKED_NICKNAMES
				/*
				 * If nptr and ca->nptr are the in the same link,
				 * give nptr the exact same access ca->nptr has
				 */
				if (IsLinked(nptr, ca->nptr))
					found = 1;
#endif /* LINKED_NICKNAMES */

			}
		}

		if (ca->hostmask)
			if (match(ca->hostmask, chkstr))
				found = 1;

		if (found)
		{
			if (nptr)
			{
				chptr = FindChannel(cptr->name);
				/* Only if the identified user is currently on the channel
				 * update usage time for this access level */
				if (chptr && (nptr->flags & NS_IDENTIFIED) &&
				        IsChannelMember(chptr, lptr))
					cptr->lastused = ca->last_used = current_ts;
			}

			if (ca->level >= level)
				/*
				 * if ca->level is greater then level, keep going because
				 * there may be another hostmask in the access list which
				 * matches lptr's userhost which could have an even bigger
				 * access level
				 */
				level = ca->level;
		}
	}

	if (level == -9999)
		return 0;
	else
		return (level);
} /* GetAccess() */

/*
HasAccess()
  Determine if lptr->nick has level on 'cptr'
*/

int
HasAccess(struct ChanInfo *cptr, struct Luser *lptr, int level)

{
	struct NickInfo *nptr;

	if ((cptr == NULL) || (lptr == NULL))
		return 0;

	nptr = FindNick(lptr->nick);

	if (cptr->flags & (CS_FORBID | CS_FORGET))
		return 0;

	if (cptr->flags & CS_SECURE)
	{
		if (nptr != NULL)
		{
			if (!(nptr->flags & NS_IDENTIFIED))
				return 0;
		}
		else
			return 0;
	}

#if defined EMPOWERADMINS || defined EMPOWERADMINS_MORE
	/*
	 * If AutoOpAdmins is disabled, check if lptr is an admin and if level
	 * is AUTOOP (or AUTOVOICE) - if so, check if they are on the
	 * channel's access list for the appropriate level; if they are, they
	 * are allowed to get opped/voiced (return 1), otherwise return 0.
	 * This extra check is needed because GetAccess() will return true if
	 * lptr is an admin, regardless of AutoOpAdmins being enabled.
	 */
	if (!AutoOpAdmins && IsValidAdmin(lptr)
		&& (level == CA_AUTOOP || level == CA_AUTOVOICE
#ifdef HYBRID7_HALFOPS
		|| level == CA_AUTOHALFOP
#endif
		))
	{
		struct ChanAccess *ca;
		char nmask[MAXLINE];
		struct NickInfo *master_nptr;

		ircsprintf(nmask, "%s!%s@%s", lptr->nick, lptr->username,
				lptr->hostname);

		master_nptr = GetMaster(nptr);

		if ((ca = OnAccessList(cptr, nmask, master_nptr)) &&
				(ca->level >= cptr->access_lvl[level]))
			return 1;

		return 0;
	}
#endif /* EMPOWERADMINS || EMPOWERADMINS_MORE */

	if (GetAccess(cptr, lptr) >= cptr->access_lvl[level])
		return 1;

	return 0;
} /* HasAccess() */

/*
 * OnAccessList()
 * Return 1 if 'hostmask' is on cptr's access list
 */
static struct ChanAccess *OnAccessList(struct ChanInfo *cptr, char
			                                       *hostmask, struct NickInfo *nptr)
{
	struct ChanAccess *ca;

	for (ca = cptr->access; ca; ca = ca->next)
	{
		if (nptr && ca->nptr)
			if (nptr == ca->nptr)
				return(ca);

		if (hostmask && ca->hostmask)
			if (match(ca->hostmask, hostmask))
				return(ca);
	}

	return(NULL);
} /* OnAccessList() */

/*
OnAkickList()
  Return 1 if 'hostmask' is on cptr's akick list
*/

static struct AutoKick *OnAkickList(struct ChanInfo *cptr, char *hostmask)
{
	struct AutoKick *ak;

	for (ak = cptr->akick; ak; ak = ak->next)
		if (match(ak->hostmask, hostmask) && (!ak->expires ||
		                                      (ak->expires - current_ts > 0)))
			return (ak);

	return (NULL);
} /* OnAkickList() */

/*
c_help()
  Give lptr->nick help on av[1]
*/

static void
c_help(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	if (ac >= 2)
	{
		char str[MAXLINE + 1];

		if (ac >= 3)
			ircsprintf(str, "%s %s", av[1], av[2]);
		else
		{
			if ((!irccmp(av[1], "ACCESS")) ||
			        (!irccmp(av[1], "AKICK")) ||
			        (!irccmp(av[1], "LEVEL")) ||
			        (!irccmp(av[1], "SET")))
				ircsprintf(str, "%s index", av[1]);
			else
			{
				struct Command *cptr;

				for (cptr = chancmds; cptr->cmd; cptr++)
					if (!irccmp(av[1], cptr->cmd))
						break;

				if (cptr->cmd)
					if ((cptr->level == LVL_ADMIN) &&
					        !(IsValidAdmin(lptr)))
					{
						notice(n_ChanServ, lptr->nick,
						       "No help available on \002%s\002", av[1]);
						return;
					}

				ircsprintf(str, "%s", av[1]);
			}
		}

		GiveHelp(n_ChanServ, lptr->nick, str, NODCC);
	}
	else
		GiveHelp(n_ChanServ, lptr->nick, NULL, NODCC);

	return;
} /* c_help() */

/*
c_register()
  Register channel av[1] with password av[2]
*/

static void
c_register(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct Userlist *userptr;
	struct NickInfo *master;
	struct Channel *chptr;

	if (ac < 3)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002REGISTER <#channel> <password>\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ,
		       "REGISTER");
		return;
	}

	if (HasFlag(lptr->nick, NS_NOREGISTER))
	{
		notice(n_ChanServ, lptr->nick,
		       "You are not permitted to register channels");
		putlog(LOG1,
		       "Flagged user %s!%s@%s attempted to register channel [%s]",
		       lptr->nick, lptr->username, lptr->hostname, av[1]);
		return;
	}

	userptr = GetUser(1, lptr->nick, lptr->username, lptr->hostname);
	if (RestrictRegister)
	{
		if (!(lptr->flags & L_OSREGISTERED) || !IsOper(userptr))
		{
			notice(n_ChanServ, lptr->nick,
			       "Use of the [\002REGISTER\002] command is restricted");
			RecordCommand("%s: %s!%s@%s failed REGISTER [%s]", n_ChanServ,
			              lptr->nick, lptr->username, lptr->hostname, av[1]);
			return;
		}
	}

	master = GetMaster(nptr);
	if (!IsValidAdmin(lptr) && MaxChansPerUser &&
	        (master->fccnt >= MaxChansPerUser))
	{
		notice(n_ChanServ, lptr->nick,
		       "You are only allowed to register [\002%d\002] channels",
		       MaxChansPerUser);
		return;
	}

	if (FindChan(av[1]) != NULL)
	{
		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is already registered",
		       av[1]);
		return;
	}

	chptr = FindChannel(av[1]);

	if (chptr == NULL)
	{
		notice(n_ChanServ, lptr->nick, "No such channel [\002%s\002]",
				av[1]);
		return;
	}

	/* We should check the nickname's age if we require nicks to have
	 * specified minimum age to be able to register channels. */
	if (MinNickAge)
	{
		if (current_ts - master->created < MinNickAge &&
				!IsValidAdmin(lptr))
		{
			notice(n_ChanServ, lptr->nick,
					"Your nickname must be older than \002%s\002 in order to register channels",
					timeago(MinNickAge, 3));
			return;
		}
	}

	if (!IsValidAdmin(lptr) && MinChanUsers &&
	        (chptr->numusers < MinChanUsers))
	{
		notice(n_ChanServ, lptr->nick,
		       "You need %d more users to register channel [\002%s\002]",
		       MinChanUsers - chptr->numusers,
		       av[1]);
		return;
	}

	if (!IsChannelOp(chptr, lptr) && !IsValidAdmin(lptr))
	{
		notice(n_ChanServ, lptr->nick,
		       "You are not a channel operator on [\002%s\002]",
		       av[1]);
		return;
	}

	cptr = MakeChan();

	if (!ChangeChanPass(cptr, av[2]))
	{
		notice(n_ChanServ, lptr->nick, "Register failed");
		RecordCommand("%s: failed to create password for registered channel [%s]",
		              n_ChanServ, av[1]);
		MyFree(cptr);
		return;
	}

	cptr->name = MyStrdup(av[1]);
	cptr->founder = MyStrdup(nptr->nick);
	cptr->created = cptr->lastused = cptr->last_founder_active = current_ts;
	SetDefaultALVL(cptr);

	/* Add Secure flag to channel by default (advice by harly) -kre */
	cptr->flags |= CS_SECURE;

	/*
	 * A Founder of a chanel should have CA_FOUNDER access.
	 * $you> /msg chanserv register #alabala blabla
	 * $you> /msg chanserv access #alabala del $you
	 * -- *Serv is gone fishing (SEGFAULT)
	 * -ags
	 */
	AddAccess(cptr, 0, 0, nptr, cptr->access_lvl[CA_FOUNDER], current_ts,
	          current_ts, "*As Founder*");

	/* add cptr to channel list */
	AddChan(cptr);

	/* add channel to the master's founder channel list */
	AddFounderChannelToNick(&master, cptr);

	/* give lptr founder access */
	AddFounder(lptr, cptr);

	/*
	 * Allow this call to cs_join even if HYBRID_ONLY is defined
	 * so we can call cs_CheckChan() on it
	 */
	cs_join(cptr);

	notice(n_ChanServ, lptr->nick,
	       "The channel [\002%s\002] is now registered under your nickname",
	       av[1]);
	notice(n_ChanServ, lptr->nick, "You have been added as a Founder");
	notice(n_ChanServ, lptr->nick,
	       "The password for %s is [\002%s\002] - Remember this for later use",
	       av[1], av[2]);

	RecordCommand("%s: %s!%s@%s REGISTER [%s]",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              av[1]);
} /* c_register() */

/*
c_drop()
  Drop registered channel av[1]
*/

static void
c_drop(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002DROP <channel> [password]\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ, "DROP");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick, ERR_CH_NOT_REGGED, av[1]);
		return;
	}

	if (ac >= 3)
	{
		if (!pwmatch(cptr->password, av[2]))
		{
			notice(n_ChanServ, lptr->nick, ERR_BAD_PASS);

			RecordCommand("%s: %s!%s@%s failed DROP [%s]",
			              n_ChanServ,
			              lptr->nick,
			              lptr->username,
			              lptr->hostname,
			              cptr->name);

			return;
		}
	}
	else if (!(IsFounder(lptr, cptr)
#ifdef EMPOWERADMINS
			   /* We want empowered (not empoweredmore) admins to be able
				* to drop forbidden and forgotten channels, too. -kre */
	           || (IsValidAdmin(lptr) && cptr->flags & (CS_FORBID | CS_FORGET))
#endif /* EMPOWERADMINS */
	          ))
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002DROP <channel> <password>\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ, "DROP");
		return;
	}

	RecordCommand("%s: %s!%s@%s DROP [%s]",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
	              cptr->name);

	/* everything checks out ok...delete the channel */
	DeleteChan(cptr);

	notice(n_ChanServ, lptr->nick,
	       "The channel [\002%s\002] has been dropped",
	       av[1]);
} /* c_drop() */

/*
c_access()
  Modify the channel access list for av[1]
*/

static void
c_access(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct Command *cmdptr;

	if (ac < 3)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002ACCESS <channel> {ADD|DEL|LIST} [mask [level]]\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ, "ACCESS");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick, ERR_CH_NOT_REGGED, av[1]);
		return;
	}

	if (!HasAccess(cptr, lptr, CA_ACCESS))
	{
		notice(n_ChanServ, lptr->nick, ERR_NEED_ACCESS,
		       cptr->access_lvl[CA_ACCESS], "ACCESS", cptr->name);
		RecordCommand("%s: %s!%s@%s failed ACCESS [%s] %s %s %s",
		              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
		              cptr->name, StrToupper(av[2]),
		              (ac >= 4) ? av[3] : "", (ac >= 5) ? av[4] : "");
		return;
	}

	cmdptr = GetCommand(accesscmds, av[2]);

	if (cmdptr && (cmdptr != (struct Command *) -1))
	{
		/* call cmdptr->func to execute command */
		(*cmdptr->func)(lptr, nptr, ac, av);
	}
	else
	{
		/* the option they gave was not valid */
		notice(n_ChanServ, lptr->nick, "%s switch [\002%s\002]",
		       (cmdptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
		       av[2]);
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ, "ACCESS");
		return;
	}

	return;
} /* c_access() */

static void
c_access_add(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	char hostmask[MAXLINE + 1];
	struct NickInfo *nickptr = NULL;
	int level; /* lptr->nick's access level */
	int newlevel, founder;

	if (!(cptr = FindChan(av[1])))
		return;

	if (ac < 5)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002ACCESS <channel> ADD <hostmask> <level>\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO, n_ChanServ, "ACCESS ADD");
		return;
	}

	if (!HasAccess(cptr, lptr, CA_ACCESS))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_NEED_ACCESS, cptr->access_lvl[CA_ACCESS], "ACCESS ADD",
		       cptr->name);
		RecordCommand("%s: %s!%s@%s failed ACCESS [%s] ADD %s %s",
		              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, cptr->name,
		              av[3], av[4]);
		return;
	}

	if (atoi(av[4]) < (-DefaultAccess[CA_FOUNDER]))
	{
		notice(n_ChanServ, lptr->nick,
		       "You cannot add an access level lower than [\002-%d\002]",
		       DefaultAccess[CA_FOUNDER]);
		return;
	}

	founder = IsFounder(lptr, cptr);
	newlevel = atoi(av[4]);
	level = GetAccess(cptr, lptr);

	/* Channel founder should be able to add CA_FOUNDER access level
	 * to the list. -harly & kre */
	if ((founder && (newlevel > cptr->access_lvl[CA_FOUNDER])) ||
	        (!founder && (level <= newlevel)))
	{
		{
			notice(n_ChanServ, lptr->nick,
			       "You cannot add an access level greater than [\002%d\002]",
			       founder ? cptr->access_lvl[CA_FOUNDER] : level - 1);
			RecordCommand("%s: %s!%s@%s failed ACCESS [%s] ADD %s %s",
			              n_ChanServ, lptr->nick, lptr->username,
			              lptr->hostname, cptr->name, av[3], av[4]);
			return;
		}
	}

	hostmask[0] = '\0';

	if (match("*!*@*", av[3]))
		strlcpy(hostmask, av[3], sizeof(hostmask));
	else if (match("*!*", av[3]))
	{
		strlcpy(hostmask, av[3], sizeof(hostmask));
		strlcat(hostmask, "@*", sizeof(hostmask));
	}
	else if (match("*@*", av[3]))
	{
		strlcpy(hostmask, "*!", sizeof(hostmask));
		strlcat(hostmask, av[3], sizeof(hostmask));
	}
	else if (match("*.*", av[3]))
	{
		strlcpy(hostmask, "*!*@", sizeof(hostmask));
		strlcat(hostmask, av[3], sizeof(hostmask));
	}
	else
	{
		/* it must be a nickname */
		if (!(nickptr = FindNick(av[3])))
		{
			notice(n_ChanServ, lptr->nick, "Nickname [\002%s\002] is not registered. Please use either registered nickname or proper mask (nick!user@host, user@host or just host)", av[3]);
			return;
		}

		if (nickptr->flags & NS_NOADD)
		{
			notice(n_ChanServ, lptr->nick, "Nickname [\002%s\002] cannot be added to channel access lists", av[3]);
			return;
		}
	}

	/* add hostmask (or nickptr) to the access list */
	if (AddAccess(cptr, lptr, hostmask, nickptr, newlevel, current_ts, 0, lptr->nick))
	{
		notice(n_ChanServ, lptr->nick,
		       "[\002%s\002] has been added to the access list "
		       "for %s with level [\002%d\002]",
		       nickptr ? nickptr->nick : hostmask, cptr->name, newlevel);

		RecordCommand("%s: %s!%s@%s ACCESS [%s] ADD %s %d",
		              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, cptr->name,
		              (nickptr != NULL) ? nickptr->nick : (hostmask[0] != '\0') ? hostmask : "unknown!",
		              newlevel);

		if ((cptr->flags & CS_VERBOSE))
		{
			char line[MAXLINE + 1];
			ircsprintf(line, "%s!%s@%s ACCESS [%s] ADD %s %d", lptr->nick,
			           lptr->username, lptr->hostname, cptr->name, nickptr ?
			           nickptr->nick : (hostmask[0] != '\0') ? hostmask : "unknown!", newlevel);
			chanopsnotice(FindChannel(cptr->name), line);
		}

		/* Notify user -KrisDuv */
		if (nickptr && (nickptr->flags & NS_IDENTIFIED))
		{
			struct Channel *chptr = FindChannel(cptr->name);

			if (cptr->flags & CS_VERBOSE)
				notice(n_ChanServ, nickptr->nick,
				       "You have been added to the access list for %s "
				       "with level [\002%d\002]", cptr->name, newlevel);

			/* autoop him if newlevel >= CA_AUTOOP */
			if (chptr)
			{
				struct Luser *luptr = FindClient(nickptr->nick);

				if (!IsChannelOp(chptr, luptr) &&
				        (newlevel >= cptr->access_lvl[CA_AUTOOP]))
				{
					char modes[MAXLINE + 1];
					ircsprintf(modes, "+o %s", nickptr->nick);
					toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
					UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
				}
			}
		}
	}
	else
	{
		notice(n_ChanServ, lptr->nick,
		       "You may not modify the access entry [\002%s\002]",
		       nickptr ? nickptr->nick : hostmask);

		RecordCommand("%s: %s!%s@%s failed ACCESS [%s] ADD %s %s",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name,
		              av[3],
		              av[4]);
	}
} /* c_access_add() */

static void
c_access_del(struct Luser *lptr, struct NickInfo *nptr,
             int ac, char **av)

{
	struct ChanInfo *cptr;
	struct ChanAccess *temp;
	char *host;
	int cnt, idx;
	struct NickInfo *nickptr;

	if (!(cptr = FindChan(av[1])))
		return;

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002ACCESS <channel> DEL <hostmask | index>\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "ACCESS DEL");
		return;
	}

	if (!HasAccess(cptr, lptr, CA_ACCESS))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_NEED_ACCESS, cptr->access_lvl[CA_ACCESS], "ACCESS DEL",
		       cptr->name);
		RecordCommand("%s: %s!%s@%s failed ACCESS [%s] DEL %s",
		              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, cptr->name,
		              av[3]);
		return;
	}

	host = NULL;
	nickptr = NULL;

	if ((idx = IsNum(av[3])))
	{
		cnt = 0;
		for (temp = cptr->access; temp; temp = temp->next, ++cnt)
		{
			if (idx == (cnt + 1))
			{
				if (temp->hostmask)
					host = MyStrdup(temp->hostmask);
				else
					nickptr = temp->nptr;

				break;
			}
		}

		if (!host && !nickptr)
		{
			notice(n_ChanServ, lptr->nick,
			       "[\002%d\002] is not a valid index",
			       idx);

			notice(n_ChanServ, lptr->nick,
			       ERR_MORE_INFO,
			       n_ChanServ,
			       "ACCESS LIST");

			return;
		}
	}
	else if (!(nickptr = FindNick(av[3])))
		host = MyStrdup(av[3]);

	if ((cnt = DelAccess(cptr, lptr, host, nickptr)) > 0)
	{
		notice(n_ChanServ, lptr->nick,
		       "[\002%s\002] has been removed from the access list for %s",
		       nickptr ? nickptr->nick : host,
		       cptr->name);

		RecordCommand("%s: %s!%s@%s ACCESS [%s] DEL %s",
		              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
		              cptr->name, nickptr ? nickptr->nick : host);

		if (cptr->flags & CS_VERBOSE)
		{
			char line[MAXLINE + 1];
			ircsprintf(line, "%s!%s@%s ACCESS [%s] DEL %s",
			           lptr->nick, lptr->username, lptr->hostname, cptr->name,
			           nickptr ? nickptr->nick : host);
			chanopsnotice(FindChannel(cptr->name), line);
		}

		/* Notify user if channel is verbose and user is identified -kre */
		if ((cptr->flags & CS_VERBOSE) && nickptr &&
		        (nickptr->flags & NS_IDENTIFIED))
		{
			notice(n_ChanServ, nickptr->nick,
			       "You have been deleted from the access list for [\002%s\002]",
			       cptr->name);
		}
	}
	else
	{
		if (cnt == (-1))
		{
			notice(n_ChanServ, lptr->nick,
			       "[\002%s\002] matches an access level higher than your own",
			       nickptr ? nickptr->nick : host);

			RecordCommand("%s: %s!%s@%s failed ACCESS [%s] DEL %s",
			              n_ChanServ,
			              lptr->nick,
			              lptr->username,
			              lptr->hostname,
			              cptr->name,
			              nickptr ? nickptr->nick : host);
		}
		else
		{
			notice(n_ChanServ, lptr->nick,
			       "[\002%s\002] was not found on the access list for %s",
			       nickptr ? nickptr->nick : host,
			       cptr->name);

			MyFree(host);

			return;
		}
	}

	MyFree(host);
} /* c_access_del() */

static void
c_access_list(struct Luser *lptr, struct NickInfo *nptr,
              int ac, char **av)

{
	struct ChanInfo *cptr;
	struct ChanAccess *ca;
	int idx;
	char *mask = NULL;

	if (!(cptr = FindChan(av[1])))
		return;


	if (cptr->flags & CS_PRIVATE)
	{
		if (!nptr)
		{
			/* the command requires a registered nickname */

			notice(n_ChanServ, lptr->nick,
			       "Your nickname is not registered");
			notice(n_ChanServ, lptr->nick,
			       "and the channel [\002%s\002] is private",
			       cptr->name);
			notice(n_ChanServ, lptr->nick,
			       ERR_MORE_INFO, n_NickServ, "REGISTER");
			return;
		}

		if (!(nptr->flags & NS_IDENTIFIED))
		{
			notice(n_ChanServ, lptr->nick,
			       "Password identification is required for [\002ACCESS LIST\002]");
			notice(n_ChanServ, lptr->nick,
			       "Type \002/MSG %s IDENTIFY <password>\002 and retry",
			       n_NickServ);
			return;
		}
	}

	if (((cptr->flags & CS_PRIVATE) && GetAccess(cptr, lptr) <= 0) ||
	        GetAccess(cptr, lptr) < 0 )
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_NEED_ACCESS, cptr->flags & CS_PRIVATE ? 1 : 0, "ACCESS LIST",
		       cptr->name);
		RecordCommand("%s: %s!%s@%s failed ACCESS [%s] LIST %s",
		              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, cptr->name,
		              (ac >= 4) ? av[3] : "");
		return;
	}

	if (ac >= 4)
		mask = av[3];

	RecordCommand("%s: %s!%s@%s ACCESS [%s] LIST %s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
	              cptr->name, mask ? mask : "");

	if (cptr->access)
	{
		notice(n_ChanServ, lptr->nick,
		       "-- Access List for [\002%s\002] --",
		       cptr->name);
		notice(n_ChanServ, lptr->nick,
		       "Num Level Hostmask             Since last use Added by");
		notice(n_ChanServ, lptr->nick,
		       "--- ----- --------             -------------- --------");
		idx = 1;
		for (ca = cptr->access; ca; ca = ca->next, idx++)
		{
			if (mask)
				if (match(mask, ca->hostmask ? ca->hostmask : ca->nptr->nick) == 0)
					continue;
			notice(n_ChanServ, lptr->nick,
			       "%-3d %-5d %-20s %-14s %s", idx, ca->level,
			       ca->hostmask ? ca->hostmask : (ca->nptr ? ca->nptr->nick : ""),
			       ca->last_used ? timeago(ca->last_used, 0) : "",
			       ca->added_by);
		}
		notice(n_ChanServ, lptr->nick,
		       "-- End of list --");
	}
	else
	{
		notice(n_ChanServ, lptr->nick,
		       "The access list for [\002%s\002] is empty",
		       cptr->name);
	}
} /* c_access_list() */

/*
c_akick()
  Modify the AKICK list for channel av[1]
*/

static void
c_akick(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct Command *cmdptr;

	if (ac < 3)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002AKICK <channel> {ADD|DEL|LIST} [time] [mask] [reason]\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "AKICK");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_CH_NOT_REGGED,
		       av[1]);
		return;
	}

	if (!HasAccess(cptr, lptr, CA_AKICK))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_NEED_ACCESS,
		       cptr->access_lvl[CA_AKICK],
		       "AKICK",
		       cptr->name);
		RecordCommand("%s: %s!%s@%s failed AKICK [%s] %s %s %s",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name,
		              StrToupper(av[2]),
		              (ac >= 4) ? av[3] : "",
		              (ac >= 5) ? av[4] : "");
		return;
	}

	cmdptr = GetCommand(akickcmds, av[2]);

	if (cmdptr && (cmdptr != (struct Command *) -1))
	{
		/* call cmdptr->func to execute command */
		(*cmdptr->func)(lptr, nptr, ac, av);
	}
	else
	{
		/* the option they gave was not valid */
		notice(n_ChanServ, lptr->nick,
		       "%s switch [\002%s\002]",
		       (cmdptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
		       av[2]);
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "AKICK");
		return;
	}

	return;
} /* c_akick() */

static void
c_akick_add(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	char hostmask[MAXLINE + 1];
	char *reason = NULL;
	int sidx;
	long expires;

	if (!(cptr = FindChan(av[1])))
		return;

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002AKICK <channel> ADD [time] <hostmask> [reason]\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "AKICK ADD");
		return;
	}

	if (MaxAkicks && (cptr->akickcnt >= MaxAkicks))
	{
		notice(n_ChanServ, lptr->nick,
		       "Maximum autokick count for [\002%s\002] has been reached",
		       cptr->name);
		return;
	}

	expires = timestr(av[3]);
	if ((ac > 4) && expires)
		sidx = 4;
	else
		sidx = 3;

	hostmask[0] = '\0';

	if (match("*!*@*", av[sidx]))
		strlcpy(hostmask, av[sidx], sizeof(hostmask));
	else if (match("*!*", av[sidx]))
	{
		strlcpy(hostmask, av[sidx], sizeof(hostmask));
		strlcat(hostmask, "@*", sizeof(hostmask));
	}
	else if (match("*@*", av[sidx]))
	{
		strlcpy(hostmask, "*!", sizeof(hostmask));
		strlcpy(hostmask, av[sidx], sizeof(hostmask));
	}
	else if (match("*.*", av[sidx]))
	{
		strlcpy(hostmask, "*!*@", sizeof(hostmask));
		strlcpy(hostmask, av[sidx], sizeof(hostmask));
	}
	else
	{
		struct Luser *ptr;
		char *mask;

		/* it must be a nickname - try to get hostmask */
		if ((ptr = FindClient(av[sidx])))
		{
			mask = HostToMask(ptr->username, ptr->hostname);
			ircsprintf(hostmask, "*!%s", mask);
			MyFree(mask);
		}
		else
		{
			strlcpy(hostmask, av[sidx], sizeof(hostmask));
			strlcat(hostmask, "!*@*", sizeof(hostmask));
		}
	}

	if (OnAkickList(cptr, hostmask))
	{
		notice(n_ChanServ, lptr->nick,
		       "[\002%s\002] matches a hostmask already on the AutoKick list for %s",
		       hostmask,
		       cptr->name);
		return;
	}

	if (ac < (sidx + 2))
	{
		char temp[MAXLINE + 1];
		ircsprintf(temp, "(by %s)", lptr->nick);
		reason = MyStrdup(temp);
	}
	else
	{
		char temp[MAXLINE + 1];
		int cutoff;

		reason = GetString(ac - (sidx + 1), av + (sidx + 1));
		cutoff = MAXLINE - 7 - strlen(lptr->nick);

		if (strlen(reason) >= cutoff)
			reason[cutoff] = 0;
		ircsprintf(temp, "%s (by %s)", reason, lptr->nick);
		MyFree(reason);
		reason = MyStrdup(temp);
	}

	/* add hostmask to the akick list */
	if (AddAkick(cptr, lptr, hostmask, reason,
	             (expires ? (expires + current_ts) : 0)))
	{
		struct ChannelUser *tempuser;
		char nuhost[MAXUSERLEN + 1];
		struct Channel *chptr;
		char modes[MAXLINE + 1];

		if (!expires)
			notice(n_ChanServ, lptr->nick,
			       "[\002%s\002] has been added to the autokick list for %s with"
			       " reason [%s]", hostmask, cptr->name, reason ? reason : "");
		else
			notice(n_ChanServ, lptr->nick,
			       "[\002%s\002] has been added to the temporary autokick list"
			       " for %s with reason [%s]", hostmask, cptr->name,
			       reason ? reason : "");

		RecordCommand("%s: %s!%s@%s AKICK [%s] ADD %s %s time(%ld)",
		              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
		              cptr->name, hostmask, reason ? reason : "", expires);

		sidx = 1;

		if (!(chptr = FindChannel(cptr->name)))
		{
			MyFree(reason);
			return;
		}

		for (tempuser = chptr->firstuser; tempuser; tempuser = tempuser->next)
		{
			if (FindService(tempuser->lptr))
				continue;

			if (IsFounder(tempuser->lptr, cptr))
				continue;

			nuhost[0] = '\0';
			strlcpy(nuhost, tempuser->lptr->nick, NICKLEN + 1);
			strlcat(nuhost, "!", sizeof(nuhost));
			strlcat(nuhost, tempuser->lptr->username, USERLEN + 1);
			strlcat(nuhost, "@", sizeof(nuhost));
			strlcat(nuhost, tempuser->lptr->hostname, HOSTLEN + 1);

			if (!match(hostmask, nuhost))
				continue;

			if (sidx == 1)
			{
				ircsprintf(modes, "+b %s", hostmask);
				toserv(":%s MODE %s %s\r\n", n_ChanServ, chptr->name, modes);
				UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
				sidx = 0;
			}

			toserv(":%s KICK %s %s :%s\r\n", n_ChanServ, cptr->name,
			       tempuser->lptr->nick, reason ? reason : "");
		}
	}
	else
	{
		notice(n_ChanServ, lptr->nick,
		       "You may not add [\002%s\002] to the autokick list for %s",
		       hostmask, cptr->name);

		RecordCommand("%s: %s!%s@%s failed AKICK [%s] ADD %s %s",
		              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
		              cptr->name, av[3], reason ? reason : "");

		MyFree(reason);
		return;
	}

	MyFree(reason);
} /* c_akick_add() */

static void
c_akick_del(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct AutoKick *temp;
	char *host = NULL;
	int cnt, idx;

	if (!(cptr = FindChan(av[1])))
		return;

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002AKICK <channel> DEL <hostmask | index>\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "AKICK DEL");
		return;
	}

	if ((idx = IsNum(av[3])))
	{
		cnt = 0;
		for (temp = cptr->akick; temp; temp = temp->next, cnt++)
			if (idx == (cnt + 1))
			{
				host = MyStrdup(temp->hostmask);
				break;
			}

		if (!host)
		{
			notice(n_ChanServ, lptr->nick,
			       "[\002%d\002] is not a valid index",
			       idx);
			notice(n_ChanServ, lptr->nick,
			       ERR_MORE_INFO,
			       n_ChanServ,
			       "AKICK LIST");
			return;
		}
	}
	else
	{
		char hostmask[MAXLINE + 1];
		hostmask[0] = '\0';

		if (match("*!*@*", av[3]))
			strlcpy(hostmask, av[3], sizeof(hostmask));
		else if (match("*!*", av[3]))
		{
			strlcpy(hostmask, av[3], sizeof(hostmask));
			strlcat(hostmask, "@*", sizeof(hostmask));
		}
		else if (match("*@*", av[3]))
		{
			strlcpy(hostmask, "*!", sizeof(hostmask));
			strlcat(hostmask, av[3], sizeof(hostmask));
		}
		else if (match("*.*", av[3]))
		{
			strlcpy(hostmask, "*!*@", sizeof(hostmask));
			strlcat(hostmask, av[3], sizeof(hostmask));
		}
		else
		{
			strlcpy(hostmask, av[3], sizeof(hostmask));
			strlcat(hostmask, "!*@*", sizeof(hostmask));
		}
		host = MyStrdup(hostmask);
	}
	if (!DelAkick(cptr, host))
	{
		notice(n_ChanServ, lptr->nick,
		       "[\002%s\002] was not found on the autokick list for %s",
		       host,
		       cptr->name);
		MyFree(host);
		return;
	}

	notice(n_ChanServ, lptr->nick,
	       "[\002%s\002] has been removed from the autokick list for %s",
	       host,
	       cptr->name);

	RecordCommand("%s: %s!%s@%s AKICK [%s] DEL %s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, cptr->name,
	              host);

	if (cptr->flags & CS_VERBOSE)
	{
		char line[MAXLINE + 1];
		ircsprintf(line, "%s!%s@%s AKICK [%s] DEL %s",
		           lptr->nick, lptr->username, lptr->hostname, cptr->name, host);
		chanopsnotice(FindChannel(cptr->name), line);
	}
	MyFree(host);
} /* c_akick_del() */

static void
c_akick_list(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo  *cptr;
	struct AutoKick *ak;
	int idx;
	char *mask = NULL;

	if (!(cptr = FindChan(av[1])))
		return;

	if (ac >= 4)
		mask = av[3];

	RecordCommand("%s: %s!%s@%s AKICK [%s] LIST %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              mask ? mask : "");

	if (cptr->akick)
	{
		notice(n_ChanServ, lptr->nick,
		       "-- AutoKick List for [\002%s\002] --",
		       cptr->name);
		notice(n_ChanServ, lptr->nick,
		       "Num %-30s %-10s Reason",
		       "Hostmask", "Expires");
		notice(n_ChanServ, lptr->nick,
		       "--- %-30s %-10s ------",
		       "--------", "-------");
		idx = 1;
		for (ak = cptr->akick; ak; ak = ak->next, idx++)
		{
			if (mask)
				if (match(mask, ak->hostmask) == 0)
					continue;
			notice(n_ChanServ, lptr->nick, "%-3d %-30s %-10s %s", idx,
			       ak->hostmask, ak->expires ? (ak->expires - current_ts > 0) ?
       timeago((ak->expires - current_ts), 4) : "Expired" :
			       "Never", ak->reason ? ak->reason : "");
		}
		notice(n_ChanServ, lptr->nick,
		       "-- End of list --");
	}
	else
	{
		notice(n_ChanServ, lptr->nick,
		       "The autokick list for [\002%s\002] is empty",
		       cptr->name);
	}
} /* c_akick_list() */

/*
c_list()
  Display list of channels matching av[1]
*/

static void
c_list(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)
{
	struct ChanInfo *temp;
	int IsAnAdmin;
	int ii;
	int mcnt; /* total matches found */
	int acnt; /* total matches - private nicks */
	int cnt;
	int match_flags = 0;

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002LIST <pattern> [options]\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ, "LIST");
		return;
	}

	RecordCommand("%s: %s!%s@%s LIST %s", n_ChanServ, lptr->nick,
	              lptr->username, lptr->hostname, av[1]);

	if (IsValidAdmin(lptr))
		IsAnAdmin = 1;
	else
		IsAnAdmin = 0;

	for (cnt = 2; cnt < ac; cnt++)
	{
		if (!ircncmp(av[cnt], "-forbid", strlen(av[cnt])))
			match_flags |= CS_FORBID;
		else if (!ircncmp(av[cnt], "-private", strlen(av[cnt])))
			match_flags |= CS_PRIVATE;
		else if (!ircncmp(av[cnt], "-forget", strlen(av[cnt])))
			match_flags |= CS_FORGET;
		else if (!ircncmp(av[cnt], "-guard", strlen(av[cnt])))
			match_flags |= CS_GUARD;
		else if (!ircncmp(av[cnt], "-noexpire", strlen(av[cnt])))
			match_flags |= CS_NOEXPIRE;
	}

	acnt = mcnt = 0;
	notice(n_ChanServ, lptr->nick,
	       "-- Listing channels matching [\002%s\002] --",
	       av[1]);

	for (ii = 0; ii < CHANLIST_MAX; ++ii)
	{
		for (temp = chanlist[ii]; temp; temp = temp->next)
		{
			if (match(av[1], temp->name))
			{

				if (match_flags && !(temp->flags & match_flags))
					continue;

				mcnt++;

				if ((IsAnAdmin) || !(temp->flags & CS_PRIVATE))
				{
					char  str[20];

					++acnt;
					if (temp->flags & CS_FORBID)
						strlcpy(str, "<< FORBIDDEN >>", sizeof(str));
					else if (temp->flags & CS_FORGET)
						strlcpy(str, "<< FORGOTTEN >>", sizeof(str));
					else if (temp->flags & CS_PRIVATE)
						strlcpy(str, "<< PRIVATE >>", sizeof(str));
					else if (temp->flags & CS_NOEXPIRE)
						strlcpy(str, "<< NOEXPIRE >>", sizeof(str));
					else if (FindChannel(temp->name))
						strlcpy(str, "<< ACTIVE >>", sizeof(str));
					else
						str[0] = '\0';

					notice(n_ChanServ, lptr->nick,
					       "%-10s %15s created %s ago",
					       temp->name, str, timeago(temp->created, 1));
				}
			}
		}
	}

	notice(n_ChanServ, lptr->nick,
	       "-- End of list (%d/%d matches shown) --",
	       acnt,
	       mcnt);
} /* c_list() */

/*
c_level()
  Change the level(s) for different privileges on channel av[1]
*/

static void
c_level(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (ac < 3)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002LEVEL <channel> {SET|RESET|LIST} [index|type [level]]\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "LEVEL");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_CH_NOT_REGGED,
		       av[1]);
		return;
	}

	if (!irccmp(av[2], "LIST"))
	{
		int ii;

		if (!HasAccess(cptr, lptr, CA_ACCESS))
        {
                notice(n_ChanServ, lptr->nick,
		       		ERR_NEED_ACCESS, cptr->access_lvl[CA_ACCESS], "LEVEL",
		       		cptr->name);

                RecordCommand("%s: %s!%s@%s failed LEVEL [%s] %s %s %s",
                                          n_ChanServ, lptr->nick, lptr->username,
                                          lptr->hostname, cptr->name, av[2],
                              (ac >= 4) ? av[3] : "",
                              (ac >= 5) ? av[4] : "");

                return;
        }

		notice(n_ChanServ, lptr->nick,
		       "-- Access Levels for [\002%s\002] --",
		       cptr->name);
		notice(n_ChanServ, lptr->nick,
		       "Index Level %-9s Description",
		       "Type");
		notice(n_ChanServ, lptr->nick,
		       "----- ----- %-9s -----------",
		       "----");
		for (ii = 0; ii < CA_FOUNDER; ++ii)
		{
			if (cptr->access_lvl[ii] <= cptr->access_lvl[CA_FOUNDER])
				notice(n_ChanServ, lptr->nick,
				       "%-5d %-5d %-9s %s", ii + 1, cptr->access_lvl[ii],
				       accessinfo[ii].cmd, accessinfo[ii].desc);
			else
				notice(n_ChanServ, lptr->nick,
				       "%-5d %-5s %-9s %s", ii + 1, "OFF", accessinfo[ii].cmd,
				       accessinfo[ii].desc);
		}
		notice(n_ChanServ, lptr->nick,
		       "-- End of list --");

		RecordCommand("%s: %s!%s@%s LEVEL [%s] LIST %s %s",
		              n_ChanServ, lptr->nick, lptr->username,
		              lptr->hostname, cptr->name,
		              (ac >= 4) ? av[3] : "",
		              (ac >= 5) ? av[4] : "");
		return;
	}

        if (!IsFounder(lptr, cptr))
        {
                notice(n_ChanServ, lptr->nick,
                       "Founder access is required for [\002LEVEL\002]");
                RecordCommand("%s: %s!%s@%s failed LEVEL [%s] %s %s %s",
                                          n_ChanServ, lptr->nick, lptr->username,
                                          lptr->hostname, cptr->name, av[2],
                              (ac >= 4) ? av[3] : "",
                              (ac >= 5) ? av[4] : "");
                return;
        }


	if (!irccmp(av[2], "SET"))
	{
		int    newlevel, index;

		if (ac < 5)
		{
			notice(n_ChanServ, lptr->nick,
			       "Syntax: \002LEVEL <channel> SET <index|type> <level>\002");
			notice(n_ChanServ, lptr->nick,
			       ERR_MORE_INFO,
			       n_ChanServ,
			       "LEVEL SET");
			return;
		}

		index = (-1);
		if (!IsNum(av[3]))
		{
			if (!irccmp(av[3], "AUTODEOP"))
				index = CA_AUTODEOP;
			else if (!irccmp(av[3], "AUTOVOICE"))
				index = CA_AUTOVOICE;
			else if (!irccmp(av[3], "CMDVOICE"))
				index = CA_CMDVOICE;
			else if (!irccmp(av[3], "ACCESS"))
				index = CA_ACCESS;
			else if (!irccmp(av[3], "CMDINVITE"))
				index = CA_CMDINVITE;
#ifdef HYBRID7_HALFOPS
			/* Add setup for autohalfop and cmdhalfop -Janos */
			else if (!irccmp(av[3], "AUTOHALFOP"))
				index = CA_AUTOHALFOP;
			else if (!irccmp(av[3], "CMDHALFOP"))
				index = CA_CMDHALFOP;
#endif /* HYBRID7_HALFOPS */

			else if (!irccmp(av[3], "AUTOOP"))
				index = CA_AUTOOP;
			else if (!irccmp(av[3], "CMDOP"))
				index = CA_CMDOP;
			else if (!irccmp(av[3], "CMDUNBAN"))
				index = CA_CMDUNBAN;
			else if (!irccmp(av[3], "AUTOKICK"))
				index = CA_AKICK;
			else if (!irccmp(av[3], "CMDCLEAR"))
				index = CA_CMDCLEAR;
			else if (!irccmp(av[3], "SET"))
				index = CA_SET;
			else if (!irccmp(av[3], "SUPEROP"))
				index = CA_SUPEROP;

			if (index == (-1))
			{
				notice(n_ChanServ, lptr->nick,
				       "Invalid type specified [\002%s\002]",
				       av[3]);
				notice(n_ChanServ, lptr->nick,
				       ERR_MORE_INFO,
				       n_ChanServ,
				       "LEVEL LIST");
				return;
			}
		}
		else
		{
			index = atoi(av[3]) - 1;
			if ((index == -1) || (index >= CA_FOUNDER))
			{
				notice(n_ChanServ, lptr->nick,
				       "The index [\002%s\002] is not valid",
				       av[3]);
				return;
			}
		}

		if (!irccmp(av[4], "OFF"))
		{
			cptr->access_lvl[index] = newlevel
			                          = cptr->access_lvl[CA_FOUNDER] + 1;
			notice(n_ChanServ, lptr->nick,
			       "The level required for [\002%s\002] has been changed to \002OFF\002 on %s",
			       accessinfo[index].cmd, cptr->name);
		}
		else
		{
			newlevel = atoi(av[4]);
			if (newlevel >= cptr->access_lvl[CA_FOUNDER])
			{
				notice(n_ChanServ, lptr->nick,
				       "You cannot create a level greater than [\002%d\002]",
				       cptr->access_lvl[CA_FOUNDER]);
				return;
			}

			cptr->access_lvl[index] = newlevel;
			notice(n_ChanServ, lptr->nick,
			       "The level required for [\002%s\002] has been changed to \002%d\002 on %s",
			       accessinfo[index].cmd,
			       newlevel,
			       cptr->name);
		}

		RecordCommand("%s: %s!%s@%s LEVEL [%s] SET %s %s",
		              n_ChanServ, lptr->nick, lptr->username,
		              lptr->hostname, cptr->name, av[3], av[4]);

		return;
	}

	if (!irccmp(av[2], "RESET"))
	{
		int index;

		if (ac < 4)
		{
			notice(n_ChanServ, lptr->nick,
			       "Syntax: \002LEVEL <channel> RESET <index|type|ALL>\002");
			notice(n_ChanServ, lptr->nick,
			       ERR_MORE_INFO,
			       n_ChanServ,
			       "LEVEL RESET");
			return;
		}

		index = (-1);
		if (!IsNum(av[3]))
		{
			if (!irccmp(av[3], "AUTODEOP"))
				index = CA_AUTODEOP;
			else if (!irccmp(av[3], "AUTOVOICE"))
				index = CA_AUTOVOICE;
			else if (!irccmp(av[3], "CMDVOICE"))
				index = CA_CMDVOICE;
			else if (!irccmp(av[3], "ACCESS"))
				index = CA_ACCESS;
			else if (!irccmp(av[3], "CMDINVITE"))
				index = CA_CMDINVITE;
			else if (!irccmp(av[3], "AUTOOP"))
				index = CA_AUTOOP;
#ifdef HYBRID7_HALFOPS
			/* Add setup for autohalfop and cmdhalfop -Janos */
			else if (!irccmp(av[3], "AUTOHALFOP"))
				index = CA_AUTOHALFOP;
			else if (!irccmp(av[3], "CMDHALFOP"))
				index = CA_CMDHALFOP;
#endif /* HYBRID7_HALFOPS */

			else if (!irccmp(av[3], "CMDOP"))
				index = CA_CMDOP;
			else if (!irccmp(av[3], "CMDUNBAN"))
				index = CA_CMDUNBAN;
			else if (!irccmp(av[3], "AUTOKICK"))
				index = CA_AKICK;
			else if (!irccmp(av[3], "CMDCLEAR"))
				index = CA_CMDCLEAR;
			else if (!irccmp(av[3], "SET"))
				index = CA_SET;
			else if (!irccmp(av[3], "SUPEROP"))
				index = CA_SUPEROP;
			else if (!irccmp(av[3], "ALL"))
				index = (-2);

			if (index == (-1))
			{
				notice(n_ChanServ, lptr->nick,
				       "Invalid type specified [\002%s\002]",
				       av[3]);
				notice(n_ChanServ, lptr->nick,
				       ERR_MORE_INFO,
				       n_ChanServ,
				       "LEVEL LIST");
				return;
			}
		}
		else
		{
			index = atoi(av[3]) - 1;
			if ((index == -1) || (index >= CA_FOUNDER))
			{
				notice(n_ChanServ, lptr->nick,
				       "The index [\002%s\002] is not valid",
				       av[3]);
				return;
			}
		}

		if (index == (-2))
		{
			/* reset everything */
			SetDefaultALVL(cptr);
			notice(n_ChanServ, lptr->nick,
			       "The access level list for %s has been reset",
			       cptr->name);
		}
		else
		{
			cptr->access_lvl[index] = DefaultAccess[index];
			notice(n_ChanServ, lptr->nick,
			       "The level required for [\002%s\002] has been reset to \002%d\002 on %s",
			       accessinfo[index].cmd,
			       cptr->access_lvl[index],
			       cptr->name);
		}

		RecordCommand("%s: %s!%s@%s LEVEL [%s] RESET %s",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name,
		              av[3]);

		return;
	} /* if (!irccmp(av[2], "RESET")) */

	/* the option they gave was not a SET, RESET, or LIST */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002LEVEL <channel> {SET|RESET|LIST} [index|type [level]]\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO,
	       n_ChanServ,
	       "LEVEL");

	return;
} /* c_level() */

/*
c_identify()
  Check if lptr->nick supplied the correct founder password for av[1]
*/

static void
c_identify(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	char nmask[MAXLINE + 1];

	if (ac < 3)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002IDENTIFY <channel> <password>\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "IDENTIFY");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_CH_NOT_REGGED,
		       av[1]);
		return;
	}

	if (!pwmatch(cptr->password, av[2]))
	{
		notice(n_ChanServ, lptr->nick, ERR_BAD_PASS);

		RecordCommand("%s: %s!%s@%s failed IDENTIFY [%s]",
		              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, av[1]);

		return;
	}

	if (IsFounder(lptr, cptr))
	{
		notice(n_ChanServ, lptr->nick,
		       "You already have founder access to [\002%s\002]",
		       av[1]);
		return;
	}

	AddFounder(lptr, cptr);

	notice(n_ChanServ, lptr->nick,
	       "Password accepted - you are now recognized as a founder for [\002%s\002]",
	       av[1]);

	ircsprintf(nmask, "%s!%s@%s", lptr->nick, lptr->username,
	           lptr->hostname);

	/* If user isn't on the access list when he identifies, give him
	 * FOUNDER access instead of SUPEROP. -harly */
	if (!OnAccessList(cptr, nmask, GetMaster(nptr)))
	{
		AddAccess(cptr, (struct Luser *) NULL, NULL, nptr,
		          cptr->access_lvl[CA_FOUNDER], current_ts, current_ts, "*As Founder*");
		notice(n_ChanServ, lptr->nick,
		       "You have been added to %s as a \002founder\002",
		       cptr->name);
	}

	RecordCommand("%s: %s!%s@%s IDENTIFY [%s]",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
	              cptr->name);
} /* c_identify() */

/*
c_set()
  Set various channel options
*/

static void
c_set(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr = NULL;
	struct Command *cmdptr = NULL;

	if (ac < 3)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002SET <channel> <option> [parameter]\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "SET");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_CH_NOT_REGGED,
		       av[1]);
		return;
	}

	if (!HasAccess(cptr, lptr, CA_SET))
	{
		char tmp[MAXLINE + 1];

		notice(n_ChanServ, lptr->nick,
		       ERR_NEED_ACCESS, cptr->access_lvl[CA_SET], "SET", av[1]);

		/* static buffers .. argh */
		strlcpy(tmp, StrToupper(av[2]), sizeof(tmp));

		RecordCommand("%s: %s!%s@%s failed SET [%s] %s",
		              n_ChanServ, lptr->nick, lptr->username,
		              lptr->hostname, cptr->name, tmp);

		return;
	}

	cmdptr = GetCommand(setcmds, av[2]);

	if (cmdptr && (cmdptr != (struct Command *) -1))
	{
		/* call cmdptr->func to execute command */
		(*cmdptr->func)(lptr, nptr, ac, av);
	}
	else
	{
		/* the option they gave was not valid */
		notice(n_ChanServ, lptr->nick,
		       "%s switch [\002%s\002]",
		       (cmdptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
		       av[2]);
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "SET");
		return;
	}
} /* c_set() */

static void
c_set_topiclock(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	RecordCommand("%s: %s!%s@%s SET [%s] TOPICLOCK %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              (ac < 4) ? "" : StrToupper(av[3]));

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Topic Lock for channel %s is [\002%s\002]",
		       cptr->name,
		       (cptr->flags & CS_TOPICLOCK) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[3], "ON"))
	{
		cptr->flags |= CS_TOPICLOCK;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Topic Lock for channel %s [\002ON\002]",
		       cptr->name);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		cptr->flags &= ~CS_TOPICLOCK;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Topic Lock for channel %s [\002OFF\002]",
		       cptr->name);
		return;
	}

	/* user gave an unknown param */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002SET <channel> TOPICLOCK {ON|OFF}\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO,
	       n_ChanServ,
	       "SET TOPICLOCK");
} /* c_set_topiclock() */

static void
c_set_private(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	RecordCommand("%s: %s!%s@%s SET [%s] PRIVATE %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              (ac < 4) ? "" : StrToupper(av[3]));

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Privacy for channel %s is [\002%s\002]",
		       cptr->name,
		       (cptr->flags & CS_PRIVATE) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[3], "ON"))
	{
		cptr->flags |= CS_PRIVATE;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Privacy for channel %s [\002ON\002]",
		       cptr->name);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		cptr->flags &= ~CS_PRIVATE;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Privacy for channel %s [\002OFF\002]",
		       cptr->name);
		return;
	}

	/* user gave an unknown param */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002SET <channel> PRIVATE {ON|OFF}\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO,
	       n_ChanServ,
	       "SET PRIVATE");
} /* c_set_private() */

/*
 * Set verbose mode on channel - code from IrcBg, slightly modified. -kre
 */
static void c_set_verbose(struct Luser *lptr, struct NickInfo *nptr, int
                          ac, char **av)
{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	RecordCommand("%s: %s!%s@%s SET [%s] VERBOSE %s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, cptr->name,
	              (ac < 4) ? "" : StrToupper(av[3]));

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Verbose for channel %s is [\002%s\002]",
		       cptr->name,
		       (cptr->flags & CS_VERBOSE) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[3], "ON"))
	{
		cptr->flags |= CS_VERBOSE;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Verbose for channel %s [\002ON\002]",
		       cptr->name);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		cptr->flags &= ~CS_VERBOSE;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Verbose for channel %s [\002OFF\002]",
		       cptr->name);
		return;
	}

	/* user gave an unknown param */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002SET <channel> VERBOSE {ON|OFF}\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO, n_ChanServ, "SET VERBOSE");

} /* c_set_verbose() */

/*
 * Notice channel ops on message when set verbose is on. Code from IrcBg.
 * -kre
 */
void chanopsnotice(struct Channel *cptr, char* msg )
{
	struct ChannelUser *tempuser;

	if (!cptr)
		return;

	for (tempuser = cptr->firstuser; tempuser; tempuser = tempuser->next)
	{
		if (IsChannelOp(cptr, tempuser->lptr))
		{
			if (FindService(tempuser->lptr))
				continue;

			notice(n_ChanServ, tempuser->lptr->nick, "%s", msg);
		}
	}
}

static void
c_set_secure(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	RecordCommand("%s: %s!%s@%s SET [%s] SECURE %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              (ac < 4) ? "" : StrToupper(av[3]));

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Security for channel %s is [\002%s\002]",
		       cptr->name,
		       (cptr->flags & CS_SECURE) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[3], "ON"))
	{
		cptr->flags |= CS_SECURE;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Security for channel %s [\002ON\002]",
		       cptr->name);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		cptr->flags &= ~CS_SECURE;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Security for channel %s [\002OFF\002]",
		       cptr->name);
		return;
	}

	/* user gave an unknown param */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002SET <channel> SECURE {ON|OFF}\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO,
	       n_ChanServ,
	       "SET SECURE");
} /* c_set_secure() */

static void
c_set_secureops(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	RecordCommand("%s: %s!%s@%s SET [%s] SECUREOPS %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              (ac < 4) ? "" : StrToupper(av[3]));

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "SecureOps for channel %s is [\002%s\002]",
		       cptr->name,
		       (cptr->flags & CS_SECUREOPS) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[3], "ON"))
	{
		cptr->flags |= CS_SECUREOPS;
		notice(n_ChanServ, lptr->nick,
		       "Toggled SecureOps for channel %s [\002ON\002]",
		       cptr->name);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		cptr->flags &= ~CS_SECUREOPS;
		notice(n_ChanServ, lptr->nick,
		       "Toggled SecureOps for channel %s [\002OFF\002]",
		       cptr->name);
		return;
	}

	/* user gave an unknown param */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002SET <channel> SECUREOPS {ON|OFF}\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO,
	       n_ChanServ,
	       "SET SECUREOPS");
} /* c_set_secureops() */

static void
c_set_splitops(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	RecordCommand("%s: %s!%s@%s SET [%s] SPLITOPS %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              (ac < 4) ? "" : StrToupper(av[3]));

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "SplitOps for channel %s is [\002%s\002]",
		       cptr->name,
		       (cptr->flags & CS_SPLITOPS) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[3], "ON"))
	{
		cptr->flags |= CS_SPLITOPS;
		notice(n_ChanServ, lptr->nick,
		       "Toggled SplitOps for channel %s [\002ON\002]",
		       cptr->name);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		cptr->flags &= ~CS_SPLITOPS;
		notice(n_ChanServ, lptr->nick,
		       "Toggled SplitOps for channel %s [\002OFF\002]",
		       cptr->name);
		return;
	}

	/* user gave an unknown param */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002SET <channel> SPLITOPS {ON|OFF}\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO,
	       n_ChanServ,
	       "SET SPLITOPS");
} /* c_set_splitops() */

static void c_set_restricted(struct Luser *lptr, struct NickInfo *nptr,
                             int ac, char **av)
{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	RecordCommand("%s: %s!%s@%s SET [%s] RESTRICTED %s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
	              cptr->name,
	              (ac < 4) ? "" : StrToupper(av[3]));

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Restricted Access for channel %s is [\002%s\002]",
		       cptr->name,
		       (cptr->flags & CS_RESTRICTED) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[3], "ON"))
	{
		cptr->flags |= CS_RESTRICTED;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Restricted Access for channel %s [\002ON\002]",
		       cptr->name);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		cptr->flags &= ~CS_RESTRICTED;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Restricted Access for channel %s [\002OFF\002]",
		       cptr->name);

		/* Leave channel if there is no need to stay -kre */
		if (!cs_ShouldBeOnChan(cptr))
			cs_part(FindChannel(cptr->name)); /* leave the channel */

		return;
	}

	/* user gave an unknown param */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002SET <channel> RESTRICTED {ON|OFF}\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO,
	       n_ChanServ,
	       "SET RESTRICTED");
} /* c_set_restricted() */

static void
c_set_forget(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	if (!IsFounder(lptr, cptr))
	{
		notice(n_ChanServ, lptr->nick,
		       "Founder access is required for [\002SET FORGET\002]");
		RecordCommand("%s: %s!%s@%s failed SET [%s] FORGET %s",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name,
		              (ac < 4) ? "" : StrToupper(av[3]));
		return;
	}

	RecordCommand("%s: %s!%s@%s SET [%s] FORGET %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              (ac < 4) ? "" : StrToupper(av[3]));

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Forgotten for channel %s is [\002%s\002]",
		       cptr->name,
		       (cptr->flags & CS_FORGET) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[3], "ON"))
	{
		cptr->flags |= CS_FORGET;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Forgotten for channel %s [\002ON\002]",
		       cptr->name);

		if (cs_ShouldBeOnChan(cptr))
			cs_part(FindChannel(cptr->name)); /* leave the channel */

		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		cptr->flags &= ~CS_FORGET;
		notice(n_ChanServ, lptr->nick,
		       "Toggled Forgotten for channel %s [\002OFF\002]",
		       cptr->name);
		return;
	}

	/* user gave an unknown param */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002SET <channel> FORGET {ON|OFF}\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO,
	       n_ChanServ,
	       "SET FORGET");
} /* c_set_forget() */

static void
c_set_guard(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	RecordCommand("%s: %s!%s@%s SET [%s] GUARD %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              (ac < 4) ? "" : StrToupper(av[3]));

	if (!AllowGuardChannel)
	{
		notice(n_ChanServ, lptr->nick,
		       "ChanGuard option is currently \002disabled\002");
		return;
	}

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "ChanGuard for channel %s is [\002%s\002]",
		       cptr->name,
		       (cptr->flags & CS_GUARD) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[3], "ON"))
	{
		cptr->flags |= CS_GUARD;
		notice(n_ChanServ, lptr->nick,
		       "Toggled ChanGuard for channel %s [\002ON\002]",
		       cptr->name);
		cs_join(cptr);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		cptr->flags &= ~CS_GUARD;
		notice(n_ChanServ, lptr->nick,
		       "Toggled ChanGuard for channel %s [\002OFF\002]",
		       cptr->name);
		cs_part(FindChannel(cptr->name));
		return;
	}

	/* user gave an unknown param */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002SET <channel> GUARD {ON|OFF}\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO,
	       n_ChanServ,
	       "SET GUARD");
} /* c_set_guard() */

static void
c_set_password(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	if (!IsFounder(lptr, cptr))
	{
		notice(n_ChanServ, lptr->nick,
		       "Only founders can change the channel password");
		RecordCommand("%s: %s!%s@%s failed SET [%s] PASSWORD ...",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name);
		return;
	}

	RecordCommand("%s: %s!%s@%s SET [%s] PASSWORD ...",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name);

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002SET <channel> PASSWORD <newpass>\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "SET PASSWORD");
		return;
	}

	if (!ChangeChanPass(cptr, av[3]))
	{
		notice(n_ChanServ, lptr->nick,
		       "Password change failed");
		return;
	}
	else
	{
		notice(n_ChanServ, lptr->nick,
		       "The password for %s has been changed to [\002%s\002]",
		       cptr->name,
		       av[3]);
		return;
	}
} /* c_set_password() */

static void
c_set_founder(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct NickInfo *fptr;

	if (!(cptr = FindChan(av[1])))
		return;

	if (!IsFounder(lptr, cptr))
	{
		notice(n_ChanServ, lptr->nick,
		       "You must IDENTIFY as a founder before resetting the founder nickname");
		RecordCommand("%s: %s!%s@%s failed SET [%s] FOUNDER %s",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name,
		              (ac < 4) ? "" : av[3]);

		return;
	}

	RecordCommand("%s: %s!%s@%s SET [%s] FOUNDER %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              (ac < 4) ? "" : av[3]);

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002SET <channel> FOUNDER <nickname>\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "SET FOUNDER");
		return;
	}

	if (!(fptr = GetLink(av[3])))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_NOT_REGGED,
		       av[3]);
		return;
	}

    if (fptr->flags & NS_NOADD)
    {
        notice(n_ChanServ, lptr->nick,
               "The nickname [\002%s\002] cannot be set founder", fptr->nick);
        return;
    }

	/* We should check the nickname's age if we require nicks to have
	 * specified minimum age to be able to become founders. */
	if (MinNickAge)
	{
		if (current_ts - fptr->created < MinNickAge &&
				!IsValidAdmin(lptr))
		{
			notice(n_ChanServ, lptr->nick,
					"The nickname [\002%s\002] must be older than \002%s\002 in order to be a founder",
					fptr->nick, timeago(MinNickAge, 3));
			return;
		}
	}

	RemoveFounderChannelFromNick(&fptr, cptr);

	if (cptr->founder)
	{
		struct NickInfo *oldnick = GetLink(cptr->founder);

		if (oldnick)
			RemoveFounderChannelFromNick(&oldnick, cptr);

		MyFree(cptr->founder);
	}

	AddFounderChannelToNick(&fptr, cptr);

	cptr->founder = MyStrdup(fptr->nick);


	notice(n_ChanServ, lptr->nick,
	       "The founder nickname for %s has been changed to [\002%s\002]",
	       cptr->name,
	       fptr->nick);
} /* c_set_founder() */

static void
c_set_successor(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct NickInfo *fptr;

	if (!(cptr = FindChan(av[1])))
		return;

	if (!IsFounder(lptr, cptr))
	{
		notice(n_ChanServ, lptr->nick,
		       "You must IDENTIFY as a founder before setting the successor nickname");
		RecordCommand("%s: %s!%s@%s failed SET [%s] SUCCESSOR %s",
		              n_ChanServ, lptr->nick, lptr->username,
		              lptr->hostname, cptr->name,
		              (ac < 4) ? "" : av[3]);
		return;
	}

	RecordCommand("%s: %s!%s@%s SET [%s] SUCCESSOR %s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
	              cptr->name,
	              (ac < 4) ? "" : av[3]);

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002SET <channel> SUCCESSOR <nickname|->\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO, n_ChanServ, "SET SUCCESSOR");
		return;
	}

	if (!irccmp(av[3], "-"))
	{
		MyFree(cptr->successor);
		cptr->successor = NULL;
		notice(n_ChanServ, lptr->nick,
		       "The successor nickname for %s has been cleared", cptr->name);
		return;
	}

	if (!(fptr = FindNick(av[3])))
	{
		notice(n_ChanServ, lptr->nick, ERR_NOT_REGGED, av[3]);
		return;
	}

    if (fptr->flags & NS_NOADD)
    {
        notice(n_ChanServ, lptr->nick,
               "The nickname [\002%s\002] cannot be set successor", fptr->nick);
        return;
    }

	MyFree(cptr->successor);

	cptr->successor = MyStrdup(fptr->nick);

	notice(n_ChanServ, lptr->nick,
	       "The successor nickname for %s has been changed to [\002%s\002]",
	       cptr->name,
	       fptr->nick);
} /* c_set_successor() */

static void
c_set_mlock(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	char modes[MAXLINE + 1];
	struct Channel *chptr;
	int minus = 0, argidx = 4;
	unsigned int ii;

	if (!(cptr = FindChan(av[1])))
		return;

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002SET <channel> MLOCK <modes>\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "SET MLOCK");
		return;
	}

	RecordCommand("%s: %s!%s@%s SET [%s] MLOCK %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              av[3]);

	cptr->modes_on = cptr->modes_off = 0;
	cptr->limit = 0;
	if (cptr->key)
	{
		MyFree(cptr->key);
		cptr->key = NULL;
	}
#ifdef DANCER
	if (cptr->forward)
	{
		MyFree(cptr->forward);
		cptr->forward = NULL;
	}
#endif /* DANCER */

	for (ii = 0; ii < strlen(av[3]); ++ii)
	{
		switch (av[3][ii])
		{
		case '+':
			{
				minus = 0;
				break;
			}
		case '-':
			{
				minus = 1;
				break;
			}

		case 'n':
		case 'N':
			{
				if (minus)
					cptr->modes_off |= MODE_N;
				else
					cptr->modes_on |= MODE_N;
				break;
			}

		case 't':
		case 'T':
			{
				if (minus)
					cptr->modes_off |= MODE_T;
				else
					cptr->modes_on |= MODE_T;
				break;
			}

		case 's':
		case 'S':
			{
				if (minus)
					cptr->modes_off |= MODE_S;
				else
					cptr->modes_on |= MODE_S;
				break;
			}

		case 'p':
		case 'P':
			{
				if (minus)
					cptr->modes_off |= MODE_P;
				else
					cptr->modes_on |= MODE_P;
				break;
			}

		case 'm':
		case 'M':
			{
				if (minus)
					cptr->modes_off |= MODE_M;
				else
					cptr->modes_on |= MODE_M;
				break;
			}

		case 'i':
		case 'I':
			{
				if (minus)
					cptr->modes_off |= MODE_I;
				else
					cptr->modes_on |= MODE_I;
				break;
			}

#ifdef DANCER
		case 'c':
		case 'C':
			{
				if (minus)
					cptr->modes_off |= MODE_C;
				else
					cptr->modes_on |= MODE_C;
				break;
			}
#endif /* DANCER */

		case 'l':
		case 'L':
			{
				if (minus)
				{
					cptr->limit = 0;
					cptr->modes_off |= MODE_L;
				}
				else
				{
					if (ac >= (argidx + 1))
						if (IsNum(av[argidx]))
							cptr->limit = atoi(av[argidx++]);
				}
				break;
			}

		case 'k':
		case 'K':
			{
				if (minus)
				{
					if (cptr->key)
						MyFree(cptr->key);
					cptr->key = NULL;
					cptr->modes_off |= MODE_K;
				}
				else
					if (ac >= (argidx + 1))
					{
						if (cptr->key)
							MyFree(cptr->key);
						cptr->key = MyStrdup(av[argidx++]);
					}
				break;
			}

#ifdef DANCER
		case 'f':
		case 'F':
			{
				if (minus)
				{
					if (cptr->forward)
						MyFree(cptr->forward);
					cptr->forward = NULL;
					cptr->modes_off |= MODE_F;
				}
				else
					if (ac >= (argidx + 1))
					{
						if (cptr->forward)
							MyFree(cptr->forward);
						cptr->forward = MyStrdup(av[argidx++]);
					}
				break;
			}
#endif /* DANCER */

		default:
			break;
		} /* switch */
	}

	modes[0] = '\0';
	if (cptr->modes_off)
	{
		strlcat(modes, "-", sizeof(modes));
		if (cptr->modes_off & MODE_S)
			strlcat(modes, "s", sizeof(modes));
		if (cptr->modes_off & MODE_P)
			strlcat(modes, "p", sizeof(modes));
		if (cptr->modes_off & MODE_N)
			strlcat(modes, "n", sizeof(modes));
		if (cptr->modes_off & MODE_T)
			strlcat(modes, "t", sizeof(modes));
		if (cptr->modes_off & MODE_M)
			strlcat(modes, "m", sizeof(modes));
		if (cptr->modes_off & MODE_I)
			strlcat(modes, "i", sizeof(modes));
#ifdef DANCER

		if (cptr->modes_off & MODE_C)
			strlcat(modes, "c", sizeof(modes));
#endif /* DANCER */

		if (cptr->modes_off & MODE_L)
			strlcat(modes, "l", sizeof(modes));
		if (cptr->modes_off & MODE_K)
			strlcat(modes, "k", sizeof(modes));
#ifdef DANCER

		if (cptr->modes_off & MODE_F)
			strlcat(modes, "f", sizeof(modes));
#endif /* DANCER */

	}

#ifdef DANCER
	if (cptr->modes_on || cptr->limit || cptr->key || cptr->forward)
#else

	if (cptr->modes_on || cptr->limit || cptr->key)
#endif /* DANCER */

	{
		strlcat(modes, "+", sizeof(modes));
		if (cptr->modes_on & MODE_S)
			strlcat(modes, "s", sizeof(modes));
		if (cptr->modes_on & MODE_P)
			strlcat(modes, "p", sizeof(modes));
		if (cptr->modes_on & MODE_N)
			strlcat(modes, "n", sizeof(modes));
		if (cptr->modes_on & MODE_T)
			strlcat(modes, "t", sizeof(modes));
#ifdef DANCER

		if (cptr->modes_on & MODE_C)
			strlcat(modes, "c", sizeof(modes));
#endif /* DANCER */

		if (cptr->modes_on & MODE_M)
			strlcat(modes, "m", sizeof(modes));
		if (cptr->modes_on & MODE_I)
			strlcat(modes, "i", sizeof(modes));
		if (cptr->limit)
			strlcat(modes, "l", sizeof(modes));
		if (cptr->key)
			strlcat(modes, "k", sizeof(modes));
#ifdef DANCER

		if (cptr->forward)
			strlcat(modes, "f", sizeof(modes));
#endif /* DANCER */

		if (cptr->limit)
		{
			char  temp[MAXLINE + 1];

			ircsprintf(temp, "%s %ld", modes, cptr->limit);
			strlcpy(modes, temp, sizeof(modes));
		}
		if (cptr->key)
		{
			strlcat(modes, " ", sizeof(modes));
			strlcat(modes, cptr->key, sizeof(modes));
		}
#ifdef DANCER
		if (cptr->forward)
		{
			strlcat(modes, " ", sizeof(modes));
			strlcat(modes, cptr->forward, sizeof(modes));
		}
#endif /* DANCER */
	}

	if (!strlen(modes))
	{
		notice(n_ChanServ, lptr->nick,
			"No valid modes in [\002%s\002], sorry",
			av[3]);
		return;
	}

	if ((chptr = FindChannel(cptr->name)))
	{
		toserv(":%s MODE %s %s %s\r\n",
		       n_ChanServ, cptr->name, modes,
		       (cptr->modes_off & MODE_K) ? chptr->key : "");
		UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
	}

	notice(n_ChanServ, lptr->nick,
	       "Now enforcing modes [\002%s\002] for %s",
	       modes,
	       cptr->name);
} /* c_set_mlock() */

static void
c_set_topic(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	char *topic;

	if (!(cptr = FindChan(av[1])))
		return;

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002SET <channel> TOPIC <topic|->\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "SET TOPIC");
		return;
	}

	if (!irccmp(av[3], "-"))
	{
		if (cptr->topic)
			MyFree(cptr->topic);
		cptr->topic = NULL;
		notice(n_ChanServ, lptr->nick,
		       "The topic for %s has been cleared",
		       cptr->name);
		RecordCommand("%s: %s!%s@%s SET [%s] TOPIC -",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name);
		return;
	}

	topic = GetString(ac - 3, av + 3);

	/* Truncate topiclen. It can't be NULL, since ac would be < 4 -kre */
	if (strlen(topic) > TOPICLEN)
		topic[TOPICLEN] = 0;

	/* if no change in topic (case sensitive), do nothing */
	if ((cptr->topic != NULL) && !strcmp(cptr->topic, topic))
	{
		notice(n_ChanServ, lptr->nick,
				"The channel %s already has that topic",
				cptr->name);
		MyFree(topic);
		return;
	}

	RecordCommand("%s: %s!%s@%s SET [%s] TOPIC %s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, cptr->name,
	              topic);

	MyFree(cptr->topic);

	cptr->topic = topic;

	cs_SetTopic(FindChannel(cptr->name), cptr->topic);

	notice(n_ChanServ, lptr->nick,
	       "The topic for %s has been set to [\002%s\002]",
	       cptr->name, cptr->topic);
} /* c_set_topic() */

static void
c_set_entrymsg(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	char *emsg;

	if (!(cptr = FindChan(av[1])))
		return;

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002SET <channel> ENTRYMSG <message|->\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "SET ENTRYMSG");
		return;
	}

	if (!irccmp(av[3], "-"))
	{
		if (cptr->entrymsg)
			MyFree(cptr->entrymsg);
		cptr->entrymsg = NULL;
		notice(n_ChanServ, lptr->nick,
		       "The entry message for %s has been cleared",
		       cptr->name);
		RecordCommand("%s: %s!%s@%s SET [%s] ENTRYMSG -",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name);
		return;
	}

	emsg = GetString(ac - 3, av + 3);

	RecordCommand("%s: %s!%s@%s SET [%s] ENTRYMSG %s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, cptr->name,
	              emsg);

	if (cptr->entrymsg)
		MyFree(cptr->entrymsg);

	cptr->entrymsg = emsg;

	notice(n_ChanServ, lptr->nick,
	       "The entry message for %s has been set to [\002%s\002]",
	       cptr->name,
	       cptr->entrymsg);
} /* c_set_entrymsg() */

static void
c_set_email(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002SET <channel> EMAIL <email address|->\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "SET EMAIL");
		return;
	}

	if (cptr->email)
		MyFree(cptr->email);

	if (!irccmp(av[3], "-"))
	{
		cptr->email = NULL;
		notice(n_ChanServ, lptr->nick,
		       "The email address for %s has been cleared",
		       cptr->name);
		RecordCommand("%s: %s!%s@%s SET [%s] EMAIL -",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name);
		return;
	}

	RecordCommand("%s: %s!%s@%s SET [%s] EMAIL %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              av[3]);

	cptr->email = MyStrdup(av[3]);

	notice(n_ChanServ, lptr->nick,
	       "The email address for %s has been set to [\002%s\002]",
	       cptr->name,
	       cptr->email);
} /* c_set_email() */

static void
c_set_url(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002SET <channel> URL <http://url|->\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "SET URL");
		return;
	}

	if (cptr->url)
		MyFree(cptr->url);

	if (!irccmp(av[3], "-"))
	{
		cptr->url = NULL;
		notice(n_ChanServ, lptr->nick,
		       "The url for %s has been cleared",
		       cptr->name);
		RecordCommand("%s: %s!%s@%s SET [%s] URL -",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name);
		return;
	}

	RecordCommand("%s: %s!%s@%s SET [%s] URL %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              av[3]);

	cptr->url = MyStrdup(av[3]);

	notice(n_ChanServ, lptr->nick,
	       "The url for %s has been set to [\002%s\002]",
	       cptr->name,
	       cptr->url);
} /* c_set_url() */

/* Set COMMENT line about channel.  -bane
 */
static void
c_set_comment(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	char *cmnt;

	if (!(cptr = FindChan(av[1])))
		return;

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002SET <channel> COMMENT <comment|->\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "SET COMMENT");
		return;
	}

	if (!irccmp(av[3], "-"))
	{
		if (cptr->comment)
			MyFree(cptr->comment);
		cptr->comment = NULL;
		notice(n_ChanServ, lptr->nick,
		       "The comment line for %s has been cleared",
		       cptr->name);
		RecordCommand("%s: %s!%s@%s SET [%s] COMMENT -",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name);
		return;
	}

	cmnt = GetString(ac - 3, av + 3);

	RecordCommand("%s: %s!%s@%s SET [%s] COMMENT %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              cmnt);

	if (cptr->comment)
		MyFree(cptr->comment);

	cptr->comment = cmnt;

	notice(n_ChanServ, lptr->nick,
	       "The comment line for %s has been set to [\002%s\002]",
	       cptr->name,
	       cptr->comment);
} /* c_set_comment() */

#ifdef PUBCOMMANDS
static void
c_set_pubcommands(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (!(cptr = FindChan(av[1])))
		return;

	RecordCommand("%s: %s!%s@%s SET [%s] PUBCOMMANDS %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              (ac < 4) ? "" : StrToupper(av[3]));

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "PubCommands for channel %s is [\002%s\002]",
		       cptr->name,
		       (cptr->flags & CS_PUBCOMMANDS) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[3], "ON"))
	{
		cptr->flags |= CS_PUBCOMMANDS;
		notice(n_ChanServ, lptr->nick,
		       "Toggled PubCommands for channel %s [\002ON\002]",
		       cptr->name);
		return;
	}

	if (!irccmp(av[3], "OFF"))
	{
		cptr->flags &= ~CS_PUBCOMMANDS;
		notice(n_ChanServ, lptr->nick,
		       "Toggled PubCommands for channel %s [\002OFF\002]",
		       cptr->name);
		return;
	}

	/* user gave an unknown param */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002SET <channel> PUBCOMMANDS {ON|OFF}\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO,
	       n_ChanServ,
	       "SET PUBCOMMANDS");
} /* c_set_pubcommands() */
#endif

/*
 * c_modes()
 * shows all modes for a chan, including the key. lvl needed cmd_invite
 * -LHG team
 */
static void c_modes(struct Luser *lptr, struct NickInfo *nptr, int ac,
                    char **av)
{
	struct ChanInfo *cptr;
	struct Channel *chptr;
	char modes[MAXLINE + 1];

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick, "Syntax: \002MODES <channel>\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ, "MODES");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick, ERR_CH_NOT_REGGED, av[1]);
		return;
	}

	if (cptr->flags & CS_FORBID)
	{
		notice(n_ChanServ, lptr->nick, "[\002%s\002] is a forbidden channel",
		       cptr->name);
		return;
	}

	if (cptr->flags & CS_FORGET)
	{
		notice(n_ChanServ, lptr->nick, "[\002%s\002] is a forgotten channel",
		       cptr->name);
		return;
	}

	if (!HasAccess(cptr, lptr, CA_CMDINVITE))
	{
		notice(n_ChanServ, lptr->nick, ERR_NEED_ACCESS,
		       cptr->access_lvl[CA_CMDINVITE], "INVITE", cptr->name);
		RecordCommand("%s: %s!%s@%s failed MODES [%s]",
		              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
		              cptr->name);
		return;
	}

	if (IsChannelMember(FindChannel(av[1]), lptr))
	{
		notice(n_ChanServ, lptr->nick, "You are already on [\002%s\002]",
		       cptr->name);
		return;
	}

	RecordCommand("%s: %s!%s@%s MODES [%s]",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, cptr->name);

	if ((chptr = FindChannel(cptr->name)))
	{
		strlcpy(modes, "+", sizeof(modes));
		if (chptr->modes & MODE_S)
			strlcat(modes, "s", sizeof(modes));
		if (chptr->modes & MODE_P)
			strlcat(modes, "p", sizeof(modes));
		if (chptr->modes & MODE_N)
			strlcat(modes, "n", sizeof(modes));
		if (chptr->modes & MODE_T)
			strlcat(modes, "t", sizeof(modes));
		if (chptr->modes & MODE_M)
			strlcat(modes, "m", sizeof(modes));
		if (chptr->modes & MODE_I)
			strlcat(modes, "i", sizeof(modes));
		if (chptr->limit)
			strlcat(modes, "l", sizeof(modes));
		if (chptr->key[0] != '\0')
			strlcat(modes, "k", sizeof(modes));
		if (chptr->limit)
		{
			char temp[MAXLINE + 1];
			ircsprintf(temp, "%s %d", modes, chptr->limit);
			strlcpy(modes, temp, sizeof(modes));
		}
		if (chptr->key[0] != '\0')
		{
			char temp[MAXLINE + 1];
			ircsprintf(temp, "%s %s", modes, chptr->key);
			strlcpy(modes, temp, sizeof(modes));
		}

		notice(n_ChanServ, lptr->nick,
		       "Channel %s has the following modes: %s",
		       cptr->name, modes);
	}
	else
		notice(n_ChanServ, lptr->nick, "The channel [\002%s\002] is empty",
		       cptr->name);
} /* c_modes() */

/*
 * c_cycle()
 * Cycle individual #channel. -LHG team
 */
static void c_cycle(struct Luser *lptr, struct NickInfo *nptr, int ac,
                    char **av)
{
	struct ChanInfo *cptr;
	struct Channel *chptr;
	char modes[MAXLINE + 1];

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick, "Syntax: \002CYCLE <channel>\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ, "CYCLE");
		return;
	}

	if ((cptr = FindChan(av[1])))
	{
		if (cptr->flags & (CS_FORBID | CS_FORGET))
		{
			notice(n_ChanServ, lptr->nick, "[\002%s\002] is a %s channel",
			       cptr->name, (cptr->flags & CS_FORBID) ? "forbidden" : "forgotten");
			return;
		}

		if (!HasAccess(cptr, lptr, CA_SET))
		{
			notice(n_ChanServ, lptr->nick,
			       ERR_NEED_ACCESS, cptr->access_lvl[CA_SET], "CYCLE", av[1]);
			return;
		}

		RecordCommand("%s: %s!%s@%s CYCLE [%s]", n_ChanServ, lptr->nick,
		              lptr->username, lptr->hostname, av[1]);

		if (!(chptr = FindChannel(cptr->name))) /* should never happen */
			return;

		if (!IsChannelMember(chptr, Me.csptr))
			cs_join(cptr);
		else
			cs_part(chptr);

		if (!(cptr->flags & CS_SECUREOPS))
		{
			cptr->flags |= CS_SECUREOPS;
			cs_CheckChan(cptr, chptr);
			cptr->flags &= ~CS_SECUREOPS;
		}
		else
			cs_CheckChan(cptr, chptr);

		strlcpy(modes, "-", sizeof(modes));
		if ((chptr->modes & MODE_M) && !(cptr->modes_on & MODE_M))
			strlcat(modes, "m", sizeof(modes));
		if ((chptr->modes & MODE_I) && !(cptr->modes_on & MODE_I))
			strlcat(modes, "i", sizeof(modes));
		if ((chptr->limit) && (!cptr->limit))
			strlcat(modes, "l", sizeof(modes));
		if ((chptr->key[0] != '\0') && (!cptr->key))
		{
			strlcat(modes, "k ", sizeof(modes));
			strlcat(modes, chptr->key, sizeof(modes));
		}
		toserv(":%s MODE %s %s\r\n", n_ChanServ, chptr->name, modes);

		UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
		if (cs_ShouldBeOnChan(cptr))
		{
			if (!IsChannelMember(chptr, Me.csptr))
				cs_joinchan(cptr);
		}
		else
			if (IsChannelMember(chptr, Me.csptr))
				cs_part(chptr);

	}
	else
		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is empty or nonexistant, no use to cycle it.",
		       av[1]);
} /* cs_cycle() */

/*
c_invite()
  Invite lptr->nick to channel av[1]
*/

static void
c_invite(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct Channel *chptr;

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002INVITE <channel>\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO,
		       n_ChanServ, "INVITE");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_CH_NOT_REGGED,
		       av[1]);
		return;
	}

	if (cptr->flags & CS_FORBID)
	{
		notice(n_ChanServ, lptr->nick,
		       "[\002%s\002] is a forbidden channel",
		       cptr->name);
		return;
	}

	if (cptr->flags & CS_FORGET)
	{
		notice(n_ChanServ, lptr->nick,
		       "[\002%s\002] is a forgotten channel",
		       cptr->name);
		return;
	}

	if (!HasAccess(cptr, lptr, CA_CMDINVITE))
	{
		notice(n_ChanServ, lptr->nick, ERR_NEED_ACCESS,
		       cptr->access_lvl[CA_CMDINVITE], "INVITE", cptr->name);
		return;
	}

	RecordCommand("%s: %s!%s@%s INVITE [%s]", n_ChanServ, lptr->nick,
	              lptr->username, lptr->hostname, cptr->name);

	if (IsChannelMember(FindChannel(av[1]), lptr))
	{
		notice(n_ChanServ, lptr->nick, "You are already on [\002%s\002]",
		       cptr->name);
		return;
	}

	chptr = FindChannel(cptr->name);

	/*
	 * Everything checks out - invite
	 *
	 * For ircd-hybrid 6+, ChanServ need to be inside the channel
	 * to invite - have ChanServ join and part
	 */

	if (chptr)
	{
		/* no need to give invites if channel is +i */
		if (!(chptr->modes & MODE_I))
		{
			notice(n_ChanServ, lptr->nick,
			       "The channel [\002%s\002] isn't +i", cptr->name);
			return;
		}

		/* it only needs to join if it's not guarding - toot */
		if (!(cptr->flags & CS_GUARD))
		{
			toserv(":%s SJOIN %ld %s + :@%s\r\n",
			       Me.name, chptr->since, cptr->name, n_ChanServ);
		}

		toserv(":%s INVITE %s %s\r\n",
		       n_ChanServ, lptr->nick, cptr->name);

		if ((cptr->flags & CS_VERBOSE))
		{
			char line[MAXLINE + 1];
			ircsprintf(line, "%s!%s@%s INVITE [%s]",
			           lptr->nick, lptr->username, lptr->hostname, cptr->name);
			chanopsnotice(FindChannel(cptr->name), line);
		}

		/* only PART if it's not in GUARD mode - toot */
		/* It will PART also if AllowGuardChannel is not set -kre */
		if (!(cptr->flags & CS_GUARD) || !AllowGuardChannel)
		{
			toserv(":%s PART %s\r\n", n_ChanServ, cptr->name);
		}
	}
	else
	{
		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is empty",
		       cptr->name);
	}
} /* c_invite() */

/*
c_op()
  Op lptr->nick on channel av[1]
*/

static void
c_op(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct Channel *chptr;
	char onicks[MAXLINE + 1], dnicks[MAXLINE + 1];
	struct UserChannel *uchan;
	struct ChannelUser *cuser;
	struct Luser *currlptr;

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002OP <channel> [nicks]\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ, "OP");
		return;
	}

	if (!irccmp(av[1], "ALL"))
		cptr = NULL;
	else if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick, ERR_CH_NOT_REGGED, av[1]);
		return;
	}

	if (HasFlag(lptr->nick, NS_NOCHANOPS))
		return;

	if (!cptr)
	{
		/* They want to be opped in all channels they are currently in. */
		for (uchan = lptr->firstchan; uchan; uchan = uchan->next)
		{
			if (HasAccess(FindChan(uchan->chptr->name), lptr, CA_CMDOP))
			{
				if (!(uchan->flags & CH_OPPED))
				{
					toserv(":%s MODE %s +o %s\r\n", n_ChanServ,
					       uchan->chptr->name, lptr->nick);
					uchan->flags |= CH_OPPED;

					if ((cuser = FindUserByChannel(uchan->chptr, lptr)))
						cuser->flags |= CH_OPPED;
				}
				else
				{
					notice(n_ChanServ, lptr->nick,
					       "You are already opped on [\002%s\002]",
					       uchan->chptr->name);
				}
			}
		}

		notice(n_ChanServ, lptr->nick,
		       "You have been opped on all channels you have access to");

		RecordCommand("%s: %s!%s@%s OP ALL", n_ChanServ, lptr->nick,
		              lptr->username, lptr->hostname);

		return;
	}

	if (!HasAccess(cptr, lptr, CA_CMDOP))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_NEED_ACCESS, cptr->access_lvl[CA_CMDOP], "OP", cptr->name);
		RecordCommand("%s: %s!%s@%s failed OP [%s]", n_ChanServ, lptr->nick,
		              lptr->username, lptr->hostname, cptr->name);
		return;
	}

	chptr = FindChannel(av[1]);
	if (ac < 3)
	{
		if (HasFlag(lptr->nick, NS_NOCHANOPS))
			return;

		if (!IsChannelMember(chptr, lptr))
		{
			notice(n_ChanServ, lptr->nick,
			       "You are not on [\002%s\002]",
			       cptr->name);
			return;
		}

		if (IsChannelOp(chptr, lptr))
		{
			notice(n_ChanServ, lptr->nick,
			       "You are already opped on [\002%s\002]", cptr->name);
			return;
		}

		strlcpy(onicks, lptr->nick, sizeof(onicks));
		dnicks[0] = '\0';
	}
	else
	{
		int ii, jj1 = 1, jj2 = 1, arc;
		char *tempnix, *tempptr, **arv;

		/* they want to op other people */
		tempnix = GetString(ac - 2, av + 2);
		tempptr = tempnix;
		arc = SplitBuf(tempnix, &arv);

		onicks[0] = '\0';
		dnicks[0] = '\0';

		for (ii = 0; ii < arc; ii++)
		{
			if (*arv[ii] == '-')
				currlptr = FindClient(arv[ii] + 1);
			else
				currlptr = FindClient(arv[ii]);

			if (!IsChannelMember(chptr, currlptr) || ((currlptr->server == Me.sptr)
                            && !IsOperator(lptr) && !(lptr->flags & L_OSREGISTERED)))
				continue;

			if ((arv[ii][0] == '-') && (IsChannelOp(chptr, currlptr)))
			{
				if (jj1 * (NICKLEN + 1) >= sizeof(dnicks))
				{
					SetModes(n_ChanServ, 0, 'o', chptr, dnicks);
					dnicks[0] = '\0';
					jj1 = 1;
				}

				strlcat(dnicks, arv[ii] + 1, sizeof(dnicks));
				strlcat(dnicks, " ", sizeof(dnicks));
				++jj1;
			}
			else if (!IsChannelOp(chptr, currlptr))
			{
				struct Luser *alptr = FindClient(arv[ii]);
				if ((cptr->flags & CS_SECUREOPS) && (alptr != lptr) &&
				        (!HasAccess(cptr, alptr, CA_AUTOOP)))
					continue;

				/*
				 * Don't op them if they are flagged
				 */
				if (HasFlag(arv[ii], NS_NOCHANOPS))
					continue;

				if (jj2 * (NICKLEN + 1) >= sizeof(onicks))
				{
					SetModes(n_ChanServ, 1, 'o', chptr, onicks);
					onicks[0] = '\0';
					jj2 = 1;
				}

				strlcat(onicks, arv[ii], sizeof(onicks));
				strlcat(onicks, " ", sizeof(onicks));
				++jj2;
			}
		}

		MyFree(tempptr);
		MyFree(arv);
	}

	SetModes(n_ChanServ, 0, 'o', chptr, dnicks);
	SetModes(n_ChanServ, 1, 'o', chptr, onicks);

	RecordCommand("%s: %s!%s@%s OP [%s]%s%s%s%s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, cptr->name,
	              strlen(onicks) ? " [+] " : "", strlen(onicks) ? onicks : "",
	              strlen(dnicks) ? " [-] " : "", strlen(dnicks) ? dnicks : "");

} /* c_op() */

#ifdef HYBRID7_HALFOPS
/* c_hop()
 * (De)Halfop nicks on channel av[1]
 * -Janos
 *
 * XXX: *Urgh!* Get rid of this nasty long code, merge with c_op() and
 * rewrite the latter.. -kre
 */
static void c_hop(struct Luser *lptr, struct NickInfo *nptr, int ac, char
                  **av)
{
	struct ChanInfo *cptr;
	struct Channel *chptr;
	char hnicks[MAXLINE + 1], dnicks[MAXLINE + 1];
	struct UserChannel *uchan;
	struct ChannelUser *cuser;
	struct Luser *currlptr;

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick,
				"Syntax: \002HALFOP <channel> [nicks]\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ, "HALFOP");
		return;
	}

	if (!irccmp(av[1], "ALL"))
		cptr = NULL;
	else if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick, ERR_CH_NOT_REGGED, av[1]);
		return;
	}

	/* We'll once get NS_NOHALFOPS... */
	if (HasFlag(lptr->nick, NS_NOCHANOPS))
		return;

	if (!cptr)
	{
		/* They want to be opped in all channels they are currently in. */
		for (uchan = lptr->firstchan; uchan; uchan = uchan->next)
		{
			if (HasAccess(FindChan(uchan->chptr->name), lptr, CA_CMDHALFOP))
			{
				if (!(uchan->flags & CH_HOPPED))
				{
					toserv(":%s MODE %s +h %s\r\n", n_ChanServ,
					       uchan->chptr->name, lptr->nick);
					uchan->flags |= CH_HOPPED;

					if ((cuser = FindUserByChannel(uchan->chptr, lptr)))
						cuser->flags |= CH_HOPPED;
				}
				else
				{
					notice(n_ChanServ, lptr->nick,
					       "You are already halfopped on [\002%s\002]",
					       uchan->chptr->name);
				}
			}
		}

		notice(n_ChanServ, lptr->nick,
		       "You have been halfopped on all channels you have access to");

		RecordCommand("%s: %s!%s@%s HALFOP ALL", n_ChanServ, lptr->nick,
		              lptr->username, lptr->hostname);

		return;
	}

	if (!HasAccess(cptr, lptr, CA_CMDHALFOP))
	{
		notice(n_ChanServ, lptr->nick, ERR_NEED_ACCESS,
		       cptr->access_lvl[CA_CMDHALFOP], "HALFOP", cptr->name);
		RecordCommand("%s: %s!%s@%s failed HALFOP [%s]",
		              n_ChanServ, lptr->nick, lptr->username,
		              lptr->hostname, cptr->name);
		return;
	}

	chptr = FindChannel(av[1]);
	if (ac < 3)
	{
		if (HasFlag(lptr->nick, NS_NOCHANOPS))
			return;

		if (!IsChannelMember(chptr, lptr))
		{
			notice(n_ChanServ, lptr->nick,
			       "You are not on [\002%s\002]", cptr->name);
			return;
		}

		if (IsChannelOp(chptr, lptr))
		{
			notice(n_ChanServ, lptr->nick,
			       "You are already opped on [\002%s\002]", cptr->name);
			return;
		}

		strlcpy(hnicks, lptr->nick, sizeof(hnicks));
		dnicks[0] = '\0';
	}
	else
	{
		int ii, jj1 = 1, jj2 = 1, arc;
		char *tempnix, *tempptr, **arv;

		/* they want to halfop other people */
		tempnix = GetString(ac - 2, av + 2);
		tempptr = tempnix;
		arc = SplitBuf(tempnix, &arv);

		hnicks[0] = '\0';
		dnicks[0] = '\0';

		for (ii = 0; ii < arc; ii++)
		{
			if (*arv[ii] == '-')
				currlptr = FindClient(arv[ii] + 1);
			else
				currlptr = FindClient(arv[ii]);

			if (!IsChannelMember(chptr, currlptr) || ((currlptr->server == Me.sptr)
                            && !IsOperator(lptr) && !(lptr->flags & L_OSREGISTERED)))
				continue;

			if (arv[ii][0] == '-')
			{
				if (jj1 * (NICKLEN + 1) >= sizeof(dnicks))
				{
					SetModes(n_ChanServ, 0, 'h', chptr, dnicks);
					dnicks[0] = '\0';
					jj1 = 1;
				}

				strlcat(dnicks, arv[ii] + 1, sizeof(dnicks));
				strlcat(dnicks, " ", sizeof(dnicks));
				++jj1;
			}
			else
			{
				struct Luser *alptr = FindClient(arv[ii]);
				if ((cptr->flags & CS_SECUREOPS) && (alptr != lptr) &&
				        (!HasAccess(cptr, alptr, CA_AUTOHALFOP)))
					continue;

				if (HasFlag(arv[ii], NS_NOCHANOPS))
					continue;

				if (jj2 * (NICKLEN + 1) >= sizeof(hnicks))
				{
					SetModes(n_ChanServ, 1, 'h', chptr, hnicks);
					hnicks[0] = '\0';
					jj2 = 1;
				}

				strlcat(hnicks, arv[ii], sizeof(hnicks));
				strlcat(hnicks, " ", sizeof(hnicks));
				++jj2;
			}
		}

		MyFree(tempptr);
		MyFree(arv);
	}

	SetModes(n_ChanServ, 0, 'h', chptr, dnicks);
	SetModes(n_ChanServ, 1, 'h', chptr, hnicks);

	RecordCommand("%s: %s!%s@%s HALFOP [%s]%s%s%s%s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
	              cptr->name,
	              strlen(hnicks) ? " [+] " : "", strlen(hnicks) ? hnicks : "",
	              strlen(dnicks) ? " [-] " : "", strlen(dnicks) ? dnicks : "");

} /* c_hop() */
#endif /* HYBRID7_HALFOPS */

/*
c_voice()
  (De)Voice nicks on channel av[1]
*/

static void
c_voice(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct Channel *chptr;
	char vnicks[MAXLINE + 1], dnicks[MAXLINE + 1];

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002VOICE <channel>\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "VOICE");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_CH_NOT_REGGED,
		       av[1]);
		return;
	}

	if (!HasAccess(cptr, lptr, CA_CMDVOICE))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_NEED_ACCESS,
		       cptr->access_lvl[CA_CMDVOICE],
		       "VOICE",
		       cptr->name);
		RecordCommand("%s: %s!%s@%s failed VOICE [%s]",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name);
		return;
	}

	chptr = FindChannel(av[1]);
	if (ac < 3)
	{
		strlcpy(vnicks, lptr->nick, sizeof(vnicks));
		dnicks[0] = '\0';
		if (!IsChannelMember(chptr, lptr))
		{
			notice(n_ChanServ, lptr->nick,
			       "You are not on [\002%s\002]",
			       cptr->name);
			return;
		}
	}
	else
	{
		int ii, jj1 = 1, jj2 = 1, arc;
		char *tempnix, *tempptr, **arv;
		struct Luser *currlptr;

		/* they want to voice other people */
		tempnix = GetString(ac - 2, av + 2);

		tempptr = tempnix;
		arc = SplitBuf(tempnix, &arv);

		vnicks[0] = '\0';
		dnicks[0] = '\0';

		for (ii = 0; ii < arc; ii++)
		{
			if (*arv[ii] == '-')
				currlptr = FindClient(arv[ii] + 1);
			else
				currlptr = FindClient(arv[ii]);

			if (!IsChannelMember(chptr, currlptr) || ((currlptr->server == Me.sptr)
                            && !IsOperator(lptr) && !(lptr->flags & L_OSREGISTERED)))
				continue;

			if (arv[ii][0] == '-')
			{
				if (jj1 * (NICKLEN + 1) >= sizeof(dnicks))
				{
					SetModes(n_ChanServ, 0, 'v', chptr, dnicks);
					dnicks[0] = '\0';
					jj1 = 1;
				}

				strlcat(dnicks, arv[ii] + 1, sizeof(dnicks));
				strlcat(dnicks, " ", sizeof(dnicks));
				++jj1;
			}
			else
			{
				if (jj2 * (NICKLEN + 1) >= sizeof(vnicks))
				{
					SetModes(n_ChanServ, 1, 'v', chptr, vnicks);
					vnicks[0] = '\0';
					jj2 = 1;
				}

				strlcat(vnicks, arv[ii], sizeof(vnicks));
				strlcat(vnicks, " ", sizeof(vnicks));
				++jj2;
			}
		}

		MyFree(tempptr);
		MyFree(arv);
	}

	SetModes(n_ChanServ, 0, 'v', chptr, dnicks);
	SetModes(n_ChanServ, 1, 'v', chptr, vnicks);

	RecordCommand("%s: %s!%s@%s VOICE [%s]%s%s%s%s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              strlen(vnicks) ? " [+] " : "",
	              strlen(vnicks) ? vnicks : "",
	              strlen(dnicks) ? " [-] " : "",
	              strlen(dnicks) ? dnicks : "");

	return;
} /* c_voice() */

/*
c_unban()
  Unban all hostmasks matching lptr->nick on channel av[1]
*/

static void
c_unban(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	char chkstr[MAXLINE + 1], bans[MAXLINE + 1];
	struct Channel *chptr;
	struct ChannelBan *bptr;
	int all = 0;
	int jj = 1;

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002UNBAN <channel> [ALL]\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "UNBAN");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_CH_NOT_REGGED,
		       av[1]);
		return;
	}

	if (!HasAccess(cptr, lptr, CA_CMDUNBAN))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_NEED_ACCESS,
		       cptr->access_lvl[CA_CMDUNBAN],
		       "UNBAN",
		       cptr->name);
		RecordCommand("%s: %s!%s@%s failed UNBAN [%s] %s",
		              n_ChanServ,
		              lptr->nick,
		              lptr->username,
		              lptr->hostname,
		              cptr->name,
		              (ac >= 3) ? av[2] : "");
		return;
	}

	ircsprintf(chkstr, "%s!%s@%s", lptr->nick, lptr->username,
	           lptr->hostname);

	if (ac >= 3)
		if (!irccmp(av[2], "ALL"))
		{
			if (!HasAccess(cptr, lptr, CA_SUPEROP))
			{
				notice(n_ChanServ, lptr->nick,
				       "SuperOp privileges are required to unban \002ALL\002 on %s",
				       cptr->name);
				return;
			}
			else
				all = 1;
		}

	if (!(chptr = FindChannel(cptr->name)))
	{
		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is not active",
		       cptr->name);
		return;
	}

	bans[0] = '\0';
	for (bptr = chptr->firstban; bptr; bptr = bptr->next)
	{
		if (all || match(bptr->mask, chkstr))
		{
			if (jj * (MAXUSERLEN + 1) >= sizeof(bans))
			{
				SetModes(n_ChanServ, 0, 'b', chptr, bans);
				bans[0] = '\0';
				jj = 1;
			}

			strlcat(bans, bptr->mask, sizeof(bans));
			strlcat(bans, " ", sizeof(bans));
			++jj;
		}
	}

	SetModes(n_ChanServ, 0, 'b', chptr, bans);

	notice(n_ChanServ, lptr->nick,
	       "All bans matching [\002%s\002] have been cleared on %s",
	       (all) ? "*!*@*" : chkstr,
	       cptr->name);

	RecordCommand("%s: %s!%s@%s UNBAN [%s] %s",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              cptr->name,
	              (all) ? "ALL" : "");

	return;
} /* c_unban() */

/*
 * c_info()
 * Display info about channel av[1]
 */
static void c_info(struct Luser *lptr, struct NickInfo *nptr, int ac, char
                   **av)
{
	struct ChanInfo *cptr;
	struct NickInfo *ni;
	char buf[MAXLINE + 1];
	int founder_online = 0, successor_online = 0;

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick, "Syntax: \002INFO <channel>\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ, "INFO");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick, ERR_CH_NOT_REGGED, av[1]);
		return;
	}

	if (((cptr->flags & CS_PRIVATE) || (cptr->flags & (CS_FORBID | CS_FORGET)))
	        && !IsFounder(lptr, cptr) && !HasAccess(cptr, lptr, CA_CMDINVITE)
#ifdef EMPOWERADMINS
	        && !IsValidAdmin(lptr)
#endif
	   )
	{
		notice(n_ChanServ, lptr->nick, "Channel [\002%s\002] is private",
		       cptr->name);
		return;
	}

	RecordCommand("%s: %s!%s@%s INFO [%s]", n_ChanServ, lptr->nick,
	              lptr->username, lptr->hostname, cptr->name);

	if ((ni = FindNick(cptr->successor)) && (ni->flags & NS_IDENTIFIED))
		successor_online = 1;
	if ((ni = FindNick(cptr->founder)) && (ni->flags & NS_IDENTIFIED))
		founder_online = 1;

	notice(n_ChanServ, lptr->nick,
	       "     Channel: %s",
	       cptr->name);

	if (cptr->founder)
		notice(n_ChanServ, lptr->nick,
		       "     Founder: %s%s%s%s", cptr->founder,
		       founder_online ? " << ONLINE >>" :
		       (cptr->last_founder_active ? ", last seen: " : ""),
		       (founder_online || !cptr->last_founder_active) ? "" :
		       timeago(cptr->last_founder_active, 1),
		       (founder_online || !cptr->last_founder_active) ? "" : " ago");

	if (cptr->successor)
		notice(n_ChanServ, lptr->nick,
		       "   Successor: %s%s%s%s",
		       cptr->successor ? cptr->successor : "",
		       successor_online ? " << ONLINE >>" :
		       (cptr->last_successor_active ? ", last seen: " : ""),
		       (successor_online || !cptr->last_successor_active) ? "" :
		       timeago(cptr->last_successor_active, 1),
		       (successor_online || !cptr->last_successor_active) ? "" : " ago");

	notice(n_ChanServ, lptr->nick, "  Registered: %s ago",
	       timeago(cptr->created, 1));

	notice(n_ChanServ, lptr->nick,
	       "   Last Used: %s ago",
	       timeago(cptr->lastused, 1));

	if (cptr->topic)
		notice(n_ChanServ, lptr->nick,
		       "       Topic: %s",
		       cptr->topic);

	if (cptr->email)
		notice(n_ChanServ, lptr->nick,
		       "       Email: %s",
		       cptr->email);

	if (cptr->url)
		notice(n_ChanServ, lptr->nick,
		       "         Url: %s",
		       cptr->url);

	if (cptr->comment)
		notice(n_ChanServ, lptr->nick,
		       "     Comment: %s",
		       cptr->comment);

	buf[0] = '\0';
	if (cptr->flags & CS_TOPICLOCK)
		strlcat(buf, "TopicLock, ", sizeof(buf));
	if (cptr->flags & CS_SECURE)
		strlcat(buf, "Secure, ", sizeof(buf));
	if (cptr->flags & CS_SECUREOPS)
		strlcat(buf, "SecureOps, ", sizeof(buf));
	if (cptr->flags & CS_GUARD)
		strlcat(buf, "ChanGuard, ", sizeof(buf));
	if (cptr->flags & CS_RESTRICTED)
		strlcat(buf, "Restricted, ", sizeof(buf));
	if (cptr->flags & CS_PRIVATE)
		strlcat(buf, "Private, ", sizeof(buf));
	if (cptr->flags & CS_FORBID)
		strlcat(buf, "Forbidden, ", sizeof(buf));
	if (cptr->flags & CS_FORGET)
		strlcat(buf, "Forgotten, ", sizeof(buf));
	if (cptr->flags & CS_NOEXPIRE)
		strlcat(buf, "NoExpire, ", sizeof(buf));
	if (cptr->flags & CS_SPLITOPS)
		strlcat(buf, "SplitOps, ", sizeof(buf));
	if (cptr->flags & CS_VERBOSE)
		strlcat(buf, "Verbose, ", sizeof(buf));
	if ((cptr->flags & CS_EXPIREBANS) && BanExpire)
		strlcat(buf, "ExpireBans, ", sizeof(buf));
#ifdef PUBCOMMANDS
	if (cptr->flags & CS_PUBCOMMANDS)
		strlcat(buf, "PubCommands, ", sizeof(buf));
#endif

	if (*buf)
	{
		/* kill the trailing "," */
		buf[strlen(buf) - 2] = '\0';
		notice(n_ChanServ, lptr->nick,
		       "     Options: %s",
		       buf);
	}

	if (IsValidAdmin(lptr))
	{
		if (cptr->flags & CS_FORBID)
		{
			if (cptr->forbidby)
				notice(n_ChanServ, lptr->nick, "    Forbid by: %s",
				       cptr->forbidby);
			if (cptr->forbidreason)
				notice(n_ChanServ, lptr->nick, "Forbid reason: %s",
				       cptr->forbidreason);
		}
	}

	buf[0] = '\0';
	if (cptr->modes_off)
	{
		strlcat(buf, "-", sizeof(buf));
		if (cptr->modes_off & MODE_S)
			strlcat(buf, "s", sizeof(buf));
		if (cptr->modes_off & MODE_P)
			strlcat(buf, "p", sizeof(buf));
		if (cptr->modes_off & MODE_N)
			strlcat(buf, "n", sizeof(buf));
		if (cptr->modes_off & MODE_T)
			strlcat(buf, "t", sizeof(buf));
#ifdef DANCER

		if (cptr->modes_off & MODE_C)
			strlcat(buf, "c", sizeof(buf));
#endif /* DANCER */

		if (cptr->modes_off & MODE_M)
			strlcat(buf, "m", sizeof(buf));
		if (cptr->modes_off & MODE_I)
			strlcat(buf, "i", sizeof(buf));
		if (cptr->modes_off & MODE_L)
			strlcat(buf, "l", sizeof(buf));
		if (cptr->modes_off & MODE_K)
			strlcat(buf, "k", sizeof(buf));
#ifdef DANCER

		if (cptr->modes_off & MODE_F)
			strlcat(buf, "f", sizeof(buf));
#endif /* DANCER */

	}
#ifdef DANCER
	if (cptr->modes_on || cptr->limit || cptr->key || cptr->forward)
#else

	if (cptr->modes_on || cptr->limit || cptr->key)
#endif /* DANCER */

	{
		strlcat(buf, "+", sizeof(buf));
		if (cptr->modes_on & MODE_S)
			strlcat(buf, "s", sizeof(buf));
		if (cptr->modes_on & MODE_P)
			strlcat(buf, "p", sizeof(buf));
		if (cptr->modes_on & MODE_N)
			strlcat(buf, "n", sizeof(buf));
		if (cptr->modes_on & MODE_T)
			strlcat(buf, "t", sizeof(buf));
#ifdef DANCER

		if (cptr->modes_on & MODE_C)
			strlcat(buf, "c", sizeof(buf));
#endif /* DANCER */

		if (cptr->modes_on & MODE_M)
			strlcat(buf, "m", sizeof(buf));
		if (cptr->modes_on & MODE_I)
			strlcat(buf, "i", sizeof(buf));
		if (cptr->limit)
			strlcat(buf, "l", sizeof(buf));
		if (cptr->key)
			strlcat(buf, "k", sizeof(buf));
#ifdef DANCER

		if (cptr->forward)
			strlcat(buf, "f", sizeof(buf));
#endif /* DANCER */

	}
	if (buf[0])
		notice(n_ChanServ, lptr->nick,
		       "   Mode Lock: %s",
		       buf);
} /* c_info() */

/*
c_clear()
  Clear modes/bans/ops/halfops/voices from a channel
*/

static void
c_clear(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct Command *cmdptr;

	if (ac < 3)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002CLEAR <channel> {OPS|"
#ifdef HYBRID7_HALFOPS
		       /* Allow halfops for hybrid7 to be cleared, too -kre */
		       "HALFOPS|"
#endif /* HYBRID7_HALFOPS */
		       "VOICES|MODES|BANS|"
#ifdef GECOSBANS
		       "GECOSBANS|"
#endif /* GECOSBANS */
		       "ALL|USERS}\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO,
		       n_ChanServ, "CLEAR");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick, ERR_CH_NOT_REGGED, av[1]);
		return;
	}

	if (!HasAccess(cptr, lptr, CA_CMDCLEAR))
	{
		notice(n_ChanServ, lptr->nick, ERR_NEED_ACCESS,
		       cptr->access_lvl[CA_CMDCLEAR], "CLEAR", cptr->name);
		RecordCommand("%s: %s!%s@%s failed CLEAR [%s] %s",
		              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
		              cptr->name, av[2]);
		return;
	}

	cmdptr = GetCommand(clearcmds, av[2]);

	if (cmdptr && (cmdptr != (struct Command *) -1))
	{
		RecordCommand("%s: %s!%s@%s CLEAR [%s] [%s]",
		              n_ChanServ, lptr->nick, lptr->username,
		              lptr->hostname, cptr->name, cmdptr->cmd);

		/* call cmdptr->func to execute command */
		(*cmdptr->func)(lptr, nptr, ac, av);
	}
	else
	{
		/* the option they gave was not valid */
		notice(n_ChanServ, lptr->nick,
		       "%s switch [\002%s\002]",
		       (cmdptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
		       av[2]);
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "CLEAR");
		return;
	}

	notice(n_ChanServ, lptr->nick,
	       "[%s] have been cleared on [\002%s\002]",
	       cmdptr->cmd,
	       cptr->name);
} /* c_clear() */

static void
c_clear_ops(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChannelUser *cuser;
	char ops[MAXLINE + 1];
	struct Channel *chptr;
	int jj = 1;

	if (!(chptr = FindChannel(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is not active", av[1]);
		return;
	}

	ops[0] = '\0';
	for (cuser = chptr->firstuser; cuser; cuser = cuser->next)
	{
		if (FindService(cuser->lptr) ||
		        !(cuser->flags & CH_OPPED))
			continue;

		if (jj * (NICKLEN + 1) >= sizeof(ops))
		{
			SetModes(n_ChanServ, 0, 'o', chptr, ops);
			ops[0] = '\0';
			jj = 1;
		}

		strlcat(ops, cuser->lptr->nick, sizeof(ops));
		strlcat(ops, " ", sizeof(ops));
		++jj;
	}

	SetModes(n_ChanServ, 0, 'o', chptr, ops);
} /* c_clear_ops() */

#ifdef HYBRID7_HALFOPS
/* Clear halfops on channel -Janos */
static void c_clear_hops(struct Luser *lptr, struct NickInfo *nptr, int
                         ac, char **av)
{
	struct ChannelUser *cuser;
	char hops[MAXLINE + 1];
	struct Channel *chptr;
	int jj = 1;

	if (!(chptr = FindChannel(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is not active", av[1]);
		return;
	}

	hops[0] = '\0';
	for (cuser = chptr->firstuser; cuser; cuser = cuser->next)
	{
		if (FindService(cuser->lptr) ||
		        !(cuser->flags & CH_HOPPED))
			continue;

		if (jj * (NICKLEN + 1) >= sizeof(hops))
		{
			SetModes(n_ChanServ, 0, 'h', chptr, hops);
			hops[0] = '\0';
			jj = 1;
		}

		strlcat(hops, cuser->lptr->nick, sizeof(hops));
		strlcat(hops, " ", sizeof(hops));
		++jj;
	}

	SetModes(n_ChanServ, 0, 'h', chptr, hops);
} /* c_clear_hops() */
#endif /* HYBRID7_HALFOPS */

static void
c_clear_voices(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct Channel *chptr;
	struct ChannelUser *cuser;
	char voices[MAXLINE + 1];
	int jj = 1;

	if (!(chptr = FindChannel(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is not active",
		       av[1]);
		return;
	}

	voices[0] = '\0';
	for (cuser = chptr->firstuser; cuser; cuser = cuser->next)
	{
		if (FindService(cuser->lptr) ||
		        !(cuser->flags & CH_VOICED))
			continue;

		if (jj * (NICKLEN + 1) >= sizeof(voices))
		{
			SetModes(n_ChanServ, 0, 'v', chptr, voices);
			voices[0] = '\0';
			jj = 1;
		}

		strlcat(voices, cuser->lptr->nick, sizeof(voices));
		strlcat(voices, " ", sizeof(voices));
		++jj;
	}

	SetModes(n_ChanServ, 0, 'v', chptr, voices);
} /* c_clear_voices() */

static void
c_clear_modes(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	char modes[MAXLINE + 1];
	struct Channel *chptr;

	if (!(chptr = FindChannel(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is not active",
		       av[1]);
		return;
	}

	strlcpy(modes, "-", sizeof(modes));

	if (chptr->modes & MODE_S)
		strlcat(modes, "s", sizeof(modes));
	if (chptr->modes & MODE_P)
		strlcat(modes, "p", sizeof(modes));
	if (chptr->modes & MODE_N)
		strlcat(modes, "n", sizeof(modes));
	if (chptr->modes & MODE_T)
		strlcat(modes, "t", sizeof(modes));
#ifdef DANCER

	if (chptr->modes & MODE_C)
		strlcat(modes, "c", sizeof(modes));
#endif /* DANCER */

	if (chptr->modes & MODE_M)
		strlcat(modes, "m", sizeof(modes));
	if (chptr->modes & MODE_I)
		strlcat(modes, "i", sizeof(modes));
	if (chptr->limit)
		strlcat(modes, "l", sizeof(modes));
	if (chptr->key[0] != '\0')
	{
		strlcat(modes, "k ", sizeof(modes));
		strlcat(modes, chptr->key, sizeof(modes));
	}
#ifdef DANCER
	if (chptr->forward)
	{
		strlcat(modes, "f ", sizeof(modes));
		strlcat(modes, chptr->forward, sizeof(modes));
	}
#endif /* DANCER */

	toserv(":%s MODE %s %s\r\n", n_ChanServ, chptr->name, modes);

	UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
} /* c_clear_modes() */

static void
c_clear_bans(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct Channel *chptr;
	struct ChannelBan *bptr;
	char bans[MAXLINE + 1];
	int jj = 1;

	if (!(chptr = FindChannel(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is not active",
		       av[1]);
		return;
	}

	bans[0] = '\0';

	for (bptr = chptr->firstban; bptr; bptr = bptr->next)
	{
		if (jj * (MAXUSERLEN + 1) >= sizeof(bans))
		{
			SetModes(n_ChanServ, 0, 'b', chptr, bans);
			bans[0] = '\0';
			jj = 1;
		}

		strlcat(bans, bptr->mask, sizeof(bans));
		strlcat(bans, " ", sizeof(bans));
		++jj;
	}

	SetModes(n_ChanServ, 0, 'b', chptr, bans);
} /* c_clear_bans() */

static void
c_clear_users(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct Channel *chptr;
	struct ChannelUser *cuser;
	char knicks[MAXLINE + 1];
	char ops[MAXLINE + 1]; /* deop before kicking */
	int jj1 = 1, jj2 = 1;

	if (!(chptr = FindChannel(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is not active",
		       av[1]);
		return;
	}

	knicks[0] = '\0';
	ops[0] = '\0';

	for (cuser = chptr->firstuser; cuser; cuser = cuser->next)
	{
		if (FindService(cuser->lptr))
			continue;

		if (cuser->flags & CH_OPPED)
		{
			if (jj1 * (NICKLEN + 1) >= sizeof(ops))
			{
				SetModes(n_ChanServ, 0, 'o', chptr, ops);
				ops[0] = '\0';
				jj1 = 1;
			}

			strlcat(ops, cuser->lptr->nick, sizeof(ops));
			strlcat(ops, " ", sizeof(ops));
			++jj1;
		}

		if (jj2 * (NICKLEN + 1) >= sizeof(knicks))
		{
			KickBan(0, n_ChanServ, chptr, knicks, "Clearing Users");
			knicks[0] = '\0';
			jj2 = 1;
		}

		strlcat(knicks, cuser->lptr->nick, sizeof(knicks));
		strlcat(knicks, " ", sizeof(knicks));
		++jj2;
	}

	SetModes(n_ChanServ, 0, 'o', chptr, ops);

	toserv(":%s MODE %s +i\r\n", n_ChanServ, chptr->name);

	UpdateChanModes(Me.csptr, n_ChanServ, chptr, "+i");

	KickBan(0, n_ChanServ, chptr, knicks, "Clearing Users");

	toserv(":%s MODE %s -i\r\n", n_ChanServ, chptr->name);

	UpdateChanModes(Me.csptr, n_ChanServ, chptr, "-i");
} /* c_clear_users() */

void
c_clear_all(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	c_clear_ops(lptr, nptr, ac, av);
#ifdef HYBRID7_HALFOPS
	/* Delete halfops, too -Janos */
	c_clear_hops(lptr, nptr, ac, av);
#endif /* HYBRID7_HALFOPS */

	c_clear_voices(lptr, nptr, ac, av);
	c_clear_modes(lptr, nptr, ac, av);
	c_clear_bans(lptr, nptr, ac, av);
#ifdef GECOSBANS

	c_clear_gecos_bans( lptr, nptr, ac, av);
#endif /* GECOSBANS */
} /* c_clear_all() */

#ifdef GECOSBANS
static void c_clear_gecos_bans(struct Luser *lptr, struct NickInfo *nptr,
                               int ac, char **av)
{
	struct Channel *chptr;
	struct ChannelGecosBan *bptr;
	char bans[MAXLINE + 1];
	int jj = 1;

	if (!(chptr = FindChannel(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is not active",
		       av[1]);
		return;
	}

	bans[0] = '\0';

	for (bptr = chptr->firstgecosban; bptr; bptr = bptr->next)
	{
		if (jj * (MAXUSERLEN + 1) >= sizeof(bans))
		{
			SetModes(n_ChanServ, 0, 'd', chptr, bans);
			bans[0] = '\0';
			jj = 1;
		}

		strlcat(bans, bptr->mask, sizeof(bans));
		strlcat(bans, " ", sizeof(bans));
		++jj;
	}

	SetModes(n_ChanServ, 0, 'd', chptr, bans);
} /* c_clear_gecos_bans() */
#endif /* GECOSBANS */

#ifdef EMPOWERADMINS

/*
c_forbid()
  Prevent anyone from using channel av[1]
*/
static void
c_forbid(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr = NULL;
	char sendstr[MAXLINE + 1];

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002FORBID <channel> [reason]\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "FORBID");
		return;
	}

	RecordCommand("%s: %s!%s@%s FORBID [%s]", n_ChanServ, lptr->nick,
	              lptr->username, lptr->hostname, av[1]);

	o_Wallops("Forbid from %s!%s@%s for channel [%s]", lptr->nick,
	          lptr->username, lptr->hostname, av[1] );

	ircsprintf(sendstr, "%s!%s@%s", lptr->nick, lptr->username,
	           lptr->hostname);

	if ((cptr = FindChan(av[1])))
	{
		/* the channel is already registered */
		if (cptr->flags & CS_FORBID)
		{
			notice(n_ChanServ, lptr->nick,
			       "The channel [\002%s\002] is already forbidden",
			       cptr->name);
			return;
		}

		cptr->flags |= CS_FORBID;
		cptr->forbidby = MyStrdup(sendstr);
		if (ac < 3)
			cptr->forbidreason = NULL;
		else
			cptr->forbidreason = GetString(ac - 2, av + 2);
	}
	else
	{
		if (av[1][0] != '#')
		{
			notice(n_ChanServ, lptr->nick,
			       "[\002%s\002] is an invalid channel",
			       av[1]);
			return;
		}

		/* Create channel - it didn't exist */
		cptr = MakeChan();
		cptr->name = MyStrdup(av[1]);
		cptr->flags |= CS_FORBID;
		cptr->created = cptr->lastused = current_ts;
		cptr->forbidby = MyStrdup(sendstr);
		if (ac < 3)
			cptr->forbidreason = NULL;
		else
			cptr->forbidreason = GetString(ac - 2, av + 2);
		SetDefaultALVL(cptr);
		AddChan(cptr);
	}

	if (FindChannel(av[1]))
	{
		/* There are people in the channel - must kick them out */
		cs_join(cptr);
	}

	notice(n_ChanServ, lptr->nick,
	       "The channel [\002%s\002] is now forbidden", av[1]);
} /* c_forbid() */

/*
 * c_unforbid()
 * Removes effects of c_forbid(), admin level required
 */
static void c_unforbid(struct Luser *lptr, struct NickInfo *nptr, int ac,
                       char **av)

{
	struct ChanInfo *cptr;

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick, "Syntax: \002UNFORBID <channel>\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ, "UNFORBID");
		return;
	}

	RecordCommand("%s: %s!%s@%s UNFORBID [%s]",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname, av[1]);

	o_Wallops("Unforbid from %s!%s@%s for channel [%s]",
	          lptr->nick, lptr->username, lptr->hostname, av[1]);

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       "[\002%s\002] is an invalid or non-existing channel", av[1]);
		return;
	}

	if (!cptr->password)
	{
		/* Well, it has empty fields - it was either from old forbid() code,
		 * or AddChan() made nickname from new forbid() - either way it is
		 * safe to delete it -kre */
		DeleteChan(cptr);

		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] has been dropped",
		       av[1]);
	}
	else
	{
		cptr->flags &= ~CS_FORBID;

		notice(n_ChanServ, lptr->nick,
		       "The channel [\002%s\002] is now unforbidden", av[1]);
	}
} /* c_unforbid() */

/*
c_setpass()
  Change the channel (av[1]) password to av[2]
*/

static void
c_setpass(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct f_users *fdrs;

	if (ac < 3)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002SETPASS <channel> <password>\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "SETPASS");
		return;
	}

	RecordCommand("%s: %s!%s@%s SETPASS [%s] ...",
	              n_ChanServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname,
	              av[1]);

	o_Wallops("SETPASS from %s!%s@%s for channel [%s]",
	          lptr->nick,
	          lptr->username,
	          lptr->hostname,
	          av[1] );

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_CH_NOT_REGGED,
		       av[1]);
		return;
	}

	if (!ChangeChanPass(cptr, av[2]))
	{
		notice(n_ChanServ, lptr->nick,
		       "Password change failed");
		RecordCommand("%s: Failed to change password for [%s] to [%s] (SETPASS)",
		              n_ChanServ,
		              cptr->name,
		              av[2]);
		return;
	}

	/* unidentify all founders */
	while (cptr->founders)
	{
		fdrs = cptr->founders->next;
		RemFounder(cptr->founders->lptr, cptr);
		cptr->founders = fdrs;
	}

	notice(n_ChanServ, lptr->nick,
	       "Founder password for %s has been changed to [\002%s\002]",
	       cptr->name,
	       av[2]);
} /* c_setpass() */

/*
c_status()
  Give the current access level of nick av[2] on channel av[1]
*/

static void
c_status(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;
	struct Luser *tmpuser;

	if (ac < 3)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002STATUS <channel> <nickname>\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "STATUS");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick,
		       ERR_CH_NOT_REGGED,
		       av[1]);
		return;
	}

	if (!(tmpuser = FindClient(av[2])))
	{
		notice(n_ChanServ, lptr->nick,
		       "[\002%s\002] is not currently online",
		       av[2]);
		return;
	}

	notice(n_ChanServ, lptr->nick,
	       "%s has an access level of [\002%d\002] on %s",
	       tmpuser->nick, GetAccess(cptr, tmpuser), cptr->name);

	RecordCommand("%s: %s!%s@%s STATUS [%s] %s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
	              cptr->name, tmpuser->nick);
} /* c_status() */

/*
c_forget()
  "forget" that a channel exists (ie: allow users to enter the channel,
but don't let anyone register it)
*/

static void
c_forget(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct ChanInfo *cptr;

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002FORGET <channel>\002");
		notice(n_ChanServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_ChanServ,
		       "FORGET");
		return;
	}

	RecordCommand("%s: %s!%s@%s FORGET [%s]",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
	              av[1]);

	o_Wallops("Forget from %s!%s@%s for channel [%s]",
	          lptr->nick, lptr->username, lptr->hostname,
	          av[1] );


	if ((cptr = FindChan(av[1])))
	{
		/* the channel is already registered */
		if (cptr->flags & CS_FORGET)
		{
			notice(n_ChanServ, lptr->nick,
			       "The channel [\002%s\002] is already forgotten",
			       cptr->name);
			return;
		}

		/*
		 * delete channel to kill the access/akick/founder list etc
		 */
		DeleteChan(cptr);
	}
	else
	{
		if (av[1][0] != '#')
		{
			notice(n_ChanServ, lptr->nick,
			       "[\002%s\002] is an invalid channel",
			       av[1]);
			return;
		}
	}

	cptr = MakeChan();
	cptr->name = MyStrdup(av[1]);
	cptr->created = current_ts;
	cptr->flags = CS_FORGET;
	SetDefaultALVL(cptr);

	AddChan(cptr);

	notice(n_ChanServ, lptr->nick,
	       "The channel [\002%s\002] is now forgotten",
	       av[1]);
} /* c_forget() */

/*
 * c_noexpire()
 * Prevent channel av[1] from expiring
 */
static void c_noexpire(struct Luser *lptr, struct NickInfo *nptr, int ac,
                       char **av)
{
	struct ChanInfo *cptr;

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick,
		       "Syntax: \002NOEXPIRE <channel> {ON|OFF}\002");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ,
		       "NOEXPIRE");
		return;
	}

	RecordCommand("%s: %s!%s@%s NOEXPIRE [%s] %s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
	              av[1], (ac < 3) ? "" : StrToupper(av[2]));

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick, ERR_CH_NOT_REGGED, av[1]);
		return;
	}

	if (ac < 3)
	{
		notice(n_ChanServ, lptr->nick,
		       "NoExpire for channel [\002%s\002] is [\002%s\002]",
		       cptr->name,
		       (cptr->flags & CS_NOEXPIRE) ? "ON" : "OFF");
		return;
	}

	if (!irccmp(av[2], "ON"))
	{
		cptr->flags |= CS_NOEXPIRE;
		notice(n_ChanServ, lptr->nick,
		       "Toggled NoExpire for channel %s [\002ON\002]",
		       cptr->name);
		return;
	}

	if (!irccmp(av[2], "OFF"))
	{
		cptr->lastused = current_ts;
		cptr->flags &= ~CS_NOEXPIRE;
		notice(n_ChanServ, lptr->nick,
		       "Toggled NoExpire for channel %s [\002OFF\002]",
		       cptr->name);
		return;
	}

	/* user gave an unknown param */
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002NOEXPIRE <channel> {ON|OFF}\002");
	notice(n_ChanServ, lptr->nick,
	       ERR_MORE_INFO,
	       n_ChanServ,
	       "NOEXPIRE");
} /* c_noexpire() */

/*
 * Clears noexpire modes setup on channels. Code taken from IrcBg and
 * slightly modified. -kre
 */
void c_clearnoexp(struct Luser *lptr, struct NickInfo *nptr, int ac,
                     char **av)
{
	int ii;
	struct ChanInfo *cptr;
	time_t currtime;
	
	if (ac > 1)
	{
		notice(n_ChanServ, lptr->nick,
			"Syntax: CLEARNOEXP");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ,
			"CLEARNOEXP");
		return;
	}

	if (!IsValidServicesAdmin(lptr))
	{
		notice(n_ChanServ, lptr->nick,
			"You must IDENTIFY as a Services Administrator to use this command");
		return;
	}

	RecordCommand("%s: %s!%s@%s CLEARNOEXP",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname);

	currtime = current_ts;

	for (ii = 0; ii < CHANLIST_MAX; ii++)
		for (cptr = chanlist[ii]; cptr; cptr = cptr->next)
			if (cptr->flags & CS_NOEXPIRE)
			{
				cptr->lastused = currtime;
				cptr->flags &= ~CS_NOEXPIRE;
			}

	notice(n_ChanServ, lptr->nick,
	       "All noexpire flags for channels have been cleared." );

} /* c_clearnoexp() */

/*
 * c_fixts()
 * Traces chanels for TS < TS_MAX_DELTA (either defined in settings or as
 * av[1]. Then such channel is found, c_clear_users() is used to sync
 * channel -kre
 */
static void c_fixts(struct Luser *lptr, struct NickInfo *nptr, int ac,
                    char **av)
{
	int tsdelta = 0;
	time_t now = 0;
	struct Channel *cptr = NULL;
	char dMsg[] = "Detected channel \002%s\002 with TS %d "
	              "below TS_MAX_DELTA %d";
	int acnt = 0;
	char **arv = NULL;
	char line[MAXLINE + 1];

	if (ac < 2)
		tsdelta = MaxTSDelta;
	else
		tsdelta = atoi(av[1]);

	now = current_ts;

	/* Be paranoid */
	if (tsdelta <= 0)
	{
		notice(n_ChanServ, lptr->nick,
		       "Wrong TS_MAX_DELTA specified, using default of 8w");
		tsdelta = 4838400; /* 8 weeks */
	}

	for (cptr = ChannelList; cptr; cptr = cptr->next)
	{
		if ((now - cptr->since) >= tsdelta)
		{
			SendUmode(OPERUMODE_Y, dMsg, cptr->name, cptr->since, tsdelta);
			notice(n_ChanServ, lptr->nick, dMsg, cptr->name, cptr->since,
			       tsdelta);
			putlog(LOG1, "%s: Bogus TS channel: [%s] (TS=%d)",
			       n_ChanServ, cptr->name, cptr->since);

			/* Use c_clear_users() for fixing channel TS. */
			ircsprintf(line, "FOOBAR %s", cptr->name);
			acnt = SplitBuf(line, &arv);
			c_clear_all(lptr, nptr, acnt, arv);

			MyFree(arv);
		}
	}
} /* c_fixts */

/*
 * c_resetlevels
 *
 * Resets levels of _all_ channels to DefaultAccess. -kre
 */
static void c_resetlevels(struct Luser *lptr, struct NickInfo *nptr, int ac,
                          char **av)
{
	int ii;
	struct ChanInfo *cptr;

	if (ac > 1)
	{
		notice(n_ChanServ, lptr->nick,
			"Syntax: RESETLEVELS");
		notice(n_ChanServ, lptr->nick, ERR_MORE_INFO, n_ChanServ,
			"RESETLEVELS");
		return;
	}
	
	if (!IsValidServicesAdmin(lptr))
 	{ 
		notice(n_NickServ, lptr->nick, 
			"You must IDENTIFY as a Services Administrator to use this command"); 
		return;
 	} 

	RecordCommand("%s: %s!%s@%s RESETLEVELS",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname);
	
	for (ii = 0; ii < CHANLIST_MAX; ii++)
		for (cptr = chanlist[ii]; cptr; cptr = cptr->next)
			SetDefaultALVL(cptr);

	notice(n_ChanServ, lptr->nick,
	       "All channels have been reset to default access levels." );

} /* c_resetlevels */

#endif /* EMPOWERADMINS */

/*
 * Routine to set Channel level to default. This fixes a *huge* bug
 * reported by KrisDuv <krisduv2000@yahoo.fr>.
 */
void SetDefaultALVL(struct ChanInfo *cptr)
{
	if (cptr->access_lvl == NULL)
		cptr->access_lvl = MyMalloc(sizeof(int) * CA_SIZE);

	memcpy(cptr->access_lvl, DefaultAccess, sizeof(int) * CA_SIZE);
}

/*
 * ExpireBans()
 * Remove any bans that have expired. -harly 
 * Checks also whether the channel uses own time value and evaluates
 * according to it.
 */
void ExpireBans(time_t unixtime)
{
	int ii;
	struct ChanInfo *cptr;
	struct ChannelBan *bptr;
	struct Channel *chptr;
	char bans[MAXLINE + 1];
	int jj = 1;

	/* If ban expire time is 0 seconds, don't remove the ban. */
	if (!BanExpire)
		return;

	for (ii = 0; ii < CHANLIST_MAX; ++ii)
	{
		for (cptr = chanlist[ii]; cptr; cptr = cptr->next)
		{
			if (cptr->flags & CS_EXPIREBANS)
			{
				if (!(chptr = FindChannel(cptr->name)))
					continue;

				bans[0] = '\0';

				for (bptr = chptr->firstban; bptr; bptr = bptr->next)
				{
					if ((unixtime - bptr->when) >= (cptr->expirebans ?
								cptr->expirebans : BanExpire))
					{
						/* Ban has expired. Remove it. */
						putlog(LOG2, "%s: Removing ban %s on %s (expired)",
						       n_ChanServ, bptr->mask, cptr->name);

						if (jj * (MAXUSERLEN + 1) >= sizeof(bans))
						{
							SetModes(n_ChanServ, 0, 'b', chptr, bans);
							bans[0] = '\0';
							jj = 1;
						}

						strlcat(bans, bptr->mask, sizeof(bans));
						strlcat(bans, " ", sizeof(bans));
						++jj;
					}
				}

				if (bans[0] != '\0')
					SetModes(n_ChanServ, 0, 'b', chptr, bans);
			} /* if (cptr->flags & CS_EXPIREBANS) */
		} /* for (cptr = chanlist[ii]; cptr; cptr = cptr->next) */
	} /* for (ii = 0; ii < CHANLIST_MAX; ++ii) */
} /* ExpireBans(time_t unixtime) */

/*
ExpireAkicks()
 Delete any akicks that have expired
 
Nice functionality but bugged. Fixed.
  -ags
*/

void
ExpireAkicks(time_t unixtime)
{
	struct AutoKick *temp, *prev, *next;
	struct ChanInfo *cptr;
	struct Channel *chptr; /* Ban removal purpose */
	int ii;
	char modes[MAXLINE + 1];

	for (ii = 0; ii < CHANLIST_MAX; ii++)
	{
		for (cptr = chanlist[ii]; cptr; cptr = cptr->next)
		{
			prev = NULL;
			for (temp = cptr->akick; temp; temp = next)
			{
				next = temp->next;

				if ((temp->expires) && (temp->expires <= unixtime))
				{
					SendUmode(OPERUMODE_Y, "*** Expired akick %s [%s]",
					          temp->hostmask, temp->reason ? temp->reason : "");

					modes[0] = '\0';
					ircsprintf(modes, "-b %s", temp->hostmask);
					modes[sizeof(modes) - 1] = '\0'; /* Paranoic - there is no
										                                                                                      ircnsprintf() :) */
					/* Send UNBAN */
					toserv(":%s MODE %s %s\r\n", n_ChanServ, cptr->name, modes);
					chptr = FindChannel(cptr->name);
					if (chptr)
						UpdateChanModes(Me.csptr, n_ChanServ, chptr, modes);
					MyFree(temp->hostmask);
					if (temp->reason)
						MyFree(temp->reason);
					if (prev)
						prev->next = temp->next;
					else
						cptr->akick = temp->next;
					/* Mind that prev pointer keeps still! */
					MyFree(temp); /* Free it! */
				}
				else
					prev = temp; /* Update prev pointer only if there is no cell to
								                                                      remove! */
			}
		}
	}
} /* ExpireAkicks() */

static void c_set_expirebans(struct Luser *lptr,
                             struct NickInfo *nptr, int ac, char **av)
{
	struct ChanInfo *cptr;
	long bantime;

	if (!BanExpire)
	{
		notice(n_ChanServ, lptr->nick,
				"EXPIREBANS is currently \002disabled\002");
		return;
	}

	if (!(cptr = FindChan(av[1])))
		return;

	RecordCommand("%s: %s!%s@%s SET [%s] EXPIREBANS %s",
	              n_ChanServ, lptr->nick, lptr->username, lptr->hostname,
	              cptr->name, (ac < 4) ? "" : StrToupper(av[3]));

	if (ac < 4)
	{
		notice(n_ChanServ, lptr->nick,
		       "EXPIREBANS for channel %s is [\002%s\002]",
		       cptr->name, (cptr->flags & CS_EXPIREBANS) ? "ON" : "OFF");

		/* also give information about the expiration time */
		notice(n_ChanServ, lptr->nick,
				"Bans expiration time for channel %s is [\002%s\002]",
				cptr->name, cptr->expirebans ? timeago(cptr->expirebans, 4)
					: timeago(BanExpire, 4));
		return;
	}

	if (!irccmp(av[3], "ON"))
	{
		cptr->flags |= CS_EXPIREBANS;
		notice(n_ChanServ, lptr->nick,
		       "Toggled EXPIREBANS for channel %s [\002ON\002]", cptr->name);
		return;
	}
	else
		if (!irccmp(av[3], "OFF"))
		{
			cptr->flags &= ~CS_EXPIREBANS;
			notice(n_ChanServ, lptr->nick,
			       "Toggled EXPIREBANS for channel %s [\002OFF\002]",
			       cptr->name);
			return;
		}

	else
		if ((bantime = timestr(av[3])))
		{
			cptr->expirebans = bantime;
			notice(n_ChanServ, lptr->nick,
				"Bans expiration time for channel %s is now set to [\002%s\002]",
				cptr->name, timeago(bantime, 4));
			return;
		}
	
	notice(n_ChanServ, lptr->nick,
	       "Syntax: \002SET <channel> EXPIREBANS {ON|OFF|<time>}\002");
	notice(n_ChanServ, lptr->nick, ERR_MORE_INFO,
	       n_ChanServ, "SET EXPIREBANS");
} /* c_set_expirebans() */

/*
 * Users can delete themselves from channels
 */
static void c_delme(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)
{

	struct ChanInfo *cptr;
	struct NickInfo *mptr;
	struct AccessChannel *acptr = NULL;

	if (ac < 2)
	{
		notice(n_ChanServ, lptr->nick,
			   "Syntax: \002DELME <channel>\002");
		notice(n_ChanServ, lptr->nick,
			   ERR_MORE_INFO,
			   n_ChanServ,
			   "DELME");
		return;
	}

	if (!(cptr = FindChan(av[1])))
	{
		notice(n_ChanServ, lptr->nick, ERR_CH_NOT_REGGED, av[1]);
		return;
	}

	mptr = GetLink(nptr->nick);

	if (mptr && mptr->AccessChannels)
		for (acptr = mptr->AccessChannels; acptr; acptr = acptr->next)
			if (acptr->cptr == cptr)
				break;

	if (acptr == NULL)
	{
		notice(n_ChanServ, lptr->nick,
		       "You are not in the access list of [\002%s\002]",
		       cptr->name);

		return;
	}

	if (acptr->accessptr->level <= cptr->access_lvl[CA_AUTODEOP])
	{
		notice(n_ChanServ, lptr->nick,
		       "You cannot do that because you have AUTODEOP access level on [\002%s\002]",
		       cptr->name);

		return;
	}

	notice(n_ChanServ, lptr->nick,
	       "You have been removed from the access list of [\002%s\002]",
	       cptr->name);

	RecordCommand("%s: %s!%s@%s DELME [%s]",
				  n_ChanServ,
				  lptr->nick,
				  lptr->username,
				  lptr->hostname,
				  cptr->name);

	DeleteAccess(cptr, acptr->accessptr);
	DeleteAccessChannel(mptr, acptr);
};

#endif /* defined(NICKSERVICES) && defined(CHANNELSERVICES) */
