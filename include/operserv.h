/*
 * operserv.h
 * Copyright (C) 1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_operserv_h
#define INCLUDED_operserv_h

#ifndef INCLUDED_conf_h
#include "conf.h"        /* PRIV_* */
#define INCLUDED_conf_h
#endif

struct Luser;
struct Channel;

/* OperServ usermodes */
#define OPERUMODE_INIT  (1 << 0)  /* umodes not initialized yet */
#define OPERUMODE_B     (1 << 1)  /* +b */
#define OPERUMODE_C     (1 << 2)  /* +c */
#define OPERUMODE_CAPC  (1 << 3)  /* +C */
#define OPERUMODE_E     (1 << 4)  /* +e */
#define OPERUMODE_CAPE  (1 << 5)  /* +E */
#define OPERUMODE_J     (1 << 6)  /* +j */
#define OPERUMODE_K     (1 << 7)  /* +k */
#define OPERUMODE_L     (1 << 8)  /* +l */
#define OPERUMODE_M     (1 << 9)  /* +m */
#define OPERUMODE_N     (1 << 10) /* +n */
#define OPERUMODE_O     (1 << 11) /* +o */
#define OPERUMODE_P     (1 << 12) /* +p */
#define OPERUMODE_CAPO  (1 << 13) /* +O */
#define OPERUMODE_S     (1 << 14) /* +s */
#define OPERUMODE_CAPS  (1 << 15) /* +S */
#define OPERUMODE_T     (1 << 16) /* +t */
#define OPERUMODE_Y     (1 << 17) /* +y */
#define OPERUMODE_D     (1 << 18) /* +d */

/* OperServ Flag macros */

#define IsServicesAdmin(x) ((x) ? (x)->flags & PRIV_SADMIN : 0)
#define  IsAdmin(x)         ((x) ? (x)->flags & PRIV_ADMIN : 0)
#define  IsOper(x)          ((x) ? (x)->flags & PRIV_OPER : 0)
#define  IsProtected(x)     ((x) ? (x)->flags & PRIV_EXCEPTION : 0)
#define  IsFriend(x)        ((x) ? (x)->flags & PRIV_FRIEND : 0)
#define  CanJupe(x)         ((x) ? (x)->flags & PRIV_JUPE : 0)
#define  CanGline(x)        ((x) ? (x)->flags & PRIV_GLINE : 0)
#define  CanChat(x)         ((x) ? (x)->flags & PRIV_CHAT : 0)

#define ONEKIL  (1024.0)
#define ONEMEG  (1024.0 * 1024.0)
#define ONEGIG  (1024.0 * 1024.0 * 1024.0)
#define ONETER  (1024.0 * 1024.0 * 1024.0 * 1024.0)
#define GMKBs(x)  ( ((x) > ONETER) ? "Terabytes" : (((x) > ONEGIG) ? "Gigabytes" : (((x) > ONEMEG) ? "Megabytes" : (((x) > ONEKIL) ? "Kilobytes" : "Bytes"))))
#define  GMKBv(x)  ( ((x) > ONETER) ? (float)((x) / ONETER) : (((x) > ONEGIG) ? (float)((x) / ONEGIG) : (((x) > ONEMEG) ? (float)((x) / ONEMEG) : (((x) > ONEKIL) ? (float)((x) / ONEKIL) : (float)(x)))))

struct OperCommand
{
  char *cmd; /* holds command */
  void (* func)(); /* function to call depending on 'cmd' */
  int dcconly; /* dcc chat command only? 1 if yes, 0 if not */
  char flag; /* flag needed to use the command */
};

struct Ignore
{
  struct Ignore *next, *prev;
  time_t expire;   /* 0 if permanent ignore */
  char *hostmask;  /* hostmask to ignore */
};

/*
 * Information for fuckover processes
 */
struct Process
{
  struct Process *next;
  pid_t pid;      /* process-id */
  char *who;      /* who started it */
  char *target;   /* target of fuckover */
};

/*
 * Prototypes
 */

void os_process(char *nick, char *command, int sockfd);
void os_join(struct Channel *chptr);
void os_join_ts_minus_1(struct Channel *chptr);
void os_part(struct Channel *chptr);
void CheckFuckoverTarget(struct Luser *fptr, char *newnick);
char *modestr(int num, char mode);

struct Ignore *OnIgnoreList(char *hostmask);
void AddIgnore(char *hostmask, time_t expire);
int DelIgnore(char *hostmask);
void ExpireIgnores(time_t unixtime);

void o_Wallops(char *format, ...);

#if defined AUTO_ROUTING && defined SPLIT_INFO
void ReconnectCheck(time_t);
#endif

/*
 * External declarations
 */

extern struct Ignore          *IgnoreList;

#endif /* INCLUDED_operserv_h */
