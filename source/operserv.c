/*
 * HybServ TS Services, Copyright (C) 1998-1999 Patrick Alken
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 * $Id$
 */

#include "defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#include <time.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#ifndef HAVE_CYGWIN
#include <signal.h>
#else
#include <sys/signal.h>
#endif /* HAVE_CYGWIN */

#include "alloc.h"
#include "channel.h"
#include "chanserv.h"
#include "client.h"
#include "conf.h"
#include "config.h"
#include "data.h"
#include "dcc.h"
#include "err.h"
#include "gline.h"
#include "hash.h"
#include "helpserv.h"
#include "hybdefs.h"
#include "jupe.h"
#include "log.h"
#include "match.h"
#include "memoserv.h"
#include "misc.h"
#include "mystring.h"
#include "nickserv.h"
#include "operserv.h"
#include "server.h"
#include "settings.h"
#include "sock.h"
#include "statserv.h"
#include "Strn.h"
#include "timestr.h"
#include "sprintf_irc.h"


/*
 * Global - list of hostmasks being ignored by services
 */
struct Ignore                 *IgnoreList = NULL;

#ifdef ALLOW_FUCKOVER

/*
 * List of current fuckover targets
 */
static struct Process *fprocs = NULL;

#endif /* ALLOW_FUCKOVER */

static char *onick, /* nickname of current user giving command */
            *ouser, /* username of current user giving command */
            *ohost; /* hostname of current user giving command */

static void os_notice(struct Luser *, int, char *, ...);
static void o_RecordCommand(int, char *format, ...);
static struct OperCommand *GetoCommand(struct OperCommand *, char *);

static void o_identify(struct Luser *, int, char **, int);
static void o_kill(struct Luser *, int, char **, int);
static void o_rehash(struct Luser *, int, char **, int);
static void o_restart(struct Luser *, int, char **, int);

#ifdef ALLOW_JUPES
static void o_jupe(struct Luser *, int, char **, int);
static void o_unjupe(struct Luser *, int, char **, int);
#endif

static void o_omode(struct Luser *, int, char **, int);
static void o_secure(struct Luser *, int, char **, int);

#ifdef ALLOW_DIE
static void o_die(struct Luser *, int, char **, int);
#endif

static void o_status(struct Luser *, int, char **, int);

#ifdef ALLOW_GLINES
static void o_gline(struct Luser *, int, char **, int);
static void o_ungline(struct Luser *, int, char **, int);
#endif

static void o_help(struct Luser *, int, char **, int);
static void o_join(struct Luser *, int, char **, int);
static void o_part(struct Luser *, int, char **, int);
static void o_clones(struct Luser *, int, char **, int);

#ifdef ALLOW_FUCKOVER
static void o_fuckover(struct Luser *, int, char **, int);
#endif

static void o_stats(struct Luser *, int, char **, int);
static void o_trace(struct Luser *, int, char **, int);
static void o_channel(struct Luser *, int, char **, int);
static void o_on(struct Luser *, int, char **, int);
static void o_off(struct Luser *, int, char **, int);
static void o_jump(struct Luser *, int, char **, int);

static void o_save(struct Luser *, int, char **, int);
static void o_reload(struct Luser *, int, char **, int);

static void o_ignore(struct Luser *, int, char **, int);
static void o_ignore_add(struct Luser *, int, char **, int);
static void o_ignore_del(struct Luser *, int, char **, int);
static void o_ignore_list(struct Luser *, int, char **, int);

#ifdef ALLOW_KILLCHAN
static void o_killchan(struct Luser *, int, char **, int);
#endif

#ifdef ALLOW_KILLHOST
static void o_killhost(struct Luser *, int, char **, int);
#endif

#ifdef HIGHTRAFFIC_MODE
static void o_htm(struct Luser *, int, char **, int);
static void o_htm_on(struct Luser *, int, char **, int);
static void o_htm_off(struct Luser *, int, char **, int);
static void o_htm_max(struct Luser *, int, char **, int);
#endif

static void o_hub(struct Luser *, int, char **, int);
static void o_umode(struct Luser *, int, char **, int);

#ifdef ALLOW_DUMP
static void o_dump(struct Luser *, int, char **, int);
#endif

static void o_set(struct Luser *, int, char **, int);

static void o_kline(struct Luser *, int, char **, int);

static void o_who(struct Luser *, int, char **, int);
static void o_boot(struct Luser *, int, char **, int);
static void o_quit(struct Luser *, int, char **, int);
static void o_motd(struct Luser *, int, char **, int);
static void o_link(struct Luser *, int, char **, int);
static void o_unlink(struct Luser *, int, char **, int);

static void DeleteIgnore(struct Ignore *iptr);

static void TakeOver(struct Channel *cptr);
static float CalcMem(char *, int socket);

#ifdef ALLOW_FUCKOVER
static void InitFuckoverProcess(char *from, char *ftarget);
#endif

static void show_trace(struct Luser *, struct Luser *, int, int);
static void show_channel(struct Luser *, struct Channel *, int);

#ifdef STATSERVICES
static void show_trace_info(struct Luser *, struct Luser *, int);
#endif

static void DisplaySettings(struct Luser *, int);

static struct UmodeInfo *FindUmode(char);

static struct OperCommand opercmds[] = {

  /*
   * Regular user commands
   */

  { "HELP", o_help, 0, 0 },
  { "IDENTIFY", o_identify, 0, 0 },
  { "REGISTER", o_identify, 0, 0 },

  /*
   * Operator commands
   */

  { "CLONES", o_clones, 0, 'o' },

#ifdef HIGHTRAFFIC_MODE
  { "HTM", o_htm, 0, 'o' },
#endif

  { "HUB", o_hub, 0, 'o' },
  { "KILL", o_kill, 0, 'o' },
  { "STATS", o_stats, 0, 'o' },
  { "STATUS", o_status, 0, 'o' },
  { "UMODE", o_umode, 0, 'o' },
  { "USERMODE", o_umode, 0, 'o' },

  /*
   * Administrator commands
   */

  { "CHANNEL", o_channel, 0, 'a' },
  { "JOIN", o_join, 0, 'a' },
  { "JUMP", o_jump, 0, 'a' },

  { "IGNORE", o_ignore, 0, 'a' },

#ifdef ALLOW_KILLCHAN
  { "KILLCHAN", o_killchan, 0, 'a' },
#endif

#ifdef ALLOW_KILLHOST
  { "KILLHOST", o_killhost, 0, 'a' },
#endif

  { "MODE", o_omode, 0, 'a' },
  { "OMODE", o_omode, 0, 'a' },
  { "PART", o_part, 0, 'a' },
  { "REROUTE", o_jump, 0, 'a' },
  { "SECURE", o_secure, 0, 'a' },
  { "TAKEOVER", o_secure, 0, 'a' },
  { "TRACE", o_trace, 0, 'a' },
  { "USER", o_trace, 0, 'a' },
  { "WHOIS", o_trace, 0, 'a' },

  /*
   * Services Administrator commands
   */

#ifdef ALLOW_DIE
  { "DIE", o_die, 0, 's' },
#endif /* ALLOW_DIE */

#ifdef ALLOW_DUMP
  { "DUMP", o_dump, 0, 's' },
  { "QUOTE", o_dump, 0, 's' },
  { "RAW", o_dump, 0, 's' },
#endif /* ALLOW_DUMP */

  { "DISABLE", o_off, 0, 's' },
  { "ENABLE", o_on, 0, 's' },

/*
 * After a lot of thinking, and a lot of input from "network gurus",
 * I've decided to disable this command, since it could be
 * considered a DoS attack.  The flood itself doesn't actually last
 * longer than 5 seconds at most, but it can be argued that during
 * that period, it could interfere with other internet activities
 * that the user is engaged in.  That is illegal.  Although
 * originally coded for use on ednet, a small group of friends, for
 * fun, if it were to be used on the wrong person, it could become
 * an excuse to sue and/or accuse the administrator of the services
 * of malicious DoS activities (especially if used often and without
 * good reason).
 *                   -Patrick Alken 10/21/1998
 */

 /*
  * "fuckover" re-enabled under #define ALLOW_FUCKOVER due to
  * user requests (undefined by default)
  * 03/07/1999
  */

#ifdef ALLOW_FUCKOVER
  { "FUCKOVER", o_fuckover, 0, 's' },
#endif

  { "OFF", o_off, 0, 's' },
  { "ON", o_on, 0, 's' },
  { "REHASH", o_rehash, 0, 's' },
  { "RELOAD", o_reload, 0, 's' },
  { "RESTART", o_restart, 0, 's' },
  { "SAVE", o_save, 0, 's' },
  { "SET", o_set, 0, 's' },
  { "SHUTDOWN", o_off, 0, 's' },

  /*
   * Miscellaneous
   */

#ifdef ALLOW_JUPES
#ifdef JUPEVOTES
/* don't let people use it in msg at all.. */
  { "JUPE", o_jupe, 1, 'j' },
#else
  { "JUPE", o_jupe, 0, 'j' },
#endif
  { "UNJUPE", o_unjupe, 0, 'j' },
#endif

#ifdef ALLOW_GLINES
  { "GLINE", o_gline, 0, 'g' },
  { "AKILL", o_gline, 0, 'g' },
  { "UNGLINE", o_ungline, 0, 'g' },
#endif

  { "KLINE", o_kline, 0, 'g' },

  /*
   * Dcc Only
   */

  { "WHO", o_who, 1, 0 },
  { "BOOT", o_boot, 1, 'a' },
  { "QUIT", o_quit, 1, 0 },
  { "MOTD", o_motd, 1, 0 },
  { "LINK", o_link, 1, 'o' },
  { "UNLINK", o_unlink, 1, 'o' },

  { 0, 0, 0, 0 }
};

/* sub-commands for OperServ IGNORE */
static struct OperCommand ignorecmds[] = {
  { "ADD", o_ignore_add, 0, 0 },
  { "DEL", o_ignore_del, 0, 0 },
  { "LIST", o_ignore_list, 0, 0 },

  { 0, 0, 0, 0 }
};

#ifdef HIGHTRAFFIC_MODE

/* sub-commands for OperServ HTM */
static struct OperCommand htmcmds[] = {
  { "ON", o_htm_on, 0, 0 },
  { "OFF", o_htm_off, 0, 0 },
  { "MAX", o_htm_max, 0, 0 },

  { 0, 0, 0, 0 }
};

#endif /* HIGHTRAFFIC_MODE */

/*
 * OperServ usermodes
 */
static struct UmodeInfo
{
  char usermode;
  int flag;
} Usermodes[] = {
  { 'b', OPERUMODE_B },     /* see tcm bot connect attempts etc */
  { 'c', OPERUMODE_C },     /* see lone connections */
  { 'C', OPERUMODE_CAPC },  /* see client connections */
  { 'd', OPERUMODE_D },     /* debug information */
  { 'e', OPERUMODE_E },     /* see clone exits */
  { 'E', OPERUMODE_CAPE },  /* see lient exits */
  { 'j', OPERUMODE_J },     /* see channel joins */
  { 'k', OPERUMODE_K },     /* see channel kicks */
  { 'l', OPERUMODE_L },     /* see netsplits/netjoins */
  { 'm', OPERUMODE_M },     /* see channel modes */
  { 'n', OPERUMODE_N },     /* see network activity */
  { 'o', OPERUMODE_O },     /* see new operators */
  { 'O', OPERUMODE_CAPO },  /* see operator kills */
  { 'p', OPERUMODE_P },     /* see channel parts */
  { 's', OPERUMODE_S },     /* see usages of service bots */
  { 'S', OPERUMODE_CAPS },  /* see server kills */
  { 't', OPERUMODE_T },     /* see channel topic changes */
  { 'y', OPERUMODE_Y },     /* see various DoTimer() events */

  { 0, 0 }
};

/*
os_process()
  Process command coming from 'nick' directed towards n_OperServ
*/

void
os_process(char *nick, char *command, int sockfd)

{
  struct DccUser *dccptr;
  struct Luser *lptr = NULL;
  int acnt;
  int bad;
  char **arv;
  struct OperCommand *cptr;
  struct Userlist *cmduser = NULL;

#ifdef OPERNICKIDENT
  struct NickInfo *nptr;
#endif /* OPERNICKIDENT */

  if (!command)
    return;

  if (!(lptr = FindClient(nick)))
#if 0
    /* Quickfix. All further code relies on lptr, however the old code did
     * not check case when lptr was not filled and sockfd == NODCC. In
     * that case every lptr->blah lookup will cause segfault. -kre */
    && (sockfd == NODCC))
#endif
    return;

  if ((Network->flags & NET_OFF) &&
      (ircncmp(command, "ON", 2) != 0) &&
      (ircncmp(command, "REGISTER", 8) != 0) &&
      (ircncmp(command, "IDENTIFY", 8) != 0))
  {
    os_notice(lptr, sockfd,
      "Services are currently \002disabled\002");
    return;
  }

  dccptr = IsDccSock(sockfd);

  if (!dccptr)
  {
    /*
     * It is a /msg command - give argument 0 so it MUST
     * match hostnames, not just nicknames - otherwise if
     * someone manages to get an oper's password, all they
     * have to do is come on with any hostname and just use
     * the oper's nickname - make sure that doesn't happen
     */
    cmduser = GetUser(0, lptr->nick, lptr->username, lptr->hostname);
  }

  if (cmduser || dccptr)
  {
    onick = ouser = ohost = NULL;

    if (!dccptr)
    {
      /*
       * As a last resort, check if there is a client on the network
       * with the nick
       */
      if (lptr)
      {
        onick = MyStrdup(lptr->nick);
        ouser = MyStrdup(lptr->username);
        ohost = MyStrdup(lptr->hostname);
      }
    }
    else
    {
      onick = MyStrdup(dccptr->nick);
      ouser = MyStrdup(dccptr->username);
      ohost = MyStrdup(dccptr->hostname);
    }

    if (!onick || !ouser || !ohost)
      return;

    if (dccptr)
      cmduser = DccGetUser(dccptr);

    if (!cmduser)
    {
      MyFree(onick);
      MyFree(ouser);
      MyFree(ohost);
      /* Be a bit more verbose on denial. -kre */
      os_notice(lptr, sockfd, "Access Denied - user not found");
      return;
    }

    if (OpersHaveAccess)
    {
      /*
       * GetUser() returned GenericOper, meaning this person is an
       * operator, but does not have an O: line. Mark them as registered
       * so they can perform Operator commands on OperServ
       *
       * If OPERNICKIDENT is defined, mark them as registered only if
       * they idented to NickServ first. It's inexcusable to risk
       * someone with just an oper's ircd password being able to badly
       * mess things up via services. Force them to at least know one
       * more password (NickServ). Ideally, an oper's ircd, nickserv and
       * operserv passwords should be different. -ike
       */

#ifdef OPERNICKIDENT
      nptr = FindNick(lptr->nick);
      if (lptr && (cmduser == GenericOper)
          && ((Network->flags & NET_OFF)
          || nptr->flags & NS_IDENTIFIED))
#else
      if (lptr && (cmduser == GenericOper))
#endif /* OPERNICKIDENT */

      lptr->flags |= L_OSREGISTERED;
    } /* if (OpersHaveAccess) */

    acnt = SplitBuf(command, &arv);
    if (acnt == 0)
    {
      MyFree(arv);
      MyFree(onick);
      MyFree(ouser);
      MyFree(ohost);
      return;
    }

    cptr = GetoCommand(opercmds, arv[0]);

    if (cptr == (struct OperCommand *) -1)
    {
      os_notice(lptr, sockfd, "Ambiguous command [%s]",
        arv[0]);
      MyFree(arv);
      MyFree(onick);
      MyFree(ouser);
      MyFree(ohost);
      return;
    }

    bad = 0;

    if (!cptr)
      bad = 1;
    else
    {
      /* 
        * check if the command is dcc only - if so, make sure it didn't
        * come from a privmsg
        */
      if ((cptr->dcconly) && (sockfd == NODCC))
        bad = 1;

      if ((cptr->flag) &&
          !IsRegistered(lptr, sockfd) &&
          !IsValidAdmin(lptr))
        bad = 1;
    }

    if (bad)
    {
      os_notice(lptr, sockfd, "Unknown command [%s]",
        arv[0]);

      if (sockfd == NODCC)
      {
        int    ii;

        /* send the msg to the partyline */
        BroadcastDcc(DCCOPS, "[%s!%s@%s]",
          lptr->nick,
          lptr->username,
          lptr->hostname);
        for (ii = 0; ii < acnt; ++ii)
          BroadcastDcc(DCCOPS, " %s", arv[ii]);
        BroadcastDcc(DCCOPS, "\n");
      }
    }
    else
    {
      /* check if the user has authorization to exec command */
      if (CheckAccess(cmduser, cptr->flag))
      {
        /* call cptr->func to execute command */
        (*cptr->func)(lptr, acnt, arv, sockfd);
        MyFree(arv);
        MyFree(onick);
        MyFree(ouser);
        MyFree(ohost);
        return;
      }
      else
        os_notice(lptr, sockfd, ERR_NOPRIVS);
    }
    MyFree(arv);
    MyFree(onick);
    MyFree(ouser);
    MyFree(ohost);
    return;
  } /* if (cmduser || dccptr) */
  else
    /* Be more verbose. -kre */
    os_notice(lptr, NODCC, "Access Denied - no O-line");

  return;
} /* os_process() */

/*
os_loaddata()
  Load OperServ database - return 1 if successful, -1 if not, and -2
if errors are unrecoverable
*/

int
os_loaddata()

{
  FILE *fp;
  char line[MAXLINE], **av;
  int ac,
      ret = 1,
      cnt,
      found;
  struct Userlist *tempuser;

  if (!(fp = fopen(OperServDB, "r")))
  {
    /* OperServ data file doesn't exist */
    return (-1);
  }

  /*
   * If this is being called from ReloadData(), make sure
   * all users' umodes are zeroed, or OperServ will
   * log a warning about multiple entries
   */
  for (tempuser = UserList; tempuser; tempuser = tempuser->next)
    tempuser->umodes = 0;

  cnt = 0;
  while (fgets(line, MAXLINE - 1, fp))
  {
    cnt++;

    ac = SplitBuf(line, &av);
    if (!ac)
    {
      /* blank line probably */
      MyFree(av);
      continue;
    }

    if (av[0][0] == ';')
    {
      /* its a comment */
      MyFree(av);
      continue;
    }

    /*
     * OperServDB format should be:
     * Nickname <umodes>
     * where <umodes> is an integer number corresponding to
     * the OPERUMODE_* flags
     */

    if (ac < 2)
    {
      fatal(1, "%s:%d Invalid database format (FATAL)",
        OperServDB,
        cnt);
      ret = (-2);
      MyFree(av);
      continue;
    }

    /*
     * Go through user list and try to find a matching nickname
     * with av[0]
     */
    found = 0;
    for (tempuser = UserList; tempuser; tempuser = tempuser->next)
    {
      if (!irccmp(av[0], tempuser->nick))
      {
        found = 1;
        if (tempuser->umodes && (tempuser->umodes != OPERUMODE_INIT))
        {
          /*
           * we've already seen a line with this nickname in
           * OperServDB, so there must be multiple entries
           */
          fatal(1, "%s:%d OperServ entry [%s] has multiple occurences (using first)",
            OperServDB,
            cnt,
            tempuser->nick);
          if (ret > 0)
            ret = (-1);
          break;
        }
        else
          tempuser->umodes = atol(av[1]);
      } /* if (!irccmp(av[0], tempuser->nick)) */
    } /* for (tempuser = UserList; tempuser; tempuser = tempuser->next) */

    if (!found)
    {
      /*
       * No O: line found matching the nickname
       */
      fatal(1, "%s:%d No O: line entry for nickname [%s] (ignoring)",
        OperServDB,
        cnt,
        av[0]);
      if (ret > 0)
        ret = (-1);
    }

    MyFree(av);
  } /* while (fgets(line, MAXLINE - 1, fp)) */

  fclose(fp);

  /*
   * Now go through the list again and anyone who's
   * usermodes are set to OPERUMODE_INIT did not have
   * an entry in oper.db - therefore give them
   * default usermodes (+cby)
   */
  for (tempuser = UserList; tempuser; tempuser = tempuser->next)
  {
    if (tempuser->umodes == OPERUMODE_INIT)
      tempuser->umodes = (OPERUMODE_C | OPERUMODE_B | OPERUMODE_Y);
  }

  return (ret);
} /* os_loaddata() */

/*
GetoCommand()
 Return a pointer to the index of opercmds[] containing the command
'name'
Returns NULL if command not found, or (struct OperCommand *) -1 if multiple
matches found
*/

static struct OperCommand *
GetoCommand(struct OperCommand *cmdlist, char *name)

{
  struct OperCommand *cmdptr, *tmp;
  int matches, /* number of matches we've had so far */
      clength;

  if (!cmdlist || !name)
    return (NULL);

  tmp = NULL;
  matches = 0;
  clength = strlen(name);
  for (cmdptr = cmdlist; cmdptr->cmd; cmdptr++)
  {
    if (!ircncmp(name, cmdptr->cmd, clength))
    {
      if (clength == strlen(cmdptr->cmd))
      {
        /*
         * name and cmdptr->cmd are the same length, so it
         * must be an exact match, don't search any further
         */
        matches = 0;
        break;
      }
      tmp = cmdptr;
      matches++;
    }
  }

  /*
   * If matches > 1, name is an ambiguous command, so the
   * user needs to be more specific
   */
  if ((matches == 1) && (tmp))
    cmdptr = tmp;

  if (cmdptr->cmd)
    return (cmdptr);

  if (matches == 0)
    return (NULL); /* no matches found */
  else
    return ((struct OperCommand *) -1); /* multiple matches found */
} /* GetoCommand() */

