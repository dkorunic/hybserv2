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
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <assert.h>
#include <time.h>

#include "alloc.h"
#include "client.h"
#include "config.h"
#include "motd.h"
#include "settings.h"
#include "sock.h"
#include "Strn.h"

/*
InitMessageFile()
 Initialize mfile
*/

void
InitMessageFile(struct MessageFile *mfile)

{
  assert(mfile != 0);

  mfile->Contents = NULL;
  mfile->DateLastChanged[0] = '\0';
} /* InitMessageFile() */

/*
ReadMessageFile()
 Read contents of fileptr->filename into a linked list structure
beginning at fileptr->Contents. Old contents of fileptr are erased
prior to reading.

Return: 1 on success
        0 on failure
*/

int
ReadMessageFile(struct MessageFile *fileptr)

{
  struct stat sb;
  struct tm *localtm;
  FILE *fptr;
  char buffer[MESSAGELINELEN + 1];
  char *ch;
  struct MessageFileLine *NewLine,
                         *CurrentLine;

  assert(fileptr != 0);
  assert(fileptr->filename != 0);

  if (stat(fileptr->filename, &sb) < 0)
    return 0; /* file doesn't exist */

  localtm = localtime(&sb.st_mtime);

  if (localtm)
    sprintf(fileptr->DateLastChanged,
      "%d/%d/%d %02d:%02d",
      localtm->tm_mday,
      localtm->tm_mon + 1,
      1900 + localtm->tm_year,
      localtm->tm_hour,
      localtm->tm_min);

  /*
   * Clear out old data
   */
  while (fileptr->Contents)
  {
    CurrentLine = fileptr->Contents->next;
    MyFree(fileptr->Contents);
    fileptr->Contents = CurrentLine;
  }

  if (!(fptr = fopen(fileptr->filename, "r")))
    return 0;

  CurrentLine = NULL;

  while (fgets(buffer, sizeof(buffer) - 1, fptr))
  {
    if ((ch = strchr(buffer, '\n')))
      *ch = '\0';

    NewLine = (struct MessageFileLine *) MyMalloc(sizeof(struct MessageFileLine));
    Strncpy(NewLine->line, buffer, MESSAGELINELEN);
    NewLine->line[MESSAGELINELEN] = '\0';

    /*
     * If it's a blank line, we need to put some kind of data
     * in, or ircd won't send the notice later - two ascii bold
     * characters should work
     */
    if (!NewLine->line[0])
      strcpy(NewLine->line, "\002\002");

    NewLine->next = NULL;

    if (fileptr->Contents)
    {
      if (CurrentLine)
        CurrentLine->next = NewLine;

      CurrentLine = NewLine;
    }
    else
    {
      fileptr->Contents = NewLine;
      CurrentLine = NewLine;
    }
  }

  fclose(fptr);

  return 1;
} /* ReadMessageFile() */

/*
SendMessageFile()
 Send file 'motdptr' to lptr
*/

void
SendMessageFile(struct Luser *lptr, struct MessageFile *motdptr)

{
  struct MessageFileLine *lineptr;

  assert(lptr && motdptr);

  for (lineptr = motdptr->Contents; lineptr; lineptr = lineptr->next)
  {
    toserv(":%s NOTICE %s :%s\n",
      n_Global,
      lptr->nick,
      lineptr->line);
  }
} /* SendMessageFile() */
