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

/*
 * encryptdb.c
 * This program will parse a given database and convert
 * plaintext passwords to their encrypted form. The
 * databases are first saved with a .orig extenstion.
 *
 * Usage: ./encryptdb *.db
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define    MAXLINE    1024

extern char *doencrypt(char *);
extern int rename(const char *oldpath, const char *newpath);

int
main(int argc, char *argv[])

{
	FILE *old, *new;
	char newpath[MAXLINE],
	     line[MAXLINE];
	char *oldpass,
	     *newpass,
	     *ch;
	int ii;

	if (argc < 2)
	{
		fprintf(stderr,
			"Usage: %s <database1> [database2 database3 ...]\n",
			argv[0]);
		return 0;
	}

	for (ii = 1; ii < argc; ii++)
	{
		sprintf(newpath, "%s.orig",
			argv[ii]);

		if (rename(argv[ii], newpath) < 0)
		{
			fprintf(stderr,
				"Unable to rename %s to %s.orig: %s\n",
				argv[ii],
				argv[ii],
				strerror(errno));
			return 0;
		}

		if (!(old = fopen(newpath, "r")))
		{
			fprintf(stderr,
				"Unable to open %s: %s\n",
				newpath,
				strerror(errno));
			return 0;
		}

		if (!(new = fopen(argv[ii], "w")))
		{
			fprintf(stderr,
				"Unable to open %s: %s\n",
				argv[ii],
				strerror(errno));
			return 0;
		}

		while (fgets(line, sizeof(line) - 1, old))
		{
			if (!strncmp(line, "->PASS", 6))
			{
				/*
				 * Ok, we got to a password line - encrypt it
				 * and write to the file
				 */
				oldpass = line + 7;
				if ((ch = strchr(oldpass, '\n')))
					*ch = '\0';
				if ((ch = strchr(oldpass, '\r')))
					*ch = '\0';

				newpass = doencrypt(oldpass);
				fprintf(new, "->PASS %s\n",
					newpass);
			}
			else
			{
				/*
				 * Just print the line to the new file
				 */
				fprintf(new, "%s", line);
			}
		}

		fclose(old);
		fclose(new);
	}

	return 0;
} /* main() */
