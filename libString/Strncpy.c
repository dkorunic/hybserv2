/*
 * libString, Copyright (C) 1999 Patrick Alken
 * This library comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this library is distributed.
 *
 * $Id$
 */

#include <sys/types.h>

/*
Strncpy()
 Optimized version of strncpy().

Inputs: dest   - destination string
        source - source string
        bytes  - number of bytes to copy

NOTE: A terminating \0 character is only copied to 'dest' if
      'source' is terminated by one, provided the limit 'bytes'
      has not yet been reached.

Return: destination string
*/

char *
Strncpy(char *dest, const char *source, const size_t bytes)

{
  register char *end = dest + bytes;
  register char *s = dest;

  while ((s < end) && (*s++ = *source++))
    ;

  return (dest);
} /* Strncpy() */
