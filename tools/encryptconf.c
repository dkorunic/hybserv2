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
 * encryptconf.c
 * This program will parse a given configuration file and convert
 * plaintext passwords (in O: lines) to their encrypted form. The
 * files are first saved with a .orig extenstion.
 *
 * Usage: ./encryptconf *.conf
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define    MAXLINE    1024

extern int rename(const char *oldpath, const char *newpath);
extern char *doencrypt(char *);

int
main(int argc, char *argv[])

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
  int ii;

  if (argc < 2)
    {
      fprintf(stderr,
              "Usage: %s <conf1> [conf2 conf3 ...]\n",
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
