/*
 * log.h
 * HybServ2 Services by HybServ2 team
 *
 * $Id$
 */

#ifndef INCLUDED_log_h
#define INCLUDED_log_h

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>        /* time_t */
#define INCLUDED_sys_types_h
#endif

/* Logging levels */

#define    LOG0    0  /* log nothing */
#define    LOG1    1  /* log security stuff - kills, admin commands etc */
#define    LOG2    2  /* same as 2, with some more events logged */
#define    LOG3    3  /* log all channel stuff as well */

void putlog(int level, char *format, ...);
void RecordCommand(char *format, ...);
void CheckLogs(time_t unixtime);

#endif /* INCLUDED_log_h */