/*
os_notice()
  args: struct Luser *lptr, int sockfd, char *msg
  purpose: send a NOTICE to 'lptr->nick' with 'msg'
  return: none
*/

static void
os_notice(struct Luser *lptr, int sockfd, char *format, ...)

{
  va_list args;
  char finstr[MAXLINE * 2];

  va_start(args, format);

  vsprintf_irc(finstr, format, args);

  va_end(args);

  if (sockfd == NODCC)
  {
    if (lptr)
      toserv(":%s NOTICE %s :%s\n",
        (ServerNotices) ? Me.name : n_OperServ,
        lptr->nick,
        finstr);
  }
  else
  {
    writesocket(sockfd, finstr);
    writesocket(sockfd, "\n");
  }
} /* os_notice() */

/*
os_join()
 Have OperServ join a channel
*/

void
os_join(struct Channel *cptr)

{
  char sendstr[MAXLINE];
  char **av;

  if (!cptr)
    return;

  ircsprintf(sendstr,
    ":%s SJOIN %ld %s + :@%s\n",
    Me.name,
    (long) cptr->since,
    cptr->name,
    n_OperServ);

  toserv(sendstr);

  SplitBuf(sendstr, &av);

  /* Add OperServ to channel nick list etc */
  AddChannel(av, 0, (char **) NULL);

  MyFree(av);
} /* os_join() */

/*
os_join_ts_minus_1
 Similar to os_join((), but have OperServ join the channel
with TS - 1
*/

void
os_join_ts_minus_1(struct Channel *cptr)

{
  char sendstr[MAXLINE];
  char **av;
  int ac;

  if (!cptr)
    return;

  ircsprintf(sendstr,
    ":%s SJOIN %ld %s + :@%s\n",
    Me.name,
    (cptr->since != 0) ? (long) cptr->since - 1 : 0,
    cptr->name,
    n_OperServ);

  toserv(sendstr);
  ac = SplitBuf(sendstr, &av);
  s_sjoin(ac, av);
  MyFree(av);
} /* os_join_ts_minus_1() */

/*
os_part()
 Have OperServ part a channel
*/

void
os_part(struct Channel *chptr)

{
  if (!chptr)
    return;

  toserv(":%s PART %s\n",
    n_OperServ,
    chptr->name);

  RemoveFromChannel(chptr, Me.osptr);
} /* os_part() */

/*
o_RecordCommand()
 Called when an OperServ command is executed - log it and
send it to users with a +s usermode
*/

static void
o_RecordCommand(int sockfd, char *format, ...)

{
  va_list args;
  char buffer[MAXLINE * 2];

  va_start(args, format);

  vsprintf_irc(buffer, format, args);

  va_end(args);

  /* log the command */
  putlog(LOG2,
    "%s: %s!%s@%s %s",
    n_OperServ,
    onick,
    ouser,
    ohost,
    buffer);

  /* send it to opers with usermode +s */
  if (sockfd == NODCC)
  {
    /*
     * If they're using the command via privmsg,
     * use the format: OperServ: nick!user@host COMMAND ...
     */
    SendUmode(OPERUMODE_S,
      "%s: %s!%s@%s %s",
      n_OperServ,
      onick,
      ouser,
      ohost,
      buffer);
  }
  else
  {
    /*
     * If they're using the command via dcc chat,
     * use the format: #nick# COMMAND ...
     */
    SendUmode(OPERUMODE_S,
      "#%s# %s",
      onick,
      buffer);
  }
} /* o_RecordCommand() */

/*
o_Wallops()
 Called when an OperServ command is executed - send a wallops
*/

void
o_Wallops(char *format, ...)

{
  va_list args;
  char buffer[MAXLINE * 2];

  if (!DoWallops)
    return;

  va_start(args, format);

  vsprintf_irc(buffer, format, args);

  va_end(args);

  toserv(":%s WALLOPS :%s: %s\n",
     Me.name,
     n_OperServ,
     buffer);

} /* o_Wallops() */

/*
o_identify()
  Checks if lptr->nick is allowed to register with password av[1]
  ac: param count
*/

static void
o_identify(struct Luser *lptr, int ac, char **av, int sockfd)

{
  struct Userlist *uptr;

#ifdef OPERNICKIDENT
  struct NickInfo *nptr;
#endif /* OPERNICKIDENT */

  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002IDENTIFY <password>\002");
    return;
  }

#ifdef OPERNICKIDENT
  nptr = FindNick (lptr->nick);
  if (!(Network->flags & NET_OFF) &&
      (!nptr || !(nptr->flags & NS_IDENTIFIED)))
  {
    os_notice(lptr, sockfd, "You must first identify to \002%s\002",
        n_NickServ);
    return;
  }
#endif /* OPERNICKIDENT */

  if (IsRegistered(lptr, sockfd))
  {
    os_notice(lptr, sockfd, "You are already identified");
    return;
  }

  /* No need here to check ohost or ouser once again, since they are
   * already checked in os_process() -kre */
  uptr = GetUser(1, onick, ouser, ohost);
  if (!uptr)
    return;

  if (operpwmatch(uptr->password, av[1]))
  {
    struct DccUser *dccptr;

    os_notice(lptr, sockfd, "You are now identified");
    if (lptr)
      lptr->flags |= L_OSREGISTERED;

    if ((dccptr = IsDccSock(sockfd)))
      ClearDccPending(dccptr);

    o_RecordCommand(sockfd,
      "IDENTIFY");
  }
  else 
  {
    /* Be more verbose. -kre */
    os_notice(lptr, sockfd, "Access Denied - wrong password");

    o_RecordCommand(sockfd,
      "failed IDENTIFY");
  }
} /* o_identify() */

/*
o_rehash()
  Rehash the configuration file
*/

static void
o_rehash(struct Luser *lptr, int ac, char **av, int sockfd)

{
  if (Rehash() == 0)
  {
    os_notice(lptr, sockfd, "Rehash complete");

    o_RecordCommand(sockfd,
      "REHASH");

    o_Wallops("REHASH");
  }
  else
  {
    os_notice(lptr, sockfd, "Rehash failed");

    o_RecordCommand(sockfd,
      "REHASH (failed)");

    o_Wallops("REHASH (failed)");
  }
} /* o_rehash() */

/*
o_restart()
  Restart services
*/

static void
o_restart(struct Luser *lptr, int ac, char **av, int sockfd)

{
  o_RecordCommand(sockfd,
    "RESTART");

  o_Wallops("RESTART");

  /* As found in Hyb6...this is needed to avoid breaking RESTART */
  unlink(PidFile);
 
  /* Reinitialise all connections and memory */
  ServReboot();

  /* And cycle it. It seems that this was missing -kre */
  CycleServers();
} /* o_restart() */

#ifdef ALLOW_DIE

/*
o_die()
  Terminates HybServ
*/

static void
o_die(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char *reason;

  if (ac < 2)
    reason = MyStrdup("");
  else
    reason = GetString(ac - 1, av + 1);

  o_RecordCommand(sockfd,
    "DIE [%s]",
    reason);

  o_Wallops("DIE [%s]",
    reason);

  os_notice(lptr, sockfd, "Shutting down");

  DoShutdown(onick, reason);
} /* o_die() */

#endif /* ALLOW_DIE */

/*
o_status()
  Displays various statistics such as jupes, glines, users etc
*/

static void
o_status(struct Luser *lptr, int ac, char **av, int sockfd)

{
  float mem;
  struct utsname un;
  struct rusage rus;

  o_RecordCommand(sockfd,
    "STATUS");

#if defined(NICKSERVICES) || defined(STATSERVICES)
  os_notice(lptr, sockfd, "Expire settings");

#ifdef NICKSERVICES

  #ifdef CHANNELSERVICES
    os_notice(lptr, sockfd, "            Channels: \002%s\002",
      timeago(ChannelExpire, 3));
  #endif

  os_notice(lptr, sockfd, "           Nicknames: \002%s\002",
    timeago(NickNameExpire, 3));

  #ifdef MEMOSERVICES
    os_notice(lptr, sockfd, "               Memos: \002%s\002",
      timeago(MemoExpire, 3));
  #endif

#endif /* NICKSERVICES */

#ifdef STATSERVICES
  os_notice(lptr, sockfd, "       StatServ data: \002%s\002",
    timeago(StatExpire, 3));
#endif

  os_notice(lptr, sockfd, "\002\002");

#endif /* defined(NICKSERVICES) || defined(STATSERVICES) */

#ifdef NICKSERVICES

  os_notice(lptr, sockfd, "Databases");

  os_notice(lptr, sockfd, "       Save interval: \002%s\002",
    timeago(DatabaseSync, 3));

  os_notice(lptr, sockfd, "           Next save: \002%s\002",
    timeago((DatabaseSync - (current_ts % DatabaseSync)), 3));

  os_notice(lptr, sockfd, " Password encryption: \002%s\002",
  #ifdef CRYPT_PASSWORDS
    "on");
  #else
    "off");
  #endif

  os_notice(lptr, sockfd, "\002\002");

#endif /* NICKSERVICES */

  os_notice(lptr, sockfd, "Security settings");

  os_notice(lptr, sockfd, "  O: line encryption: \002%s\002",
  #ifdef CRYPT_OPER_PASSWORDS
    "on");
  #else
    "off");
  #endif

  os_notice(lptr, sockfd, "    Flood Protection: \002%s\002",
    (FloodProtection) ? "on" : "off");

  if (FloodProtection)
  {
    os_notice(lptr, sockfd, "       Flood trigger: \002%d\002 messages in \002%s\002",
      FloodCount,
      timeago(FloodTime, 3));

    os_notice(lptr, sockfd, "         Ignore time: \002%s\002",
      timeago(IgnoreTime, 3));
  
    os_notice(lptr, sockfd, "    Permanent Ignore: \002%d\002 offenses",
      IgnoreOffenses);
  }

#ifdef NICKSERVICES

  #ifdef CHANNELSERVICES
    os_notice(lptr, sockfd, "   Maximum AutoKicks: \002%d\002 per channel",
      MaxAkicks);
  #endif

  os_notice(lptr, sockfd, "  Nicknames held for: \002%s\002",
    timeago(NSReleaseTimeout, 3));

#endif /* NICKSERVICES */

  if (MaxClones)
  {
    os_notice(lptr, sockfd, "      Maximum clones: \002%d\002 (autokill: \002%s\002)",
      MaxClones,
      (AutoKillClones) ? "on" : "off");
  } /* if (MaxClones) */

  os_notice(lptr, sockfd, "   High-Traffic Mode: \002%s\002",
  #ifdef HIGHTRAFFIC_MODE
    (HTM == 1) ? "on" : "off");
  #else
    "disabled");
  #endif

  os_notice(lptr, sockfd, "   Restricted Access: \002%s\002",
    (RestrictedAccess) ? "on" : "off");

  os_notice(lptr, sockfd, "\002\002");
  os_notice(lptr, sockfd, "General Status");

  /* Returns -1 on failure, but _not_ 0 on success! -kre */
  if (uname(&un) != -1)
  {
    os_notice(lptr, sockfd, "          Running on: \002%s %s\002",
      un.sysname,
      un.release);
  }
  else
    os_notice(lptr, sockfd, "          Running on: \002*unknown*\002");

  mem = CalcMem(NULL, NODCC);
  os_notice(lptr, sockfd, "        Memory Usage: \002%0.2f\002 kb (\002%0.2f\002 mb)",
    mem / 1024,
    (mem / 1024) / 1024);

  if (getrusage(RUSAGE_SELF, &rus) == -1)
    putlog(LOG1, "o_status: getrusage() error: %s",
      strerror(errno));
  else
  {
    time_t seconds;

    seconds = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
    if (seconds == 0)
      seconds = 1;
    os_notice(lptr, sockfd, "            Cpu Time: \002%02d:%02d\002 (mins:secs)",
      seconds / 60,
      seconds % 60);
  }

  if (HubSock != NOSOCKET)
  {
    os_notice(lptr, sockfd, "         Current Hub: \002%s\002 (connected for [\002%s\002])",
      currenthub->realname ? currenthub->realname : currenthub->hostname,
      timeago(currenthub->connect_ts, 0));
  }
  else
    os_notice(lptr, sockfd, "         Current Hub: \002none\002");

  os_notice(lptr, sockfd, "Hub Connect Interval: \002%s\002",
    timeago(ConnectFrequency, 3));

  if (AutoLinkFreq)
    os_notice(lptr, sockfd, "   Tcm Link Interval: \002%s\002",
      timeago(AutoLinkFreq, 3));

  os_notice(lptr, sockfd,   "         Compiled at: \002%s\002, \002%s\002", __DATE__,
		  __TIME__);
} /* o_status() */

/*
o_kill()
  Issues a KILL for av[1] if lptr->nick is authorized
*/

static void
o_kill(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char *reason;
  int ii;
  struct Luser *kptr;

  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002KILL <nick> [reason]\002");
    return;
  }

  kptr = FindClient(av[1]);

  if (!kptr)
  {
    os_notice(lptr, sockfd,
      "No such user: %s",
      av[1]);
    return;
  }

  if (FindService(kptr))
  {
    os_notice(lptr, sockfd, "%s!%s@%s is a Service",
      kptr->nick,
      kptr->username,
      kptr->hostname);
    return;
  }

  if (ac < 3)
    reason = MyStrdup("");
  else
    reason = GetString(ac - 2, av + 2);

  if (kptr->flags & L_OSREGISTERED)
    ii = 1;
  else
    ii = 0;

  /* Check if the user has an exception flag */
  if (IsProtected(GetUser(ii, kptr->nick, kptr->username, kptr->hostname)))
  {
    /* The user is protected */
    os_notice(lptr, sockfd, "%s!%s@%s is protected", 
      kptr->nick, kptr->username, kptr->hostname);

    putlog(LOG2,
      "%s!%s@%s attempted to KILL protected user %s [%s]",
      onick, ouser, ohost, av[1], reason);

    MyFree(reason);
    return;
  }

  o_RecordCommand(sockfd,
    "KILL %s [%s]",
    kptr->nick,
    reason);

  o_Wallops("KILL %s [%s]",
    kptr->nick,
    reason);

  toserv(":%s KILL %s :%s!%s (%s (Requested by %s))\n",
    n_OperServ, av[1], Me.name, n_OperServ, reason, onick);

  /* remove the killed nick from list */
  DeleteClient(kptr);

  os_notice(lptr, sockfd,
    "%s has been killed [%s]", av[1], reason);

  MyFree(reason);

  if (Me.sptr)
    Me.sptr->numoperkills++;

  Network->TotalOperKills++;
#ifdef STATSERVICES
  if (Network->TotalOperKills > Network->OperKillsT)
    Network->OperKillsT = Network->TotalOperKills;
#endif
} /* o_kill() */

#ifdef ALLOW_JUPES

/*
o_jupe()
  Jupes server av[1] with reason av[2]-.  (Prevents a server from
  connecting to the network)
*/

static void
o_jupe(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char *reason; /* reason for jupe */
  char sendstr[MAXLINE], whostr[MAXLINE], **arv;
  struct Jupe *tempjupe;
  FILE *fp;
  int ii,
      nickjupe = 1;
  struct tm *jupe_tm;
  time_t CurrTime;
  struct Luser *jptr;
  int bogusnick,
      bogusserv;
  int dots;

  if (ac < 2)
  {
    os_notice(lptr, sockfd,
      "Syntax: \002JUPE <server/nick> [reason]\002");
    return;  
  }

  /*
   * Make sure we don't jupe our hub server
   * (thanks good!)
   */
  if (currenthub && currenthub->realname)
  {
    if (match(av[1], currenthub->realname))
    {
      os_notice(lptr, sockfd,
        "[\002%s\002] matches services' current hub server, cancelling jupe",
        av[1]);
      return;
    }
  }

  bogusserv = 0;
  bogusnick = 0;
  dots = 0;

  for (ii = 0; ii < strlen(av[1]); ++ii)
  {
    if (IsKWildChar(av[1][ii]))
      nickjupe = 0;

    if (!IsServChar(av[1][ii]))
      bogusserv = 1;

    if (!IsNickChar(av[1][ii]))
      bogusnick = 1;

    /*
     * A server jupe must have at least 1 dot (".").
     */
    if (av[1][ii] == '.')
      ++dots;
  }

  if (nickjupe && bogusnick)
  {
    os_notice(lptr, sockfd,
      "Invalid nickname: %s",
      av[1]);
    return;
  }
  else if (!nickjupe && (bogusserv || !dots))
  {
    os_notice(lptr, sockfd,
      "Invalid server: %s",
      av[1]);
    return;
  }

  /* Check if jupe already exists */
  for (tempjupe = JupeList; tempjupe; tempjupe = tempjupe->next)
  {
    if (match(av[1], tempjupe->name))
    {
      os_notice(lptr, sockfd,
        "A jupe already exists for %s [%s]",
        tempjupe->name,
        tempjupe->reason);
      return;
    }
  }

  if (JupeFile)
    fp = fopen(JupeFile, "a+");
  else
    fp = fopen(ConfigFile, "a+");

  if (!fp)
  {
    os_notice(lptr, sockfd, "Unable to open config file");
    return;
  }

#ifdef JUPEVOTES
  if (!nickjupe)
    {
      if (AddVote(av[1], onick) == -1)
        {
          os_notice(lptr, sockfd, "You have already voted for %s to be Juped",
                    av[1]);
          return;
        }
      else
        {
          os_notice(lptr, sockfd, "Your jupe vote for %s has been counted, total votes: %d/%d",
                    av[1], CountVotes(av[1]), JUPEVOTES);
          o_RecordCommand(sockfd, "VOTE:%d/%d JUPE %s",
                          CountVotes(av[1]), JUPEVOTES, av[1]);
          o_Wallops("VOTE:%d/%d JUPE %s",
                    CountVotes(av[1]), JUPEVOTES, av[1]);
        }

      if (CountVotes(av[1]) < JUPEVOTES)
        return;
      else
        DeleteVote(av[1]);
    }
#endif

  if (ac < 3)
  {
    if (nickjupe)
      reason = MyStrdup("Jupitered Nick");
    else
      reason = MyStrdup("Jupitered Server");
  }
  else
    reason = GetString(ac - 2, av + 2);

  CurrTime = current_ts;
  jupe_tm = localtime(&CurrTime);
  ircsprintf(whostr, "%d/%02d/%02d %s@%s",
    1900 + jupe_tm->tm_year, jupe_tm->tm_mon + 1, jupe_tm->tm_mday,
    onick, n_OperServ);

  fprintf(fp, "J:%s:%s:%s\n", av[1], reason, whostr);
  fclose(fp);

  AddJupe(av[1], reason, whostr); /* add jupe to list */

  os_notice(lptr, sockfd,
    "Jupe for %s [%s] has been successfully added to config file",
    av[1],
    reason);

  o_RecordCommand(sockfd,
    "JUPE %s [%s]",
    av[1],
    reason);

  o_Wallops("JUPE %s by %s [%s]",
    av[1],
    onick,
    reason);

  if (!nickjupe)
    DoJupeSquit(av[1], reason, whostr);
  else
  {
    if ((jptr = FindClient(av[1])))
    {
      /* kill the juped nick */
      toserv(":%s KILL %s :%s!%s (%s)\n",
        n_OperServ,
        av[1],
        Me.name,
        n_OperServ,
        reason);
      DeleteClient(jptr);

      if (Me.sptr)
        ++Me.sptr->numoperkills;

      ++Network->TotalOperKills;

    #ifdef STATSERVICES
      if (Network->TotalOperKills > Network->OperKillsT)
        Network->OperKillsT = Network->TotalOperKills;
    #endif
    } /* if ((jptr = FindClient(av[1]))) */

    /* replace nick with a fake user */
    ircsprintf(sendstr,
      "NICK %s 1 %ld +i %s %s %s :%s\n",
      av[1], (long) CurrTime, JUPED_USERNAME, JUPED_HOSTNAME,
      Me.name, reason);
    toserv(sendstr);
    SplitBuf(sendstr, &arv);
    AddClient(arv);
  } /* else */
  
  MyFree(reason);
} /* o_jupe() */

/*
o_unjupe()
  Removes jupe on server av[1]
*/

static void
o_unjupe(struct Luser *lptr, int ac, char **av, int sockfd)

{
  int jcnt; /* number of matching jupes found */
  char line[MAXLINE], linetemp[MAXLINE];
  char tmpfile[MAXLINE];
  char *configname;
  char *key;
  char *jhost;
  struct Jupe *tempjupe, *next;
  FILE *configfp;
  /*
   * random file to store all lines except the ones which match
   * the jupes
   */
  FILE *fp;

  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002UNJUPE <server>\002");
    return;
  }

  o_RecordCommand(sockfd,
    "UNJUPE %s",
    av[1]);

  o_Wallops("UNJUPE %s by %s",
    av[1],
    onick);

  /* remove J: line from config file */
  if (JupeFile)
    configname = JupeFile;
  else
    configname = ConfigFile;

  if ((configfp = fopen(configname, "r")) == NULL)
  {
    os_notice(lptr, sockfd, "Unable to open config file");
    return;
  }

  ircsprintf(tmpfile, "%s.tmp", configname);

  fp = fopen(tmpfile, "w"); 
  jcnt = 0;
  while (fgets(line, MAXLINE - 1, configfp))
  {
    strcpy(linetemp, line);
    key = strtok(line, ":");
    if ((*key == 'J') || (*key == 'j'))
    {
      jhost = strtok(NULL, ":"); 
      if (match(av[1], jhost) == 0)
        fputs(linetemp, fp);
      else
        ++jcnt;
    }
    else
      fputs(linetemp, fp);
  } /* while () */
  fclose(configfp);
  fclose(fp);

  rename(tmpfile, configname);

  if (jcnt == 0)
  {
    os_notice(lptr, sockfd, "No jupes matching [%s] found", 
      av[1]);
    return;
  }

  for (tempjupe = JupeList; tempjupe; )
  {
    if (match(av[1], tempjupe->name))
    {
      if (tempjupe->isnick)
      {
        struct Luser *tempuser;
        
        /* if theres a pseudo client holding the nick, kill it */
        if ((tempuser = FindClient(tempjupe->name)))
        {
          toserv(":%s QUIT :UnJuped\n",
            tempuser->nick);
          DeleteClient(tempuser);
        }
      }
      else
      {
        /*
         * squit all pseudo servers we created while they were juped
         */
        do_squit(tempjupe->name, "UnJuped");
      }

      next = tempjupe->next;

      DeleteJupe(tempjupe); /* remove jupe from list */

      tempjupe = next;
    } /* if (match(av[1], tempjupe->name)) */
    else
      tempjupe = tempjupe->next;
  }

  os_notice(lptr, sockfd,
    "Jupes matching [%s] removed (%d matches)", 
    av[1], 
    jcnt);
} /* o_unjupe() */

