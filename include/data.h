/*
 * data.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_data_h
#define INCLUDED_data_h

#include "stdinc.h"
#include "config.h"
#include "operserv.h"

void BackupDatabases(time_t);
int ReloadData(void);
FILE *CreateDatabase(char *, char *);

int WriteDatabases(void);
int WriteOpers(void);
int WriteIgnores(void);
int WriteStats(void);
int WriteNicks(void);
int WriteChans(void);
int WriteMemos(void);

#ifdef SEENSERVICES
int WriteSeen(void);
int es_loaddata(void);
#endif /* SEENSERVICES */

void LoadData(void);
int ns_loaddata(void);
int cs_loaddata(void);
int ms_loaddata(void);
int ss_loaddata(void);
int os_loaddata(void);
int ignore_loaddata(void);

#endif /* INCLUDED_data_h */
