/*
 * misc.h
 * Copyright (C) 1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_misc_h
#define INCLUDED_misc_h

struct Luser;
struct Userlist;

#define   NODCC     (-1)
#define   DCCALL    (-2)
#define   DCCOPS    (-3)

struct Command
{
  char *cmd;       /* holds command */
  void (* func)(); /* corresponding function */

  /*
   * LVL_NONE if anyone can execute it
   * LVL_IDENT if nick needs to have IDENTIFY'd before using 'cmd'
   * LVL_ADMIN if nick needs to match an admin line
   */
  int level;
};

struct aService
{
  char **name;         /* nickname of service bot */
  char **ident;        /* ident of service bot */
  char **desc;         /* description of service bot */
  struct Luser **lptr; /* pointer to service bot */
};

/*
 * Prototypes
 */

void debug(char *format, ...);
void fatal(int keepgoing, char *format, ...);
void notice(char *source, char *dest, char *format, ...);

void DoShutdown(char *who, char *reason);

struct Command *GetCommand(struct Command *, char *);
int pwmatch(char *, char *);
int operpwmatch(char *, char *);
struct Luser *GetService(char *);
struct Luser *FindService(struct Luser *);
int IsInNickArray(int, char **, char *);
int IsNum(char *);
char *HostToMask(char *, char *);

char *Substitute(char *, char *, int);

char* stripctrlsymbols( char * source );
char* stripformatsymbols( char * source );
int checkforproc( char* source );

#ifdef CRYPT_PASSWORDS
char *hybcrypt(char *, char *);
char *make_des_salt(void);
char *make_md5_salt(void);
char *make_md5_salt_oldpasswd(char *);
#endif /* CRYPT_PASSWORDS */

/*
 * External declarations
 */

extern  struct aService ServiceBots[];

#ifdef CRYPT_PASSWORDS
extern int UseMD5;
#endif /* CRYPT_PASSWORDS */

#endif /* INCLUDED_misc_h */
