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
#include <sys/types.h>
#include <string.h>
#include <time.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif

#include "config.h"
#include "hybdefs.h"
#include "match.h"
#include "misc.h"
#include "timestr.h"
#include "sprintf_irc.h"

/*
 * timeago()
 *
 * Uses the TS "timestamp" to make a corresponding time string to the long
 * format:
 * X years X weeks X days (Xh Xm Xs)
 * or the short format:
 * Xy Xw Xd Xh Xm Xs
 * depending on the flag parameter.
 *
 * Values for "flag":
 * 0 - display (presenttime - timestamp) in short format
 * 1 - display (presenttime - timestamp) in long format
 * 2 - display (timestamp) in short format
 * 3 - display (timestamp) in long format
 * 4 - display (timestamp) in short format but with no spaces
 */
char *timeago(time_t timestamp, int flag)
{
  static char final[50];
  char temp[50];
  time_t delta;
  long years, weeks, days, hours, minutes, seconds;
  int longfmt;
  int spaces;

  /* put spaces in the string? */
  spaces = 1;

  switch (flag)
    {
    case 0:
      {
        delta = current_ts - timestamp;
        longfmt = 0;
        break;
      }

    case 1:
      {
        delta = current_ts - timestamp;
        longfmt = 1;
        break;
      }

    case 2:
      {
        delta = timestamp;
        longfmt = 0;
        break;
      }

    case 3:
      {
        delta = timestamp;
        longfmt = 1;
        break;
      }

    case 4:
      {
        delta = timestamp;
        longfmt = 0;
        spaces = 0;
        break;
      }

    default:
      {
        /* shouldn't happen */
        return NULL;
      }
    } /* switch (flag) */

  years = weeks = days = hours = minutes = seconds = 0;
  years = (delta / 31536000);
  weeks = (delta / 604800) % 52;
  days = (delta / 86400) % 7;
  hours = (delta / 3600) % 24;
  minutes = (delta / 60) % 60;
  seconds = (delta % 60);

  final[0] = '\0';
  if (years)
    {
      if (longfmt)
        ircsprintf(temp, "%ld year%s ", years, (years == 1) ? "" : "s");
      else
        {
          ircsprintf(temp, "%ldy", years);
          if (spaces)
            strcat(temp, " ");
        }
      strcat(final, temp);
    }

  if (weeks)
    {
      if (longfmt)
        ircsprintf(temp, "%ld week%s ", weeks, (weeks == 1) ? "" : "s");
      else
        {
          ircsprintf(temp, "%ldw", weeks);
          if (spaces)
            strcat(temp, " ");
        }
      strcat(final, temp);
    }

  if (days)
    {
      if (longfmt)
        ircsprintf(temp, "%ld day%s ", days, (days == 1) ? "" : "s");
      else
        {
          ircsprintf(temp, "%ldd", days);
          if (spaces)
            strcat(temp, " ");
        }
      strcat(final, temp);
    }

  if (years || weeks || days)
    {
      if (longfmt)
        {
          /*
           * We don't want to have a string like: 1 week (0h 0m 0s) so make
           * sure at least one of these values is non-zero
           */
          if (hours || minutes || seconds)
            {
              ircsprintf(temp, "(%ldh %ldm %lds)", hours, minutes, seconds);
              strcat(final, temp);
            }
        }
      else
        {
          temp[0] = '\0';
          if (hours)
            {
              ircsprintf(temp, "%ldh", hours);
              if (spaces)
                strcat(temp, " ");
              strcat(final, temp);
            }

          if (minutes)
            {
              ircsprintf(temp, "%ldm", minutes);
              if (spaces)
                strcat(temp, " ");
              strcat(final, temp);
            }

          if (seconds)
            {
              ircsprintf(temp, "%lds", seconds);
              if (spaces)
                strcat(temp, " ");
              strcat(final, temp);
            }

          /*
           * kill the trailing space
           */
          if (spaces && final[0])
            final[strlen(final) - 1] = '\0';
        } /* else (!longfmt) */
    } /* if (years || weeks || days) */
  else
    {
      /* its uptime is less than a day */
      if (hours)
        {
          if (longfmt)
            ircsprintf(temp,
                       "%ld hour%s ",
                       hours,
                       (hours == 1) ? "" : "s");
          else
            {
              ircsprintf(temp, "%ldh", hours);
              if (spaces)
                strcat(temp, " ");
            }
          strcat(final, temp);
        }

      if (minutes)
        {
          if (longfmt)
            ircsprintf(temp, "%ld minute%s ",
                       minutes, (minutes == 1) ? "" : "s");
          else
            {
              ircsprintf(temp, "%ldm", minutes);
              if (spaces)
                strcat(temp, " ");
            }
          strcat(final, temp);
        }

      if (seconds)
        {
          if (longfmt)
            ircsprintf(temp, "%ld second%s ",
                       seconds, (seconds == 1) ? "" : "s");
          else
            {
              ircsprintf(temp, "%lds", seconds);
              if (spaces)
                strcat(temp, " ");
            }
          strcat(final, temp);
        }

      if (!final[0])
        {
          if (longfmt)
            strcpy(final, "0 seconds");
          else
            strcpy(final, "0s");
        }
      else if (spaces)
        {
          /* kill the trailing space (" ") on the end */
          final[strlen(final) - 1] = '\0';
        }
    }

  return (final);
} /* timeago() */

