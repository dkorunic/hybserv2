/*
 * seenserv.h
 * Copyright (C) 2000 demond
 *
 */

#ifndef INCLUDED_seenserv_h
#define INCLUDED_seenserv_h

#ifndef INCLUDED_config_h
#include "config.h"        /* SEENSERVICES */
#define INCLUDED_config_h
#endif

#ifdef SEENSERVICES

struct Seen {
  struct Seen *prev, *next, *seen;
  int type;                /* 1 - QUIT, 2 - NICK */
  char nick[NICKLEN + 1];
  char *userhost, *msg;
  time_t time;
};

typedef struct Seen aSeen;

extern int seenc;
extern aSeen *seenp, *seenb; 

void es_process(char *nick, char *command);
void es_add(char *nick, char *user, char *host, char *msg, time_t time, int type);

#endif /* SEENSERVICES */

#endif /* INCLUDED_seenserv_h */
