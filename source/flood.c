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

#include "channel.h"
#include "chanserv.h"
#include "client.h"
#include "config.h"
#include "flood.h"
#include "log.h"
#include "misc.h"
#include "operserv.h"
#include "settings.h"
#include <time.h>

/*
 * Certain functions, like SendUmode() could possibly
 * be called very rapidly for certain events like netjoins/splits
 * where +d users would be notified of every client/server quit -
 * we don't want a +d user to be flooded with a couple thousand
 * messages, so keep FloodTable[] to record how many times
 * these functions were called in the last few seconds.
 * FloodTable[0] will be set to the TS when one of these
 * functions is first called. After that, FloodTable[1] will
 * be used. FloodHits gets incremented everytime the function
 * is called. IsFlood() will determine whether the function
 * is being called too much in too little time.
 */
time_t FloodTable[2] = { 0, 0 };
int FloodHits = 0;
int ActiveFlood = 0; /* is a flood currently taking place? */

/*
FloodCheck()
  Called after a *Serv is deoped or kicked from a channel to stop a
possible flood
  kick = 1 if its a kick, 0 if its a deop
*/

int
FloodCheck(struct Channel *chptr, struct Luser *lptr,
           struct Luser *servptr, int kick)

{
  int normal = 1; /* rejoin normally? */
  int ret = 0;

  if (!chptr || !servptr || !lptr)
    return 0;

  ++chptr->floodcnt;
  if (!chptr->flood_ts[0])
    chptr->flood_ts[0] = current_ts;
  else
  {
    chptr->flood_ts[1] = current_ts;
    if (chptr->floodcnt == 5)
    {
      chptr->floodcnt = 0;
      if ((chptr->flood_ts[1] - chptr->flood_ts[0]) <= 20)
      {
        /*
         * they kicked/deoped a *Serv 5+ times in 20 seconds, join
         * with TS - 1, to stop getting kicked/deoped
         */
        normal = 0;
        ret = 1;
      }
      else
      {
        chptr->floodcnt = 1;
        chptr->flood_ts[0] = current_ts;
      }
    } /* if (chptr->floodcnt == 5) */
    else if ((chptr->flood_ts[1] - chptr->flood_ts[0]) > 20)
    {
      /*
       * 20 seconds has passed since their last kick/deop,
       * reset everything, and rejoin normally
       */
      chptr->floodcnt = 1;
      chptr->flood_ts[0] = current_ts;
    }
  } /* else (if (!chptr->flood_ts[0])) */

  if (servptr == Me.osptr)
  {
    /* 
     * only rejoin the channel if it is a monitored channel -
     * if it tries to rejoin after being kicked/deoped from a
     * .omode, it may cause a join flood because of
     * someone's lame script, and it won't even set the
     * modes after it gets in - leaving a ghost
     */
    if (IsChannel(chptr->name))
    {
      if (normal)
      {
        /* don't rejoin after a deop - UpdateChanModes() handles that */
        if (kick)
          os_join(chptr);
      }
      else
      {
        if (!kick)
          os_part(chptr);
        os_join_ts_minus_1(chptr);

        putlog(LOG2,
          "%s: %s flood from %s!%s@%s on %s",
          n_OperServ,
          kick ? "kick" : "deop",
          lptr->nick,
          lptr->username,
          lptr->hostname,
          chptr->name);
      }
    }
  }

#if defined (NICKSERVICES) && defined(CHANNELSERVICES)

  else
  {
    struct ChanInfo *chanptr;

    chanptr = FindChan(chptr->name);
    if ((servptr == Me.csptr) && chanptr)
    {
      if (normal)
      {
        if (kick)
          cs_join(chanptr);
      }
      else
      {
        if (!kick)
          cs_part(chptr);
        cs_join_ts_minus_1(chanptr);
        putlog(LOG2,
          "%s: %s flood from %s!%s@%s on %s",
          n_ChanServ,
          kick ? "kick" : "deop",
          lptr->nick,
          lptr->username,
          lptr->hostname,
          chptr->name);
      }
    }
  } /* else */

#endif

  return (ret);
} /* FloodCheck() */

/*
IsFlood()
 Determines whether a flood is occuring in a certain function
such as SendUmode(). Everytime one of these functions is called,
they call IsFlood() which will increment FloodHits, and update
FloodTable[].  If FloodTable[1] - FloodTable[0] <= BCFloodTime,
and FloodHits == BCFloodCount, one or more of the functions was
called BCFloodCount times in less than BCFloodTime seconds -
assume a flood
 Return 1 if flood detected, 0 if not
*/

int
IsFlood()

{
  time_t currtime;

  if (!BCFloodCount || !BCFloodTime)
    return (0);

  currtime = current_ts;

  ++FloodHits;

  if (!FloodTable[0])
  {
    /*
     * It is the first time a flood-checking function was called -
     * set FloodTable[0] to the current TS, and return
     */
    FloodTable[0] = currtime;
    return (0);
  }

  /*
   * FloodTable[0] will contain the TS of the very first call
   * to a flood-checking function. FloodTable[1] will contain
   * the TS of the most recent call to a flood-checking function.
   * FloodHits will represent the total number of times the
   * function(s) were called. If FloodHits is greater or equal
   * to BCFloodCount, check the difference of FloodTable[0] and
   * FloodTable[1].
   * The difference of the two will be the amount of time that
   * has passed since the first and last call to the function(s).
   * If this time difference is less than BCFloodTime seconds, we
   * have a flood (BCFloodCount+ calls in less than BCFloodTime seconds)
   */

  FloodTable[1] = currtime;

  if (FloodHits >= BCFloodCount)
  {
    if ((FloodTable[1] - FloodTable[0]) <= BCFloodTime)
    {
      FloodTable[0] = currtime;
      return (1);
    }
    else
    {
      /*
       * The flood-checking function(s) were called >= BCFloodCount
       * times, but in an overall reasonable amount of time, so reset
       * our flood variables
       */
      FloodTable[0] = currtime;
      FloodHits = 1;
    }
  }
  else if ((FloodTable[1] - FloodTable[0]) > BCFloodTime)
  {
    /*
     * The function(s) haven't been called BCFloodCount times,
     * but already there is more than a BCFloodTime second
     * difference between the first and most recent call, so
     * any further calls will only increase the gap - reset
     * the variables
     */
    FloodTable[0] = 0;
    FloodHits = 0;

  }

  /*
   * The function(s) haven't been called BCFloodCount times yet,
   * or they have been, but in a time period greater than BCFloodTime
   * seconds - no need to worry
   */
  return (0);
} /* IsFlood() */
