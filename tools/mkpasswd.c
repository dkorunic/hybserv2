/* simple password generator by Nelson Minar (minar@reed.edu)
 * copyright 1991, all rights reserved.
 * You can use this code as long as my name stays with it.
 *
 * md5 patch by Walter Campbell <wcampbel@botbay.net>
 * Modernization, getopt, etc for the Hybrid IRCD team
 *
 * $Id$
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define FLAG_MD5     0x00000001
#define FLAG_DES     0x00000002
#define FLAG_SALT    0x00000004
#define FLAG_PASS    0x00000008

extern char *getpass();
extern char *crypt();


char *make_des_salt();
char *make_md5_salt();
char *make_md5_salt_para(char *);
void usage();
static char saltChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

int main(argc, argv)
int argc;
char *argv[];
{
  char *plaintext = NULL;
  extern char *optarg;
  extern int optind;
  int c;
  char *saltpara = NULL;
  char *salt;
  int flag = 0;

  srandom(time(NULL));

  while( (c=getopt(argc, argv, "mdhs:p:")) != -1)
  {
    switch(c)
    {
      case 'm':
        flag |= FLAG_MD5;
        break;
      case 'd':
        flag |= FLAG_DES;
        break;
      case 's':
        flag |= FLAG_SALT;
        saltpara = optarg;
        break;
      case 'p':
        flag |= FLAG_PASS;
        plaintext = optarg;
        break;
      case 'h':
        usage();
        break;
      default:
        break;
    }
  }

  if (flag & FLAG_MD5) {
    if (flag & FLAG_SALT)
      salt = make_md5_salt_para(saltpara);
    else
      salt = make_md5_salt();
  } else {
    if (flag & FLAG_SALT) {
      if ((strlen(saltpara) == 2)) {
        salt = saltpara;
      } else {
        printf("Invalid salt, please enter 2 alphanumeric characters\n");
        exit(1);
      }
    } else {
      salt = make_des_salt();
    }
  }

  if (flag & FLAG_PASS) {
    if (!plaintext)
      printf("Please enter a valid password\n");
  } else {
    plaintext = getpass("plaintext: ");
  }

  printf("%s\n", crypt(plaintext, salt));
  return 0;
}

char *make_des_salt()
{
  static char salt[3];
  char* saltptr=salt;
  salt[0] = saltChars[random() % 64];
  salt[1] = saltChars[random() % 64];
  salt[2] = '\0';
  return saltptr;
}

char *make_md5_salt_para(char *saltpara)
{
  static char salt[21];
  char* saltptr=salt;
  if (saltpara && (strlen(saltpara) <= 16)) {
    sprintf(salt, "$1$%s$", saltpara);
    return saltptr;
  }
  printf("Invalid Salt, please use up to 16 random alphanumeric characters\n");
  exit(1);
}
  
char *make_md5_salt()
{
  static char salt[13];
  int i;
  char* saltptr=salt;
  salt[0] = '$';
  salt[1] = '1';
  salt[2] = '$';
  for (i=3; i<=10; i++)
    salt[i] = saltChars[random() % 64];
  salt[11] = '$';
  salt[12] = '\0';
  return saltptr;
}

void usage()
{
  printf("mkpasswd [-m|-d] [-s salt] [-p plaintext]\n");
  printf("-m Generate an MD5 password\n");
  printf("-d Generate a DES password\n");
  printf("-s Specify a salt, 2 alphanumeric characters for DES, up to 16 for MD5\n");
  printf("-p Specify a plaintext password to use\n");
  printf("Example: mkpasswd -m -s 3dr -p test\n");
  exit(0);
}
