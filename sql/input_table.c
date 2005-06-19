/*
 * Inputs nick.db and chan.db into nick table and chan table. This
 * additional utility will dump all existing records into the
 * corresponding tables in defined database. -kre
 *
 * NOTE: At this moment this is undergoing work and is not (obviously)
 * ready for wide use. -kre
 *
 * $Id$
 */

#include "defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif

#include "alloc.h"
#include "chanserv.h"
#include "client.h"
#include "conf.h"
#include "config.h"
#include "channel.h"
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
#include "sock.h"
#include "timestr.h"
#include "sprintf_irc.h"

/* use code from ns_loaddata() and use SQLPutNick instead of AddNick */
int input_nick()
{
  FILE *fp;
  char line[MAXLINE], **av;
  char *keyword;
  int ac, ret = 1, cnt = 0;
  int islink = 0;
  struct NickInfo *nptr = NULL;

  if (!(fp = fopen(NickServDB, "r")))
    return -1;

  /* load data into list */
  while (fgets(line, MAXLINE - 1, fp))
  {
    cnt++;
    ac = SplitBuf(line, &av);

    if (!ac || (av[0][0] == ';'))
    {
      MyFree(av);
      continue;
    }

    if (!ircncmp("->", av[0], 2))
    {
      /* check if there are enough args */
      if (ac < 2)
      {
        fatal(1, "%s:%d Invalid database format (FATAL)",
          NickServDB, cnt);
        ret = -2;
        continue;
      }

      /* check if there is no nickname associated with the data */
      if (!nptr)
      {
        fatal(1, "%s:%d No nickname associated with data",
          NickServDB, cnt);
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
            NickServDB, cnt, nptr->nick);
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
            NickServDB, cnt, nptr->nick);
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
            NickServDB, cnt, nptr->nick);
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
      else if (LastSeenInfo && !ircncmp(keyword, "LASTQMSG", 6))
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
         * we've come to a new nick entry, so add the last nick
         * to the list before proceeding
         */
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
#if 0
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
        nptr->flags &= ~NS_IDENTIFIED;
#if 0
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
}

int input_chan()
{
  /* use code from cs_loaddata() and use SQLPutChan instead of AddChan */
}

int main()
{
}
