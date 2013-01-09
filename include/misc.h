/*
 * misc.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_misc_h
#define INCLUDED_misc_h

#include "stdinc.h"
#include "config.h"

#define   NODCC     (-1)
#define   DCCALL    (-2)
#define   DCCOPS    (-3)

struct Luser;

struct Command
{
	char *cmd;       /* holds command */
	void (*func)(); /* corresponding function */

	/* LVL_NONE if anyone can execute it
	 * LVL_IDENT if nick needs to have IDENTIFY'd before using 'cmd'
	 * LVL_ADMIN if nick needs to match an admin line */
	int level;
};

struct aService
{
	char **name;         /* nickname of service bot */
	char **ident;        /* ident of service bot */
	char **desc;         /* description of service bot */
	struct Luser **lptr; /* pointer to service bot */
};

void debug(char *, ...);
void fatal(int, char *, ...);
void notice(char *, char *, char *, ...);
void DoShutdown(char *, char *);
struct Command *GetCommand(struct Command *, char *);
int pwmatch(char *, char *);
int operpwmatch(char *, char *);
struct Luser *GetService(char *);
struct Luser *FindService(struct Luser *);
int IsInNickArray(int, char **, char *);
int IsNum(char *);
char *HostToMask(char *, char *);
char *Substitute(char *, char *, int);
char* stripformatsymbols(char *);
int checkforproc(char *);

#ifdef CRYPT_PASSWORDS
char *hybcrypt(char *, char *);
char *make_des_salt(void);
char *make_md5_salt(void);
char *make_md5_salt_oldpasswd(char *);
#endif /* CRYPT_PASSWORDS */

extern struct aService ServiceBots[];
#ifdef CRYPT_PASSWORDS
extern int UseMD5;
#endif /* CRYPT_PASSWORDS */

#endif /* INCLUDED_misc_h */
