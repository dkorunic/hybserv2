/*
 * HybServ2 Services by HybServ2 team
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
#include <unistd.h>
#include <string.h>

#include "alloc.h"
#include "channel.h"
#include "chanserv.h"
#include "conf.h"
#include "config.h"
#include "data.h"
#include "init.h"
#include "nickserv.h"
#include "settings.h"
#include "show.h"
#include "timestr.h"

#ifdef NICKSERVICES

static void InitData();
static long parseargs(int, char **);


static void DisplayNick(struct NickInfo *, int);

#ifdef CHANNELSERVICES
static void DisplayChan(struct ChanInfo *, int);
#endif

char *Filename; /* for use with -f command line option */
int ncidx; /* index of first given nickname/channel in av[] */

/*
InitData()
 Load config file and databases
*/

static void
InitData()

{
  if (chdir(HPath) != 0)
    {
      fprintf(stderr,
              "HPath is an invalid directory, please check %s\n",
              SETPATH);
      exit (0);
    }

  InitLists();
  LoadConfig(ConfigFile);
  LoadData();
} /* InitData() */

/*
parseargs()
 Parse command line arguements given to shownicks or showchans
*/

static long
parseargs(int ac, char *av[])

{
  int ii;
  long flags;

  Filename = NULL;
  ncidx = 0;

  flags = 0;

  /*
   * First, go through arguements too look for "-" switches
   */
  for (ii = 1; ii < ac; ii++)
    {
      if (av[ii][0] == '-')
        {
          switch (av[ii][1])
            {
            case 'h':
            case '?':
              {
                flags |= SHOW_HELP;
                break;
              } /* case 'h' */

            case 'n':
              {
                flags |= SHOW_TOTALCOUNT;
                break;
              } /* case 'n' */

            case 'f':
              {
                if (!Filename)
                  {
                    if ((ii + 1) >= ac)
                      {
                        /*
                         * They gave just -f with no filename
                         */
                        flags |= SHOW_HELP;
                        break;
                      }

                    Filename = av[ii + 1];
                    ii++;
                  }

                flags |= SHOW_FILE;
                break;
              } /* case 'f' */

            default:
              {
                flags |= SHOW_HELP;
                break;
              } /* default */
            } /* switch (av[ii][1]) */
        } /* if (av[ii][0] == '-') */
      else
        {
          /*
           * The argument is a specific nickname or channel
           */
          flags |= SHOW_DETAIL;
          if (!ncidx)
            ncidx = ii;
        }
    } /* for (ii = 1; ii < ac; ii++) */

  return (flags);
} /* parseargs() */

/*
ShowNicknames()
 Invoked through "shownicks" to display nickname information
*/

void
ShowNicknames(int ac, char *av[])

{
  long flags;
  int ii;
  struct NickInfo *nptr;

  flags = parseargs(ac, av);

  if ((flags & SHOW_FILE) && Filename)
    {
      MyFree(NickServDB);
      NickServDB = MyStrdup(Filename);
    }

  InitData();

  fprintf(stderr, "\n");

  if (flags & SHOW_HELP)
    {
      fprintf(stderr, "\
              Usage: %s [-h|?] [-n] [-f file] [nick1 nick2 ...]\n\
              -h/-?     : Display this help screen\n\
              -n        : Display number of registered nicknames\n\
              -f file   : Use \"file\" as nickname database\n\
              nick1 ... : Display information about specific nicknames\n\
              \n\
              If no nicknames are specified, the entire database will be\n\
              printed. Detailed information will be shown for given nicknames.\n\
              If -n is specified, ONLY the number of registered nicknames will\n\
              be displayed.\n\
              \n",
              av[0]);
      exit(0);
    }

  if (!(flags & SHOW_TOTALCOUNT))
    {
      /*
       * If they gave specific nicknames, display
       * detailed information about them - otherwise
       * display all nicknames and the time they were
       * last seen
       */
      if (flags & SHOW_DETAIL)
        {
          for (ii = ncidx; ii < ac; ii++)
            {
              if ((nptr = FindNick(av[ii])))
                DisplayNick(nptr, 1);
              else
                fprintf(stderr, "No such nickname: %s\n", av[ii]);

              fprintf(stderr, "\n");
            }
        }
      else
        {
          /*
           * Print out all nicknames compactly
           */
          fprintf(stderr,
                  "%-30s %-30s\n",
                  "Nickname",
                  "Time Since Last Seen");
          for (ii = 0; ii < NICKLIST_MAX; ii++)
            {
              for (nptr = nicklist[ii]; nptr; nptr = nptr->next)
                DisplayNick(nptr, 0);
            }
          fprintf(stderr, "\n");
        }
    } /* if (!(flags & SHOW_TOTALCOUNT)) */

  fprintf(stderr, "There are %d registered nicknames\n",
          Network->TotalNicks);
} /* ShowNicknames() */

