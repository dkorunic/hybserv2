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
 * encryptdb.c
 * This program will parse a given database and convert
 * plaintext passwords to their encrypted form. The
 * databases are first saved with a .orig extenstion.
 *
 * Usage: ./encryptdb *.db
 */

#include "stdinc.h"

#define    MAXLINE    1024

#define    FLAG_MD5   0x00000001
#define    FLAG_DES   0x00000002


extern char *doencrypt(char *);
extern char *doencrypt_md5(char *);
extern int rename(const char *, const char *);
void usage(void);

int main(int argc, char *argv[])
{
	FILE *old, *new;
	char newpath[MAXLINE],
	line[MAXLINE];
	char *oldpass,
	*newpass,
	*ch;
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

				/*
				 * Encryption type: DES (default) or MD5
				 */
				if (flag & FLAG_MD5)
					newpass = doencrypt_md5(oldpass);
				else
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


void usage ()
{
	printf("Usage: encryptdb [-m|-d] <database1> "
	       "[database2 database3 ...]\n");
	printf("         -m Use MD5 encyrption\n");
	printf("         -d Use DES encyrption (default)\n");
	printf("Example: encryptdb -m test\n");
	exit(0);
}
