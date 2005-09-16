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

#include "stdinc.h"
#include "alloc.h"
#include "config.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "mystring.h"
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

  final = MyMalloc(MAXLINE);
  final[0] = '\0';

  ii = 0;
  while (ii < ac)
    {
      ircsprintf(temp, "%s%s", av[ii], ((ii + 1) >= ac) ? "" : " ");
      strlcat(final, temp, MAXLINE);
      ++ii;
    }

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

/*
 * strlcat and strlcpy were ripped from openssh 2.5.1p2
 * They had the following Copyright info: 
 *
 *
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright    
 *    notice, this list of conditions and the following disclaimer in the  
 *    documentation and/or other materials provided with the distribution. 
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED `AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t siz)
{
  char *d = dst;
  const char *s = src;
  size_t n = siz, dlen;

  while (n-- != 0 && *d != '\0')
    d++;

  dlen = d - dst;
  n    = siz - dlen;

  if (n == 0)
    return(dlen + strlen(s));

  while (*s != '\0')
  {
    if (n != 1)
    {
      *d++ = *s;
      n--;
    }

    s++;
  }

  *d = '\0';
  return dlen + (s - src); /* count does not include NUL */
}
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t siz)
{
  char *d = dst;
  const char *s = src;
  size_t n = siz;

  /* Copy as many bytes as will fit */
  if (n != 0 && --n != 0)
  {
    do
    {
      if ((*d++ = *s++) == 0)
        break;
    } while (--n != 0);
  }

  /* Not enough room in dst, add NUL and traverse rest of src */
  if (n == 0)
  {
    if (siz != 0)
      *d = '\0'; /* NUL-terminate dst */
    while (*s++)
      ;
  }

  return s - src - 1; /* count does not include NUL */
}
#endif