/*
DisplayNick()
 Display information for realptr. If 'detail' is 1, give
details, otherwise just time last seen
*/

static void
DisplayNick(struct NickInfo *realptr, int detail)

{
  struct NickInfo *nptr;
  char  buf[MAXLINE];
  int cnt;

  if (!(nptr = GetMaster(realptr)))
    return;

  if (detail)
    {
      fprintf(stderr,
              "Nickname:            %s\n",
              realptr->nick);

      fprintf(stderr,
              "Registered:          %s ago\n",
              timeago(realptr->created, 1));

      if (realptr->lastseen)
        fprintf(stderr,
                "Last Seen:           %s ago\n",
                timeago(realptr->lastseen, 1));

      if (LastSeenInfo)
        {
          if (realptr->lastu && realptr->lasth)
            fprintf(stderr,
                    "Last Seen Address:   %s@%s\n",
                    realptr->lastu,
                    realptr->lasth);

          if (realptr->lastqmsg)
            fprintf(stderr,
                    "Last Quit Message:   %s\n",
                    realptr->lastqmsg);
        } /* if (LastSeenInfo) */

      if (realptr->email)
        fprintf(stderr,
                "Email Address:       %s\n",
                realptr->email);

      if (realptr->url)
        fprintf(stderr,
                "Url:                 %s\n",
                realptr->url);

      buf[0] = '\0';
      if (nptr->flags & NS_PROTECTED)
        strcat(buf, "Kill Protection, ");
      if (nptr->flags & NS_NOEXPIRE)
        strcat(buf, "NoExpire, ");
      if (nptr->flags & NS_AUTOMASK)
        strcat(buf, "AutoMask, ");
      if (nptr->flags & NS_PRIVATE)
        strcat(buf, "Private, ");
      if (nptr->flags & NS_FORBID)
        strcat(buf, "Forbidden, ");
      if (nptr->flags & NS_SECURE)
        strcat(buf, "Secure, ");
      if (nptr->flags & NS_UNSECURE)
        strcat(buf, "UnSecure, ");
      if (nptr->flags & NS_HIDEALL)
        strcat(buf, "Hidden, ");
      if (nptr->flags & NS_MEMOS)
        strcat(buf, "AllowMemos, ");
      if (nptr->flags & NS_MEMONOTIFY)
        strcat(buf, "MemoNotify, ");
      if (nptr->flags & NS_MEMOSIGNON)
        strcat(buf, "MemoSignon, ");

      if (*buf)
        {
          buf[strlen(buf) - 2] = '\0';
          fprintf(stderr,
                  "Nickname Options:    %s\n",
                  buf);
        }

#ifdef LINKED_NICKNAMES

      if (nptr->master || nptr->nextlink)
        {
          struct NickInfo *tmp;

          cnt = 0;

          fprintf(stderr, "Linked Nicknames:\n");

          for (tmp = nptr->master ? nptr->master : nptr; tmp; tmp = tmp->nextlink)
            {
              fprintf(stderr, "                     %d) %s\n",
                      ++cnt,
                      tmp->nick);
            }
        }

#endif /* LINKED_NICKNAMES */

#ifdef CHANNELSERVICES

      if (nptr->FounderChannels)
        {
          struct aChannelPtr *tmpchan;

          fprintf(stderr,
                  "Channels Registered:\n");

          cnt = 0;
          for (tmpchan = nptr->FounderChannels; tmpchan; tmpchan = tmpchan->next)
            {
              fprintf(stderr,
                      "                     %d) %s\n",
                      ++cnt,
                      tmpchan->cptr->name);
            }
        }

      if (nptr->AccessChannels)
        {
          struct AccessChannel *acptr;

          fprintf(stderr,
                  "Access Channels:\n");

          cnt = 0;
          for (acptr = nptr->AccessChannels; acptr; acptr = acptr->next)
            {
              fprintf(stderr,
                      "                     %d) (%-3d) %s\n",
                      ++cnt,
                      acptr->accessptr->level,
                      acptr->cptr->name);
            }
        }

#endif /* CHANNELSERVICES */

    } /* if (detail) */
  else
    {
      /*
       * Just display: nickname   time-last-seen
       */
      fprintf(stderr,
              "%-30s %-30s\n",
              realptr->nick,
              timeago(realptr->lastseen, 0));
    }
} /* DisplayNick() */

