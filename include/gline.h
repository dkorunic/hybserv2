/*
 * gline.h
 * Copyright (C) 1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_gline_h
#define INCLUDED_gline_h

#ifndef INCLUDED_config_h
#include "config.h"         /* ALLOW_GLINES */
#define INCLUDED_config_h
#endif

#ifdef ALLOW_GLINES

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>         /* time_t */
#define INCLUDED_sys_types_h
#endif

struct Luser;

struct Gline
{
  struct Gline *next, *prev;
  char *username;      /* username of gline */
  char *hostname;      /* hostname of gline */
  char *reason;        /* reason for Gline */
  char *who;           /* who made the gline */
  time_t expires;      /* 0 if its not a temp gline */
};

/*
 * Prototypes
 */

void AddGline(char *user, char *host, char *reason, long expire);
void DeleteGline(struct Gline *gptr);

struct Gline *IsGline(char *user, char *host);
void CheckGlined(struct Luser *lptr);

#ifdef HYBRID_GLINES
void ExecuteGline(char *user, char *host, char *reason);
#endif

#ifdef HYBRID7_GLINES
void Execute7Gline(char *user, char *host, char *reason);
#endif

void ExpireGlines(time_t unixtime);

/*
 * External declarations
 */

extern struct Gline          *GlineList;

#endif /* ALLOW_GLINES */

#endif /* INCLUDED_gline_h */
