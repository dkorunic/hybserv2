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
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#ifndef HAVE_CYGWIN
#include <signal.h>
#else
#include <sys/signal.h>
#include <signal.h>
#endif /* HAVE_CYGWIN */

#include "alloc.h"
#include "chanserv.h"
#include "client.h"
#include "conf.h"
#include "config.h"
#include "data.h"
#include "dcc.h"
#include "flood.h"
#include "hash.h"
#include "hybdefs.h"
#include "log.h"
#include "memoserv.h"
#include "misc.h"
#include "mystring.h"
#include "nickserv.h"
#include "operserv.h"
#include "settings.h"
#include "sock.h"
#include "timer.h"
#include "sprintf_irc.h"

#ifdef HAVE_SOLARIS_THREADS
#include <thread.h>
#include <synch.h>
#else
#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif
#endif

static struct Luser *introduce(char *, char *, char *);

/*
ProcessSignal()
  args: int sig
  purpose: process system signal specified by 'sig'
  return: none
*/

void
ProcessSignal(int sig)

{
#if !defined HAVE_SOLARIS_THREADS && !defined HAVE_PTHREADS
  InitSignals();
#endif
  
  switch (sig)
    {
      /* rehash configuration file */
    case SIGHUP:
      {
        SendUmode(OPERUMODE_Y,
                  "*** Received SIGHUP, rehashing configuration file and databases");
        putlog(LOG1,
               "Received signal SIGHUP, rehashing configuration file and databases");

        Rehash();

        if (ReloadDbsOnHup)
          ReloadData();

        signal(SIGHUP, ProcessSignal); /* reset the signal */
        break;
      }

#if 0
      /* restart services */
    case SIGINT:
      {
#ifdef DEBUGMODE
        DoShutdown("console", "Recieved SIGINT");
        exit(1);
#else

        SendUmode(OPERUMODE_Y,
                  "*** Received SIGINT, restarting services");
        putlog(LOG1, "Received signal SIGINT, restarting");
        ServReboot();
        signal(SIGINT, ProcessSignal);
#endif

        break;
      }
#endif

    case SIGPIPE:
      {
        putlog(LOG1, "Received signal SIGPIPE, ignoring");
        break;
      }

      /*
       * this is required to prevent a child process from becoming a
       * zombie which just sits out there taking up fds
       */
    case SIGCHLD:
      {
        wait(NULL);
        signal(SIGCHLD, ProcessSignal);
        break;
      }

      /* something really died */
    case SIGTERM:
      {
        putlog(LOG1, "Received SIGTERM, shutting down");
        SendUmode(OPERUMODE_Y,
                  "*** Received SIGTERM, shutting down");
        DoShutdown(NULL, "SIGTERM Received");
      }
    }
} /* ProcessSignal() */

/*
InitListenPorts()
  args: none
  purpose: initialize ports for listening
  return: none
*/

void
InitListenPorts()

{
  struct PortInfo *pptr;

  for (pptr = PortList; pptr; pptr = pptr->next)
    DoListen(pptr);
} /* InitListenPorts() */

/*
InitLists()
  args: none
  purpose: initiate linked lists
  return: none
*/

void
InitLists()

{
  memset((void *) &Me, 0, sizeof(Me));

#ifdef NICKSERVICES

  memset((void *) &nicklist, 0, sizeof(nicklist));

#ifdef CHANNELSERVICES

  memset((void *) &chanlist, 0, sizeof(chanlist));
#endif

#ifdef MEMOSERVICES

  memset((void *) &memolist, 0, sizeof(memolist));
#endif

#endif /* NICKSERVICES */

  /* List for network stuff - oper/user/chan count etc */
  Network = (struct NetworkInfo *) MyMalloc(sizeof(struct NetworkInfo));

  memset((void *) Network, 0, sizeof(struct NetworkInfo));

  /*
   * Create a generic oper structure for any oper who doesn't
   * have an O: line in hybserv.conf
   */
  GenericOper = (struct Userlist *) MyMalloc(sizeof(struct Userlist));
  GenericOper->nick = MyStrdup("");
  GenericOper->username = MyStrdup("");
  GenericOper->hostname = MyStrdup("");
  GenericOper->password = MyStrdup("");
  GenericOper->flags = (PRIV_OPER | PRIV_JUPE | PRIV_GLINE);

#ifdef RECORD_SPLIT_TS

  GenericOper->split_ts = GenericOper->whensplit = 0;
#endif

} /* InitLists() */