#endif /* ALLOW_JUPES */

/*
o_omode()
  Sets modes av[2]- on channel av[1] (requires "a" flag)
*/

static void
o_omode(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char *omodes;
  struct Channel *chptr;

  if (ac < 3) 
  {
    os_notice(lptr, sockfd,
      "Syntax: \002OMODE <#channel> <modes>\002");
    return;
  }

  omodes = GetString(ac - 2, av + 2);

  o_RecordCommand(sockfd,
    "OMODE from %s - %s %s",
    lptr->nick, av[1], omodes);

  o_Wallops("OMODE from %s - %s %s",
    lptr->nick, av[1], omodes);

  if (!(chptr = FindChannel(av[1])))
  {
    os_notice(lptr, sockfd, "No such channel: %s", av[1]);
    return;
  }

  /* set the modes */
  DoMode(chptr, omodes, 1);

  MyFree(omodes);
} /* o_omode() */

/*
o_secure()
  Secures channel av[1] by deopping non "f" users, oping "f" users,
and unbanning administrators
*/

static void
o_secure(struct Luser *lptr, int ac, char **av, int sockfd)

{
  struct Channel *cptr;

  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002SECURE <#channel>\002");
    return;
  }

  o_RecordCommand(sockfd,
    "SECURE %s",
    av[1]);

  o_Wallops("SECURE %s",
    av[1]);

  cptr = FindChannel(av[1]);
  if (cptr)
  {
    TakeOver(cptr);
    os_notice(lptr, sockfd, "%s has been secured",
      cptr->name);
  }
  else
    os_notice(lptr, sockfd, "No such channel: %s",
      av[1]);
} /* o_secure() */

#ifdef ALLOW_GLINES

/*
o_gline()
  Glines user av[1] from the network with reason av[2]- (bans them
  from entire network)
*/

static void
o_gline(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char whostr[MAXLINE];
  char *reason, *hostmask;
  char *username,
       *hostname;
  int ii, charcnt, sidx;
  FILE *fp;
  struct Luser *user, *prev;
  struct Gline *gptr;
  struct tm *gline_tm;
  time_t CurrTime;
  long expires;
  char uhost[UHOSTLEN + 2];

  if (ac < 2)
  {
    os_notice(lptr, sockfd,
      "Syntax: \002GLINE [time] <user@host> [reason]\002");
    return;
  }

  /* It _has_ to be ac counter check here - if not, and time not specified
   * OperServ will crash instantly, trying to gline probably wrong stuff
   * from av[2] if _any_ digit is present in av[1] since timestr will then
   * return non-zero value. Also, it is necessary to check if `@' is
   * contained in av[2] -kre */
  expires = timestr(av[1]);
  if ((strchr(av[1], '@') == NULL) && (ac > 2) && expires)
  {
    sidx = 3;
    hostmask = av[2];
  }
  else
  {
    sidx = 2;
    hostmask = av[1];
  }

  if (NonStarChars)
  {
    /* Check if a valid user@host was given */
    charcnt = 0;
    for (ii = strlen(hostmask) - 1; ii > 0; ii--)
    {
      if (IsKWildChar(hostmask[ii]))
        continue;

      charcnt++;
    }

    if (NonStarChars > charcnt)
    {
      os_notice(lptr, sockfd,
        "[%s] is too general, please include "
        "at least %d non (*,?,@,.) characters",
        hostmask, NonStarChars);
      return;
    }
  } /* if (NonStarChars) */

  if (!(hostname = strchr(hostmask, '@')))
  {
    username = NULL;
    hostname = hostmask;
  }
  else
  {
    username = hostmask;
    *hostname++ = '\0';
  }

  /* Check if hostmask matches an exception user */
  if (IsProtectedHost(username, hostname))
  {
    os_notice(lptr, sockfd, "%s@%s matches a protected host, unable to gline", 
      username ? username : "*",
      hostname);
    return;
  }

  /* Check if gline already exists */
  if ((gptr = IsGline(username ? username : "*", hostname)))
  {
    os_notice(lptr, sockfd,
      "Gline already exists for %s@%s [%s]", 
      gptr->username,
      gptr->hostname, 
      gptr->reason);
    return;  
  }

  if (GlineFile)
    fp = fopen(GlineFile, "a+");
  else
    fp = fopen(ConfigFile, "a+");

  if (fp == NULL)
  {
    os_notice(lptr, sockfd, "Unable to open config file");
    return;
  }

  if (ac < (sidx + 1))
    reason = MyStrdup("");
  else
    reason = GetString(ac - sidx, av + sidx);

  CurrTime = current_ts;
  gline_tm = localtime(&CurrTime);
  ircsprintf(whostr, "%d/%02d/%02d %s@%s",
    1900 + gline_tm->tm_year, gline_tm->tm_mon + 1,
    gline_tm->tm_mday, onick, n_OperServ);

  /* don't add temp glines to the config file */
  if (!expires)
    fprintf(fp, "G:%s@%s:%s:%s\n",
      username ? username : "*",
      hostname,
      reason,
      whostr);

  fclose(fp);

  ircsprintf(uhost, "%s@%s", username ? username : "*", hostname);

  /* add gline to list */
  AddGline(uhost, reason, whostr, expires);

  if (!expires)
  {
    os_notice(lptr, sockfd, "Gline for %s@%s [%s] has been activated", 
      username ? username : "*",
      hostname,
      reason);
  }
  else
  {
    os_notice(lptr, sockfd,
      "Temporary gline [%s] for %s@%s [%s] activated",
      av[1],
      username ? username : "*",
      hostname,
      reason);
  }

  if (!expires)
  {
    o_RecordCommand(sockfd,
      "GLINE from %s for %s@%s [%s]",
      lptr->nick, username ? username : "*", hostname, reason);

    o_Wallops("GLINE from %s for %s@%s [%s]",
      lptr->nick, username ? username : "*", hostname, reason);
  }
  else
  {
    o_RecordCommand(sockfd,
      "GLINE from %s for %s@%s [%s] (%s)",
      lptr->nick, username ? username : "*", hostname, reason, av[1]);

    o_Wallops("GLINE from %s for %s@%s [%s] (%s)",
      lptr->nick, username ? username : "*", hostname, reason, av[1]);
  }

  /* Check if any users on the network match the new gline */
  prev = NULL;
  for (user = ClientList; user; )
  {
    if (user->server == Me.sptr)
    {
      user = user->next;
      continue;
    }

    if (username)
      if (!match(username, user->username))
      {
        user = user->next;
        continue;
      }

    if (match(hostname, user->hostname))
    {
      toserv(":%s KILL %s :%s!%s (Glined: %s (%s))\n",
        n_OperServ,
        user->nick,
        Me.name,
        n_OperServ,
        reason,
        whostr);

      if (Me.sptr)
        Me.sptr->numoperkills++;

      Network->TotalOperKills++;
    #ifdef STATSERVICES
      if (Network->TotalOperKills > Network->OperKillsT)
        Network->OperKillsT = Network->TotalOperKills;
    #endif

      if (prev)
      {
        DeleteClient(user);
        user = prev;
      }
      else
      {
        DeleteClient(user);
        user = NULL;
      }
    }

    prev = user;

    if (user)
      user = user->next;
    else
      user = ClientList;
  }

#ifdef HYBRID_GLINES
  ExecuteGline(username, hostname, reason);
#endif /* HYBRID_GLINES */

#ifdef HYBRID7_GLINES
  Execute7Gline(username, hostname, reason);
#endif /* HYBRID7_GLINES */

  MyFree(reason);
} /* o_gline() */

/*
o_ungline()
  Removes gline av[1] from gline list
*/

static void
o_ungline(struct Luser *lptr, int ac, char **av, int sockfd)

{
  int gcnt;
  char line[MAXLINE], linetemp[MAXLINE];
  char tmpfile[MAXLINE];
  char chkstr[MAXLINE];
  char *configname;
  char *key;
  char *ghost;
  char *user,
       *host;
  struct Gline *gptr, *next;
  FILE *configfp;
  /*
   * random file to store all lines except the ones
   * which match the glines
   */
  FILE *fp;

  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002UNGLINE <user@host>\002");
    return;
  }

  o_RecordCommand(sockfd,
    "UNGLINE %s",
    av[1]);

  o_Wallops("UNGLINE %s",
    av[1]);

  strcpy(chkstr, av[1]);
  if (!(host = strchr(av[1], '@')))
  {
    user = NULL;
    host = av[1];
  }
  else
  {
    user = av[1];
    *host++ = '\0';
  }

  gcnt = 0;
  for (gptr = GlineList; gptr; gptr = next)
  {
    next = gptr->next;

    if (user && !match(user, gptr->username))
      continue;

    if (match(host, gptr->hostname))
    {
      ++gcnt;

      /*
       * remove gline from list
       */
      DeleteGline(gptr);
    }
  }

  if (gcnt == 0)
  {
    os_notice(lptr, sockfd,
      "No glines matching %s found", 
      chkstr);
    return;
  }

  if (GlineFile)
    configname = GlineFile;
  else
    configname = ConfigFile;

  if ((configfp = fopen(configname, "r")) == NULL)
  {
    os_notice(lptr, sockfd, "Unable to open config file");
    return;
  }

  ircsprintf(tmpfile, "%s.tmp", configname);

  fp = fopen(tmpfile, "w"); 

  while (fgets(line, MAXLINE - 1, configfp))
  {
    strcpy(linetemp, line);
    key = strtok(line, ":");
    if ((*key == 'g') || (*key == 'G'))
    {
      ghost = strtok(NULL, ":");
      if (match(chkstr, ghost) == 0)
        fputs(linetemp, fp);
    }
    else
      fputs(linetemp, fp);
  } /* while () */

  fclose(configfp);
  fclose(fp);

  rename(tmpfile, configname);

  os_notice(lptr, sockfd,
    "Glines for %s removed [%d matches]", 
    chkstr, 
    gcnt);
} /* o_ungline() */

#endif /* ALLOW_GLINES */

/*
o_help()
  Lets give this user some help :-)
*/

static void
o_help(struct Luser *lptr, int ac, char **av, int sockfd)

{
  if (ac >= 2)
  {
    struct OperCommand *cptr;

    for (cptr = opercmds; cptr->cmd; ++cptr)
      if (!irccmp(av[1], cptr->cmd))
        break;

    if (cptr->cmd && cptr->flag)
    {
      if (!CheckAccess(GetUser(1, onick, ouser, ohost), cptr->flag) || 
          !IsRegistered(lptr, sockfd))
      {
        os_notice(lptr, sockfd,
          ERR_NO_HELP,
          av[1],
          "");
        return;
      }
    }

    o_RecordCommand(sockfd,
      "HELP %s",
      av[1]);

    GiveHelp(n_OperServ, lptr ? lptr->nick : NULL, av[1], sockfd);
  }
  else
  {
    o_RecordCommand(sockfd,
      "HELP");

    GiveHelp(n_OperServ, lptr ? lptr->nick : NULL, NULL, sockfd);
  }
} /* o_help() */

/*
o_join()
  Adds channel av[1] to list of monitored channels
*/

static void
o_join(struct Luser *lptr, int ac, char **av, int sockfd)

{
  FILE *fp;
  struct Chanlist *tempchan;

  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002JOIN <#channel>\002");
    return;
  }

  o_RecordCommand(sockfd,
    "JOIN %s",
    av[1]);

  if ((tempchan = IsChannel(av[1])))
  {
    os_notice(lptr, sockfd, "%s is already being monitored",
      tempchan->name);
    return;
  }

  AddMyChan(av[1]);
  os_join(FindChannel(av[1]));

  os_notice(lptr, sockfd, "Now monitoring %s", 
    av[1]);

  if ((fp = fopen(ConfigFile, "a")) == NULL)
  {
    os_notice(lptr, sockfd, "Unable to open config file");
    return;
  }
  fprintf(fp, "C:%s\n", av[1]);
  fclose(fp);
} /* o_join() */

/*
o_part()
  Removes channel av[1] to list of monitored channels
*/

static void
o_part(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char line[MAXLINE], linetemp[MAXLINE];
  char *key, *ptemp;
  FILE *fp;
  FILE *configfp;
  char tmpfile[MAXLINE];
  struct Chanlist *chanptr, *tempchan, *prev;
  struct Channel *chptr;

  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002PART <#channel>\002");
    return;
  }

  chanptr = IsChannel(av[1]);
  chptr = FindChannel(av[1]);

  if (!chanptr || !chptr)
  {
    os_notice(lptr, sockfd, "%s is not being monitored", 
      av[1]);
    return;
  }

  o_RecordCommand(sockfd,
    "PART %s",
    chanptr->name);

  os_part(chptr);

  /*
   * Remove channel from ChanList
   */

  prev = NULL;

  for (tempchan = ChanList; tempchan; )
  {
    if (tempchan == chanptr)
    {
      MyFree(tempchan->name);

      if (prev)
      {
        prev->next = tempchan->next;
        MyFree(tempchan);
        tempchan = prev;
      }
      else
      {
        ChanList = tempchan->next;
        MyFree(tempchan);
        tempchan = NULL;
      }

      Network->MyChans--;

      /*
       * There should be only one match, so break
       */
      break;
    }

    prev = tempchan;

    if (tempchan)
      tempchan = tempchan->next;
    else
      tempchan = ChanList;
  }

  /* Remove channel from config file */
  if ((configfp = fopen(ConfigFile, "r")) == NULL)
  {
    os_notice(lptr, sockfd, "Unable to open config file");
    return;
  }

  ircsprintf(tmpfile, "%s.tmp", ConfigFile);
  fp = fopen(tmpfile, "w"); 

  while (fgets(line, MAXLINE - 1, configfp))
  {
    strcpy(linetemp, line);
    key = strtok(line, ":");
    if (!irccmp(key, "c"))
    {
      ptemp = strtok(NULL, "\r\n");
      if (ircncmp(ptemp, av[1], strlen(av[1])) != 0)
        fputs(linetemp, fp);
    }
    else
      fputs(linetemp, fp);
  } /* while () */

  fclose(fp);
  fclose(configfp);

  rename(tmpfile, ConfigFile);

  os_notice(lptr, sockfd, "No longer monitoring %s", 
    av[1]);
} /* o_part() */

/*
o_clones()
  Displays current clones on the network
*/

static void
o_clones(struct Luser *lptr, int ac, char **av, int sockfd)

{
  int ii,
      doneset,
      cnt,
      clcnt;
  struct Luser *tempuser;

  o_RecordCommand(sockfd,
    "CLONES");

  os_notice(lptr, sockfd, "-- Processing --");

  clcnt = 0;
  for (ii = 0; ii < HASHCLIENTS; ii++)
  {
    if (cloneTable[ii].list == NULL)
      continue;

    for (tempuser = cloneTable[ii].list; tempuser; tempuser = tempuser->cnext)
    {
      /* we know there must be at least 1 user in the list */
      if (tempuser->cnext)
      {
        if (!CloneMatch(tempuser, tempuser->cnext))
          continue;

        doneset = 0;

        cnt = 1;
        while (1)
        {
          if (!doneset)
          {
            /*
             * Only print out the heading (user@hostname) once per
             * batch of clones. The next time we get here,
             * doneset will be 1, so we won't print out the
             * heading again
             */
            os_notice(lptr, sockfd, "%s@%s",
              tempuser->username[0] == '~' ? tempuser->username + 1 : tempuser->username,
              tempuser->hostname);
            os_notice(lptr, sockfd, "  %d) %-10s [%s]", 
              cnt++, 
              tempuser->nick, 
              tempuser->server ? tempuser->server->name : "*unknown*");
            doneset = 1;

            ++clcnt;
          } /* if (!doneset) */

          os_notice(lptr, sockfd, "  %d) %-10s [%s]", 
            cnt++, 
            tempuser->cnext->nick, 
            tempuser->cnext->server ? tempuser->cnext->server->name : "*unknown*");
          
          ++clcnt;

          if (tempuser->cnext->cnext)
          {
            if (CloneMatch(tempuser->cnext, tempuser->cnext->cnext))
            {
              /*
               * We have yet another clone, continue the loop
               */
              tempuser = tempuser->cnext;
            }
            else
            {
              /*
               * All clones are grouped one after another. Since
               * we found two clients that do not match, there
               * can be no more possible clones of tempuser,
               * break out of loop
               */
              break;
            }
          } /* if (tempuser->cnext->cnext) */
          else
          {
            /*
             * next user is NULL, break out of while loop
             */
            break;
          }
        } /* while (1) */
      } /* if (tempuser->cnext) */
    } /* for (tempuser = cloneTable[ii].list; tempuser; tempuser = tempuser->cnext) */
  } /* for (ii = 0; ii < HASHCLIENTS; ii++) */

  os_notice(lptr, sockfd,
    "-- End of list (%d total clones found) --",
    clcnt);
} /* o_clones() */

#ifdef ALLOW_FUCKOVER

/*
o_fuckover()
  flood client av[1] with ircd codes until it pings out or sendq
is overflowed
*/

static void
o_fuckover(struct Luser *lptr, int ac, char **av, int sockfd)

{
  int ii;
  struct Luser *fptr;
  struct Process *ftmp;

  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002FUCKOVER <nickname>\002");
    return;
  }

  fptr = FindClient(av[1]);
  if (!fptr)
  {
    os_notice(lptr, sockfd, "No such nick [%s]",
      av[1]);
    return;
  }

  o_RecordCommand(sockfd,
    "FUCKOVER %s",
    fptr->nick);

  o_Wallops("FUCKOVER %s",
    fptr->nick);

  if (fptr->server == Me.sptr)
  {
    os_notice(lptr, sockfd,
      "[%s] is a pseudo nickname, canceling flood",
      fptr->nick);
    return;
  }

  if (fptr->flags & L_OSREGISTERED)
    ii = 1;
  else
    ii = 0;

  /* Check if the user has an exception flag */
  if (IsProtected(GetUser(ii, fptr->nick, fptr->username, fptr->hostname)))
  {
    /* The user is protected */
    os_notice(lptr, sockfd, "%s!%s@%s is protected", 
      fptr->nick,
      fptr->username,
      fptr->hostname);

    putlog(LOG2, "%s!%s@%s attempted to FUCKOVER protected user %s",
      onick,
      ouser,
      ohost,
      fptr->nick);

    return;
  }

  /* check if av[1] is already being flooded */
  for (ftmp = fprocs; ftmp; ftmp = ftmp->next)
  {
    if (!irccmp(av[1], ftmp->target))
    {
      os_notice(lptr, sockfd,
        "Server flood already in progress for %s[%s@%s]",
        fptr->nick,
        fptr->username,
        fptr->hostname);
      return;
    }
  }

  os_notice(lptr, sockfd,
  	"Initiating flood for %s[%s@%s]",
  	fptr->nick,
  	fptr->username,
  	fptr->hostname);

  /*
   * start the flood
   */
  InitFuckoverProcess(onick, fptr->nick);

  SendUmode(OPERUMODE_Y,
    "*** Server flood activated for %s[%s@%s]", 
    fptr->nick,
    fptr->username,
    fptr->hostname);
} /* o_fuckover() */

/*
InitFuckoverProcess()
  backend for o_fuckover - fork() a process to flood fptr->nick,
and add it to process table fprocs
*/

static void
InitFuckoverProcess(char *from, char *ftarget)

{
  int pid;

  if (!from || !ftarget)
    return; /* shouldn't happen */

  pid = fork();
  switch (pid)
  {
    case -1: return;

    /* 
     * Child: flood the target
     */
    case 0:
    {
      time_t fstart, timecheck;
      int ii, stop = 0;

      fstart = current_ts;
      while (!stop)
      {
        for (ii = 200; ii < 512; ++ii)
        {
          /*
           * Stop flood after 20 secs in case the client can handle
           * it - not likely, unless the target's server has an
           * enormous sendQ
           */
          timecheck = current_ts;
          if ((timecheck - fstart) > 20)
          {
            SendUmode(OPERUMODE_Y,
              "*** Time limit expired, canceling flood on [%s]",
              ftarget);

            stop = 1;
            break;
          }

          /* send null string to target */
          toserv(":%s %d %s :\n", 
            Me.name,
            ii,
            ftarget);
        }
      }

      exit(-1);
      break;
    } /* case 0: */

    /* 
     * Parent: add sub-process to fprocs list
     */
    default:
    {
      struct Process *newproc;

      newproc = (struct Process *) MyMalloc(sizeof(struct Process));
      memset(newproc, 0, sizeof(struct Process));

      newproc->who = MyStrdup(from);
      newproc->target = MyStrdup(ftarget);
      newproc->pid = pid;

      newproc->next = fprocs;
      fprocs = newproc;

      break;
    }
  } /* switch (pid) */
} /* InitFuckoverProcess() */

