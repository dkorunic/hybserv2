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
#include "dcc.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "operserv.h"
#include "settings.h"
#include "sprintf_irc.h"
#include "sprintf_irc.h"
#include "mystring.h"
#include "alloc.h"

FILE *logfp = NULL;

int OpenLogFile(void)
{
	/* check if log is already open */
	if (logfp == NULL)
	{
		char LogFileName[MAXLINE + 1];

		/* generate a proper name */
		if (LogFile == NULL)
			LogFile = MyStrdup("hybserv.log");

		if (LogPath == NULL)
			LogPath = MyStrdup(".");

		ircsprintf(LogFileName, "%s/%s", LogPath, LogFile);

		if ((logfp = fopen(LogFileName, "a+")) == NULL)
		{
#ifdef DEBUGMODE
			printf("Unable to open log file: %s\n", LogFile);
#endif
			return 0;
		}
		else
		{
			/* set line buffering */
			setvbuf(logfp, NULL, _IOLBF, 0);

			putlog(LOG1, "Opened log file successfully");
		}
	}

	return 1;
}

void CloseLogFile(void)
{
	if (logfp != NULL)
	{
		putlog(LOG1, "Closing log file");
		fclose(logfp);
		logfp = NULL;
	}

	return;
}

/*
putlog()
  args: int level, char *fmt, va_alist
  purpose: log 'fmt' to log file
  return: none
*/

void
putlog(int level, char *format, ...)

{
	time_t CurrTime;
	char buf[MAXLINE + 1];
	va_list args;

	/*
	 * Do not log anything if LogLevel equals 0 - also,
	 * if the given level is greater than LogLevel, do not
	 * log it
	 */
	if ((LogLevel == 0) || (LogLevel < level))
		return;

	if (logfp == NULL)
	{
		if (!OpenLogFile())
			return;
	}

	CurrTime = current_ts;
	strlcpy(buf, ctime(&CurrTime), sizeof(buf));

	/*
	 * Erase the \n character that ctime() puts on the end, in order
	 * to concat the string we want to log
	 */
	buf[strlen(buf) - 1] = ' ';

	fprintf(logfp, "%s", buf);

	va_start(args, format);

	/* write the string to the log file */
	vfprintf(logfp, format, args);
	fprintf(logfp, "\n");

#ifdef DEBUGMODE
	fprintf(stderr, "Logged> ");
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
#endif

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
	char tmplog[MAXLINE + 1];
	char LogFileName[MAXLINE + 1];
	char olddate[MAXLINE + 1];
	char *currdate;
	struct tm *log_tm;
	time_t oldts;
	DIR *dp;
	struct dirent *dirp;
	int lmatches, len, ret;

	/* infinite logging means we won't rotate logs at all because of user
	 * safety: think of gazillions of files in a single directory */
	if (!MaxLogs)
		return;

	/*
	 * We must now check if there are MaxLogs log files
	 * in HPath/. If so, delete the oldest one to make
	 * room for the current one.
	 */
	if (!(dp = opendir(LogPath)))
	{
		putlog(LOG1, "Error reading log directory: %s",
			   strerror(errno));
		return;
	}

	ircsprintf(LogFileName, "%s/%s", LogPath, LogFile);
	ircsprintf(tmplog, "%s.", LogFileName);
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
			if (!olddate[0])
				strlcpy(olddate, dirp->d_name + len, sizeof(olddate));
			else
			{
				currdate = dirp->d_name + len;
				if (atol(olddate) > atol(currdate))
					strlcpy(olddate, currdate, sizeof(olddate));
			}
		}
	}

	if ((lmatches >= MaxLogs) && *olddate)
	{
		/*
		 * There are too many log files in the directory,
		 * delete the oldest one - it will be: LogFile.olddate
		 */
		ircsprintf(tmplog, "%s/%s.%s", LogPath, LogFile,
				   olddate);
		unlink(tmplog);
	}

	closedir(dp);

	/*
	 * Now rename the current log file. Use the TS of one
	 * second ago since this log file is actually yesterday's,
	 * because it just turned midnight.
	 *
	 * Woo. Ugly :-)
	 */

	oldts = unixtime - 1;
	log_tm = localtime(&oldts);
	ircsprintf(tmplog, "%s/%s.%d%02d%02d",
			   LogPath, LogFile, 1900 + log_tm->tm_year, log_tm->tm_mon +
			   1, log_tm->tm_mday);

	ret = rename(LogFileName, tmplog);
	if (ret == -1)
	{
		putlog(LOG1, "Error renaming log file %s to %s: %s",
			   LogFileName, tmplog, strerror(errno));
		return;
	}


	CloseLogFile();
	OpenLogFile();
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
	putlog(LOG2, "%s", buffer);

	/* send it to opers with usermode +s */
	SendUmode(OPERUMODE_S, "%s", buffer);
} /* RecordCommand() */