/*
InitSignals()
 Initialize signal hooks
*/

void
InitSignals()

{
#ifdef HAVE_SOLARIS_THREADS
  thread_t signalid;

  thr_create(NULL, 0, p_CheckSignals, NULL, THR_DETACHED, &signalid);

#else
#ifdef HAVE_PTHREADS

  pthread_attr_t attr;
  pthread_t signalid;

  /* pthreads and signal() do not go well together */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&signalid, &attr, p_CheckSignals, NULL);

#else

  /* setup signal hooks */
  signal(SIGHUP, ProcessSignal);
  signal(SIGTERM, ProcessSignal);
  /*  signal(SIGINT, ProcessSignal); */
  signal(SIGCHLD, ProcessSignal);
  signal(SIGPIPE, ProcessSignal);

#endif /* HAVE_PTHREADS */
#endif

} /* InitSignals() */

/*
PostCleanup()
  Called after we are disconnected from a hub server, to
reset some linked lists
*/

void
PostCleanup()

{
  Me.sptr = NULL;
  Me.hub = NULL;

  Me.osptr = NULL;

#ifdef NICKSERVICES

  Me.nsptr = NULL;

#ifdef CHANNELSERVICES

  Me.csptr = NULL;
#endif

#ifdef MEMOSERVICES

  Me.msptr = NULL;
#endif

#endif /* NICKSERVICES */

#ifdef STATSERVICES

  Me.ssptr = NULL;
#endif

#ifdef HELPSERVICES

  Me.hsptr = NULL;
#endif

#ifdef SEENSERVICES

  Me.esptr = NULL;
#endif

} /* PostCleanup() */

/*
introduce()
  Introduce 'nick' to the network with 'info'
*/

static struct Luser *
      introduce(char *nick, char *ident, char *info)

  {
    char sendstr[MAXLINE];
    time_t CurrTime = current_ts;
    char **av;
    struct Luser *lptr;

    ircsprintf(sendstr, "NICK %s 1 %ld %s %s %s %s :%s\n", nick, (long)
               CurrTime, ServiceUmodes, ident, Me.name, Me.name, info);
    toserv(sendstr);

    SplitBuf(sendstr, &av);
    lptr = AddClient(av); /* Add 'nick' to user list */
    MyFree(av);

    return (lptr);
  } /* introduce() */

/*
InitServs()
  args: struct Luser *servptr
  purpose: introduce *SERV_NICKs to the network after a successful
           connection - if servptr specified, introduce only
           that service
  return: none
*/

void InitServs(struct Luser *servptr)
{
  struct aService *sptr;

  if (servptr)
    {
      /*
       * A service nick was killed, determine which one it was and
       * re-introduce them. Now, the service will have been removed from the
       * luser linked list already if it was a kill. However, s_kill() will
       * have called GetService(), which returns a pointer to Me.*sptr,
       * depending on which *Serv was killed. Therefore, 'servptr' will
       * still correctly point to a Me.*sptr, even though it really points
       * to garbage. So it is still safe to compare servptr to Me.*sptr's.
       */

      for (sptr = ServiceBots; sptr->name; ++sptr)
        {
          if (servptr == *(sptr->lptr))
            {
              *(sptr->lptr) = introduce(*(sptr->name), *(sptr->ident),
                                        *(sptr->desc));
              return; /* no need to keep searching */
            }
        }

      return;
    }

  /*
   * Services probably just connected to the network,
   * introduce all service nicks
   */

  for (sptr = ServiceBots; sptr->name; ++sptr)
    {
      *(sptr->lptr) = introduce(*(sptr->name), *(sptr->ident),
                                *(sptr->desc));
    }
} /* InitServs() */
