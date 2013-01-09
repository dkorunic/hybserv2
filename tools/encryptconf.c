/*
 * Hybserv2 Services by Hybserv2 team
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 */

/*
 * encryptconf.c
 * This program will parse a given configuration file and convert
 * plaintext passwords (in O: lines) to their encrypted form. The
 * files are first saved with a .orig extenstion.
 *
 * Usage: ./encryptconf *.conf
 */

#include "stdinc.h"

#define    MAXLINE    1024

#define    FLAG_MD5   0x00000001
#define    FLAG_DES   0x00000002

extern int rename(const char *, const char *);
extern char *doencrypt(char *);
extern char *doencrypt_md5(char *);
void usage(void);

int main(int argc, char *argv[])
{
	FILE *old, *new;
	char newpath[MAXLINE],
	line[MAXLINE];
	char *oldpass,
	*newpass,
	*ch;
	char *userhost,
	*nick,
	*flags;
	int ii = 1;
	int c;
	int flag = 0;

	if (argc < 2)
		usage();

	while ( (c=getopt(argc, argv, "h?d:m:")) != -1)
	{
		switch(c)
		{
		case 'm':
			flag |= FLAG_MD5;
			ii = 2;
			break;
		case 'd':
			flag |= FLAG_DES;
			ii = 2;
			break;
		case 'h':
		case '?':
			usage();
			break;
		default:
			printf("Invalid Option: -%c\n", c);
			break;
		}
	}

	for (; ii < argc; ii++)
	{
		snprintf(newpath, sizeof(newpath), "%s.orig",
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
			if ((*line == 'o') || (*line == 'O'))
			{
				/*
				 * Ok, we got to an O: line - encrypt it and write
				 * to the file
				 */
				userhost = strtok(line + 2, ":");
				oldpass = strtok(NULL, ":");
				nick = strtok(NULL, ":");
				flags = strtok(NULL, ":");

				if (!userhost || !oldpass || !nick || !flags)
					continue;

				if ((ch = strchr(flags, '\n')))
					*ch = '\0';
				if ((ch = strchr(flags, '\r')))
					*ch = '\0';

				printf("oldpass = [%s]\n", oldpass);

				/*
				 * Encryption type: DES (default) or MD5
				 */
				if (flag & FLAG_MD5)
					newpass = doencrypt_md5(oldpass);
				else
					newpass = doencrypt(oldpass);
				fprintf(new, "O:%s:%s:%s:%s\n",
				        userhost,
				        newpass,
				        nick,
				        flags);
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


void usage ()

{
	printf("Usage: encryptconf [-m|-d] <conf1> [conf2 conf3 ...]\n");
	printf("         -m Use MD5 encyrption\n");
	printf("         -d Use DES encyrption (default)\n");
	printf("Example: encryptconf -m test\n");
	exit(0);
}