/*
CheckFuckoverTarget()
  Called when a user quits, or changes their nick (newnick != NULL) to
see if they are a target of o_fuckover() - if so, halt the process
(and if newnick != NULL, redirect the flood to the new nick).
*/

void
CheckFuckoverTarget(struct Luser *fptr, char *newnick)

{
  struct Process *ftmp, *prev;
  int killret;

  if (!fptr)
    return;

  prev = NULL;
  for (ftmp = fprocs; ftmp; )
  {
    if (!irccmp(fptr->nick, ftmp->target))
    {
      /* we have a match - kill the process id */
      killret = kill(ftmp->pid, SIGKILL);

      /* if its a nick change, start flooding the new nick */
      if (newnick)
      {
        SendUmode(OPERUMODE_Y,
          "*** Redirecting flood from [%s] to [%s]",
          ftmp->target,
          newnick);
        InitFuckoverProcess(ftmp->who, newnick);
      }
      else if (killret == 0)
      {
        SendUmode(OPERUMODE_Y,
          "*** Flood on [%s] successful",
          ftmp->target);
      }

      MyFree(ftmp->who);
      MyFree(ftmp->target);

      if (prev)
      {
        prev->next = ftmp->next;
        MyFree(ftmp);
        ftmp = prev;
      }
      else
      {
        fprocs = ftmp->next;
        MyFree(ftmp);
        ftmp = NULL;
      }

      break;
    }

    prev = ftmp;
    if (ftmp)
      ftmp = ftmp->next;
    else
      ftmp = fprocs;
  }
} /* CheckFuckoverTarget() */

#endif /* ALLOW_FUCKOVER */

/*
o_trace()
  Displays user information on all users matching a certain
pattern, as well as certain options, if specified

Options:
  -ops      Limit matches to irc operators
  -nonops   Limit matches to non irc operators
  -clones   Limit matches to clones
  -kill     Kill all matches
  -info     Display statistical information for matches
  -realname Limit matches to those who match the given realname
  -server   Limit matches to those who are on the given server
  -msg      Send all matches the given message
  -long     Display in long format (default to short)
  -nolimit  Ignore MaxTrace limit
*/

static void
o_trace(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char *target;
  char chkstr[MAXLINE];
  char *argbuf;
  int argmem,
      alen;
  struct Luser *tempuser, *prev;
  int  cnt,
      bad,
      exactmatch; /* 1 if target is a user's nick, not a hostmask */
  int ops, /* 1 if -ops, 2 if -nonops */
      showinfo, /* show statistics about matches */
      clones, /* show only clones */
      kill, /* kill matches */
      showlong, /* show matches in long format */
      nolimit; /* ignore MaxTrace limit */
  int showmatches;

  struct timeval TimeOut;
  int selret; /* select() return value */
  fd_set writefds;

  struct Server *servptr;
  char *realname;
  char msgbuf[MAXLINE];
    
  target = NULL;
  servptr = NULL;
  realname = NULL;
  *msgbuf = '\0';
  ops = 0;
  showinfo = 0;
  clones = 0;
  kill = 0;
  showlong = 0;
  nolimit = 0;

  showmatches = 1;

  for (cnt = 1; cnt < ac; ++cnt)
  {
    alen = strlen(av[cnt]);

    if (!ircncmp(av[cnt], "-ops", alen))
      ops = 1;
    else if (!ircncmp(av[cnt], "-nonops", alen))
      ops = 2;
    else if (!ircncmp(av[cnt], "-clones", alen))
      clones = 1;
    else if (!ircncmp(av[cnt], "-kill", alen))
      kill = 1;
    else if (!ircncmp(av[cnt], "-long", alen))
      showlong = 1;
    else if (!ircncmp(av[cnt], "-nolimit", alen))
      nolimit = 1;
    else if (!ircncmp(av[cnt], "-info", alen))
    {
    #ifndef STATSERVICES
      os_notice(lptr, sockfd, "Stat Services are not enabled");
      return;
    #endif

      showinfo = 1;
    }
    else if (!ircncmp(av[cnt], "-realname", alen))
    {
      if (++cnt >= ac)
      {
        os_notice(lptr, sockfd, "No realname specified");
        return;
      }
      realname = av[cnt];
    }
    else if (!ircncmp(av[cnt], "-server", alen))
    {
      if (++cnt >= ac)
      {
        os_notice(lptr, sockfd, "No server specified");
        return;
      }

      if (!(servptr = FindServer(av[cnt])))
      {
        os_notice(lptr, sockfd, "No such server: %s",
          av[cnt]);
        return;
      }
    }
    else if (!ircncmp(av[cnt], "-msg", alen))
    {
      if (++cnt >= ac)
      {
        os_notice(lptr, sockfd, "No message specified");
        return;
      }

      while (cnt < ac)
      {
        strncat(msgbuf, av[cnt++], sizeof(msgbuf) - strlen(msgbuf) - 20);
        strcat(msgbuf, " ");
      }
      msgbuf[strlen(msgbuf) - 1] = '\0';

      break;
    }
    else
    {
      /*
       * it wasn't an -option; assume it is the pattern they
       * want to trace
       */
      if (!target)
        target = av[cnt];
    }
  }

  if (!target)
  {
    os_notice(lptr, sockfd, "No pattern specified");
    os_notice(lptr, sockfd,
      "Syntax: \002TRACE <pattern> [options]\002");
    return;
  }

  if (LimitTraceKills && nolimit && kill)
  {
    os_notice(lptr, sockfd,
    	"You are not permitted to combine -nolimit and -kill");
    return;
  }

  argmem = 200;
  if (realname)
    argmem += strlen(realname) + 1;
  if (servptr)
    argmem += strlen(servptr->name) + 1;
  if (*msgbuf)
    argmem += strlen(msgbuf) + 1;

  argbuf = (char *) MyMalloc(argmem + 4);

  ircsprintf(argbuf, "[%s] ", target);

  if (showlong)
    strcat(argbuf, "-long ");

  if (nolimit)
    strcat(argbuf, "-nolimit ");

  if (ops == 1)
    strcat(argbuf, "-ops ");
  else if (ops == 2)
    strcat(argbuf, "-nonops ");

  if (clones)
    strcat(argbuf, "-clones ");

  if (showinfo)
    strcat(argbuf, "-info ");

  if (realname)
  {
    strcat(argbuf, "-realname ");
    strcat(argbuf, realname);
    strcat(argbuf, " ");
  }

  if (servptr)
  {
    strcat(argbuf, "-server ");
    strcat(argbuf, servptr->name);
    strcat(argbuf, " ");
  }

  if (kill)
    strcat(argbuf, "-kill ");

  if (*msgbuf)
  {
    strcat(argbuf, "-msg ");
    strcat(argbuf, msgbuf);
  }

  o_RecordCommand(sockfd,
    "TRACE %s",
    argbuf);

  MyFree(argbuf);

  if (sockfd != NODCC)
  {
    /*
     * If it is a telnet connection, use select() to determine
     * if the socket is writeable. Otherwise, if they do
     * a trace -nolimit, and error could possibly occur if
     * we fill up the sendq buffer.
     */
    TimeOut.tv_sec = 1;
    TimeOut.tv_usec = 0;
    FD_ZERO(&writefds);

    FD_SET(sockfd, &writefds);
  }

  exactmatch = 0;

  if ((tempuser = FindClient(target)))
    exactmatch = 1;

  cnt = 0;
  bad = 0;
  prev = (struct Luser *) NULL;

  if (!exactmatch)
    tempuser = ClientList;

  for ( ; tempuser; )
  {
    if ((ops == 1) && (!IsOperator(tempuser)))
      bad = 1; /* -ops */
    else if ((ops == 2) && (IsOperator(tempuser)))
      bad = 1; /* -nonops */
    else if ((servptr) && (tempuser->server) && (servptr != tempuser->server))
      bad = 1; /* -server */
    else if ((clones == 1) && !IsClone(tempuser))
      bad = 1; /* -clones */
    else if ((realname) && (tempuser->realname))
    {
      if (match(realname, tempuser->realname) == 0)
        bad = 1; /* -realname */
    }

    if (bad)
    {
      /* this user is not a good match */

      if (exactmatch)
      {
        /*
         * The given target was the nickname of a valid client,
         * not a userhost mask - since the client didn't pass
         * the [options] test, stop the loop
         */
        break;
      }

      bad = 0;
      tempuser = tempuser->next;
      continue;
    }

    ircsprintf(chkstr, "%s!%s@%s", tempuser->nick,
      tempuser->username, tempuser->hostname);

    if (exactmatch || (match(target, chkstr)))
    {
      cnt++;
      if (*msgbuf) /* -msg */
      {
        /*
         * Make sure we don't send a message to a service
         * nick - ircd would assume a ghost and issue a KILL
         * if that were to happen
         */
        if (tempuser->server == Me.sptr)
        {
          cnt--;

          if (exactmatch)
            break;

          tempuser = tempuser->next;
          continue;
        }

        if (!nolimit && (cnt > MaxTrace))
        {
          os_notice(lptr, sockfd,
            "-- More than %d matches found, halting messages --", 
            MaxTrace);
          showmatches = 0;
          break;
        }

        os_notice(lptr, sockfd,
          "** Noticing %s",
          tempuser->nick);

        /* send 'msgbuf' to matches through a NOTICE */
        toserv(":%s NOTICE %s :%s\n",
          n_OperServ,
          tempuser->nick,
          msgbuf);
      }
      else if (kill) /* -kill */
      {
        /* make sure we don't kill a service nick */
        if (tempuser->server == Me.sptr)
        {
          cnt--;

          if (exactmatch)
            break;

          tempuser = tempuser->next;
          continue;
        }

        if (!nolimit && (cnt > MaxKill))
        {
          os_notice(lptr, sockfd,
            "-- More than %d matches found, halting kills --",
            MaxKill);
          showmatches = 0;
          break;
        }

        os_notice(lptr, sockfd,
          "** Killing %s",
          tempuser->nick);

        toserv(":%s KILL %s :%s!%s (#%d (%s@%s))\n",
          n_OperServ,
          tempuser->nick,
          Me.name,
          n_OperServ,
          cnt,
          onick,
          n_OperServ);

        DeleteClient(tempuser);

        if (exactmatch)
          break;

        if (prev)
          tempuser = prev;
        else
        {
          tempuser = ClientList;
          continue;
        }
      }
      else
      {
        if (!nolimit && (cnt > MaxTrace))
        {
          os_notice(lptr, sockfd,
            "-- More than %d matches found, truncating list --", 
            MaxTrace);
          showmatches = 0;
          break;
        }

        if (sockfd != NODCC)
        {
          selret = select(FD_SETSIZE, NULL, &writefds, NULL, &TimeOut);
          if (selret < 0)
          {
            os_notice(lptr, sockfd,
              "-- Error occurred during write: %s --",
              strerror(errno));
            break;
          }
        }

        os_notice(lptr, sockfd, "-----");

        if (showinfo) /* -info */
        {
        #ifdef STATSERVICES
          show_trace_info(lptr, tempuser, sockfd);
        #endif
        }
        else
        {
          /* display normally */
          show_trace(lptr, tempuser, sockfd, showlong);
        }
      }
    } /* if (match(target, chkstr)) */

    if (exactmatch)
      break;

    prev = tempuser;
    tempuser = tempuser->next;
  } /* for ( ; tempuser; ) */

  if (showmatches)
  {
    os_notice(lptr, sockfd, "%d match%s found",
      cnt,
      (cnt == 1) ? "" : "es");
  }
} /* o_trace() */

/*
show_trace()
  Show o_trace info
*/

static void
show_trace(struct Luser *lptr, struct Luser *target, int sockfd, int showlong)

{
  struct UserChannel *tempchan;
  char tmp[MAXLINE];

  if (!target)
    return;

  if (!showlong)
  {
    /*
     * Display in short format: nick!user@host (realname) [server]
     */
    os_notice(lptr, sockfd,
      "%s!%s@%s (%s) [%s]",
      target->nick,
      target->username,
      target->hostname,
      target->realname,
      target->server ? target->server->name : "*unknown*");
    return;
  }

  os_notice(lptr, sockfd,
    "User:      %s (%s@%s)",
    target->nick,
    target->username,
    target->hostname);
  os_notice(lptr, sockfd,
    "Realname:  %s",
    target->realname);
  os_notice(lptr, sockfd,
    "Server:    %s",
    target->server ? target->server->name : "*unknown*");

  strcpy(tmp, "+");
  if (target->umodes & UMODE_I)
    strcat(tmp, "i");
  if (target->umodes & UMODE_S)
    strcat(tmp, "s");
  if (target->umodes & UMODE_W)
    strcat(tmp, "w");
  if (target->umodes & UMODE_O)
    strcat(tmp, "o");
  os_notice(lptr, sockfd,
    "Usermodes: %s",
    tmp);

  strcpy(tmp, ctime((time_t *) &target->since));
  tmp[strlen(tmp) - 1] = '\0';
  os_notice(lptr, sockfd,
    "Signon:    %s",
    tmp);

  if (target->firstchan)
  {
    if (sockfd == NODCC)
      toserv(":%s NOTICE %s :Channels:  ",
        (ServerNotices) ? Me.name : n_OperServ,
        lptr->nick);
    else
      writesocket(sockfd, "Channels:  ");

    for (tempchan = target->firstchan; tempchan; tempchan = tempchan->next)
    {
      if (tempchan->flags & CH_OPPED)
      {
        if (sockfd == NODCC)
          toserv("@");
        else
          writesocket(sockfd, "@");
      }
      if (tempchan->flags & CH_VOICED)
      {
        if (sockfd == NODCC)
          toserv("+");
        else
          writesocket(sockfd, "+");
      }
      if (sockfd == NODCC)
        toserv("%s ",
          tempchan->chptr->name);
      else
      {
        writesocket(sockfd, tempchan->chptr->name);
        writesocket(sockfd, " ");
      }
    } /* for (tempchan = target->firstchan; tempchan; tempchan = tempchan->next) */

    if (sockfd == NODCC)
      toserv("\n");
    else
      writesocket(sockfd, "\n");
  }
} /* show_trace() */

#ifdef STATSERVICES

static void
show_trace_info(struct Luser *lptr, struct Luser *target, int sockfd)

{
  os_notice(lptr, sockfd,
    "User:         %s (%s@%s)",
    target->nick,
    target->username,
    target->hostname);

  os_notice(lptr, sockfd,
    "Ops:          %ld",
    target->numops);
  os_notice(lptr, sockfd,
    "DeOps:        %ld",
    target->numdops);
  os_notice(lptr, sockfd,
    "Voices:       %ld",
    target->numvoices);
  os_notice(lptr, sockfd,
    "DeVoices:     %ld",
    target->numdvoices);
  os_notice(lptr, sockfd,
    "Kicks:        %ld",
    target->numkicks);
  os_notice(lptr, sockfd,
    "Nick Changes: %ld",
    target->numnicks);
  os_notice(lptr, sockfd,
    "Kills:        %ld",
    target->numkills);
} /* show_trace_info() */

#endif /* STATSERVICES */

/*
o_channel() 
 Displays channel information on channels matching <pattern>
and [options]

Options:
 -minimum   - limit matches to channels with at least the given
              number of users
 -maximum   - limit matches to channels with at most the given
              number of users
 -banmatch  - limit matches to channels who have the given
              hostmask on their ban list
 -exmatch   - limit matches to channels who have the given
              hostmask on their ban exception list
 -nolimit   - Ignore MaxChannel limit
*/

static void
o_channel(struct Luser *lptr, int ac, char **av, int sockfd)

{
  struct Channel *cptr;
  char argbuf[MAXLINE];
  int cnt, /* number of matches */
      bad,
      exactmatch;
  char *target;
  int min, /* -min */
      max; /* -max */
  char *banmatch, /* -banmatch */
       *exmatch; /* -exmatch */
  int nolimit; /* -nolimit */
  char temp[MAXLINE];
  int alen,
      showmatches;
    
  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002CHANNEL <pattern> [options]\002");
    return;
  }

  target = NULL;
  min = 0;
  max = 0;
  banmatch = NULL;
  exmatch = NULL;
  nolimit = 0;

  showmatches = 1;

  for (cnt = 1; cnt < ac; cnt++)
  {
    alen = strlen(av[cnt]);

    if (!ircncmp(av[cnt], "-nolimit", alen))
      nolimit = 1;
    if (!ircncmp(av[cnt], "-minimum", alen))
    {
      cnt++;
      if (cnt >= ac)
      {
        os_notice(lptr, sockfd, "No minimum value specified");
        return;
      }

      min = atoi(av[cnt]);
    }
    else if (!ircncmp(av[cnt], "-maximum", alen))
    {
      ++cnt;
      if (cnt >= ac)
      {
        os_notice(lptr, sockfd, "No maximum value specified");
        return;
      }

      max = atoi(av[cnt]);
    }
    else if (!ircncmp(av[cnt], "-banmatch", alen))
    {
      cnt++;
      if (cnt >= ac)
      {
        os_notice(lptr, sockfd, "No hostmask specified");
        return;
      }

      banmatch = av[cnt];
    }
    else if (!ircncmp(av[cnt], "-exmatch", alen))
    {
      cnt++;
      if (cnt >= ac)
      {
        os_notice(lptr, sockfd, "No hostmask specified");
        return;
      }

      exmatch = av[cnt];
    }
    else
    {
      /*
       * it wasn't an -option; assume it is the pattern they
       * want to use
       */
      if (!target)
        target = av[cnt];
    }
  }

  if (!target)
  {
    os_notice(lptr, sockfd, "No pattern specified");
    os_notice(lptr, sockfd,
      "Syntax: \002CHANNEL <pattern> [options]\002");
    return;
  }

  ircsprintf(argbuf, "[%s] ", target);

  if (nolimit)
    strcat(argbuf, "-nolimit ");

  if (min)
  {
    strcat(argbuf, "-minimum");
    ircsprintf(temp, "%s %d ", argbuf, min);
    strcpy(argbuf, temp);
  }

  if (max)
  {
    strcat(argbuf, "-maximum");
    ircsprintf(temp, "%s %d ", argbuf, max);
    strcpy(argbuf, temp);
  }

  if (banmatch)
  {
    strcat(argbuf, "-banmatch ");
    strcat(argbuf, banmatch);
    strcat(argbuf, " ");
  }

  if (exmatch)
  {
    strcat(argbuf, "-exmatch ");
    strcat(argbuf, exmatch);
    strcat(argbuf, " ");
  }

  o_RecordCommand(sockfd,
    "CHANNEL %s",
    argbuf);

  cnt = 0;
  bad = 0;
  exactmatch = 0;

  if ((cptr = FindChannel(target)))
    exactmatch = 1;

  if (!exactmatch)
    cptr = ChannelList;

  for (; cptr; cptr = cptr->next)
  {
    if (exactmatch || (match(target, cptr->name)))
    {
      if ((min) && (min > cptr->numusers)) /* -minimum */
        bad = 1;
      else if ((max) && (max < cptr->numusers)) /* -maximum */
        bad = 1;
      else if ((banmatch) && !MatchBan(cptr, banmatch)) /* -banmatch */
        bad = 1;
      else if ((exmatch) && !MatchException(cptr, exmatch)) /* -exmatch */
        bad = 1;

      if (bad)
      {
        /*
         * cptr did not pass the [options] screening
         */
        if (exactmatch)
          break;

        bad = 0;
        continue;
      }

      cnt++;

      if (!nolimit && (cnt > MaxChannel))
      {
        os_notice(lptr, sockfd,
          "--- More than %d matches found, truncating list ---", 
          MaxChannel);
        showmatches = 0;
        break;
      }

      show_channel(lptr, cptr, sockfd);
    } /* if (match(target, tempchan->name)) */

    if (exactmatch)
      break;
  } /* for (; cptr; cptr = cptr->next) */

  if (showmatches)
  {
    os_notice(lptr, sockfd, "%d match%s found",
      cnt,
      (cnt == 1) ? "" : "es");
  }
} /* o_channel() */

/*
show_channel()
  Show o_channel info
*/

static void
show_channel(struct Luser *lptr, struct Channel *cptr, int sockfd)

