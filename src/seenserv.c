/*
 * Hybserv2 Services by Hybserv2 team
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 * seenserv.c
 * Copyright (C) 2000 demond
 * Later changes by Hybserv2 team
 */

#include "stdinc.h"
#include "alloc.h"
#include "client.h"
#include "channel.h"
#include "conf.h"
#include "config.h"
#include "data.h"
#include "hash.h"
#include "helpserv.h"
#include "chanserv.h"
#include "seenserv.h"
#include "hybdefs.h"
#include "match.h"
#include "misc.h"
#include "timestr.h"
#include "mystring.h"
#include "settings.h"
#include "sock.h"
#include "log.h"
#include "err.h"
#include "sprintf_irc.h"

#ifdef SEENSERVICES

#define NOSQUITSEEN
#define MAXWILDSEEN	100

int seenc = 0;
aSeen *seenp = NULL, *seenb = NULL;

static void es_seen(struct Luser *, int, char **);
static void es_seennick(struct Luser *, int, char **);
static void es_help(struct Luser *, int, char **);
static void es_seenstat(struct Luser *, int, char **);
static void FreeSeen(void);
static void es_join(struct Luser *, int, char **, int);
static void es_part(struct Luser *, int, char **);

static struct Command seencmds[] =
    {
	    { "SEEN", es_seen, LVL_NONE },
	    { "SEENNICK", es_seennick, LVL_NONE },
	    { "HELP", es_help, LVL_NONE },
	    { "SEENSTAT", es_seenstat, LVL_ADMIN },
	    { "JOIN", es_join, LVL_NONE },
	    { "PART", es_part, LVL_NONE },
	    { 0, 0, 0 }
    };

/*
 * es_process()
 *
 * Process command coming from 'nick' directed towards n_SeenServ
 */
void es_process(char *nick, char *command)
{
	int acnt;
	char **arv;
	struct Command *eptr;
	struct Luser *lptr;

	if (!command || !(lptr = FindClient(nick)))
		return ;

	if (Network->flags & NET_OFF)
	{
		notice(n_SeenServ, lptr->nick,
		       "Services are currently \002disabled\002");
		return ;
	}

	acnt = SplitBuf(command, &arv);
	if (acnt == 0)
	{
		MyFree(arv);
		return ;
	}

	eptr = GetCommand(seencmds, arv[0]);

	if (!eptr || (eptr == (struct Command *) - 1))
	{
		notice(n_SeenServ, lptr->nick,
		       "%s command [%s]",
		       (eptr == (struct Command *) - 1) ? "Ambiguous" : "Unknown",
		       arv[0]);
		MyFree(arv);
		return ;
	}

	/*
	 * Check if the command is for admins only - if so, check if they match
	 * an admin O: line.  If they do, check if they are registered with
	 * OperServ, if so, allow the command
	 */
	if ((eptr->level == LVL_ADMIN) && !(IsValidAdmin(lptr)))
	{
		notice(n_SeenServ, lptr->nick, "Unknown command [%s]", arv[0]);
		MyFree(arv);
		return ;
	}

	/* call eptr->func to execute command */
	(*eptr->func)(lptr, acnt, arv);

	MyFree(arv);

	return ;
} /* es_process() */

/*
 * es_loaddata()
 *
 * Load Seen database - return 1 if successful, -1 if not, and -2 if the
 * errors are unrecoverable
 */
int es_loaddata()
{
	FILE *fp;
	char line[MAXLINE + 1], **av;
	char *keyword;
	int ac, ret = 1, cnt, type = 0;
	aSeen *seen;

	if ((fp = fopen(SeenServDB, "r")) == NULL)
	{
		/* SeenServ data file doesn't exist */
		return ( -1);
	}

	FreeSeen();
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
			if (ac < 4)
			{
				fatal(1, "%s:%d Invalid database format (FATAL)", SeenServDB,
				      cnt);
				ret = -2;
				MyFree(av);
				continue;
			}

			keyword = av[0] + 2;
			type = 0;
			if (!ircncmp(keyword, "QUIT", 4))
			{
				type = 1;
			}
			else if (!ircncmp(keyword, "NICK", 4))
			{
				type = 2;
			}
			if (type)
			{
				seen = MyMalloc(sizeof(aSeen));
				memset(seen, 0, sizeof(aSeen));
				strlcpy(seen->nick, av[1], NICKLEN + 1);
				seen->userhost = MyStrdup(av[2]);
				seen->msg = (type == 1) ? MyStrdup(av[4] + 1) : NULL;
				seen->time = atol(av[3]);
				seen->type = type;
				seen->prev = seenp;
				seen->next = NULL;
				if (seenp)
					seenp->next = seen;
				seenp = seen;
				if (!seenb)
					seenb = seen;
				++seenc;
			}
		}

		MyFree(av);
	} /* while */

	fclose(fp);

	return (ret);
} /* es_loaddata */

