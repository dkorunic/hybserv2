#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

static char saltChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

extern int rename(const char *oldpath, const char *newpath);
extern char *crypt();

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
