/*
 * HybServ2 Services by HybServ2 team
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 * seenserv.c
 * Copyright (C) 2000 demond
 */

#include "defs.h"

#define NOSQUITSEEN
#define MAXWILDSEEN	100

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif

#include "alloc.h"
#include "client.h"
#include "channel.h"
#include "conf.h"
#include "config.h"
#include "hash.h"
#include "helpserv.h"
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

int seenc = 0;
aSeen *seenp = NULL, *seenb = NULL;

static void es_seen(struct Luser *, int, char **);
static void es_seennick(struct Luser *, int, char **);
static void es_help(struct Luser *, int, char **);
static void es_unseen(struct Luser *, int, char **);
static void es_seenstat(struct Luser *, int, char **);
static void FreeSeen();

static struct Command seencmds[] =
    {
      { "SEEN", es_seen, LVL_NONE },
      { "SEENNICK", es_seennick, LVL_NONE },
      { "HELP", es_help, LVL_NONE },
      { "UNSEEN", es_unseen, LVL_ADMIN },
      { "SEENSTAT", es_seenstat, LVL_ADMIN },
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
  char line[MAXLINE], **av;
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
  while (fgets(line, MAXLINE - 1, fp))
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
              strncpy(seen->nick, av[1], NICKLEN);
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
  char userhost[USERLEN + HOSTLEN + 2], **av, *mymsg;
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
  strncpy(seen->nick, nick, NICKLEN);
  strncpy(userhost, user, USERLEN);
  strcat(userhost, "@");
  strncat(userhost, host, HOSTLEN);
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
  int i, count;
  aSeen *seen, *first = NULL, *saved = NULL, *sorted[5];
  char nuhost[NICKLEN + USERLEN + HOSTLEN + 3], sendstr[256];
  time_t mytime, last;
  char seenstring[MAXLINE];

  if (ac < 2)
    {
      notice(n_SeenServ, lptr->nick,
             "Syntax: SEEN <nick|hostmask>");
      notice(n_SeenServ, lptr->nick,
             ERR_MORE_INFO, n_SeenServ, "SEEN");
      return ;
    }

  memset(seenstring, 0, MAXLINE);

  if (strchr(av[1], '*') || strchr(av[1], '?') ||
      strchr(av[1], '@') || strchr(av[1], '!'))
    {
      if (match("*!*@*", av[1]))
        strncpy(seenstring, av[1], MAXLINE - 1);
      else if (match("*!*", av[1]))
      {
        strncpy(seenstring, av[1], MAXLINE - 3);
        strcat(seenstring, "@*");
      }
      else if (match("*@*", av[1]))
      {
        strcpy(seenstring, "*!");
        strncat(seenstring, av[1], MAXLINE - 3);
      }
      else
        strncpy(seenstring, av[1], MAXLINE);

      count = 0;
      for (seen = seenp; seen; seen = seen->prev)
        {
          memset(nuhost, 0, sizeof(nuhost));
          strncpy(nuhost, seen->nick, NICKLEN);
          strcat(nuhost, "!");
          strncat(nuhost, seen->userhost, USERLEN + HOSTLEN + 1);
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
        if (count == 0)
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
            for (last = 0; saved; saved = saved->seen)
            {
              if ((saved->time < mytime) && (saved->time > last))
              {
                sorted[i] = saved;
                last = saved->time;
              }
            }
            mytime = sorted[i]->time;
          }
        }

      ircsprintf(sendstr, "I found %d match(es), ", count);
      if (count > 5)
        strcat(sendstr, "here are the 5 most recent, ");
      strcat(sendstr, "sorted:");
      count = i;
      for (i = 0; i < count; i++)
        {
          strcat(sendstr, " ");
          strcat(sendstr, sorted[i]->nick);
        }
      strcat(sendstr, ". ");
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
      char str[MAXLINE];
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
 * es_unseen()
 *
 * Removes all wild matches of av[1] from seen list 
 */
static void es_unseen(struct Luser *lptr, int ac, char **av)
{
  aSeen *saved, *seen = seenp;
  char nuhost[NICKLEN + USERLEN + HOSTLEN + 3];

  if (ac < 2)
    {
      notice(n_SeenServ, lptr->nick,
             "Syntax: UNSEEN <mask>");
      notice(n_SeenServ, lptr->nick,
             ERR_MORE_INFO,
             n_SeenServ,
             "UNSEEN");
      return ;
    }

  RecordCommand("%s: %s!%s@%s UNSEEN [%s]",
                n_SeenServ,
                lptr->nick,
                lptr->username,
                lptr->hostname,
                av[1]);

  while (seen)
    {
      saved = seen->prev;
      memset(nuhost, 0, sizeof(nuhost));
      strncpy(nuhost, seen->nick, NICKLEN);
      strcat(nuhost, "!");
      strncat(nuhost, seen->userhost, USERLEN + HOSTLEN + 1);
      if (match(av[1], nuhost))
        {
          if (seen->prev)
            seen->prev->next = seen->next;
          if (seen->next)
            seen->next->prev = seen->prev;
          if (seenp == seen)
            seenp = seen->prev;
          if (seenb == seen)
            seenb = seen->next;
          MyFree(seen->userhost);
          MyFree(seen->msg);
          MyFree(seen);
        }
      seen = saved;
    }

  notice(n_SeenServ, lptr->nick, "Done.");

} /* es_unseen() */

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

#endif /* SEENSERVICES */