void es_add(char *nick, char *user, char *host, char *msg, time_t time,
            int type)
{
	int ac;
	char userhost[MAXUSERLEN + 1], **av, *mymsg;
	aSeen *seen = MyMalloc(sizeof(aSeen));

#ifdef NOSQUITSEEN
	if (type == 1)
	{
		mymsg = MyStrdup(msg);
		ac = SplitBuf(mymsg, &av);
		if (ac == 2)
		{
			if (FindServer(av[0]) && FindServer(av[1]))
			{
				MyFree(mymsg);
				MyFree(av);
				MyFree(seen);
				return ;
			}
		}
		MyFree(mymsg);
		MyFree(av);
	}
#endif

	if (++seenc > SeenMaxRecs)
	{
		aSeen *sp = seenb;
		MyFree(seenb->userhost);
		MyFree(seenb->msg);
		if (seenb->next)
			seenb->next->prev = NULL;
		seenb = seenb->next;
		MyFree(sp);
		seenc--;
	}

	memset(userhost, 0, sizeof(userhost));
	memset(seen, 0, sizeof(aSeen));
	strlcpy(seen->nick, nick, NICKLEN + 1);
	strlcpy(userhost, user, USERLEN + 1);
	strlcat(userhost, "@", sizeof(userhost));
	strlcat(userhost, host, HOSTLEN);
	seen->userhost = MyStrdup(userhost);
	seen->msg = (type == 1) ? MyStrdup(msg) : NULL;
	seen->time = time;
	seen->type = type;
	seen->prev = seenp;
	seen->next = NULL;
	if (seenp)
		seenp->next = seen;
	seenp = seen;
	if (!seenb)
		seenb = seen;
}

static void FreeSeen()
{
	aSeen *seen;

	while ((seen = seenp))
	{
		MyFree(seen->userhost);
		MyFree(seen->msg);
		seenp = seen->prev;
		MyFree(seen);
	}
	seenp = seenb = NULL;
	seenc = 0;
}

/*
 * es_seen()
 *
 * Give lptr seen info on av[1] wild matches (nick + userhost)
 */
