/*
 * log.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_log_h
#define INCLUDED_log_h

#include "stdinc.h"
#include "config.h"

/* Logging levels */
#define    LOG0    0  /* log nothing */
#define    LOG1    1  /* log security stuff - kills, admin commands etc */
#define    LOG2    2  /* same as 2, with some more events logged */
#define    LOG3    3  /* log all channel stuff as well */

void putlog(int, char *, ...);
void RecordCommand(char *, ...);
void CheckLogs(time_t);
int OpenLogFile(void);
void CloseLogFile(void);

#endif /* INCLUDED_log_h */