#ifdef CHANNELSERVICES

/*
ShowChannels()
 Invoked through "showchans" to display channel information
*/

void
ShowChannels(int ac, char *av[])

{
  long flags;
  int ii;
  struct ChanInfo *cptr;

  flags = parseargs(ac, av);

  if ((flags & SHOW_FILE) && Filename)
    {
      MyFree(ChanServDB);
      ChanServDB = MyStrdup(Filename);
    }

  InitData();

  fprintf(stderr, "\n");

  if (flags & SHOW_HELP)
    {
      fprintf(stderr, "\
              Usage: %s [-h|?] [-n] [-f file] [chan1 chan2 ...]\n\
              -h/-?     : Display this help screen\n\
              -n        : Display number of registered channels\n\
              -f file   : Use \"file\" as channel database\n\
              chan1 ... : Display information about specific channels\n\
              \n\
              If no channels are specified, the entire database will be\n\
              printed. Detailed information will be shown for given channels.\n\
              If -n is specified, ONLY the number of registered channels will\n\
              be displayed.\n\
              \n",
              av[0]);
      exit(0);
    }

  if (!(flags & SHOW_TOTALCOUNT))
    {
      /*
       * If they gave specific channels, display
       * detailed information about them - otherwise
       * display all channels and the time they were
       * last used
       */
      if (flags & SHOW_DETAIL)
        {
          for (ii = ncidx; ii < ac; ii++)
            {
              if ((cptr = FindChan(av[ii])))
                DisplayChan(cptr, 1);
              else
                fprintf(stderr, "No such channel: %s\n", av[ii]);

              fprintf(stderr, "\n");
            }
        }
      else
        {
          /*
           * Print out all channels compactly
           */
          fprintf(stderr,
                  "%-30s %-30s\n",
                  "Channel",
                  "Time Since Last Used");
          for (ii = 0; ii < CHANLIST_MAX; ii++)
            {
              for (cptr = chanlist[ii]; cptr; cptr = cptr->next)
                DisplayChan(cptr, 0);
            }

          fprintf(stderr, "\n");
        }
    } /* if (!(flags & SHOW_TOTALCOUNT)) */

  fprintf(stderr, "There are %d registered channels\n",
          Network->TotalChans);
} /* ShowChannels() */

/*
DisplayChan()
 Display information for chanptr. If 'detail' is 1, give
details, otherwise just time last used
*/

static void
DisplayChan(struct ChanInfo *chanptr, int detail)