static void es_seen(struct Luser *lptr, int ac, char **av)
{
	int i, count, j;
	aSeen *seen, *first = NULL, *saved = NULL; 
	aSeen *sorted[5] = { NULL, NULL, NULL, NULL, NULL };
	char nuhost[MAXUSERLEN + 1], sendstr[MAXLINE + 1];
	time_t mytime, last;
	char seenstring[MAXLINE + 1];

	if (ac < 2)
	{
		notice(n_SeenServ, lptr->nick,
		       "Syntax: SEEN <nick|hostmask>");
		notice(n_SeenServ, lptr->nick,
		       ERR_MORE_INFO, n_SeenServ, "SEEN");
		return ;
	}

	seenstring[0] = '\0';

	if (strchr(av[1], '*') || strchr(av[1], '?') ||
	        strchr(av[1], '@') || strchr(av[1], '!'))
	{
		if (match("*!*@*", av[1]))
			strlcpy(seenstring, av[1], MAXLINE + 1);
		else if (match("*!*", av[1]))
		{
			strlcpy(seenstring, av[1], MAXLINE - 1);
			strlcat(seenstring, "@*", MAXLINE + 1);
		}
		else if (match("*@*", av[1]))
		{
			strlcpy(seenstring, "*!", MAXLINE - 1);
			strlcat(seenstring, av[1], MAXLINE + 1);
		}
		else
			strlcpy(seenstring, av[1], MAXLINE + 1);

		count = 0;
		for (seen = seenp; seen != NULL; seen = seen->prev)
		{
			memset(nuhost, 0, sizeof(nuhost));
			strlcpy(nuhost, seen->nick, NICKLEN + 1);
			strlcat(nuhost, "!", sizeof(nuhost));
			strlcat(nuhost, seen->userhost, USERLEN + HOSTLEN + 1);
			if (match(seenstring, nuhost))
			{
				seen->seen = saved;
				saved = seen;
				if (++count > MAXWILDSEEN)
					break;
			}
		}
		first = saved;

		if (count > MAXWILDSEEN)
		{
			notice(n_SeenServ, lptr->nick,
			       "I found more than %d matches to your query; "
			       "please refine it to see any output", MAXWILDSEEN);
			return ;
		}
		else
			if ((first == NULL) || (count == 0))
			{
				notice(n_SeenServ, lptr->nick,
				       "I found no matching seen records to your query");
				return ;
			}
			else
			{
				mytime = current_ts + 1;
				for (i = 0; (i < 5) && (i < count); ++i)
				{
					saved = first;
					for (last = 0; saved != NULL; saved = saved->seen)
					{
						if ((saved->time <= mytime) && (saved->time > last))
						{
							for (j = 0; j < i; j++)
								if (!irccmp(saved->nick, sorted[j]->nick) &&
								        saved->time == sorted[j]->time)
								{
									j = -1;
									break;
								}
							if (j != -1)
							{
								sorted[i] = saved;
								last = saved->time;
							}
						}
					}
					mytime = sorted[i]->time;
				}
			}

		ircsprintf(sendstr, "I found %d match(es), ", count);
		if (count > 5)
			strlcat(sendstr, "here are the 5 most recent, ", sizeof(sendstr));
		strlcat(sendstr, "sorted:", sizeof(sendstr));
		count = i;
		for (i = 0; i < count; i++)
		{
			strlcat(sendstr, ", ", sizeof(sendstr));
			strlcat(sendstr, sorted[i]->nick, sizeof(sendstr));
		}
		strlcat(sendstr, ". ", sizeof(sendstr));
		if (sorted[0]->type == 1)
		{
			notice(n_SeenServ, lptr->nick,
			       "%s %s (%s) was last seen %s ago, quiting: %s",
			       sendstr, sorted[0]->nick, sorted[0]->userhost,
			       timeago(sorted[0]->time, 0), sorted[0]->msg);
		}
		else
			if (sorted[0]->type == 2)
			{
				notice(n_SeenServ, lptr->nick,
				       "%s %s (%s) was last seen %s ago, changing nicks",
				       sendstr, sorted[0]->nick, sorted[0]->userhost,
				       timeago(sorted[0]->time, 0));
			}
	}
	else
	{
		es_seennick(lptr, ac, av);
	}
} /* es_seen */

/*
 * es_seennick()
 *
 * Give lptr seen info on av[1] nick (exact match)
 */
static void es_seennick(struct Luser *lptr, int ac, char **av)
{
	aSeen *seen, *recent, *saved = NULL;
	struct Luser *aptr;

	if (ac < 2)
	{
		notice(n_SeenServ, lptr->nick,
		       "Syntax: SEENNICK <nickname>");
		notice(n_SeenServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_SeenServ,
		       "SEENNICK");
		return ;
	}

	if ((aptr = FindClient(av[1])))
	{
		notice(n_SeenServ, lptr->nick, "%s is on IRC right now!", aptr->nick);
		return ;
	}

	for (seen = seenp; seen; seen = seen->prev)
	{
		if (!irccmp(seen->nick, av[1]))
		{
			seen->seen = saved;
			saved = seen;
		}
	}

	if (saved)
	{
		recent = saved;
		for (; saved; saved = saved->seen)
		{
			if (saved->time > recent->time)
				recent = saved;
		}
		if (recent->type == 1)
		{
			notice(n_SeenServ, lptr->nick, "I last saw %s (%s) %s ago, quiting: %s", recent->nick,
			       recent->userhost, timeago(recent->time, 0), recent->msg);
		}
		else if (recent->type == 2)
		{
			notice(n_SeenServ, lptr->nick, "I last saw %s (%s) %s ago, changing nicks", recent->nick,
			       recent->userhost, timeago(recent->time, 0));
		}
	}
	else
	{
		notice(n_SeenServ, lptr->nick, "I haven't seen %s recently", av[1]);
	}

} /* es_seennick */

/*
 * es_help()
 *
 *  Give lptr help on command av[1]
 */