{
  struct ChannelBan *tempban;
#ifdef GECOSBANS
  struct ChannelGecosBan *tempgecosban;	
#endif /* GECOSBANS */
  struct Exception *tempe;
  struct ChannelUser *tempuser;
  char *btime; /* time ban was set */
  char modes[MAXLINE]; /* channel modes */
  int bcnt = 0, /* ban count */
      ecnt = 0, /* exception count */
      ocnt = 0, /* chan ops */
      vcnt = 0, /* chan voices */
      icnt = 0, /* chan ircops */
      ncnt = 0; /* nonops */

  os_notice(lptr, sockfd, "Channel Information for %s:", 
    cptr->name);
  strcpy(modes, "+");
  if (cptr->modes & MODE_S)
    strcat(modes, "s");
  if (cptr->modes & MODE_P)
    strcat(modes, "p");
  if (cptr->modes & MODE_N)
    strcat(modes, "n");
  if (cptr->modes & MODE_T)
    strcat(modes, "t");
  if (cptr->modes & MODE_M)
    strcat(modes, "m");
  if (cptr->modes & MODE_I)
    strcat(modes, "i");
  if (cptr->limit)
    strcat(modes, "l");
  if (cptr->key && *cptr->key)
    strcat(modes, "k");

  if (cptr->limit)
  {
    char  temp[MAXLINE];

    ircsprintf(temp, "%s %d", modes, cptr->limit);
    strcpy(modes, temp);
  }

  if (cptr->key && *cptr->key)
  {
    char  temp[MAXLINE];

    ircsprintf(temp, "%s %s", modes, cptr->key);
    strcpy(modes, temp);
  }

  os_notice(lptr, sockfd, 
    "Modes:     %s",
    modes);
  strcpy(modes, ctime(&cptr->since));
  modes[strlen(modes) - 1] = '\0';
  os_notice(lptr, sockfd,
    "Created:   %s",
    modes);

  ocnt = ncnt = vcnt = icnt = bcnt = 0;
  for (tempuser = cptr->firstuser; tempuser; tempuser = tempuser->next)
  {
    if (tempuser->flags & CH_OPPED)
      ocnt++;
    else
      ncnt++;
    if (tempuser->flags & CH_VOICED)
      vcnt++;
    if (tempuser->lptr->umodes & UMODE_O)
      icnt++;
  }

  os_notice(lptr, sockfd, 
    "Ops:       %d",
    ocnt);
  os_notice(lptr, sockfd,
    "Voices:    %d",
    vcnt);
  os_notice(lptr, sockfd,
    "NonOps:    %d",
    ncnt);
  os_notice(lptr, sockfd,
    "IrcOps:    %d",
    icnt);
  os_notice(lptr, sockfd,
    "Total:     %d",
    cptr->numusers);

  if (sockfd == NODCC)
    toserv(":%s NOTICE %s :Nicks:     ",
      (ServerNotices) ? Me.name : n_OperServ,
      lptr->nick);
  else
    writesocket(sockfd, "Nicks:     ");
  for (tempuser = cptr->firstuser; tempuser; tempuser = tempuser->next)
  {
    if (tempuser->flags & CH_OPPED)
    {
      if (sockfd == NODCC)
        toserv("@");
      else
        writesocket(sockfd, "@");
    }
    if (tempuser->flags & CH_VOICED)
    {
      if (sockfd == NODCC)
        toserv("+");
      else
        writesocket(sockfd, "+");
    }
    if (sockfd == NODCC)
      toserv("%s ",
        tempuser->lptr->nick);
    else
    {
      writesocket(sockfd, tempuser->lptr->nick);
      writesocket(sockfd, " ");
    }
  }
  if (sockfd == NODCC)
    toserv("\n");
  else
    writesocket(sockfd, "\n");

  if (cptr->firstban)
  {
    os_notice(lptr, sockfd, "Bans:");
    for (tempban = cptr->firstban; tempban; tempban = tempban->next)
    {
      bcnt++;
      btime = (char *) ctime(&tempban->when);
      btime[strlen(btime) - 1] = '\0';
      os_notice(lptr, sockfd, " [%2d] [%-10s] [%-10s] [%-15s]",
        bcnt,
        tempban->mask,
        tempban->who ? tempban->who : "unknown",
        btime);
    }
  }

#ifdef GECOSBANS
 if (cptr->firstgecosban)
  {
    os_notice(lptr, sockfd, "Gecos field Bans:");
    for (tempgecosban = cptr->firstgecosban;
        tempgecosban; tempgecosban = tempgecosban->next)
    {
      bcnt++;
      btime = (char *) ctime(&tempgecosban->when);
      btime[strlen(btime) - 1] = '\0';
      os_notice(lptr, sockfd, " [%2d] [%-10s] [%-10s] [%-15s]",
        bcnt, tempgecosban->mask,
        tempgecosban->who ? tempgecosban->who : "unknown",
        btime);
    }
  }
#endif /* GECOSBANS */

  if (cptr->exceptlist)
  {
    os_notice(lptr, sockfd, "Ban Exceptions:");
    for (tempe = cptr->exceptlist; tempe; tempe = tempe->next)
    {
      ecnt++;
      btime = (char *) ctime(&tempe->when);
      btime[strlen(btime) - 1] = '\0';
      os_notice(lptr, sockfd, " [%2d] [%-10s] [%-10s] [%-15s]",
        ecnt,
        tempe->mask,
        tempe->who ? tempe->who : "unknown",
        btime);
    }
  }
} /* show_channel() */

/*
o_on()
  Reactivate Services
*/

static void
o_on(struct Luser *lptr, int ac, char **av, int sockfd)

{
  if (!(Network->flags & NET_OFF))
  {
    os_notice(lptr, sockfd,
      "Services are not disabled");
    return;
  }

  Network->flags &= ~NET_OFF;
  os_notice(lptr, sockfd,
    "Services have been \002reactivated\002");

  o_RecordCommand(sockfd,
    "ON");

  o_Wallops("ON");
} /* o_on() */

/*
o_off()
  Deactivate services
*/

static void
o_off(struct Luser *lptr, int ac, char **av, int sockfd)

{
  if (Network->flags & NET_OFF)
  {
    os_notice(lptr, sockfd,
      "Services are already disabled");
    return;
  }

  Network->flags |= NET_OFF;
  os_notice(lptr, sockfd,
    "Services have been \002disabled\002");

  o_RecordCommand(sockfd,
    "OFF");

  o_Wallops("OFF");
} /* o_off() */

/*
o_jump()
  Reroute to another hub server (av[1])
*/

static void
o_jump(struct Luser *lptr, int ac, char **av, int sockfd)

{
  int port, tempsock;
  struct Servlist *temp;

  if (ac < 2)
  {
    os_notice(lptr, sockfd,
      "Syntax: \002JUMP {server} [port]\002");
    return;
  }

  for (temp = ServList; temp; temp = temp->next)
    if (match(av[1], temp->hostname))
      break;

  if (!temp)
  {
    os_notice(lptr, sockfd,
      "[\002%s\002] is an invalid hub server",
      av[1]);
    return;
  }

  if (ac >= 3)
  {
    if (!(port = IsNum(av[2])))
    {
      os_notice(lptr, sockfd, "Invalid port [\002%s\002]",
        av[2]);
      return;
    }
  }
  else
    port = temp->port;

  o_RecordCommand(sockfd,
    "JUMP %s %d",
    temp->hostname,
    port);

  o_Wallops("JUMP %s %d",
    temp->hostname,
    port);

  /* Attempt connection to server */
  if ((tempsock = ConnectHost(temp->hostname, port)) >= 0) 
  {

  #ifdef HAVE_PTHREADS
    /*
     * Set this to 0, so p_CheckTimer won't call DoTimer()
     * which might cycle the server list
     */
    /*GoodTimer = 0;*/
  #endif

#if 0
    toserv("SQUIT %s :ReRouting\n",
      currenthub->realname ? currenthub->realname : "*");
#endif
    /* Do ERROR string instead of lame SQUIT -kre */
    toserv(":%s ERROR :Rerouting\n", Me.name);
    toserv(":%s QUIT\n", Me.name);

    /* kill old connection and clear out user/chan lists etc */
    close(HubSock);
    ClearUsers();
    ClearChans();
    ClearServs();

    /*
     * This has to come after ClearUsers(), or RECORD_SPLIT_TS
     * will not work correctly
     */
    ClearHashes(0);

    HubSock = tempsock;

    currenthub->connect_ts = 0;
    temp->connect_ts = current_ts;
    currenthub = temp;

    /* send PASS/SERVER combo */
    signon();

  #ifdef HAVE_PTHREADS
    /*GoodTimer = 1;*/
  #endif

  }
  else
  {
    close(tempsock);
    os_notice(lptr, sockfd,
      "Unable to connect to port \002%d\002 of \002%s\002: %s",
      port,
      temp->hostname,
      strerror(errno));
    return;
  }
} /* o_jump() */

/*
o_save()
  Save nickname/channel databases or settings.conf
depending on arguements
Valid Options:
  -dbs    : Save all databases
  -sets   : Save all settings
  -all    : Save everything
*/

static void
o_save(struct Luser *lptr, int ac, char **av, int sockfd)

{
  int goodsave,
      ii,
      alen;
  int savesets, /* -sets */
      savedbs; /* -dbs */
  char argbuf[MAXLINE];

  savesets = 0;
  savedbs = 0;

  if (ac >= 2)
  {
    for (ii = 1; ii < ac; ii++)
    {
      alen = strlen(av[ii]);
      if (!ircncmp(av[ii], "-dbs", alen))
        savedbs = 1;
      else if (!ircncmp(av[ii], "-sets", alen))
        savesets = 1;
      else if (!ircncmp(av[ii], "-all", alen))
        savedbs = savesets = 1;
    }
  }
  else
    savedbs = 1; /* default to database save */

  argbuf[0] = '\0';
  if (savedbs && savesets)
    strcat(argbuf, "-all ");
  else if (savedbs)
    strcat(argbuf, "-databases ");
  else if (savesets)
    strcat(argbuf, "-settings ");

  o_RecordCommand(sockfd,
    "SAVE %s",
    argbuf);

  if (savedbs)
  {
    goodsave = WriteDatabases();

    if (goodsave)
    {
      os_notice(lptr, sockfd,
        "Databases have been saved");
    }
    else
    {
      os_notice(lptr, sockfd,
        "Database save failed");

      /* 
       * we don't need to log the failure, since WriteDatabases()
       * would have done so
       */
    }
  } /* if (savedbs) */

  if (savesets)
  {
    goodsave = SaveSettings();

    if (goodsave)
    {
      os_notice(lptr, sockfd,
        "Settings have been saved");
    }
    else
    {
      os_notice(lptr, sockfd,
        "Settings save failed");
    }
  } /* if (savesets) */
} /* o_save() */

/*
o_reload()
  Reload nickname/channel/memo/stat databases
*/

static void
o_reload(struct Luser *lptr, int ac, char **av, int sockfd)

{
  int goodreload;

  goodreload = ReloadData();

  if (goodreload)
    os_notice(lptr, sockfd, "Reload successful");
  else
    os_notice(lptr, sockfd,
      "Reload contained errors, continuing to use old databases");

  o_RecordCommand(sockfd,
    "RELOAD %s",
    goodreload ? "" : "(failed)");
} /* o_reload() */

/*
o_set()
 Configure setting av[1] to value av[2]. If av[1] is "list",
list all settings.
*/

static void
o_set(struct Luser *lptr, int ac, char **av, int sockfd)

{
  struct Directive *dptr;
  char sendstr[MAXLINE],
       tmp[MAXLINE];
  int ii,
      pcnt;

  if (ac < 2)
  {
    os_notice(lptr, sockfd,
      "Syntax: \002SET <item | list> [value]\002");
    return;
  }

  if (!ircncmp(av[1], "list", strlen(av[1])))
  {
    o_RecordCommand(sockfd,
      "SET LIST");

    DisplaySettings(lptr, sockfd);

    return;
  }

  if (!(dptr = FindDirective(av[1])))
  {
    os_notice(lptr, sockfd,
      "Unknown setting: %s",
      av[1]);
    return;
  }

  if (ac < 3)
  {
    /*
     * They didn't give a new value, display the current one
     */

    sendstr[0] = '\0';
    for (ii = 0; ii < PARAM_MAX; ++ii)
    {
      if (!dptr->param[ii].type)
        break;

      switch (dptr->param[ii].type)
      {
        case PARAM_STRING:
        {
          ircsprintf(tmp, "\"%s\" ",
            *(char **) dptr->param[ii].ptr);
          strcat(sendstr, tmp);
          break;
        }

        case PARAM_TIME:
        {
          ircsprintf(tmp, "%s ",
            timeago(*(long *) dptr->param[ii].ptr, 2));
          strcat(sendstr, tmp);
          break;
        }

        case PARAM_INT:
        case PARAM_SET:
        case PARAM_PORT:
        {
          ircsprintf(tmp, "%d ", *(int *) dptr->param[ii].ptr);
          strcat(sendstr, tmp);
          break;
        }
      } /* switch (dptr->param[ii].type) */
    }
  
    o_RecordCommand(sockfd,
      "SET [%s]",
      dptr->name);

    os_notice(lptr, sockfd,
      "[\002%s\002] is set to: %s",
      dptr->name,
      sendstr);

    return;
  }

  if (dptr->flag == D_NORUNTIME)
  {
    os_notice(lptr, sockfd,
      "The directive [\002%s\002] is not run-time configurable",
      dptr->name);
    return;
  }

  /*
   * Starting position of our first arguement
   */
  pcnt = 2;

  sendstr[0] = '\0';
  for (ii = 0; ii < PARAM_MAX; ++ii, ++pcnt)
  {
    if (!dptr->param[ii].type)
      break;

    /*
     * Now assign our variable (dptr->param[ii].ptr) to the
     * corresponding av[] slot
     */
    if ((dptr->param[ii].type != PARAM_SET) && (pcnt >= ac))
    {
      os_notice(lptr, sockfd,
        "Not enough arguements to [%s] directive",
        dptr->name);
      return;
    }

    switch (dptr->param[ii].type)
    {
      case PARAM_STRING:
      {
        char *strptr;

        if ((*(char **) dptr->param[ii].ptr) != NULL)
        {
          /*
           * make sure we free the old string before allocating
           * a new one.
           */
          MyFree(*(char **) dptr->param[ii].ptr);
        }

        if (*av[pcnt] == ':')
          strptr = av[pcnt] + 1;
        else
          strptr = av[pcnt];

        *(char **) dptr->param[ii].ptr = MyStrdup(strptr);

        ircsprintf(tmp, "\"%s\" ", strptr);
        strcat(sendstr, tmp);

        break;
      }

      case PARAM_INT:
      {
        int value;

        value = IsNum(av[pcnt]);
        if (!value)
        {
          os_notice(lptr, sockfd,
            "Invalid integer: %s",
            av[pcnt]);
          return;
        }

        /*
         * We have to call atoi() anyway since IsNum() would
         * have returned 1 if av[pcnt] was "0", but IsNum()
         * is a good way of making sure we have a valid integer
         */
        value = atoi(av[pcnt]);
        *(int *) dptr->param[ii].ptr = value;

        ircsprintf(tmp, "%d ", value);
        strcat(sendstr, tmp);

        break;
      }

      case PARAM_TIME:
      {
        long value;

        value = timestr(av[pcnt]);
        if (!value)
        {
          os_notice(lptr, sockfd,
            "Invalid time format: %s",
            av[pcnt]);
          return;
        }

        *(long *) dptr->param[ii].ptr = value;

        ircsprintf(tmp, "%s ", timeago(value, 2));
        strcat(sendstr, tmp);

        break;
      }

      case PARAM_SET:
      {
        int value;

        value = IsNum(av[pcnt]);
        if (!value)
        {
          os_notice(lptr, sockfd,
            "Invalid integer (must be 1 or 0): %s",
            av[pcnt]);
          return;
        }

        value = atoi(av[pcnt]);

        if (value)
        {
          *(int *) dptr->param[ii].ptr = 1;
          strcat(sendstr, "1 ");
        }
        else
        {
          *(int *) dptr->param[ii].ptr = 0;
          strcat(sendstr, "0 ");
        }

        break;
      }

      case PARAM_PORT:
      {
        int value;

        value = IsNum(av[pcnt]);
        if (!value)
        {
          os_notice(lptr, sockfd,
            "Invalid port number (must be between 1 and 65535): %s",
            av[pcnt]);
          return;
        }

        value = atoi(av[pcnt]);

        if ((value < 1) || (value > 65535))
        {
          os_notice(lptr, sockfd,
            "Invalid port number (must be between 1 and 65535): %s",
            av[pcnt]);
          return;
        }

        *(int *) dptr->param[ii].ptr = value;

        ircsprintf(tmp, "%d ", value);
        strcat(sendstr, tmp);

        break;
      }

      default:
      {
        /* we shouldn't get here */
        break;
      }
    } /* switch (dptr->param[ii].type) */
  } /* for (ii = 0; ii < PARAM_MAX; ii++, pcnt++) */

  o_RecordCommand(sockfd,
    "SET [%s] %s",
    dptr->name,
    sendstr);

  os_notice(lptr, sockfd,
    "The value of [\002%s\002] has been set to: %s",
    dptr->name,
    sendstr);
} /* o_set() */

/*
DisplaySettings()
 Output a list of all settings and their values
*/

static void
DisplaySettings(struct Luser *lptr, int sockfd)

{
  struct Directive *dptr;
  int ii,
      cnt;
  char sendstr[MAXLINE],
       tmp[MAXLINE];

  os_notice(lptr, sockfd,
    "-- Listing settings --");

  cnt = 0;
  for (dptr = directives; dptr->name; ++dptr, ++cnt)
  {
    sendstr[0] = '\0';
    for (ii = 0; ii < PARAM_MAX; ii++)
    {
      if (!dptr->param[ii].type)
        break;

      switch (dptr->param[ii].type)
      {
        case PARAM_STRING:
        {
          /* Try to write out string only if non-null, ie is set -kre */
          if (*(char **)dptr->param[ii].ptr)
          {
            ircsprintf(tmp, "\"%s\" ",
              *(char **) dptr->param[ii].ptr);
            strcat(sendstr, tmp);
          }
          break;
        }

        case PARAM_TIME:
        {
          ircsprintf(tmp, "%s ",
            timeago(*(long *) dptr->param[ii].ptr, 2));
          strcat(sendstr, tmp);
          break;
        }

        case PARAM_INT:
        case PARAM_SET:
        case PARAM_PORT:
        {
          ircsprintf(tmp, "%d ",
            *(int *) dptr->param[ii].ptr);
          strcat(sendstr, tmp);
          break;
        }
      } /* switch (dptr->param[ii].type) */
    }

    os_notice(lptr, sockfd,
      "%-20s %-30s",
      dptr->name,
      sendstr);
  }

  os_notice(lptr, sockfd,
    "-- End of list (%d settings) --",
    cnt);
} /* DisplaySettings() */

/*
o_ignore()
  Modify the services ignorance list
*/

static void
o_ignore(struct Luser *lptr, int ac, char **av, int sockfd)

{
  struct OperCommand *cptr;

  if (ac < 2)
  {
    os_notice(lptr, sockfd,
      "Syntax: \002IGNORE {ADD|DEL|LIST} [nick|mask|index]\002");
    return;
  }

  cptr = GetoCommand(ignorecmds, av[1]);

  if (cptr && (cptr != (struct OperCommand *) -1))
  {
    /* call cptr->func to execute command */
    (*cptr->func)(lptr, ac, av, sockfd);
  }
  else
  {
    /* the option they gave was not valid */
    os_notice(lptr, sockfd,
      "%s switch [\002%s\002]",
      (cptr == (struct OperCommand *) -1) ? "Ambiguous" : "Unknown",
      av[1]);
    os_notice(lptr, sockfd,
      "Syntax: \002IGNORE {ADD|DEL|LIST} [mask|nickname|index]\002");
  }
} /* o_ignore() */

static void
o_ignore_add(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char hostmask[MAXLINE];
  time_t expire = (time_t) NULL;

  if (ac < 3)
  {
    os_notice(lptr, sockfd,
      "Syntax: \002IGNORE ADD <nickname|hostmask> [time]\002");
    return;
  }

  if (match("*!*@*", av[2]))
    strcpy(hostmask, av[2]);
  else if (match("*!*", av[2]))
  {
    strcpy(hostmask, av[2]);
    strcat(hostmask, "@*");
  }
  else if (match("*@*", av[2]))
  {
    strcpy(hostmask, "*!");
    strcat(hostmask, av[2]);
  }
  else if (match("*.*", av[2]))
  {
    strcpy(hostmask, "*!*@");
    strcat(hostmask, av[2]);
  }
  else
  {
    struct Luser *ptr;
    char *mask;

    /* it must be a nickname - try to get hostmask */
    if ((ptr = FindClient(av[2])))
    {
      mask = HostToMask(ptr->username, ptr->hostname);
      strcpy(hostmask, "*!");
      strcat(hostmask, mask);
      MyFree(mask);
    }
    else
    {
      strcpy(hostmask, av[2]);
      strcat(hostmask, "!*@*");
    }
  }

  if (OnIgnoreList(hostmask))
  {
    os_notice(lptr, sockfd,
      "The hostmask [\002%s\002] is already on the ignorance list",
      hostmask);
    return;
  }

  if (ac >= 4)
    expire = timestr(av[3]);

  o_RecordCommand(sockfd,
    "IGNORE ADD %s %s",
    hostmask,
    (ac < 4) ? "(perm)" : av[3]);

  AddIgnore(hostmask, expire);

  if (expire)
  {
    os_notice(lptr, sockfd,
      "[\002%s\002] has been added to the ignorance list with an expire of %s",
      hostmask,
      timeago(expire, 3));
  }
  else
  {
    os_notice(lptr, sockfd,
      "[\002%s\002] has been added to the ignorance list with no expiration",
      hostmask);
  }
} /* o_ignore_add() */

static void
o_ignore_del(struct Luser *lptr, int ac, char **av, int sockfd)

{
  struct Ignore *temp;
  char *host = NULL;
  int idx;

  if (ac < 3)
  {
    os_notice(lptr, sockfd,
      "Syntax: \002IGNORE DEL <hostmask | index>\002");
    return;
  }

  if ((idx = IsNum(av[2])))
  {
    int cnt = 0;

    for (temp = IgnoreList; temp; temp = temp->next, ++cnt)
    {
      if (idx == (cnt + 1))
      {
        host = MyStrdup(temp->hostmask);
        break;
      }
    }

    if (!host)
    {
      os_notice(lptr, sockfd,
        "[\002%d\002] is not a valid index",
        idx);
      return;
    }
  }
  else
    host = MyStrdup(av[2]);

  o_RecordCommand(sockfd,
    "IGNORE DEL %s",
    host);

  if (!DelIgnore(host))
  {
    os_notice(lptr, sockfd,
      "[\002%s\002] was not found on the ignorance list",
      host);
    MyFree(host);
    return;
  }

  os_notice(lptr, sockfd,
    "[\002%s\002] has been removed from the ignorance list",
    host);

  MyFree(host);
} /* o_ignore_del() */

static void
o_ignore_list(struct Luser *lptr, int ac, char **av, int sockfd)