{
  char buf[MAXLINE];

  if (detail)
    {
      fprintf(stderr,
              "Channel:         %s\n",
              chanptr->name);

      if (chanptr->founder)
        fprintf(stderr,
                "Founder:         %s\n",
                chanptr->founder);

      if (chanptr->successor)
        fprintf(stderr,
                "Successor:       %s\n",
                chanptr->successor);

      fprintf(stderr,
              "Registered:      %s ago\n",
              timeago(chanptr->created, 1));

      fprintf(stderr,
              "Last Used:       %s ago\n",
              timeago(chanptr->lastused, 1));

      if (chanptr->topic)
        fprintf(stderr,
                "Topic:           %s\n",
                chanptr->topic);

      if (chanptr->entrymsg)
        fprintf(stderr,
                "Entry Message:   %s\n",
                chanptr->entrymsg);

      if (chanptr->email)
        fprintf(stderr,
                "Email:           %s\n",
                chanptr->email);

      if (chanptr->url)
        fprintf(stderr,
                "Url:             %s\n",
                chanptr->url);

      buf[0] = '\0';
      if (chanptr->flags & CS_TOPICLOCK)
        strcat(buf, "TopicLock, ");
      if (chanptr->flags & CS_SECURE)
        strcat(buf, "Secure, ");
      if (chanptr->flags & CS_SECUREOPS)
        strcat(buf, "SecureOps, ");
      if (chanptr->flags & CS_GUARD)
        strcat(buf, "ChanGuard, ");
      if (chanptr->flags & CS_RESTRICTED)
        strcat(buf, "Restricted, ");
      if (chanptr->flags & CS_PRIVATE)
        strcat(buf, "Private, ");
      if (chanptr->flags & CS_FORBID)
        strcat(buf, "Forbidden, ");
      if (chanptr->flags & CS_FORGET)
        strcat(buf, "Forgotten, ");
      if (chanptr->flags & CS_NOEXPIRE)
        strcat(buf, "NoExpire, ");
      if (chanptr->flags & CS_SPLITOPS)
        strcat(buf, "SplitOps, ");
#ifdef GECOSBANS
      if ((chanptr->flags & CS_EXPIREBANS) && BanExpire)
        strcat(buf, "Expirebans, ");
#endif

      if (*buf)
        {
          /* kill the trailing "," */
          buf[strlen(buf) - 2] = '\0';
          fprintf(stderr,
                  "Channel Options: %s\n",
                  buf);
        }

      buf[0] = '\0';
      if (chanptr->modes_off)
        {
          strcat(buf, "-");
          if (chanptr->modes_off & MODE_S)
            strcat(buf, "s");
          if (chanptr->modes_off & MODE_P)
            strcat(buf, "p");
          if (chanptr->modes_off & MODE_N)
            strcat(buf, "n");
          if (chanptr->modes_off & MODE_T)
            strcat(buf, "t");
          if (chanptr->modes_off & MODE_M)
            strcat(buf, "m");
#ifdef DANCER
          if (chanptr->modes_off & MODE_C)
            strcat(buf, "c");
#endif /* DANCER */
          if (chanptr->modes_off & MODE_I)
            strcat(buf, "i");
          if (chanptr->modes_off & MODE_L)
            strcat(buf, "l");
          if (chanptr->modes_off & MODE_K)
            strcat(buf, "k");
#ifdef DANCER
          if (chanptr->modes_off & MODE_F)
            strcat(buf, "f");
#endif /* DANCER */
        }

#ifdef DANCER
      if (chanptr->modes_on || chanptr->limit || chanptr->key ||
          chanptr->forward)
#else
      if (chanptr->modes_on || chanptr->limit || chanptr->key)
#endif /* DANCER */
        {
          strcat(buf, "+");
          if (chanptr->modes_on & MODE_S)
            strcat(buf, "s");
          if (chanptr->modes_on & MODE_P)
            strcat(buf, "p");
          if (chanptr->modes_on & MODE_N)
            strcat(buf, "n");
          if (chanptr->modes_on & MODE_T)
            strcat(buf, "t");
#ifdef DANCER
          if (chanptr->modes_on & MODE_C)
            strcat(buf, "c");
#endif /* DANCER */
          if (chanptr->modes_on & MODE_M)
            strcat(buf, "m");
          if (chanptr->modes_on & MODE_I)
            strcat(buf, "i");
          if (chanptr->limit)
            strcat(buf, "l");
          if (chanptr->key)
            strcat(buf, "k");
#ifdef DANCER
          if (chanptr->forward)
            strcat(buf, "f");
#endif /* DANCER */
        }

      if (*buf)
        {
          fprintf(stderr,
                  "Mode Lock:       %s\n",
                  buf);
        }
    } /* if (detail) */
  else
    {
      /*
       * Just display: channel  time-last-used
       */
      fprintf(stderr,
              "%-30s %-30s\n",
              chanptr->name,
              timeago(chanptr->lastused, 0));
    }
} /* DisplayChan() */

#endif /* CHANNELSERVICES */

#endif /* NICKSERVICES */
