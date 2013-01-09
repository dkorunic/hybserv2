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

static char saltChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

extern int rename(const char *oldpath, const char *newpath);
extern char *crypt(const char *, const char *);

#ifdef HAVE_SOLARIS
extern char *crypt_md5(const char *, const char *);
#endif

/*
doencrypt()
 Encrypt a plaintext string. Returns a pointer to encrypted
string
*/

char *
doencrypt(char *plaintext)

{
	char salt[3];

	srandom(time(0)); /* may not be the BEST salt, but its close */
	salt[0] = saltChars[random() % 64];
	salt[1] = saltChars[random() % 64];
	salt[2] = 0;

	return (crypt(plaintext, salt));
} /* doencrypt() */

/*
doencrypt_md5()
 MD5 encryption of plaintext string. Returns a pointer to 
 encrypted string.
 
 Taken from Hybrid7 mkpasswd tool.  -bane
*/

char *
doencrypt_md5(char *plaintext)

{
	char salt[13];
	int i;

	salt[0] = '$';
	salt[1] = '1';
	salt[2] = '$';
	for (i = 3; i <= 10; i++)
		salt[i] = saltChars[random() % 64];
	salt[11] = '$';
	salt[12] = '\0';

#ifdef HAVE_SOLARIS

	return (crypt_md5(plaintext, salt));
#else

	return (crypt(plaintext, salt));
#endif
} /* doencrypt_md5() */
