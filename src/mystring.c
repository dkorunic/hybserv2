/*
 * Hybserv2 Services by Hybserv2 team
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
#include <stdarg.h>
#include <string.h>

#include "alloc.h"
#include "config.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "sprintf_irc.h"

/*
 * StrToupper()
 *
 * args: char *str
 * purpose: convert the string 'str' to uppercase
 * return: the uppercase string
*/
char *StrToupper(char *str)
{
  unsigned int ii;
  static char retstr[MAXLINE];

  if (!str)
    return (NULL);

  retstr[0] = '\0';
  for (ii = 0; ii < strlen(str); ++ii)
    retstr[ii] = ToUpper(str[ii]);

  retstr[ii] = '\0';

  return (retstr);
} /* StrToupper() */

/*
 * StrTolower()
 *
 * args: char *str
 * purpose: convert the string 'str' to lowercase
 * return: the lowercase string
 */
char *StrTolower(char *str)
{
  unsigned int ii;
  static char retstr[MAXLINE];

  if (!str)
    return (NULL);

  retstr[0] = '\0';
  for (ii = 0; ii < strlen(str); ++ii)
    retstr[ii] = ToLower(str[ii]);

  retstr[ii] = '\0';

  return (retstr);
} /* StrTolower() */

/*
 * GetString()
 *
 * Reverse the array av back into a normal string assuming there are 'ac'
 * indices in the array. Space is allocated for the new string.
 */
char *GetString(int ac, char **av)
{
  char *final;
  char temp[MAXLINE];
  int ii;

  final = (char *) MyMalloc(sizeof(char));
  *final = '\0';

  ii = 0;
  while (ii < ac)
    {
      ircsprintf(temp, "%s%s", av[ii], ((ii + 1) >= ac) ? "" : " ");

      final = (char *) MyRealloc(final,
                                 strlen(final) + strlen(temp) + sizeof(char));
      strcat(final, temp);

      ++ii;
    }

  /* Routine to get rid of spaces at the end of new string. However, we
   * don't need it atm -kre */
#if 0
  ii = strlen(final) - 1;
  while (final[ii] == ' ' && ii)
    final[ii--] = 0;
#endif

  return (final);
} /* GetString() */

/*
 * SplitBuf()
 *
 * Take string "buff" and insert NULLs in the spaces between words. Keep
 * pointers to the beginning of each word, and store them in "array".
 * Returns the number of words in "buff"
 */
int SplitBuf(char *buff, char ***array)
{
  int argsize = 8;
  int acnt, ii;
  char *temp1, *tempbuf;

  /* Be safe. If something down fails, it will point
   * to NULL anyway -kre */
  *array = NULL;

  /* Perform this check -kre */
  if (buff == NULL)
    return 0;

  tempbuf = buff;

  ii = strlen(tempbuf);

  /* No reason to parse this at all -kre */
  if (!ii)
    return 0;

  /* try to kill trailing \r or \n chars */
  if (IsEOL(tempbuf[ii - 1]))
    tempbuf[ii - 1] = '\0';

  /*
   * When hybrid sends channel bans during a netjoin, it leaves
   * a preceding space (not sure why) - just make sure there
   * are no preceding spaces
   *
   * Note that Hybrid7 sends also spaces after SJOIN's, like in
   * <fl_> :irc.vchan SJOIN 978405679 #ircd-coders +sptna : @Diane @dia
   * -kre
   */
  while (IsSpace(*tempbuf))
    ++tempbuf;

  /* Check if tempbuf contained only but spaces -kre */
  if (!*tempbuf)
    return 0;

  *array = (char **) MyMalloc(sizeof(char *) * argsize);
  memset(*array, 0, sizeof(char *) * argsize);
  acnt = 0;
  while (*tempbuf)
    {
      if (acnt == argsize)
        {
          argsize += 8;
          *array = (char **) MyRealloc(*array, sizeof(char *) * argsize);
        }

      if ((*tempbuf == ':') && (acnt != 0))
        {
          (*array)[acnt++] = tempbuf;
          /* (*array)[acnt++] = tempbuf + 1; */
          tempbuf = "";
        }
      else
        {
          /* Why use strpbrk() on only 1 character? We have faster strchr for
           * that. -kre */
          /* temp1 = strpbrk(tempbuf, " "); */
          temp1 = strchr(tempbuf, ' ');
          if (temp1)
            {
              *temp1++ = 0;
              while (IsSpace(*temp1))
                ++temp1;
            }
          else
            temp1 = tempbuf + strlen(tempbuf);

          (*array)[acnt++] = tempbuf;
          tempbuf = temp1;
        }
    }

  return (acnt);
} /* SplitBuf() */
