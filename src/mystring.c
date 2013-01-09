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

		while (IsSpace(*buf))
			++buf;

		if (*buf == '\0')
			return x;
	}
	while (x < MAXPARAM - 1);

	(*array)[x++] = p;
	(*array)[x] = NULL;

	return x;
} /* SplitBuf() */
