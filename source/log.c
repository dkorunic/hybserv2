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
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif

#include "config.h"
#include "dcc.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "operserv.h"
#include "settings.h"
#include "sprintf_irc.h"
#include "sprintf_irc.h"

/*
putlog()
  args: int level, char *fmt, va_alist
  purpose: log 'fmt' to log file
  return: none
*/

void
putlog(int level, char *format, ...)

{
  FILE *fp;
  time_t CurrTime;
  char buf[MAXLINE];
  va_list args;

  /*
   * Do not log anything if LogLevel equals 0 - also,
   * if the given level is greater than LogLevel, do not
   * log it
   */
  if ((LogLevel == 0) || (LogLevel < level))
    return;

  if (!LogFile)
    return;

  if ((fp = fopen(LogFile, "a+")) == NULL)
    {
#ifdef DEBUGMODE
      printf("Unable to open log file: %s\n", LogFile);
#endif

      return;
    }

  CurrTime = current_ts;
  strcpy(buf, ctime(&CurrTime));

  /*
   * Erase the \n character that ctime() puts on the end, in order
   * to concat the string we want to log
   */
  buf[strlen(buf) - 1] = ' ';

  fprintf(fp, "%s", buf);

  va_start(args, format);

  /* write the string to the log file */
  vfprintf(fp, format, args);
  fprintf(fp, "\n");

#ifdef DEBUGMODE

  fprintf(stderr, "Logged> ");
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
#endif

  fclose(fp);

  va_end(args);
} /* putlog() */

/*
CheckLogs()
 Called at midnight each day to rename the current log file
to: LogFile.YYYYMMDD, and also check if we have more than
MaxLogs log files. If so, erase the oldest one, to make
room for the newest one.
*/

void
CheckLogs(time_t unixtime)

{
  char tmplog[MAXLINE],
  olddate[MAXLINE];
  char *currdate;
  struct tm *log_tm;
  time_t oldts;
  DIR *dp;
  struct dirent *dirp;
  int lmatches,
  len;

  if (MaxLogs)
    {
      /*
       * We must now check if there are MaxLogs log files
       * in HPath/. If so, delete the oldest one to make
       * room for the current one.
       */

      if (!(dp = opendir(HPath)))
        {
          putlog(LOG1, "Error reading log directory: %s",
                 strerror(errno));
          return;
        }

      ircsprintf(tmplog, "%s.", LogFile);
      len = strlen(tmplog);

      lmatches = 0;
      olddate[0] = '\0';
      currdate = NULL;

      /*
       * Go through all the files in the directory and
       * pick out the ones that match "LogFile."
       */
      while ((dirp = readdir(dp)))
        {
          if (!ircncmp(dirp->d_name, tmplog, len))
            {
              ++lmatches;

              /*
               * Now check the date on the log file to see
               * if its the oldest.
               */
              if (!*olddate)
                strcpy(olddate, dirp->d_name + len);
              else
                {
                  currdate = dirp->d_name + len;
                  if (atol(olddate) > atol(currdate))
                    strcpy(olddate, currdate);
                }
            }
        }

      if ((lmatches >= MaxLogs) && *olddate)
        {
          /*
           * There are too many log files in the directory,
           * delete the oldest one - it will be: LogFile.olddate
           */
          ircsprintf(tmplog, "%s/%s.%s", HPath, LogFile, olddate);
          unlink(tmplog);
        }

      closedir(dp);
    } /* if (MaxLogs) */

  /*
   * Now rename the current log file. Use the TS of one
   * second ago since this log file is actually yesterday's,
   * because it just turned midnight.
   */

  oldts = unixtime - 1;
  log_tm = localtime(&oldts);
  ircsprintf(tmplog, "%s.%d%02d%02d",
             LogFile, 1900 + log_tm->tm_year, log_tm->tm_mon + 1, log_tm->tm_mday);

  rename(LogFile, tmplog);
} /* CheckLogs() */

/*
RecordCommand()
 Called when someone executes a NickServ/ChanServ/MemoServ/StatServ
command - log it and send it to opers with usermode +s
*/

void
RecordCommand(char *format, ...)

{
  va_list args;
  char buffer[MAXLINE * 2];

  va_start(args, format);

  vsprintf_irc(buffer, format, args);

  va_end(args);

  /* log the command */
  stripformatsymbols( buffer );
  putlog(LOG2, buffer);

  /* send it to opers with usermode +s */
  SendUmode(OPERUMODE_S, buffer);
} /* RecordCommand() */