{
  struct Ignore *tmp;
  int idx;
  char *mask = NULL;
  time_t currtime = current_ts;

  if (ac >= 3)
    mask = av[2];

  o_RecordCommand(sockfd,
    "IGNORE LIST %s",
    mask ? mask : "");

  if (IgnoreList)
  {
    os_notice(lptr, sockfd,
      "-- Ignorance List --");
    os_notice(lptr, sockfd,
      "Num Expire (min) Hostmask");
    os_notice(lptr, sockfd,
      "--- ------------ --------");
    idx = 1;
    for (tmp = IgnoreList; tmp; tmp = tmp->next, idx++)
    {
      if (mask)
        if (match(mask, tmp->hostmask) == 0)
          continue;
      os_notice(lptr, sockfd,
        "%-3d %-12.1f %-35s",
        idx,
        tmp->expire ? ((float)(tmp->expire - currtime) / 60) : (float)0,
        tmp->hostmask);
    }
    os_notice(lptr, sockfd,
      "-- End of list --");
  }
  else
    os_notice(lptr, sockfd,
      "The ignorance list is empty");
} /* o_ignore_list() */

/*
AddIgnore()
  Add 'hostmask' to ignore list with 'expire'
*/

void
AddIgnore(char *hostmask, time_t expire)

{
  struct Ignore *ptr;

//  ptr = (struct Ignore *) MyMalloc(sizeof(struct Ignore));
//  ptr->hostmask = MyStrdup(hostmask);

  ptr = (struct Ignore *) malloc(sizeof(struct Ignore));
  ptr->hostmask = MyStrdup(hostmask);

  if (!expire)
    ptr->expire = (time_t) 0; /* no expiration */
  else
    ptr->expire = (current_ts + expire);

	ptr->prev = NULL;
  ptr->next = IgnoreList;
  if (ptr->next)
  	ptr->next->prev = ptr;

  IgnoreList = ptr;
  putlog(LOG1, "Added to IGNORE LIST: ptr %d, prev %d, next %d, hostmask %s", ptr, ptr->prev, ptr->next, ptr->hostmask);
} /* AddIgnore() */

/*
OnIgnoreList()
  Return ptr to ignore structure containing 'mask'
*/

struct Ignore *
OnIgnoreList(char *mask)

{
  struct Ignore *tmp;

  if (!mask)
    return (NULL);

  for (tmp = IgnoreList; tmp; tmp = tmp->next)
  {
    if (match(tmp->hostmask, mask))
      return (tmp);
  }

  return (NULL);
} /* OnIgnoreList() */

/*
DelIgnore()
  Delete 'mask' from ignore list; return number of matches
*/

int
DelIgnore(char *mask)

{
  struct Ignore *temp, *next;
  int cnt;

  if (!mask)
    return 0;

  if (!IgnoreList)
    return 0;

  cnt = 0;

  for (temp = IgnoreList; temp; temp = next)
  {
  	next = temp->next;

    if (match(mask, temp->hostmask))
    {
    	++cnt;
    	DeleteIgnore(temp);
    }
  }

  return (cnt);
} /* DelIgnore() */

/*
DeleteIgnore()
 Free Ignore structure 'iptr'
*/

static void
DeleteIgnore(struct Ignore *iptr)

{
	if (iptr->next)
		iptr->next->prev = iptr->prev;
	if (iptr->prev)
		iptr->prev->next = iptr->next;
	else
		IgnoreList = iptr->next;

  putlog(LOG1, "Deleted from IGNORE LIST: ptr %ld, prev %ld, next %ld, hostmask %s", iptr, iptr->prev, iptr->next, iptr->hostmask);

	MyFree(iptr->hostmask);
	MyFree(iptr);
} /* DeleteIgnore() */

/*
ExpireIgnores()
 Check if any ignores have expired - if so, remove them
from the list
*/

void
ExpireIgnores(time_t unixtime)

{
  struct Ignore *temp, *next;

  for (temp = IgnoreList; temp; temp = next)
  {
  	next = temp->next;

    if (temp->expire && (unixtime >= temp->expire))
    {
      SendUmode(OPERUMODE_Y,
        "*** Expired ignore: %s",
        temp->hostmask);

			DeleteIgnore(temp);
		}
  }
} /* ExpireIgnores() */

/*
o_who()
  Displays list of users/bots on partyline
*/

static void
o_who(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char idle[MAXLINE];
  char prefix;
  int AreBots = 0, mins;
  struct Userlist *tempuser;
  struct DccUser *tempconn;
  char uhost[UHOSTLEN + 2];

  o_RecordCommand(sockfd,
    "WHO");

  os_notice(lptr, sockfd,
    "Current users: (%% = Services Admin, * = Admin, + = Oper)");

  for (tempconn = connections; tempconn; tempconn = tempconn->next)
  {
    if (IsDccPending(tempconn) && !(tempconn->flags & SOCK_DCC))
      continue;
    
    if (tempconn->flags & SOCK_NEEDID)
      continue;

    if (tempconn->flags & SOCK_TCMBOT)
    {
      AreBots = 1;
      continue;
    }

    tempuser = DccGetUser(tempconn);
    assert(tempuser != NULL);

    if (IsServicesAdmin(tempuser))
      prefix = '%';
    else if (IsAdmin(tempuser))
      prefix = '*';
    else if (IsOper(tempuser))
      prefix = '+';
    else
      prefix = ' ';

    mins = ((current_ts - tempconn->idle) / 60) % 60;
    if (mins >= 5)
    {
      strcpy(idle, "idle: ");
      strcat(idle, timeago(tempconn->idle, 0));
    }
    else
      idle[0] = '\0';

    ircsprintf(uhost, "%s@%s", tempconn->username, tempconn->hostname);

    os_notice(lptr, sockfd, "  %c%-10s %-25s %s",
      prefix,
      tempconn->nick,
      uhost,
      idle);
  }

  if (AreBots)
  {
    os_notice(lptr, sockfd, "\r\nBot connections:");
    for (tempconn = connections; tempconn; tempconn = tempconn->next)
    {
      if (!(tempconn->flags & SOCK_TCMBOT))
        continue;

      if (tempconn->flags & SOCK_BOTHUB)
      {
        os_notice(lptr, sockfd, " -> %s [%s@%s]",
          tempconn->nick,
          tempconn->username,
          tempconn->hostname);
      }
      else
      {
        os_notice(lptr, sockfd, " <- %s [%s@%s]",
          tempconn->nick,
          tempconn->username,
          tempconn->hostname);
      }
    }
  } /* if (AreBots) */
} /* o_who() */

/*
o_boot()
  Remove user av[1] from the partyline with message av[2]-
*/

static void
o_boot(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char *reason;
  struct DccUser *dccptr;

  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002BOOT <nick> [reason]\002");
    return;
  }

  if (!(dccptr = IsOnDcc(av[1])))
  {
    os_notice(lptr, sockfd, "%s is not on the partyline\n", 
      av[1]);
    return;
  }

  if (ac < 3)
    reason = MyStrdup("");
  else
    reason = GetString(ac - 2, av + 2);

  o_RecordCommand(sockfd,
    "BOOT %s %s",
    av[1],
    reason);

  BroadcastDcc(DCCALL, "%s has been booted by %s [%s]\n",
    av[1],
    onick,
    reason);

  os_notice(lptr, dccptr->socket,
    "You have been booted by %s [%s]",
    onick,
    reason);

  CloseConnection(dccptr);

  MyFree(reason);
} /* o_boot() */

/*
o_quit()
  Removes lptr->nick from the partyline with message av[1]-
*/

static void
o_quit(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char *reason;
  struct DccUser *dccptr;

  if (!(dccptr = IsDccSock(sockfd)))
    return; /* shouldn't happen */

  if (ac < 2)
    reason = MyStrdup("");
  else
    reason = GetString(ac - 1, av + 1);


  os_notice(lptr, sockfd, "Closing connection: %s", 
    reason);

  BroadcastDcc(DCCALL, "%s (%s@%s) has left the partyline [%s]\n",
    onick,
    ouser,
    ohost,
    reason);

  CloseConnection(dccptr);

  MyFree(reason);
} /* o_quit() */

/*
o_motd()
  Display motd
*/

static void
o_motd(struct Luser *lptr, int ac, char **av, int sockfd)

{
  o_RecordCommand(sockfd,
    "MOTD");

  SendMotd(sockfd);
} /* o_motd() */

/*
o_link()
  Attempt to link to TCM bot av[1]
*/

static void
o_link(struct Luser *lptr, int ac, char **av, int sockfd)

{
  struct Botlist *bptr;

  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002LINK <botnick>\002");
    return;
  }

  if (!(bptr = IsBot(av[1])))
  {
    os_notice(lptr, sockfd, "No such bot: %s", 
      av[1]);
    return;
  }

  o_RecordCommand(sockfd,
    "LINK %s",
    bptr->name);

  os_notice(lptr, sockfd, "Attempting to link to %s", 
    bptr->name);
  os_notice(lptr, sockfd, "Making connection to %s:%d",
    bptr->hostname,
    bptr->port);

  ConnectToTCM(onick, bptr);
} /* o_link() */

/*
o_unlink()
  Unlink TCM bot av[1] with reason av[2]-
*/

static void
o_unlink(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char sendstr[MAXLINE];
  char *reason;
  struct DccUser *botptr, *tmp;

  if (ac < 2)
  {
    os_notice(lptr, sockfd,
      "Syntax: \002UNLINK <bot name> [reason]\002");
    return;
  }

  if (!(botptr = GetBot(av[1])))
  {
    os_notice(lptr, sockfd, "No such bot: %s",
      av[1]);
    return;
  }

  if (ac < 3)
    reason = MyStrdup("");
  else
    reason = GetString(ac - 2, av + 2);

  o_RecordCommand(sockfd,
    "UNLINK %s %s",
    botptr->nick,
    reason);

  if (reason[0] == '\0')
  {
    ircsprintf(sendstr, "(%s)", onick);
    MyFree(reason);
    reason = MyStrdup(sendstr);
  }
  else
  {
    ircsprintf(sendstr, "(%s: %s)", onick, reason);
    MyFree(reason);
    reason = MyStrdup(sendstr);
  }

  for (tmp = connections; tmp; tmp = tmp->next)
  {
    if (tmp == botptr)
      continue;

    if (tmp->flags & SOCK_TCMBOT)
    {
      ircsprintf(sendstr, "(%s) Unlinked from %s %s\n",
        n_OperServ, botptr->nick, reason);
      writesocket(tmp->socket, sendstr);
    }
    else
    {
      ircsprintf(sendstr, "*** Unlinked from %s %s\n",
        botptr->nick, reason);
      writesocket(tmp->socket, sendstr);
    }
  }

  CloseConnection(botptr);

  MyFree(reason);
} /* o_unlink() */

/*
o_stats()
 Displays various stats depending on switch

Switches:
   O : O:lines
   B : bot lines
   S : server list lines
   U : uptime
   M : memory info
   I : restricted host lines
   G : glines
   J : jupes
   P : port lines
   C : monitored channels
   ? : recv/send info
*/

static void
o_stats(struct Luser *lptr, int ac, char **av, int sockfd)

{
  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002STATS <switch>\002");
    return;
  }

  o_RecordCommand(sockfd,
    "STATS %c",
    ToUpper(*av[1]));

  switch (av[1][0])
  {
    case 'b':
    case 'B':
    {
      struct Botlist *tempbot;
      char uh[UHOSTLEN + 2];
    
      if (BotList || RemoteBots)
      {
        os_notice(lptr, sockfd, "-- TCM Bot Lines --");
        os_notice(lptr, sockfd, "    [Nickname       ] [Hostname                 ] [Port ]");

        for (tempbot = BotList; tempbot; tempbot = tempbot->next)
        {
          os_notice(lptr, sockfd, "[B] [%-15s] [%-25s] [%-5d]",
            tempbot->name,
            tempbot->hostname,
            tempbot->port);
        }

        for (tempbot = RemoteBots; tempbot; tempbot = tempbot->next)
        {
          ircsprintf(uh, "%s@%s", tempbot->username, tempbot->hostname);

          os_notice(lptr, sockfd, "[L] [%-15s] [%-25s]",
            tempbot->name,
            uh);
        }

        os_notice(lptr, sockfd, "-- End of list --");
      }
      else
        os_notice(lptr, sockfd, "No bot lines");
      break;
    } /* case 'B' */

    case 's':
    case 'S':
    {
      struct Servlist *tempserv;

      os_notice(lptr, sockfd, "-- Server List --");
      os_notice(lptr, sockfd,
        "[Hostname                      ] [Port ]");
      for (tempserv = ServList; tempserv; tempserv = tempserv->next)
      {
        os_notice(lptr, sockfd, "[%-30s] [%-5d]",
          tempserv->hostname,
          tempserv->port);
      }
      os_notice(lptr, sockfd, "-- End of list --");

      break;
    } /* case 'S' */

    case 'o':
    case 'O':
    {
      char flags[MAXLINE];
      char uhost[UHOSTLEN + 2];
      struct Userlist *tempuser;
      int cnt = 0;

      os_notice(lptr, sockfd, "-- Authorized Users --");
      os_notice(lptr, sockfd,
        "[Nickname      ] [Hostmask                 ] [Flags     ]");
      for (tempuser = UserList; tempuser; tempuser = tempuser->next)
      {
        cnt++;

        flags[0] = '\0';
        if (tempuser->flags & PRIV_ADMIN)
          strcat(flags, "a");
        if (tempuser->flags & PRIV_CHAT)
          strcat(flags, "d");
        if (tempuser->flags & PRIV_EXCEPTION)
          strcat(flags, "e");
        if (tempuser->flags & PRIV_FRIEND)
          strcat(flags, "f");
        if (tempuser->flags & PRIV_GLINE)
          strcat(flags, "g");
        if (tempuser->flags & PRIV_JUPE)
          strcat(flags, "j");
        if (tempuser->flags & PRIV_OPER)
          strcat(flags, "o");
        if (tempuser->flags & PRIV_SADMIN)
          strcat(flags, "S");

        ircsprintf(uhost, "%s@%s", tempuser->username,
            tempuser->hostname);

        os_notice(lptr, sockfd, "[%-14s] [%-25s] [%-10s]",
          tempuser->nick,
          uhost,
          flags);
      }

      os_notice(lptr, sockfd,
        "-- End of list (%d users found) --",
        cnt);

      break;
    }
    case 'u':
    case 'U':
    {
      os_notice(lptr, sockfd,
        "%s has been up for %s",
        Me.name,
        timeago(TimeStarted, 1));
      break;
    } /* case 'U' */
    case 'm':
    case 'M':
    {
      CalcMem(onick, sockfd);
      break;
    } /* case 'M' */
    case 'i':
    case 'I':
    {
      struct rHost *temphost;
      char uhost[UHOSTLEN + 2];

      os_notice(lptr, sockfd, "-- Restricted Hostmasks --");
      os_notice(lptr, sockfd, "[Hostmask                 ] [Max connections]");
      for (temphost = rHostList; temphost; temphost = temphost->next)
      {
        ircsprintf(uhost, "%s@%s", temphost->username,
            temphost->hostname);

        os_notice(lptr, sockfd, "[%-25s] [%-15d]",
          uhost,
          temphost->hostnum);
      }

      os_notice(lptr, sockfd, "-- End of list --");
      break;
    } /* case 'I' */

    case 'g':
    case 'G':
    {
    #ifdef ALLOW_GLINES
      struct Gline *gptr;
      time_t currtime = current_ts;
      char expstr[MAXLINE];
      char uh[UHOSTLEN + 2];

      os_notice(lptr, sockfd, "-- Listing Glines --");
       os_notice(lptr, sockfd,
        "[Hostmask                 ] [Reason                   ] [Expire] [Who            ]");
      for (gptr = GlineList; gptr; gptr = gptr->next)
      {
        if (gptr->expires)
        {
          ircsprintf(expstr, "%-6.1f",
            (((float) (gptr->expires - currtime)) / 60));
        }
        else
          ircsprintf(expstr, "never ");

        ircsprintf(uh, "%s@%s", gptr->username, gptr->hostname);

        os_notice(lptr, sockfd,
          "[%-25s] [%-25s] [%s] [%-15s]",
          uh,
          gptr->reason,
          expstr,
	  gptr->who);
      }

      os_notice(lptr, sockfd, "-- End of list (%d gline%s) --",
        Network->TotalGlines,
        (Network->TotalGlines == 1) ? "" : "s");
    #else

      os_notice(lptr, sockfd, "Glines are disabled");

    #endif /* ALLOW_GLINES */

      break;
    } /* case 'G' */

    case 'j':
    case 'J':
    {
    #ifdef ALLOW_JUPES
      struct Jupe *tempjupe;

    #ifdef JUPEVOTES
    if (VoteList)
    {
      struct JupeVote *tempvote;
      int i;

      os_notice(lptr, sockfd, "-- Listing Jupe Votes --");
      os_notice(lptr, sockfd,
                "[Server              ] [Votes] [Who       ]");
      for (tempvote = VoteList; tempvote; tempvote = tempvote->next)
      {
        os_notice(lptr, sockfd, "[%-20s] [%-5d] [%-10s]",
                  tempvote->name,
                  tempvote->count,
                  tempvote->who[0]);
        for (i = 1; i < JUPEVOTES; i++)
          {
           if (tempvote->who[i])
            os_notice(lptr, sockfd, "                               [%-10s]",
                      tempvote->who[i]);
          }
       }
    }
    #endif
      os_notice(lptr, sockfd, "-- Listing Jupes --");
      os_notice(lptr, sockfd,
        "[Server/Nickname          ] [Reason                        ] [Who               ]");
      for (tempjupe = JupeList; tempjupe; tempjupe = tempjupe->next)
      {
        os_notice(lptr, sockfd, "[%-25s] [%-30s] [%-15s]",
          tempjupe->name,
          tempjupe->reason,
	  tempjupe->who);
      }
      os_notice(lptr, sockfd, "-- End of list (%d jupe%s) --", 
        Network->TotalJupes,
        (Network->TotalJupes == 1) ? "" : "s");
    #else
      os_notice(lptr, sockfd, "Jupes are disabled");
    #endif

      break;
    } /* case 'J' */

    case 'p':
    case 'P':
    {
      struct PortInfo *tempport;

      os_notice(lptr, sockfd, "-- Port List --");
      os_notice(lptr, sockfd,
        "[Port #] [Type    ] [Hostmask                 ]");
      for (tempport = PortList; tempport; tempport = tempport->next)
      {
        os_notice(lptr, sockfd,
          "[%-6d] [%-8s] [%-25s]",
          tempport->port,
          (tempport->type == PRT_TCM) ? "TCM" : "Users",
          tempport->host ? tempport->host : "*.*");
      }
      os_notice(lptr, sockfd, "-- End of list --");

      break;
    } /* case 'P' */

    case 'c':
    case 'C':
    {
      struct Chanlist *tempchan;

      os_notice(lptr, sockfd, "-- Listing Channels --");
      os_notice(lptr, sockfd,
        "[Channel name             ]");
      for (tempchan = ChanList; tempchan; tempchan = tempchan->next)
      {
        os_notice(lptr, sockfd, "[%-25s]", 
          tempchan->name);
      }

      os_notice(lptr, sockfd, "-- End of list (%d channel%s) --", 
        Network->MyChans,
        (Network->MyChans == 1) ? "" : "s");

      break;
    } /* case 'C' */
    
    case '?':
    {
      time_t uptime = current_ts - TimeStarted;

      os_notice(lptr, sockfd,
        "Total Sent %10.2f %s (%4.1f K/s)",
        GMKBv((float)Network->SendB),
        GMKBs((float)Network->SendB),
        (float)((float)((float)Network->SendB / (float)1024.0) / (float)(uptime)));
      os_notice(lptr, sockfd,
        "Total Recv %10.2f %s (%4.1f K/s)",
        GMKBv((float)Network->RecvB),
        GMKBs((float)Network->RecvB),
        (float)((float)((float)Network->RecvB / (float)1024.0) / (float)(uptime)));

      break;
    } /* case '?' */

    default:
    {
      os_notice(lptr, sockfd, "Unknown switch [%c]",
        av[1][0]);
      break;
    }
  } /* switch (letter[0]) */
} /* o_stats() */

#ifdef ALLOW_KILLCHAN

/*
o_killchan()
 Kills all members of a channel who meet the criteria given
by [options]

Valid options:
 -ops       - Limit matches to channel ops
 -nonops    - Limit matches to non channel ops
 -voices    - Limit matches to channel voices
 -nonvoices - Limit matches to non channel voices
 -nonopers  - Do not kill IRC Operators

Does not affect Service Bots or O: line exception users
*/

