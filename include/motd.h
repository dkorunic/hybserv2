/*
 * motd.h
 * Copyright (C) 1998-1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_motd_h
#define INCLUDED_motd_h

#define MESSAGELINELEN 89
#define MAX_DATESTRING 32

struct Luser;

/*
 * This idea of caching motd file entries is from ircd-hybrid
 */

struct MessageFileLine
{
  struct MessageFileLine *next;
  char line[MESSAGELINELEN + 1];
};

struct MessageFile
{
  char *filename;
  struct MessageFileLine *Contents;
  char DateLastChanged[MAX_DATESTRING + 1];
};

/*
 * Prototypes
 */

void InitMessageFile(struct MessageFile *mptr);
int ReadMessageFile(struct MessageFile *mptr);
void SendMessageFile(struct Luser *lptr, struct MessageFile *mptr);

#endif /* INCLUDED_motd_h */
