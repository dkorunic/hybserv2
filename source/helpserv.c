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

#include <stdio.h>
#include <string.h>

#include "alloc.h"
#include "chanserv.h"
#include "client.h"
#include "conf.h"
#include "config.h"
#include "hash.h"
#include "helpserv.h"
#include "hybdefs.h"
#include "match.h"
#include "memoserv.h"
#include "misc.h"
#include "mystring.h"
#include "nickserv.h"
#include "operserv.h"
#include "settings.h"
#include "sock.h"
#include "statserv.h"
#include "seenserv.h"
#include "sprintf_irc.h"

#ifdef HELPSERVICES

static void hs_givehelp(struct Luser *, int, char **);

static struct Command helpcmds[] = {
  { "HELP", hs_givehelp, LVL_NONE },
  { "OPERSERV", hs_givehelp, LVL_NONE },

#ifdef NICKSERVICES

  { "NICKSERV", hs_givehelp, LVL_NONE },

  #ifdef CHANNELSERVICES
  { "CHANSERV", hs_givehelp, LVL_NONE },
  #endif

  #ifdef MEMOSERVICES
  { "MEMOSERV", hs_givehelp, LVL_NONE },
  #endif

#endif /* NICKSERVICES */

#ifdef STATSERVICES
  { "STATSERV", hs_givehelp, LVL_NONE },
#endif

#ifdef SEENSERVICES
  { "SEENSERV", hs_givehelp, LVL_NONE },
#endif

  { 0, 0, 0 }
};

/*
hs_process()
  Process command coming from 'nick' directed towards n_HelpServ
*/

void
hs_process(char *nick, char *command)