static void
o_killchan(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char *reason;
  struct Channel *chptr;
  struct ChannelUser *tempuser, *next;
#if 0
  int ii;
#endif
  int bad,
      cnt;
  int alen;
  int nonops, /* -nonops */
      ops, /* -ops */
      nonvoices, /* -nonvoices */
      voices, /* -voices */
      nonopers; /* -nonopers */
  char argbuf[MAXLINE];

  if (ac < 2)
  {
    os_notice(lptr, sockfd,
      "Syntax: \002KILLCHAN [options] <channel> [reason]\002");
    return;
  }

  chptr = NULL;
  reason = NULL;
  nonopers = 0;
  ops = 0;
  nonops = 0;
  voices = 0;
  nonvoices = 0;

  for (cnt = 1; cnt < ac; cnt++)
  {
    alen = strlen(av[cnt]);
    if (!ircncmp(av[cnt], "-nonopers", alen))
      nonopers = 1;
    else if (!ircncmp(av[cnt], "-ops", alen))
      ops = 1;
    else if (!ircncmp(av[cnt], "-nonops", alen))
      nonops = 1;
    else if (!ircncmp(av[cnt], "-voices", alen))
      voices = 1;
    else if (!ircncmp(av[cnt], "-nonvoices", alen))
      nonvoices = 1;
    else
    {
      if (!(chptr = FindChannel(av[cnt++])))
      {
        os_notice(lptr, sockfd,
          "Invalid channel: %s",
          av[cnt - 1]);
        return;
      }

      if (ac >= (cnt + 1))
        reason = GetString(ac - cnt, av + cnt);
      else
        reason = MyStrdup("");

      break;
    }
  }

  if (!reason)
    reason = MyStrdup("");

  if (!chptr)
  {
    os_notice(lptr, sockfd,
      "No channel specified");
    MyFree(reason);
    return;
  }

  argbuf[0] = '\0';

  if (nonopers)
    strcat(argbuf, "-nonopers ");

  if (ops)
    strcat(argbuf, "-ops ");

  if (nonops)
    strcat(argbuf, "-nonops ");

  if (voices)
    strcat(argbuf, "-voices ");

  if (nonvoices)
    strcat(argbuf, "-nonvoices ");

  o_RecordCommand(sockfd,
    "KILLCHAN %s %s[%s]",
    chptr->name,
    argbuf,
    reason);

  o_Wallops("KILLCHAN %s %s[%s]",
    chptr->name,
    argbuf,
    reason);

  for (tempuser = chptr->firstuser; tempuser; tempuser = next)
  {
    next = tempuser->next;

    bad = 0;

    if (FindService(tempuser->lptr))
      bad = 1;
    else if (nonopers && IsOperator(tempuser->lptr))
      bad = 1;
    else if (ops && !(tempuser->flags & CH_OPPED))
      bad = 1;
    else if (nonops && (tempuser->flags & CH_OPPED))
      bad = 1;
    else if (voices && !(tempuser->flags & CH_VOICED))
      bad = 1;
    else if (nonvoices && (tempuser->flags & CH_VOICED))
      bad = 1;
    else
    {
      if (tempuser->lptr->flags & L_OSREGISTERED)
     /* 
      * IMHO, this code is questionable. I think it would be better to
      * skip L_OSREGISTERED (ie: services oper registered) nicknames, and
      * not to check O: lines configuration. -kre
      */
#if 0
        ii = 1;
      else
        ii = 0;
      if (IsProtected(GetUser(ii, tempuser->lptr->nick,
              tempuser->lptr->username, tempuser->lptr->hostname)))
#endif
        bad = 1;
    }

    if (bad)
      continue;

    toserv(":%s KILL %s :%s!%s (%s (%s@%s))\n",
      n_OperServ,
      tempuser->lptr->nick,
      Me.name,
      n_OperServ,
      reason,
      onick,
      n_OperServ);

    DeleteClient(tempuser->lptr);
  }

  os_notice(lptr, sockfd, "Channel [%s] has been killed",
    chptr->name);

  MyFree(reason);
} /* o_killchan() */

#endif /* ALLOW_KILLCHAN */

#ifdef ALLOW_KILLHOST

/*
o_killhost()
  Kills all users with the user@host av[1] (unless they have an "e"
flag) with reason av[2]-
*/

static void
o_killhost(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char *reason;
  int ii;
  struct Luser *tempuser, *next;
  char *user,
       *host;

  if (ac < 2)
  {
    os_notice(lptr, sockfd, "Syntax: \002KILLHOST <hostmask>\002");
    return;
  }

  if (ac < 3)
    reason = MyStrdup("");
  else
    reason = GetString(ac - 2, av + 2);

  o_RecordCommand(sockfd, "KILLHOST %s [%s]", av[1], reason);

  o_Wallops("KILLHOST %s [%s]", av[1], reason);

  if (!(host = strchr(av[1], '@')))
  {
    user = NULL;
    host = av[1];
  }
  else
  {
    user = av[1];
    *host++ = '\0';
  }

  if (IsProtectedHost(user ? user : "*", host))
  {
    os_notice(lptr, sockfd,
      "%s@%s matches a protected host, unable to kill",
      user ? user : "*",
      host);
    MyFree(reason);
    return;
  }
  
  ii = 0;
  for (tempuser = ClientList; tempuser; tempuser = next)
  {
    next = tempuser->next;

    if (FindService(tempuser))
      continue;

    if (user)
    {
      if (!match(user, tempuser->username))
        continue;
    }

    if (match(host, tempuser->hostname))
    {
      ++ii;
      if (ii > MaxKill)
      {
        os_notice(lptr, sockfd,
          "-- More than %d matches found, halting kills --",
          MaxKill);
        break;
      }

      toserv(":%s KILL %s :%s!%s (%s (%s@%s))\n",
        n_OperServ, tempuser->nick, Me.name, n_OperServ, reason, onick,
        n_OperServ);

      DeleteClient(tempuser);
    }
  }

  if (ii <= MaxKill)
  {
    os_notice(lptr, sockfd,
      "%d match%s of [%s@%s] killed",
      ii,
      (ii == 1) ? "" : "es",
      user ? user : "*",
      host);
  }

  MyFree(reason);
} /* o_killhost() */

#endif /* ALLOW_KILLHOST */

#ifdef HIGHTRAFFIC_MODE

static void
o_htm(struct Luser *lptr, int ac, char **av, int sockfd)

{
  float currload;
  struct OperCommand *cptr;

  o_RecordCommand(sockfd,
    "HTM %s %s",
    (ac < 2) ? "" : StrToupper(av[1]),
    (ac < 3) ? "" : av[2]);

  o_Wallops("HTM %s %s",
    (ac < 2) ? "" : StrToupper(av[1]),
    (ac < 3) ? "" : av[2]);

  if (ac < 2)
  {
    currload = ((float) (Network->RecvB - Network->CheckRecvB) / 
                (float) 1024) / (float) HTM_INTERVAL;

    os_notice(lptr, sockfd,
      "High-Traffic mode is \002%s\002",
      (HTM) ? "ON" : "OFF");
    os_notice(lptr, sockfd,
      "Maximum load: \002%d.00\002 K/s",
      ReceiveLoad);
    os_notice(lptr, sockfd,
      "Current load: \002%0.2f\002 K/s",
      currload);

    return;
  }

  cptr = GetoCommand(htmcmds, av[1]);

  if (cptr && (cptr != (struct OperCommand *) -1))
  {
    /* call cptr->func to execute command */
    (*cptr->func)(lptr, ac, av, sockfd);
  }
  else
  {
    /* the option they gave was not valid */
    os_notice(lptr, sockfd,
      "%s switch [\002%s\002]",
      (cptr == (struct OperCommand *) -1) ? "Ambiguous" : "Unknown",
      av[1]);
  }
} /* o_htm() */

static void
o_htm_on(struct Luser *lptr, int ac, char **av, int sockfd)

{
  float currload;

  if (HTM)
  {
    os_notice(lptr, sockfd, "High-Traffic mode is currently active");
    return;
  }

  currload = ((float) (Network->RecvB - Network->CheckRecvB) / 
              (float) 1024) / (float) HTM_INTERVAL;

  HTM = 1;
  HTM_ts = current_ts;

  putlog(LOG1,
    "Entering high-traffic mode (%0.2f K/s): Forced by %s!%s@%s",
    currload,
    onick,
    ouser,
    ohost);

  SendUmode(OPERUMODE_Y,
    "*** Entering high-traffic mode (%0.2f K/s): Forced by %s!%s@%s\n",
    currload,
    onick,
    ouser,
    ohost);

  os_notice(lptr, sockfd, "High-Traffic mode is now ON");
} /* o_htm_on() */

static void
o_htm_off(struct Luser *lptr, int ac, char **av, int sockfd)

{
  float currload;

  if (!HTM)
  {
    os_notice(lptr, sockfd, "High-Traffic mode is currently inactive");
    return;
  }

  currload = ((float) (Network->RecvB - Network->CheckRecvB) / 
              (float) 1024) / (float) HTM_INTERVAL;

  HTM = HTM_ts = 0;

  putlog(LOG1,
    "Resuming standard traffic operations (%0.2f K/s): Forced by %s!%s@%s",
    currload,
    onick,
    ouser,
    ohost);

  SendUmode(OPERUMODE_Y,
    "*** Resuming standard traffic operations (%0.2f K/s): Forced by %s!%s@%s\n",
    currload,
    onick,
    ouser,
    ohost);

  os_notice(lptr, sockfd, "High-Traffic mode is now OFF");
} /* o_htm_off() */

static void
o_htm_max(struct Luser *lptr, int ac, char **av, int sockfd)

{
  int newmax;

  if (ac < 3)
  {
    os_notice(lptr, sockfd,
      "Syntax: HTM MAX <new rate>");
    return;
  }

  if (!(newmax = IsNum(av[2])))
  {
    os_notice(lptr, sockfd,
      "[%s] is an invalid number",
      av[2]);
    return;
  }

  ReceiveLoad = newmax;

  putlog(LOG1, "HTM rate changed to %d K/s by %s!%s@%s",
    newmax,
    onick,
    ouser,
    ohost);

  SendUmode(OPERUMODE_Y,
    "*** New HTM Rate: %d K/s (forced by %s!%s@%s)\n",
    newmax,
    onick,
    ouser,
    ohost);

  os_notice(lptr, sockfd,
    "The HTM Rate has been set to: %d K/s",
    newmax);
} /* o_htm_max() */

#endif /* HIGHTRAFFIC_MODE */

/*
o_hub()
 Go through server list and display any servers that have av[1]
for a hub
*/

static void
o_hub(struct Luser *lptr, int ac, char **av, int sockfd)

{
  struct Server *hubptr, *tmpserv;
  int cnt;

  o_RecordCommand(sockfd,
    "HUB %s",
    (ac < 2) ? "" : av[1]);

  if (ac < 2)
  {
    /*
     * They didn't give a hub name..list all hubs
     */

    os_notice(lptr, sockfd,
      "-- Listing current hubs --");
    os_notice(lptr, sockfd,
      "[Hub Name                      ] [# servers]");

    cnt = 0;
    for (tmpserv = ServerList; tmpserv; tmpserv = tmpserv->next)
    {
      if (tmpserv->numservs > 1)
      {
        cnt++;
        os_notice(lptr, sockfd,
          "[%-30s] [%-9ld]",
          tmpserv->name,
          tmpserv->numservs);
      }
    }

    os_notice(lptr, sockfd,
      "-- End of list (%d hub%s found) --",
      cnt,
      (cnt == 1) ? "" : "s");

    return;
  }

  if (!(hubptr = FindServer(av[1])))
  {
    os_notice(lptr, sockfd,
      "No such server: \002%s\002",
      av[1]);
    return;
  }

  if (hubptr->numservs <= 1)
  {
    os_notice(lptr, sockfd,
      "\002%s\002 appears to be a leaf",
      hubptr->name);
    return;
  }

  os_notice(lptr, sockfd, "-- \002%s\002 --",
    hubptr->name);
  os_notice(lptr, sockfd,
    "[Leaf Name                     ] [Time Connected      ]");

  /*
   * go through server list and try to find servers who's uplink
   * matches hubptr
   */
  cnt = 0;
  for (tmpserv = ServerList; tmpserv; tmpserv = tmpserv->next)
  {
    if (tmpserv->uplink == hubptr)
    {
      ++cnt;
      os_notice(lptr, sockfd,
        "[%-30s] [%-20s]",
        tmpserv->name,
        timeago(tmpserv->connect_ts, 0));
    }
  }

  os_notice(lptr, sockfd,
    "-- End of list (%d lea%s found) --",
    cnt,
    (cnt == 1) ? "f" : "ves");
} /* o_hub() */

/*
o_umode()
 Configure various user modes for operator lptr->nick.

Possible Usermodes:
  b - show all tcm bot link attempts, invalid connections attempts
      etc
  c - see all clone connections
  C - see all client connections
  d - debugging usermode - combination of all other usermodes
      plus debug() calls
  e - see all clone exits
      (Note: exits are only shown if STATSERVICES is defined,
       since HashDelClient() only finds clone matches to
       update StatServ unique client counts)
  E - see all client exits
  j - see channel joins
  k - see channel kicks
  l - show whenever a server links/delinks to/from the network
  m - see channel mode changes
  n - see nickname changes
  o - see new operators
  O - see operator kills
  p - see channel parts
  s - show commands executed through service nicks
  S - see server kills
  t - see channel topic changes
  y - show various events such as ignore expiration, database
      saves, gline expiration, hub server connections etc.
*/

static void
o_umode(struct Luser *lptr, int ac, char **av, int sockfd)

{
  struct UmodeInfo  *umodeptr;
  struct Userlist *userptr, *tempuser;
  char str[MAXLINE];
  char *tmpstr;
  int minus,
      ii;
  long newmodes;

  o_RecordCommand(sockfd,
    "UMODE %s",
    (ac < 2) ? "" : av[1]);

  if (sockfd != NOSOCKET)
    userptr = DccGetUser(IsDccSock(sockfd));
  else
    userptr = GetUser(1, onick, ouser, ohost);

  if (!userptr)
    return;

  if (ac < 2)
  {
    /*
     * They didn't give an arguement - simply display their
     * current usermodes
     */
    ii = 0;
    for (umodeptr = Usermodes; umodeptr->usermode; ++umodeptr)
    {
      if (userptr->umodes & umodeptr->flag)
        str[ii++] = umodeptr->usermode;
    }
    str[ii] = '\0';

    os_notice(lptr, sockfd,
      "Your usermodes are currently [\002+%s\002]",
      str);

    return;
  }

  /*
   * Parse the given usermodes
   */
  minus = 0;
  for (tmpstr = av[1]; *tmpstr; tmpstr++)
  {
    if (*tmpstr == '+')
      minus = 0;
    else if (*tmpstr == '-')
      minus = 1;
    else
    {
      /* try to find the usermode in Usermodes[] */
      if ((umodeptr = FindUmode(*tmpstr)))
      {
        /*
         * its a usermode, if minus is 1, remove the usermode,
         * otherwise add it to userptr->umodes
         */
        if (minus)
          userptr->umodes &= ~umodeptr->flag;
        else
          userptr->umodes |= umodeptr->flag;
      }
    }
  }

  /*
   * Now go through UserList, and find all structs that have
   * this nickname - update their umodes
   */
  newmodes = userptr->umodes;
  for (tempuser = UserList; tempuser; tempuser = tempuser->next)
  {
    if (!irccmp(userptr->nick, tempuser->nick))
      tempuser->umodes = newmodes;
  }

  /*
   * Show them their new usermodes
   */
  ii = 0;
  for (umodeptr = Usermodes; umodeptr->usermode; ++umodeptr)
  {
    if (userptr->umodes & umodeptr->flag)
      str[ii++] = umodeptr->usermode;
  }
  str[ii] = '\0';

  os_notice(lptr, sockfd,
    "Your usermodes are now [\002+%s\002]",
    str);
} /* o_umode() */

#ifdef ALLOW_DUMP

/*
o_dump()
 Send a raw string to the server
*/

static void
o_dump(struct Luser *lptr, int ac, char **av, int sockfd)

{
  char *dstr;
  char tempstr[MAXLINE];
  char **newav;
  int newac;

  if (ac < 2)
  {
    os_notice(lptr, sockfd,
      "Syntax: \002DUMP <string>\002");
    return;
  }

  dstr = GetString(ac - 1, av + 1);

  o_RecordCommand(sockfd,
    "DUMP [%s]",
    dstr);

  toserv("%s\n", dstr);

  /*
   * Technically, this av almost the same as the arv[] passed to
   * ProcessInfo earlier, sending it through the whole routine again could
   * cause problems - make a new one
   */
  strcpy(tempstr, dstr);
  newac = SplitBuf(tempstr, &newav);
  ProcessInfo(newac, newav);

  MyFree(dstr);
  MyFree(newav);
} /* o_dump() */

#endif /* ALLOW_DUMP */

/*
FindUmode()
 Attempt to find the usermode 'usermode' in the Usermodes[] array.
Return a pointer to the corresponding index
*/

static struct UmodeInfo *
FindUmode(char usermode)

{
  struct  UmodeInfo  *umodeptr;

  for (umodeptr = Usermodes; umodeptr->usermode; ++umodeptr)
  {
    if (umodeptr->usermode == usermode)
      return (umodeptr);
  }

  return (NULL);
} /* FindUmode() */

/*
modestr()
  return number of 'mode's specified by 'num'
  ie: if mode = 'o' and num = 3, return 'ooo'
*/

char *
modestr(int num, char mode)

{
  int ii;
  char *done;

  if (!num)
    return (NULL);

  done = (char *) MyMalloc((sizeof(char) * num) + 1);
  done[0] = '\0';

  for (ii = 0; ii < num; ++ii)
    done[ii] = mode;

  done[ii] = '\0';

  return done;
} /* modestr() */

/*
TakeOver
 args: struct Channel *cptr
 purpose: join 'cptr->name', deop all users who don't match "f",
          op those who do, clear all conflicting modes (imslk),
          and clear all bans matching an admin
 return: none
*/

static void
TakeOver(struct Channel *cptr)

