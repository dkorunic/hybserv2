/*
 * simple password generator by Nelson Minar (minar@reed.edu)
 * copyright 1991, all rights reserved.
 * You can use this code as long as my name stays with it.
 *
 * $Id$
 */

#include	<stdio.h>

extern char *getpass();
extern char *doencrypt(char *);
extern int rename(const char *oldpath, const char *newpath);

int
main(int argc, char *argv[])

{
	char *plaintext;

	plaintext = getpass("Enter plaintext: ");

	printf("%s\n", doencrypt(plaintext));

	return 0;
}