{
  int acnt;
  char **arv;
  struct Command *hptr;
  struct Luser *lptr;

  if (!command || !(lptr = FindClient(nick)))
    return;

  if (Network->flags & NET_OFF)
  {
    notice(n_HelpServ, lptr->nick,
      "Services are currently \002disabled\002");
    return;
  }

  acnt = SplitBuf(command, &arv);
  if (acnt == 0)
  {
    MyFree(arv);
    return;
  }

  hptr = GetCommand(helpcmds, arv[0]);

  if (!hptr || (hptr == (struct Command *) -1))
  {
    notice(n_HelpServ, lptr->nick,
      "%s command [%s]",
      (hptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
      arv[0]);
    MyFree(arv);
    return;
  }

  /*
   * Check if the command is for admins only - if so,
   * check if they match an admin O: line.  If they do,
   * check if they are registered with OperServ,
   * if so, allow the command
   */
  if ((hptr->level == LVL_ADMIN) && !(IsValidAdmin(lptr)))
  {
    notice(n_HelpServ, lptr->nick, "Unknown command [%s]",
      arv[0]);
    MyFree(arv);
    return;
  }

  /* call hptr->func to execute command */
  (*hptr->func)(lptr, acnt, arv);

  MyFree(arv);

  return;
} /* hs_process() */

/*
hs_givehelp()
  Main HelpServ routine - give lptr->nick help on av[2]; av[1] should
be the *Serv which has the command
*/

static void
hs_givehelp(struct Luser *lptr, int ac, char **av)

{
  struct Luser *serviceptr;

  if (!irccmp(av[0], "HELP"))
  {
    if (ac >= 2)
      GiveHelp(n_HelpServ, lptr->nick, av[1], NODCC);
    else
      GiveHelp(n_HelpServ, lptr->nick, NULL, NODCC);
  }
  else
  {
    char  str[MAXLINE];

    if (!(serviceptr = GetService(av[0])))
    {
      /*
       * this shouldn't happen unless they entered a partial
       * service nick, and the abbreviation code allowed it
       * to go through
       */
      notice(n_HelpServ, lptr->nick,
        "[\002%s\002] is an invalid service nickname",
        av[0]);
      return;
    }

    if (ac < 2)
      strcpy(str, "HELP");
    else if (ac >= 3)
    {
      ircsprintf(str, "HELP %s %s", av[1], av[2]);
    }
    else
    {
      ircsprintf(str, "HELP %s", av[1]);
    }

    if (serviceptr == Me.osptr)
      os_process(lptr->nick, str, NODCC);

  #ifdef NICKSERVICES

    if (serviceptr == Me.nsptr)
      ns_process(lptr->nick, str);

    #ifdef CHANNELSERVICES
      if (serviceptr == Me.csptr)
        cs_process(lptr->nick, str);
    #endif

    #ifdef MEMOSERVICES
      if (serviceptr == Me.msptr)
        ms_process(lptr->nick, str);
    #endif

  #endif /* NICKSERVICES */

  #ifdef STATSERVICES

    if (serviceptr == Me.ssptr)
      ss_process(lptr->nick, str);
      
  #endif /* STATSERVICES */

  #ifdef SEENSERVICES

    if (serviceptr == Me.esptr)
      es_process(lptr->nick, str);

  #endif /* SEENSERVICES */

  } /* else */
} /* hs_givehelp() */

#endif /* HELPSERVICES */

/*
GiveHelp()
  args: char *helpnick, char *command
  purpose: give 'helpnick' help on the command specified in 'command';
           if no command is given, output a list of available commands
  return: none         
*/

void
GiveHelp(char *Serv, char *helpnick, char *command, int sockfd)

{
  FILE *fp;
  char sendstr[MAXLINE];
  char line[MAXLINE];
  char *final;
  struct Luser *servptr;

  if (!(servptr = GetService(Serv)))
    return;

  if (!command) /* they want general help */
  {
    if (sockfd == NODCC) /* they want /msg commands */
    {
      sendstr[0] = '\0';
      if (servptr == Me.osptr)
        ircsprintf(sendstr, "%s/operserv/index", HelpPath);

    #ifdef NICKSERVICES

      else if (servptr == Me.nsptr)
        ircsprintf(sendstr, "%s/nickserv/index", HelpPath);

      #ifdef CHANNELSERVICES
        else if (servptr == Me.csptr)
          ircsprintf(sendstr, "%s/chanserv/index", HelpPath);
      #endif

      #ifdef MEMOSERVICES
        else if (servptr == Me.msptr)
          ircsprintf(sendstr, "%s/memoserv/index", HelpPath);
      #endif

    #endif /* NICKSERVICES */

    #ifdef STATSERVICES
      else if (servptr == Me.ssptr)
        ircsprintf(sendstr, "%s/statserv/index", HelpPath);
    #endif /* STATSERVICES */

    #ifdef HELPSERVICES
      else if (servptr == Me.hsptr)
        ircsprintf(sendstr, "%s/helpserv/index", HelpPath);
    #endif /* HELPSERVICES */

    #ifdef GLOBALSERVICES
      else if (servptr == Me.gsptr)
        ircsprintf(sendstr, "%s/global/index", HelpPath);
    #endif /* GLOBALSERVICES */

    #ifdef SEENSERVICES
      else if (servptr == Me.esptr)
          ircsprintf(sendstr, "%s/seenserv/index", HelpPath );
    #endif /* SEENSERVICES */

      if (sendstr[0])
      {
        if ((fp = fopen(sendstr, "r")) == NULL)
        {
          notice(Serv, helpnick, "Unable to open help file");
          return;
        }
      }
      else
        return; /* something got messed up =/ */
    }
    else /* they want dcc commands (OperServ only) */
    {
      if (servptr == Me.osptr)
      {
        ircsprintf(sendstr, "%s/operserv/dcc/index", HelpPath);
        if ((fp = fopen(sendstr, "r")) == NULL)
        {
          writesocket(sockfd, "Unable to open help file\n");
          return;
        }
      }
      else
        return; /* no other *Serv's allow dcc chat */
    }

    while (fgets(line, MAXLINE - 1, fp))
    {
      if (IsEOL(*line))
      {
        if (sockfd != NODCC)
          writesocket(sockfd, "\r\n");
        else
          notice(Serv, helpnick, "\002\002");

        continue;
      }

      final = Substitute(helpnick, line, sockfd);
      if (final && (final != (char *) -1))
      {
        if (sockfd == NODCC)
          notice(Serv, helpnick, final);
        else
          writesocket(sockfd, final);
      }
      if (final != (char *) -1)
        MyFree(final);
    }
    fclose(fp);
  }
  else /* they want help on a specific command */
  {
    int cac;
    char **cav;
    char helparg[MAXLINE], arg2[MAXLINE];

    arg2[0] = '\0';

    /* Bug found by larne and binder -kre */
    if (strchr(command, '/') || strstr(command, ".."))
    {
      ircsprintf(sendstr, "Invalid help string");
      if (sockfd == NODCC)
        notice(Serv, helpnick, sendstr);
      else
      {
        writesocket(sockfd, sendstr);
        writesocket(sockfd, "\n");
      }
      return;
    }

    /* Proceed with splitting -kre */
    cac = SplitBuf(command, &cav);
    strcpy(helparg, StrTolower(cav[0]));

    if (cac > 1)
      strcpy(arg2, StrTolower(cav[1]));

    if (sockfd == NODCC)
    {
      if (servptr == Me.osptr)
        ircsprintf(sendstr, "%s/operserv/%s", HelpPath, helparg);

    #ifdef NICKSERVICES

      else if (servptr == Me.nsptr)
      {
        if (arg2[0])
          ircsprintf(sendstr, "%s/nickserv/%s/%s",
            HelpPath,
            helparg,
            arg2);
        else
          ircsprintf(sendstr, "%s/nickserv/%s", HelpPath, helparg);
      }

      #ifdef CHANNELSERVICES
        else if (servptr == Me.csptr)
        {
          if (arg2[0])
            ircsprintf(sendstr, "%s/chanserv/%s/%s",
              HelpPath,
              helparg,
              arg2);
          else
            ircsprintf(sendstr, "%s/chanserv/%s", HelpPath, helparg);
        }
      #endif /* CHANNELSERVICES */

      #ifdef MEMOSERVICES
        else if (servptr == Me.msptr)
        {
          if (arg2[0])
            ircsprintf(sendstr, "%s/memoserv/%s/%s",
              HelpPath,
              helparg,
              arg2);
          else
            ircsprintf(sendstr, "%s/memoserv/%s", HelpPath, helparg);
        }
      #endif /* MEMOSERVICES */

    #endif /* NICKSERVICES */

    #ifdef STATSERVICES
      else if (servptr == Me.ssptr)
      {
        if (arg2[0])
          ircsprintf(sendstr, "%s/statserv/%s/%s",
            HelpPath,
            helparg,
            arg2);
        else
          ircsprintf(sendstr, "%s/statserv/%s", HelpPath, helparg);
      }
    #endif /* STATSERVICES */

    #ifdef HELPSERVICES
      else if (servptr == Me.hsptr)
      {
        if (arg2[0])
          ircsprintf(sendstr, "%s/helpserv/%s/%s",
            HelpPath,
            helparg,
            arg2);
        else
          ircsprintf(sendstr, "%s/helpserv/%s", HelpPath, helparg);
      }
    #endif /* HELPSERVICES */

    #ifdef GLOBALSERVICES
      else if (servptr == Me.gsptr)
      {
        if (arg2[0])
          ircsprintf(sendstr, "%s/global/%s/%s",
            HelpPath,
            helparg,
            arg2);
        else
          ircsprintf(sendstr, "%s/global/%s", HelpPath, helparg);
      }
    #endif /* GLOBALSERVICES */

    }
    else
    {
      ircsprintf(sendstr, "%s/operserv/%s", HelpPath, helparg);
      if ((fp = fopen(sendstr, "r")) == NULL)
        ircsprintf(sendstr, "%s/operserv/dcc/%s", HelpPath, helparg);
      else
        fclose(fp);
    }

    MyFree(cav);

    if ((fp = fopen(sendstr, "r")) == NULL)
    {
      ircsprintf(sendstr, "No help available on \002%s %s\002", helparg, arg2);
      if (sockfd == NODCC)
        notice(Serv, helpnick, sendstr);
      else
      {
        writesocket(sockfd, sendstr);
        writesocket(sockfd, "\n");
      }
      return;
    }
    while (fgets(line, MAXLINE - 1, fp))
    {
      final = Substitute(helpnick, line, sockfd);
      if (sockfd == NODCC)
      {
        if (!(final == (char *) -1))
          notice(Serv, helpnick, final ? final : "\002\002");
      }
      else
      {
        writesocket(sockfd, final ? final : "");
        /*writesocket(sockfd, "\n");*/
      }
      if (final && (final != (char *) -1))
        MyFree(final);
    }
    fclose(fp);
  }  /* else */
} /* GiveHelp() */
