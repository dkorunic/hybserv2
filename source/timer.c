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

#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#ifndef HAVE_CYGWIN
#include <signal.h>
#else
#include <sys/signal.h>
#endif /* HAVE_CYGWIN */

#include "chanserv.h"
#include "config.h"
#include "data.h"
#include "dcc.h"
#include "flood.h"
#include "gline.h"
#include "hybdefs.h"
#include "init.h"
#include "log.h"
#include "memoserv.h"
#include "nickserv.h"
#include "operserv.h"
#include "settings.h"
#include "sock.h"
#include "statserv.h"
#include "timer.h"
#include "jupe.h"

#ifdef HAVE_SOLARIS_THREADS
#include <thread.h>
#include <synch.h>
#else
#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif
#endif

/* Global timer that will reduce local usage of time() -kre */
time_t current_ts;

#if defined HAVE_PTHREADS || defined HAVE_SOLARIS_THREADS

/*
 * 1 if ok to call DoTimer()
 */
int               GoodTimer = 1;

/*
 * p_CheckSignals()
 *
 * This thread will continually wait for various signals, and call
 * ProcessSignal() appropriately
*/

void *p_CheckSignals()
{
  sigset_t set;
  int caught;

  sigemptyset(&set);
  /*  sigaddset(&set, SIGINT); */
  sigaddset(&set, SIGHUP);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGCHLD);
  sigaddset(&set, SIGPIPE);

#ifdef HAVE_SOLARIS_THREADS

  thr_sigsetmask(SIG_BLOCK, &set, NULL);
#else

  pthread_sigmask(SIG_BLOCK, &set, NULL);
#endif

  while (1)
    {
#ifdef HAVE_SOLARIS_THREADS
      caught = sigwait(&set);
#else

      sigwait(&set, &caught); /* wait until we get a signal */
#endif

      ProcessSignal(caught); /* we got a signal, process it */
    }

  return (NULL);
} /* p_CheckSignals() */

/*
 * p_CheckTime()
 *
 * This function will be called from a new thread to enter an infinite
 * loop and call DoTimer() once every second
*/
void *p_CheckTime()
{
  current_ts = time(NULL);

  while (1)
    {
      sleep(1);
      if (GoodTimer)
        DoTimer(current_ts);

      /*
       * Every five minutes, reset current_ts to time() in case a DoTimer()
       * call took over a second, in which case current_ts will be a little
       * off
       */
      if ((current_ts % 300) == 0)
        current_ts = time(NULL);
      else
        current_ts++;
    }

  return (NULL);
} /* p_CheckTime() */

#endif /* HAVE_PTHREADS */

/*
 * DoTimer()
 *
 * Execute various commands every second - such as checking for expired
 * temp glines etc.
 */