/*
 * timestr()
 *
 * This function is similar to timeago(), but it takes an arguement
 * string of the format "Xw Xd Xh Xm Xs", and converts it to seconds.
 */
long timestr(char *format)
{
  long seconds;
  char digitbuf[MAXLINE];
  char fmtbuf[MAXLINE];
  long bufvalue; /* integer value of number stored in digitbuf[] */
  char *ptr;
  int cnt;

  if (!format)
    return (0);

  seconds = 0;

  if (IsNum(format))
    {
      /*
       * format is a number, meaning it doesn't have a 'w', 'd', 'h',
       * 'm', or 's' in it - assume it is in seconds so we don't
       * break the below loop
       */
      ircsprintf(fmtbuf, "%ss", format);
    }
  else
    strcpy(fmtbuf, format);

  cnt = 0;
  for (ptr = fmtbuf; *ptr; ptr++)
    {
      while (IsSpace(*ptr))
        ptr++;

      if (IsDigit(*ptr))
        digitbuf[cnt++] = *ptr;
      else
        {
          /*
           * We reached a non-digit character - increment seconds variable
           * accordingly:
           *  w - week   (604800 seconds)
           *  d - day    (86400 seconds)
           *  h - hour   (3600 seconds)
           *  m - minute (60 seconds)
           *  s - second
           */

          digitbuf[cnt] = '\0';
          bufvalue = atol(digitbuf);
          cnt = 0;

          switch (*ptr)
            {
            case 'W': /* weeks */
            case 'w':
              {
                seconds += (bufvalue * 604800);
                break;
              } /* case 'W' */
            case 'D': /* days */
            case 'd':
              {
                seconds += (bufvalue * 86400);
                break;
              } /* case 'D' */
            case 'H': /* hours */
            case 'h':
              {
                seconds += (bufvalue * 3600);
                break;
              } /* case 'H' */
            case 'M': /* minutes */
            case 'm':
              {
                seconds += (bufvalue * 60);
                break;
              } /* case 'M' */
            case 'S': /* seconds */
            case 's':
              {
                seconds += bufvalue;
                break;
              } /* case 'S' */

            default:
              break;
            } /* switch (*ptr) */
        }
    } /* for (ptr = format; *ptr; ptr++) */

  return (seconds);
} /* timestr() */

/*
 * GetTime()
 *
 * Attempt to get the current time in seconds/useconds using
 * gettimeofday(). If that fails, get it in seconds/0.
 */
struct timeval *GetTime(struct timeval *timer)
  {
    if (!timer)
      return (NULL);

#ifdef HAVE_GETTIMEOFDAY

    gettimeofday(timer, NULL);
    return (timer);

#else

    timer->tv_sec = current_ts;
    timer->tv_usec = 0;
    return (timer);

#endif /* HAVE_GETTIMEOFDAY */
  } /* GetTime() */

/*
 * GetGMTOffset()
 * Return the GMT offset from localtime (in seconds)
 */
long GetGMTOffset(time_t unixtime)
{
#if 0
  struct tm *gmt,
        *lt;
  long days,
  hours,
  minutes,
  seconds;
#endif

  struct tm *tm_gmt;

  tm_gmt = gmtime(&TimeStarted);
  return (TimeStarted - mktime(tm_gmt));

#if 0

  gmt = gmtime(&unixtime);
  lt = localtime(&unixtime);

  days = (long) (lt->tm_yday - gmt->tm_yday);
  if (days < (-1))
    hours = 24;
  else if (days > 1)
    hours = (-24);
  else
    hours = (days * 24) + (long) (lt->tm_hour - gmt->tm_hour);

  minutes = (hours * 60) + (long) (lt->tm_min - gmt->tm_min);
  seconds = (minutes * 60) + (long) (lt->tm_sec - gmt->tm_sec);

  return (seconds);
#endif
} /* GetGMTOffset() */