static void es_help(struct Luser *lptr, int ac, char **av)
{
	if (ac >= 2)
	{
		char str[MAXLINE + 1];
		struct Command *sptr;

		for (sptr = seencmds; sptr->cmd; sptr++)
			if (!irccmp(av[1], sptr->cmd))
				break;

		if (sptr->cmd)
			if ((sptr->level == LVL_ADMIN) &&
			        !(IsValidAdmin(lptr)))
			{
				notice(n_SeenServ, lptr->nick,
				       "No help available on \002%s\002",
				       av[1]);
				return ;
			}

		ircsprintf(str, "%s", av[1]);

		GiveHelp(n_SeenServ, lptr->nick, str, NODCC);
	}
	else
		GiveHelp(n_SeenServ, lptr->nick, NULL, NODCC);
} /* es_help() */

/*
 * es_seenstat()
 *
 * Give lptr seen statistics
 */
static void es_seenstat(struct Luser *lptr, int ac, char **av)
{
	int total = 0, quits = 0, nicks = 0;
	aSeen *seen;

	RecordCommand("%s: %s!%s@%s SEENSTAT",
	              n_SeenServ,
	              lptr->nick,
	              lptr->username,
	              lptr->hostname);

	for (seen = seenp; seen; seen = seen->prev)
	{
		total++;
		if (seen->type == 1)
			quits++;
		else if (seen->type == 2)
			nicks++;
	}

	notice(n_SeenServ, lptr->nick, "Total number of nicks seen: %d", total);
	notice(n_SeenServ, lptr->nick, "...of these, QUIT msgs: %d", quits);
	notice(n_SeenServ, lptr->nick, "...of these, NICK msgs: %d", nicks);

} /* es_seenstat() */

/*
 * ss_join - joins channel whitout performing any check
 */
void ss_join(struct Channel *cptr)
{
  char sendstr[MAXLINE];
  char **av;
  
  if (!cptr)
    return;  

  ircsprintf(sendstr,
             ":%s SJOIN %ld %s + :+%s\r\n",
             Me.name, (long) cptr->since, cptr->name, n_SeenServ);
  toserv(sendstr);
  SplitBuf(sendstr, &av);

  AddChannel(av, 0, (char **) NULL);
  
  MyFree(av);
} /* ss_join() */

/* ss_part - removes SeenServ from channel */
void ss_part(struct Channel *chptr)
{
  if (!chptr)
    return;
  if (IsChannelMember(chptr, Me.esptr))
    toserv(":%s PART %s\r\n", n_SeenServ, chptr->name);
  RemoveFromChannel(chptr, Me.esptr);
} /* ss_part() */

static void
es_join(struct Luser *lptr, int ac, char **av, int sockfd)

{
  struct Channel *chptr;
  struct ChanInfo *cptr;
  
  if (ac < 2)
    return;

  if (!(chptr = FindChannel(av[1])))
    {  
	  notice(n_SeenServ, lptr->nick, "No such channel [\002%s\002]",
			  av[1]);
      return;
    }
  
	if ((cptr = FindChan(chptr->name)))
	{
		if (IsFounder(lptr, cptr) || IsAdmin(lptr))
			cptr->flags |= CS_SEENSERV;
		else
		{
			notice(n_SeenServ, lptr->nick,
					"You are not the FOUNDER of [\002%s\002]",
					cptr->name);
			return;
		}
	}
	else
	{
		if (!IsAdmin(lptr))
		{
			notice(n_SeenServ, lptr->nick,
					"The channel [\002%s\002] is not registered",
					chptr->name);
			return;
		}
	}

	ss_join(chptr);
} /* es_join() */

/*
es_part()
  Have SeenServ part 'chptr'
*/
void
es_part(struct Luser *lptr, int ac, char **av)

{
  struct Channel *chptr;
  struct ChanInfo *cptr;
  
  if (ac < 2)
    return;
   
  if (!(chptr = FindChannel(av[1])))
    {  
	  notice(n_SeenServ, lptr->nick, "No such channel [\002%s\002]",
			  av[1]);
      return;
    }

	if ((cptr = FindChan(chptr->name)))
	{
		if (IsFounder(lptr, cptr) || IsAdmin(lptr))
			cptr->flags &= ~CS_SEENSERV;
		else
		{
			notice(n_SeenServ, lptr->nick,
					"You are not the FOUNDER of [\002%s\002]",
					cptr->name);
			return;
		}
	}
	else
	{
		if (!IsAdmin(lptr))
		{
			notice(n_SeenServ, lptr->nick,
					"The channel [\002%s\002] is not registered",
					chptr->name);
			return;
		}
	}

	ss_part(chptr);
} /* es_part() */

#endif /* SEENSERVICES */
