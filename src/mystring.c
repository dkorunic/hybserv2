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
  int ii = 0;
  static char retstr[MAXLINE + 1];

  if (str == NULL)
    return NULL;

  for (; ii < strlen(str); ++ii)
    retstr[ii] = ToUpper(str[ii]);

  retstr[ii] = '\0';

  return retstr;
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
  int ii = 0;
  static char retstr[MAXLINE + 1];

  if (str == NULL)
    return NULL;

  for (; ii < strlen(str); ++ii)
    retstr[ii] = ToLower(str[ii]);

  retstr[ii] = '\0';

  return retstr;
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
  int ii = 0, bw = 0;

  final = MyMalloc(MAXLINE + 1);
  final[0] = '\0';

  for (; ii < ac; ++ii)
  {
    strlcat(final, av[ii], MAXLINE + 1);
    bw = strlcat(final, " ", MAXLINE + 1);
  }
  if (bw)
    final[bw - 1] = '\0';

  return final;
} /* GetString() */

/*
 * SplitBuf()
 *
 * Take string "buff" and insert NULLs in the spaces between words. Keep
 * pointers to the beginning of each word, and store them in "array".
 * Returns the number of words in "buff"
 */
int SplitBuf(char *string, char ***array)
{
  int x = 0;
  int len;
  char *p, *buf = string;

  *array = NULL;

  if ((buf == NULL) || (*buf == '\0'))
    return 0;

  *array = MyMalloc(sizeof(char *) * (MAXPARAM + 1));

  len = strlen(string);
  if (IsEOL(string[len - 1]))
    string[len - 1] = '\0';

  (*array)[x] = NULL;

  /* contained only spaces */
  while (IsSpace(*buf))
    ++buf;

  if (*buf == '\0')
    return x;

  do
  {
    (*array)[x++] = buf;
    (*array)[x] = NULL;
    if ((p = strchr(buf, ' ')) != NULL)
    {
      if (*(p + 1) == ':')
      {
        *p = '\0';
        (*array)[x++] = p + 1;
        (*array)[x] = NULL;
        return x;
      }

      *p++ = '\0';
      buf = p;
    }
    else
      return x;

    while (*buf == ' ')
      ++buf;

    if (*buf == '\0')
      return x;
  } while (x < MAXPARAM - 1);

  (*array)[x++] = p;
  (*array)[x] = NULL;
  
  return x;
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