{
  int ii, acnt, mcnt;
  char sendstr[MAXLINE], done[MAXLINE];
  char *opnicks, *dopnicks;
  char **av, *abans, *mtmp;
  struct ChannelBan *tempban;
  struct Userlist *tempuser;
  struct ChannelUser *tempnick;

  if (!cptr)
    return;

#ifdef SAVE_TS

  if (!IsChannel(cptr->name))
    os_join_ts_minus_1(cptr);

#endif /* SAVE_TS */

  /*
   * we don't need to deop anyone if we join with TS - 1, since its
   * already done for us
   */

  /* deop non "f" flag users, op "f" flag users */
  opnicks = (char *) MyMalloc(sizeof(char));
  opnicks[0] = '\0';
  dopnicks = (char *) MyMalloc(sizeof(char));
  dopnicks[0] = '\0';
  for (tempnick = cptr->firstuser; tempnick; tempnick = tempnick->next)
  {
    if (FindService(tempnick->lptr))
      continue;

    if (tempnick->lptr->flags & L_OSREGISTERED)
      ii = 1;
    else
      ii = 0;

    tempuser = GetUser(ii, tempnick->lptr->nick, 
        tempnick->lptr->username, tempnick->lptr->hostname);
    if (IsChannelOp(cptr, tempnick->lptr) && !IsFriend(tempuser))
    {
      dopnicks = (char *) MyRealloc(dopnicks, strlen(dopnicks)
          + strlen(tempnick->lptr->nick) + (2 * sizeof(char)));
      strcat(dopnicks, tempnick->lptr->nick);
      strcat(dopnicks, " ");
    }
    else if (!IsChannelOp(cptr, tempnick->lptr) && IsFriend(tempuser))
    {
      opnicks = (char *) MyRealloc(opnicks, strlen(opnicks)
          + strlen(tempnick->lptr->nick) + (2 * sizeof(char)));
      strcat(opnicks, tempnick->lptr->nick);
      strcat(opnicks, " ");
    }
  } /* for (tempnick ..) */
  
#ifdef SAVE_TS

  if (IsChannel(cptr->name) || (cptr->since == 0))
  {
    /*
     * if OperServ is monitoring the channel, then it didn't join with
     * TS-1 and noone deoped, therefore we must manually deop
     * non "f" flag users - likewise if the channel's TS is 0,
     * join_ts_minus_1() would not deop anyone
     */

#else

  if (1)
  {

#endif /* !SAVE_TS */

    acnt = SplitBuf(dopnicks, &av);
    memset(&done, 0, MAXLINE);
    mcnt = 1;
    for (ii = 0; ii < acnt; ++ii)
    {
      strcat(done, av[ii]);
      strcat(done, " ");
      if (mcnt == MaxModes)
      {
        mcnt = 0;
        mtmp = modestr(MaxModes, 'o');
        ircsprintf(sendstr, "-%s %s", mtmp, done);
        DoMode(cptr, sendstr, 0);
        MyFree(mtmp);
        memset(&done, 0, MAXLINE);
      }
      mcnt++;
    }
    if (done[0] != '\0')
    {
      mtmp = modestr(mcnt - 1, 'o');
      ircsprintf(sendstr, "-%s %s", mtmp, done);
      DoMode(cptr, sendstr, 0);
      MyFree(mtmp);
    }
    MyFree(av);
  }
  else
  {
    if (cptr->since == 0)
    {
      /* if the channel's TS is 0, we can't join with TS-1, so kick all
         the users */

      acnt = SplitBuf(dopnicks, &av);
      for (ii = 0; ii < acnt; ii++)
      {
        toserv(":%s KICK %s %s :\n",
          n_OperServ,
          cptr->name,
          av[ii]);
        RemoveFromChannel(cptr, FindClient(av[ii]));
      }
      MyFree(av);
    }
  }
  MyFree(dopnicks);

  /* clear bans matching an administrator */
  abans = (char *) MyMalloc(sizeof(char));
  abans[0] = '\0';
  for (tempban = cptr->firstban; tempban; tempban = tempban->next)
  {
    if (MatchesAdmin(tempban->mask) == 1)
    {
      abans = (char *) MyRealloc(abans, strlen(abans) + strlen(tempban->mask) + (2 * sizeof(char)));
      strcat(abans, tempban->mask);
      strcat(abans, " ");
    }
  }

  memset(&done, 0, MAXLINE);
  acnt = SplitBuf(abans, &av);
  mcnt = 1;
  for (ii = 0; ii < acnt; ++ii)
  {
    strcat(done, av[ii]);
    strcat(done, " ");
    if (mcnt == MaxModes)
    {
      mcnt = 0;
      mtmp = modestr(MaxModes, 'b');
      ircsprintf(sendstr, "-%s %s", mtmp, done);
      DoMode(cptr, sendstr, 0);
      MyFree(mtmp);
      memset(&done, 0, MAXLINE);
    }
    mcnt++;
  }
  if (done[0] != '\0')
  {
    mtmp = modestr(mcnt - 1, 'b');
    ircsprintf(sendstr, "-%s %s", mtmp, done);
    DoMode(cptr, sendstr, 0);
    MyFree(mtmp);
  }
  MyFree(abans);
  MyFree(av);

  /* clear all bad modes (smilk) */
  memset(&done, 0, MAXLINE);
  strcpy(done, "+ntp-");
  if (cptr->modes & MODE_S)
    strcat(done, "s");
  if (cptr->modes & MODE_M)
    strcat(done, "m");
  if (cptr->modes & MODE_I)
    strcat(done, "i");
  if (cptr->limit)
    strcat(done, "l");
  if (cptr->key)
  {
    strcat(done, "k ");
    strcat(done, cptr->key);
  }

  DoMode(cptr, done, 0);

  /* op "f" flag users */
  memset(&done, 0, MAXLINE);
  acnt = SplitBuf(opnicks, &av);
  mcnt = 1;
  for (ii = 0; ii < acnt; ++ii)
  {
    strcat(done, av[ii]);
    strcat(done, " ");
    if (mcnt == MaxModes)
    {
      mcnt = 0;
      mtmp = modestr(MaxModes, 'o');
      ircsprintf(sendstr, "+%s %s", mtmp, done);
      DoMode(cptr, sendstr, 0);
      MyFree(mtmp);
      memset(&done, 0, MAXLINE);
    }
    ++mcnt;
  }
  if (done[0] != '\0')
  {
    mtmp = modestr(mcnt - 1, 'o');
    ircsprintf(sendstr, "+%s %s", mtmp, done);
    DoMode(cptr, sendstr, 0);
    MyFree(mtmp);
  }
  MyFree(opnicks);
  MyFree(av);

#ifdef SAVE_TS
  if (!IsChannel(cptr->name))
    os_part(cptr);
#endif
} /* TakeOver() */

/*
CalcMem()
  Determine memory usage of HybServ and send the info to 'socket'
or 'nick'
*/

static float
CalcMem(char *nick, int socket)

{
  struct Luser *tempuser;
  struct UserChannel *userc;
  struct ChannelUser *chanu;
#ifdef GECOSBANS
  struct ChannelGecosBan *tempgecosban;
#endif /* GECOSBANS */

  struct Channel *tempchan;
  struct ChannelBan *tempban;
  struct Exception *tempe;

#ifndef BLOCK_ALLOCATION
  struct Server *tempserv;
#endif
  struct Userlist *tempuserlist;
  struct Servlist *tempservlist;
  struct Chanlist *tempchanlist;
  struct Botlist *tempbot;
  struct rHost *temprhost;
  struct PortInfo *tempprt;

#ifdef ALLOW_JUPES
  struct Jupe *tempjupe;
#endif

#ifdef ALLOW_GLINES
  struct Gline *gptr;
#endif

#ifdef NICKSERVICES
  struct NickInfo *nptr;

#ifdef CHANNELSERVICES
  struct ChanInfo *cptr;
#endif

#ifdef MEMOSERVICES
  struct MemoInfo *mptr;
#endif

#endif /* NICKSERVICES */

#ifdef STATSERVICES
  struct HostHash *hosth;
#endif

#if defined(NICKSERVICES) || defined(STATSERVICES)
  int ii;
#endif

  struct Ignore *tmpig;

  struct DccUser *tmpconn;

  char sendstr[MAXLINE];

  float clientm;          /* memory used by clients */
  float servm;            /* memory used by servers */
  float chanm;            /* memory used by channels */
  float chanbanm;         /* memory used by channel bans */
#ifdef GECOSBANS
  float changecosbanm;    /* memory used by gecos bans */
#endif /* GECOSBANS */
  float chanexceptm;      /* memory used by channel exceptions */
  float confm;            /* total memory used by conf lines */
  float connm;            /* memory used by dcc connections */
  float hashc;            /* client hash */
  float hashch;           /* channel hash */
  float hashcl;           /* clone hash */
  float hashs;            /* server hash */
  unsigned long chanbanc; /* number of channel bans */
#ifdef GECOSBANS
  unsigned long changecosbanc; /* number of gecos bans */
#endif /* GECOSBANS */
  unsigned long chanexceptc; /* number of channel exceptions */
  unsigned long confc;    /* total number of conf lines */

#ifdef NICKSERVICES
  float nsm; /* mem used by NickServ */

#ifdef CHANNELSERVICES
  float csm; /* mem used by ChanServ */
#endif

#ifdef MEMOSERVICES
  float msm; /* mem used by MemoServ */
#endif

#endif /* NICKSERVICES */

#ifdef STATSERVICES
  float ssm; /* mem used by StatServ */
#endif

  float igm; /* mem used for ignore list */
  unsigned long igc; /* number of ignore entries */

#ifdef BLOCK_ALLOCATION
  Heap *heapptr;
  SubBlock *sub;
#endif

  float total; /* total memory */

  clientm = 0;
  for (tempuser = ClientList; tempuser; tempuser = tempuser->next)
  {
  #ifndef BLOCK_ALLOCATION
    clientm += (strlen(tempuser->nick) + strlen(tempuser->username) +
                strlen(tempuser->hostname) + strlen(tempuser->realname) +
                sizeof(struct Luser));
  #endif

    for (userc = tempuser->firstchan; userc; userc = userc->next)
      clientm += sizeof(struct UserChannel);
  }

#ifdef BLOCK_ALLOCATION

  clientm += sizeof(Heap);
  heapptr = ClientHeap;
  for (sub = heapptr->base; sub; sub = sub->next)
    clientm += ((heapptr->ElementsPerBlock * heapptr->ElementSize) +
                sizeof(SubBlock));

#endif /* BLOCK_ALLOCATION */

  chanm = 0;
  chanbanm = 0;
#ifdef GECOSBANS
  changecosbanm = 0;
#endif /* GECOSBANS */
  chanexceptm = 0;
  chanbanc = 0;
#ifdef GECOSBANS
  changecosbanc = 0;
#endif /* GECOSBANS */
  chanexceptc = 0;
  for (tempchan = ChannelList; tempchan; tempchan = tempchan->next)
  {
  #ifndef BLOCK_ALLOCATION
    int   keylen = 0;

    if (tempchan->key)
      keylen = strlen(tempchan->key);

    chanm += (strlen(tempchan->name) + keylen + sizeof(struct Channel));
  #endif /* BLOCK_ALLOCATION */

    for (chanu = tempchan->firstuser; chanu; chanu = chanu->next)
      chanm += sizeof(struct ChannelUser);

    for (tempban = tempchan->firstban; tempban; tempban = tempban->next)
    {
      chanbanc++;
      chanbanm += (strlen(tempban->mask) + sizeof(struct ChannelBan));
      if (tempban->who)
        chanbanm += strlen(tempban->who);
    }

#ifdef GECOSBANS
    for (tempgecosban = tempchan->firstgecosban; tempgecosban;
        tempgecosban = tempgecosban->next)
    {
      changecosbanc++;
      changecosbanm += (strlen(tempgecosban->mask) +
          sizeof(struct ChannelGecosBan));
      if (tempgecosban->who)
        changecosbanm += strlen(tempgecosban->who);
    }
#endif /* GECOSBANS */

    for (tempe = tempchan->exceptlist; tempe; tempe = tempe->next)
    {
      ++chanexceptc;
      chanexceptm += (strlen(tempe->mask) + sizeof(struct Exception));
      if (tempe->who)
        chanexceptm += strlen(tempe->who);
    }
  }

#ifdef BLOCK_ALLOCATION

  chanm += sizeof(Heap);
  heapptr = ChannelHeap;
  for (sub = heapptr->base; sub; sub = sub->next)
    chanm += ((heapptr->ElementsPerBlock * heapptr->ElementSize) +
              sizeof(SubBlock));

#endif /* BLOCK_ALLOCATION */

  servm = 0;

#ifdef BLOCK_ALLOCATION
  servm += sizeof(Heap);
  heapptr = ServerHeap;
  for (sub = heapptr->base; sub; sub = sub->next)
    servm += ((heapptr->ElementsPerBlock * heapptr->ElementSize) +
              sizeof(SubBlock));
#else
  for (tempserv = ServerList; tempserv; tempserv = tempserv->next)
    servm += (strlen(tempserv->name) + sizeof(struct Server));
#endif /* BLOCK_ALLOCATION */

  confm = (strlen(Me.admin) + strlen(Me.name) + strlen(Me.info));
  confc = 2;

  for (tempuserlist = UserList; tempuserlist; tempuserlist = tempuserlist->next)
  {
    confm += (strlen(tempuserlist->username) +
              strlen(tempuserlist->hostname) +
              strlen(tempuserlist->password) +
              sizeof(struct Userlist));
    confc++;
  }
  
  for (tempservlist = ServList; tempservlist; tempservlist = tempservlist->next)
  {
    confm += (strlen(tempservlist->hostname) +
              strlen(tempservlist->password) +
              sizeof(struct Servlist));

    if (tempservlist->realname)
      confm += strlen(tempservlist->realname);

    confc++;
  }

  for (tempchanlist = ChanList; tempchanlist; tempchanlist = tempchanlist->next)
  {
    confm += (strlen(tempchanlist->name) + sizeof(struct Chanlist));
    confc++;
  }

  for (tempbot = BotList; tempbot; tempbot = tempbot->next)
  {
    confm += (strlen(tempbot->name) +
              strlen(tempbot->hostname) +
              strlen(tempbot->password) +
              sizeof(struct Botlist));
    if (tempbot->username)
      confm += strlen(tempbot->username);
    confc++;
  }

  for (tempbot = RemoteBots; tempbot; tempbot = tempbot->next)
  {
    confm += (strlen(tempbot->name) +
              strlen(tempbot->username) +
              strlen(tempbot->hostname) +
              strlen(tempbot->password) +
              sizeof(struct Botlist));
    confc++;
  }

  for (temprhost = rHostList; temprhost; temprhost = temprhost->next)
  {
    confm += (strlen(temprhost->username) +
              strlen(temprhost->hostname) +
              sizeof(struct rHost));
    confc++;
  }

  for (tempprt = PortList; tempprt; tempprt = tempprt->next)
  {
    confm += sizeof(struct PortInfo);
    if (tempprt->host)
      confm += strlen(tempprt->host);
    confc++;
  }

#ifdef ALLOW_JUPES
  for (tempjupe = JupeList; tempjupe; tempjupe = tempjupe->next)
    confm += (strlen(tempjupe->name) +
              strlen(tempjupe->reason) +
              strlen(tempjupe->who) +
              sizeof(struct Jupe));
  confc += Network->TotalJupes;
#endif

#ifdef ALLOW_GLINES
  for (gptr = GlineList; gptr; gptr = gptr->next)
  {
    confm += (strlen(gptr->username) +
              strlen(gptr->hostname) +
              strlen(gptr->reason) +
              strlen(gptr->who) +
              sizeof(struct Gline));
    /* 
     * make sure the gline isn't temporary because it wouldn't have
     * a conf line
     */
    if (!(gptr->expires))
      confc++;
  }
#endif

  igc = 0;
  igm = 0;
  for (tmpig = IgnoreList; tmpig; tmpig = tmpig->next)
  {
    igc++;
    igm += (strlen(tmpig->hostmask) + sizeof(struct Ignore));
  }

  connm = 0;
  for (tmpconn = connections; tmpconn; tmpconn = tmpconn->next)
  {
    connm += sizeof(struct DccUser);
    if (tmpconn->nick)
      connm += strlen(tmpconn->nick);
    if (tmpconn->username)
      connm += strlen(tmpconn->username);
    if (tmpconn->hostname)
      connm += strlen(tmpconn->hostname);
  }

#ifdef NICKSERVICES

  nsm = sizeof(nicklist);
  for (ii = 0; ii < NICKLIST_MAX; ++ii)
  {
    if (nicklist[ii])
    {
      struct NickHost *hptr;

      for (nptr = nicklist[ii]; nptr; nptr = nptr->next)
      {
        nsm += (strlen(nptr->nick) + sizeof(struct NickInfo));
        if (nptr->password)
          nsm += strlen(nptr->password);
        if (nptr->email)
          nsm += strlen(nptr->email);
        if (nptr->url)
          nsm += strlen(nptr->url);

        if (nptr->lastu)
          nsm += strlen(nptr->lastu);
        if (nptr->lasth)
          nsm += strlen(nptr->lasth);
        if (nptr->lastqmsg)
          nsm += strlen(nptr->lastqmsg);

        for (hptr = nptr->hosts; hptr; hptr = hptr->next)
          nsm += (strlen(hptr->hostmask) + sizeof(struct NickHost));
      }
    }
  } /* for (ii = 0; ii < NICKLIST_MAX; ii++) */
  
#ifdef CHANNELSERVICES

  csm = sizeof(chanlist);
  for (ii = 0; ii < CHANLIST_MAX; ++ii)
  {
    struct AutoKick *ak;
    struct ChanAccess *ca;

    for (cptr = chanlist[ii]; cptr; cptr = cptr->next)
    {
      csm += (strlen(cptr->name) + sizeof(struct ChanInfo));
      if (cptr->password)
        csm += strlen(cptr->password);
      if (cptr->founder)
        csm += strlen(cptr->founder);
      if (cptr->successor)
        csm += strlen(cptr->successor);
      if (cptr->topic)
        csm += strlen(cptr->topic);
      if (cptr->key)
        csm += strlen(cptr->key);
      if (cptr->entrymsg)
        csm += strlen(cptr->entrymsg);
      if (cptr->email)
        csm += strlen(cptr->email);
      if (cptr->url)
        csm += strlen(cptr->url);
      for (ak = cptr->akick; ak; ak = ak->next)
      {
        csm += (strlen(ak->hostmask) + sizeof(struct AutoKick));
        if (ak->reason)
          csm += strlen(ak->reason);
      }
      for (ca = cptr->access; ca; ca = ca->next)
      {
        csm += sizeof(struct ChanAccess);
        if (ca->hostmask)
          csm += strlen(ca->hostmask);
      }
    }
  } /* for (ii = 0; ii < CHANLIST_MAX; ii++) */

#endif /* CHANNELSERVICES */

#ifdef MEMOSERVICES

  msm = sizeof(memolist);
  for (ii = 0; ii < MEMOLIST_MAX; ++ii)
  {
    struct Memo *memoptr;

    for (mptr = memolist[ii]; mptr; mptr = mptr->next)
    {
      msm += (strlen(mptr->name) + sizeof(struct MemoInfo));
      for (memoptr = mptr->memos; memoptr; memoptr = memoptr->next)
        msm += (strlen(memoptr->sender) + strlen(memoptr->text) + sizeof(struct Memo));
    }
  } /* for (ii = 0; ii < MEMOLIST_MAX; ii++) */

#endif /* MEMOSERVICES */

#endif /* NICKSERVICES */

#ifdef STATSERVICES

  ssm = 0;
  for (ii = 0; ii < HASHCLIENTS; ++ii)
  {
    if (hostTable[ii].list)
    {
      for (hosth = hostTable[ii].list; hosth; hosth = hosth->hnext)
        ssm += (strlen(hosth->hostname) + sizeof(struct HostHash) + sizeof(aHashEntry));
    }
    else
      ssm += sizeof(aHashEntry);
  }
  
#endif /* STATSERVICES */

  hashc = HASHCLIENTS * sizeof(aHashEntry);
  hashch = HASHCHANNELS * sizeof(aHashEntry);
  hashcl = HASHCLIENTS * sizeof(aHashEntry);
  hashs = HASHSERVERS * sizeof(aHashEntry);

  total = clientm +
          chanm +
          chanbanm +
#ifdef GECOSBANS
          changecosbanm +
#endif /* GECOSBANS */
          chanexceptm +
          servm +
          confm +
          connm +
          hashc +
          hashch +
          hashcl +
          hashs;

#ifdef NICKSERVICES
  total += nsm;

#ifdef CHANNELSERVICES
  total += csm;
#endif

#ifdef MEMOSERVICES
  total += msm;
#endif

#endif /* NICKSERVICES */

#ifdef STATSERVICES
  total += ssm;
#endif

  total += igm;

  if ((nick) || (socket != NODCC))
  {
    /* User structures usage */
    ircsprintf(sendstr,
      "Clients:    %5.0f (%10.0fb) (%10.2fkb)\n",
      Network->TotalUsers,
      clientm,
      clientm / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

    /* Server structures usage */
    ircsprintf(sendstr,
      "Servers:    %5.0f (%10.0fb) (%10.2fkb)\n",
      Network->TotalServers,
      servm,
      servm / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

    /* Channel structures usage */
    ircsprintf(sendstr,
      "Channels:   %5.0f (%10.0fb) (%10.2fkb)\n",
      Network->TotalChannels,
      chanm,
      chanm / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

    /* Channel ban mem usage */
    ircsprintf(sendstr,
      "Bans:       %5ld (%10.0fb) (%10.2fkb)\n",
      chanbanc,
      chanbanm,
      chanbanm / 1024);
#ifdef GECOSBANS
   /* Channel gecos ban mem usage */
    ircsprintf(sendstr,
      "Gecos Bans:       %5ld (%10.0fb) (%10.2fkb)\n",
      changecosbanc,
      changecosbanm,
      changecosbanm / 1024);
#endif /* GECOSBANS */
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

    /* Channel exception memory usage */
    ircsprintf(sendstr,
      "Exceptions: %5ld (%10.0fb) (%10.2fkb)\n",
      chanexceptc,
      chanexceptm,
      chanexceptm / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

    ircsprintf(sendstr,
      "Conf lines: %5ld (%10.0fb) (%10.2fkb)\n",
      confc,
      confm,
      confm / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

    ircsprintf(sendstr,
      "Ignores: %8ld (%10.0fb) (%10.2fkb)\n",
      igc,
      igm,
      igm / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

    ircsprintf(sendstr,
      "Partyline: %6d (%10.0fb) (%10.2fkb)\n",
      Network->TotalConns,
      connm,
      connm / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

  #ifdef NICKSERVICES
    ircsprintf(sendstr,
      "NickServ:         (%10.0fb) (%10.2fkb)\n",
      nsm,
      nsm / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

  #ifdef CHANNELSERVICES
    ircsprintf(sendstr,
      "ChanServ:         (%10.0fb) (%10.2fkb)\n",
      csm,
      csm / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);
  #endif

  #ifdef MEMOSERVICES
    ircsprintf(sendstr,
      "MemoServ:         (%10.0fb) (%10.2fkb)\n",
      msm,
      msm / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);
  #endif

  #endif /* NICKSERVICES */

  #ifdef STATSERVICES
    ircsprintf(sendstr,
      "StatServ:         (%10.0fb) (%10.2fkb)\n",
      ssm,
      ssm / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);
  #endif /* STATSERVICES */

    /* Client Hash */
    ircsprintf(sendstr,
      "Client Hash:      (%10.0fb) (%10.2fkb)\n",
      hashc,
      hashc / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

    /* Channel Hash */
    ircsprintf(sendstr,
      "Channel Hash:     (%10.0fb) (%10.2fkb)\n",
      hashch,
      hashch / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

    /* Clone Hash */
    ircsprintf(sendstr,
      "Clone Hash:       (%10.0fb) (%10.2fkb)\n",
      hashcl,
      hashcl / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);

    /* Total memory used */
    ircsprintf(sendstr,
      "Total:            (%10.0fb) (%10.2fkb) (%10.2fmb)\n",
      total,
      total / 1024,
      (total / 1024) / 1024);
    if (socket == NODCC)
      toserv(":%s NOTICE %s :%s",
        n_OperServ,
        nick,
        sendstr);
    else
      writesocket(socket, sendstr);
  } /* if (nick || (socket != NODCC)) */

  return (total);
} /* CalcMem() */

#if defined AUTO_ROUTING && defined SPLIT_INFO
void ReconnectCheck(time_t expire)
{
  struct Server *temphub, *templeaf;
  struct cHost *temphost;

  /* XXX - This will connect only disconnected servers, and will try try
   * to connect those who appear in M: but are not in server list, simply
   * because we don't know their split_ts. I'm sure this can be done
   * better -kre */
  for (temphost=cHostList; temphost; temphost=temphost->next)
  {
    if (!(temphub=FindServer(temphost->hub)))
      continue;
    if (!(templeaf=FindServer(temphost->leaf)))
      continue;
    /* Check if hub is connected, and if it is, check difference between
     * split_ts time and current time */
    if (temphub->uplink && !templeaf->uplink &&
        expire-templeaf->split_ts>temphost->re_time)
    {
      toserv(":%s CONNECT %s %d :%s\n", Me.name, temphost->leaf,
          temphost->port, temphost->hub);
#ifdef DEBUG
      fprintf(stderr, "CONNECTing %s to %s:%d after %s time\n",
          temphost->leaf, temphost->hub, temphost->port,
          timeago(temphost->re_time, 0));
#endif
      return; /* We don't want to lag DoTimer much, don't we? */
    }
  }
}  
#endif

/* 
 * o_kline()
 * Side effects: will kline user, locally or globally, depends on U or
 * share lines.
 * -kre
 */
static void o_kline(struct Luser *lptr, int ac, char **av, int sockfd)
{

  char *klinestr;
  
  /* At least we need two arguments */
  if (ac < 2)
  {
    os_notice(lptr, sockfd,
        "Syntax: \002KLINE [time] <nick|user@host> [:reason]");
    return;
  }

  klinestr = GetString(ac - 1, av + 1);

  /* It is IMHO stupid to parse it and send to IRCD that will parse it all
   * over again, so we will just hand it over to ircd. -kre */

#ifdef HYBRID7_KLINE
  toserv(":%s KLINE %s %s %s\n", Me.name, n_OperServ, "*", klinestr);
#else
  toserv(":%s KLINE %s %s\n", Me.name, n_OperServ, klinestr);
#endif

  MyFree(klinestr);
}