void DoTimer(time_t unixtime)
{
#ifdef HIGHTRAFFIC_MODE

  if (!HTM)
    {
      float currload;

      /*
       * Calculate the current K/s the hub is sending us - if it
       * exceeds ReceiveLoad, switch to High Traffic Mode
       */
      currload = ((float) (Network->RecvB - Network->CheckRecvB) /
                  (float) 1024) / (float) HTM_INTERVAL;

      if (currload >= (float) ReceiveLoad)
        {
          HTM = 1;
          HTM_ts = unixtime;
          putlog(LOG1, "Entering high-traffic mode (%0.2f K/s)",
                 currload);
          SendUmode(OPERUMODE_Y,
                    "*** Entering high-traffic mode (%0.2f K/s)",
                    currload);
          return;
        }
    }

  if ((unixtime % HTM_INTERVAL) == 0)
    {
      if (!Network->LastRecvB)
        Network->LastRecvB = Network->RecvB;
      else
        {
          Network->CheckRecvB = Network->LastRecvB;
          Network->LastRecvB = Network->RecvB;
        }
    }

  /*
   * While we are in high-traffic mode, do not execute any
   * of the regular timer functions
   */
  if (HTM)
    return;

#endif /* HIGHTRAFFIC_MODE */

  if ((HubSock == NOSOCKET) && ((unixtime % ConnectFrequency) == 0))
    CycleServers();

  if (BindAttemptFreq && ((unixtime % BindAttemptFreq) == 0))
    DoBinds();

  /* exec this every 5 seconds */
  if ((unixtime % 5) == 0)
    {
      /*
       * Check if any dcc sockets have recently EOF'd - if so,
       * terminate the connection
       */
      CheckEOF();

      /*
       * Check if any ident requests have exceeded IdentTimeout
       * seconds
       */
      ExpireIdent(unixtime);

      /*
       * Check if any telnet clients haven't entered their password
       * within TelnetTimeout seconds
       */
      ExpireTelnet(unixtime);

      /*
       * Check for expired ignores
       */
      ExpireIgnores(unixtime);
    } /* if ((unixtime % 5) == 0) */

  if ((unixtime % 30) == 0)
    {
#ifdef NICKSERVICES

      /*
       * Check if there are any nick stealers to kill
       */
      CollisionCheck(unixtime);

#endif /* NICKSERVICES */

    } /* if ((unixtime % 30) == 0) */

  if ((unixtime % 60) == 0)
    {
#ifdef ALLOW_GLINES

      /*
       * Check for expired temp. glines every 1 minute
       */
      ExpireGlines(unixtime);

#endif

#ifdef JUPEVOTES
      /* expire server jupe votes */
      ExpireVotes(unixtime);
#endif

#if defined AUTO_ROUTING && defined SPLIT_INFO
      /* Check for splitted servers and optionally reconnect them -kre */
      ReconnectCheck(unixtime);
#endif

    } /* if ((unixtime % 60) == 0) */

#if !defined(HYBRID_ONLY) && defined(NICKSERVICES) && defined(CHANNELSERVICES)

  if (InhabitTimeout && ((unixtime % InhabitTimeout) == 0))
    {
      /*
       * Check for any empty channels that ChanServ is monitoring -
       * part the channel if ChanServ is the only one in there
       */
      CheckEmptyChans();
    }

#endif /* !defined(HYBRID_ONLY) && defined(NICKSERVICES) && defined(CHANNELSERVICES) */

  if ((unixtime % 3600) == 0)
    {
      /*
       * Check for expired nicknames, channels, and memos every hour
       */

#ifdef NICKSERVICES

      ExpireNicknames(unixtime);

#ifdef CHANNELSERVICES

      ExpireChannels(unixtime);
#endif

#ifdef MEMOSERVICES

      ExpireMemos(unixtime);
#endif

#endif /* NICKSERVICES */

#ifdef STATSERVICES
      /*
       * Check for any HostHash entries which have had 0 clients
       * for the past StatExpire days
       */
      ExpireStats(unixtime);
#endif

    } /* if ((unixtime % 3600) == 0) */

#if defined(NICKSERVICES) && defined(MEMOSERVICES)

  if (MemoPurgeFreq && ((unixtime % MemoPurgeFreq) == 0))
    {
      SendUmode(OPERUMODE_D,
                "%s: Purging memos marked for deletion",
                n_MemoServ);

      /*
       * Go through memo list and delete any marked for
       * deletion
       */
      PurgeCheck();
    }

#endif /* defined(NICKSERVICES) && defined(MEMOSERVICES) */

  if (AutoLinkFreq && (unixtime % AutoLinkFreq) == 0)
    {
      /*
       * Attempt to link all tcm bots who are not already linked
       */
      LinkBots();
    } /* if (AutoLinkFreq && (unixtime % AutoLinkFreq) == 0) */

  /*
   * we have to add gmt_offset here or it won't be true at
   * midnight unless we are in the GMT timezone - for instance,
   * if we are in the EST timezone, we would be 5 hours earlier
   * than GMT, and thus ((unixtime % 86400) == 0) would only
   * be true at 19:00, so if we add gmt_offset (which would
   * be negative) to unixtime (at midnight), the value would
   * be the TS of 19:00, which would return true
   */
  if (((unixtime + gmt_offset) % 86400) == 0)
    {
#ifdef STATSERVICES
      /*
       * every day at midnight reset the daily max
       * user/oper/server/channel count
       */
      Network->MaxUsersT = Network->TotalUsers;
      Network->MaxOperatorsT = Network->TotalOperators;
      Network->MaxServersT = Network->TotalServers;
      Network->MaxChannelsT = Network->TotalChannels;

      Network->MaxUsersT_ts = unixtime;
      Network->MaxOperatorsT_ts = unixtime;
      Network->MaxServersT_ts = unixtime;
      Network->MaxChannelsT_ts = unixtime;

      Network->OperKillsT = 0;
      Network->ServKillsT = 0;

      putlog(LOG2, "%s: Reset daily counts",
             n_StatServ);
#endif /* STATSERVICES */

      /*
       * every day at midnight backup the log file
       * to the format: logfile.YYYYMMDD and start
       * a new one.
       */
      CheckLogs(unixtime);
    } /* if (((unixtime + gmt_offset) % 86400) == 0) */

  if (BackupFreq && (((unixtime + gmt_offset) % BackupFreq) == 0))
    {
      /*
       * Backup database files
       */
      SendUmode(OPERUMODE_Y,
                "*** Backing up databases");

      BackupDatabases(unixtime);
    }

  if ((unixtime % DatabaseSync) == 0)
    {
      SendUmode(OPERUMODE_D,
                "*** Saving databases");

      WriteDatabases();
    } /* if ((unixtime % DatabaseSync) == 0) */

  /*
   * If SendUmode() detected a "function-calling" flood,
   * check if 20 seconds have passed; if so, remove flood
   * protection
   */
  if (ActiveFlood && ((unixtime - ActiveFlood) % 20 == 0))
    {
      ActiveFlood = 0;
      SendUmode(OPERUMODE_Y,
                "*** Flood time limit expired, broadcasting resumed");
    }

#ifdef STATSERVICES

  /*
   * Every minute ping all servers to update lag information
   */
  if (LagDetect && ((unixtime % 60) == 0))
    {
      DoPings();
    }

#endif /* STATSERVICES */

  if (!SafeConnect && currenthub)
    {
      /*
       * currenthub->connect_ts + ConnectBurst is the TS
       * of when we can assume we are safely connected to
       * the network - that is, there is no more initial
       * connect burst information coming. So, we can now
       * broadcast clone/client connections to +y users
       * without fearing they will get flooded with hundreds
       * of them from an initial burst.
       * Make sure currenthub->realname is not null, so we
       * can tell if we are actually connected to a hub :-)
       */
      if (currenthub->realname &&
          (unixtime >= (currenthub->connect_ts + ConnectBurst)))
        SafeConnect = 1;
    } /* if (!SafeConnect && currenthub) */
} /* DoTimer() */
