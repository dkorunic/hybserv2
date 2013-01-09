/*
 * Hybserv2 Services by Hybserv2 team
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 */

#include "stdinc.h"
#include "config.h"
#include "hybdefs.h"
#include "match.h"
#include "misc.h"
#include "timestr.h"
#include "sprintf_irc.h"
#include "mystring.h"

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
	static char final[TIMELEN];
	char temp[TIMELEN];
	time_t delta;
	long years, weeks, days, hours, minutes, seconds;
	int longfmt;
	int spaces;

	/* reinit the time */
	current_ts = time(NULL);

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

	/* years */
	years = delta / (365 * 86400);
	delta %= (365 * 86400);

	/* weeks */
	weeks = delta / (7 * 86400);
	delta %= (7 * 86400);

	/* days */
	days = delta / 86400;
	delta %= 86400;

	/* hours */
	hours = delta / 3600;
	delta %= 3600;

	/* minutes */
	minutes = delta / 60;
	delta %= 60;

	/* and finally seconds */
	seconds = delta;

	final[0] = '\0';

	if (years)
	{
		if (longfmt)
			ircsprintf(temp, "%ld year%s ", years, (years == 1) ? "" : "s");
		else
		{
			ircsprintf(temp, "%ldy", years);
			if (spaces)
				strlcat(temp, " ", sizeof(temp));
		}
		strlcat(final, temp, sizeof(final));
	}

	if (weeks)
	{
		if (longfmt)
			ircsprintf(temp, "%ld week%s ", weeks, (weeks == 1) ? "" : "s");
		else
		{
			ircsprintf(temp, "%ldw", weeks);
			if (spaces)
				strlcat(temp, " ", sizeof(temp));
		}
		strlcat(final, temp, sizeof(final));
	}

	if (days)
	{
		if (longfmt)
			ircsprintf(temp, "%ld day%s ", days, (days == 1) ? "" : "s");
		else
		{
			ircsprintf(temp, "%ldd", days);
			if (spaces)
				strlcat(temp, " ", sizeof(temp));
		}
		strlcat(final, temp, sizeof(final));
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
				strlcat(final, temp, sizeof(final));
			}
		}
		else
		{
			temp[0] = '\0';
			if (hours)
			{
				ircsprintf(temp, "%ldh", hours);
				if (spaces)
					strlcat(temp, " ", sizeof(temp));
				strlcat(final, temp, sizeof(final));
			}

			if (minutes)
			{
				ircsprintf(temp, "%ldm", minutes);
				if (spaces)
					strlcat(temp, " ", sizeof(temp));
				strlcat(final, temp, sizeof(final));
			}

			if (seconds)
			{
				ircsprintf(temp, "%lds", seconds);
				if (spaces)
					strlcat(temp, " ", sizeof(temp));
				strlcat(final, temp, sizeof(final));
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
					strlcat(temp, " ", sizeof(final));
			}
			strlcat(final, temp, sizeof(final));
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
					strlcat(temp, " ", sizeof(temp));
			}
			strlcat(final, temp, sizeof(final));
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
					strlcat(temp, " ", sizeof(temp));
			}
			strlcat(final, temp, sizeof(final));
		}

		if (!final[0])
		{
			if (longfmt)
				strlcpy(final, "0 seconds", sizeof(final));
			else
				strlcpy(final, "0s", sizeof(final));
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
	char digitbuf[MAXLINE + 1];
	char fmtbuf[MAXLINE + 1];
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
		strlcpy(fmtbuf, format, sizeof(fmtbuf));

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
 * GetTZOffset()
 * Return the GMT offset from localtime (in seconds), calculating the DST
 * offset too
 *
 * Ripped off from Tcpdump with trivial changes.
 */
long GetTZOffset(time_t unixtime)
{
	struct tm *tm_gmt, *tm_loc;
	struct tm s_tm_gmt; /* successive calls get overwriten */
	long dt, dy;

	if (!unixtime)
		unixtime = time(NULL);

	tm_gmt = &s_tm_gmt;
	*tm_gmt = *gmtime(&unixtime);
	tm_loc = localtime(&unixtime);

	/* calculate base seconds offset */
	dt = (tm_loc->tm_hour - tm_gmt->tm_hour) * 3600 +
		(tm_loc->tm_min - tm_gmt->tm_min) * 60 +
		(tm_loc->tm_sec - tm_gmt->tm_sec);

	/* is year or julian day different? */
	dy = tm_loc->tm_year - tm_gmt->tm_year;

	if (!dy)
		dy = tm_loc->tm_yday - tm_gmt->tm_yday;

	dt += dy * 86400;

	return dt;
} /* GetTZOffset() */
