/*
 * motd.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_motd_h
#define INCLUDED_motd_h

#include "stdinc.h"
#include "config.h"

#define MESSAGELINELEN 256
#define MAX_DATESTRING 32

struct Luser;

struct MessageFileLine
{
	struct MessageFileLine *next;
	char *line;
};

struct MessageFile
{
	char *filename;
	struct MessageFileLine *Contents;
	char DateLastChanged[MAX_DATESTRING + 1];
};

void InitMessageFile(struct MessageFile *);
int ReadMessageFile(struct MessageFile *);
void SendMessageFile(struct Luser *, struct MessageFile *);

#endif /* INCLUDED_motd_h */
