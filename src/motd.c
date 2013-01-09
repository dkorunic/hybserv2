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
#include "alloc.h"
#include "client.h"
#include "config.h"
#include "motd.h"
#include "settings.h"
#include "sock.h"
#include "sprintf_irc.h"
#include "misc.h"
#include "mystring.h"

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
	char *final;
	struct MessageFileLine *NewLine,
				*CurrentLine;

	assert(fileptr != 0);
	assert(fileptr->filename != 0);

	/* XXX: TOCTOU bug ahoy */
	if (stat(fileptr->filename, &sb) < 0)
		return 0; /* file doesn't exist */

	localtm = localtime(&sb.st_mtime);

	if (localtm)
		ircsprintf(fileptr->DateLastChanged, "%d/%d/%d %02d:%02d",
		           localtm->tm_mday, localtm->tm_mon + 1, 1900 + localtm->tm_year,
		           localtm->tm_hour, localtm->tm_min);

	/*
	 * Clear out old data
	 */
	while (fileptr->Contents != NULL)
	{
		CurrentLine = fileptr->Contents->next;
		MyFree(fileptr->Contents->line);
		MyFree(fileptr->Contents);
		fileptr->Contents = CurrentLine;
	}

	if ((fptr = fopen(fileptr->filename, "r")) == NULL)
		return 0;

	CurrentLine = NULL;

	while (fgets(buffer, sizeof(buffer), fptr))
	{
		if ((ch = strchr(buffer, '\n')) != NULL)
			*ch = '\0';

		NewLine = MyMalloc(sizeof(struct MessageFileLine));

		if (*buffer != '\0')
		{
			final = Substitute(NULL, buffer, NODCC);
			if (final && (final != (char *) -1))
				NewLine->line = final;
			else
				NewLine->line = MyStrdup(" ");
		}
		else
			NewLine->line = MyStrdup(" ");

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
void SendMessageFile(struct Luser *lptr, struct MessageFile *motdptr)
{
	struct MessageFileLine *lineptr;

	assert(lptr && motdptr);

	if (motdptr->Contents == NULL)
		return;

	for (lineptr = motdptr->Contents; lineptr != NULL; lineptr =
	            lineptr->next)
		notice(n_Global, lptr->nick, "%s", lineptr->line);
} /* SendMessageFile() */
