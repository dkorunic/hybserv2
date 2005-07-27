/*
 * global.h
 * Hybserv2 Services by Hybserv2 team
 *
 * $Id$
 */

#ifndef INCLUDED_global_h
#define INCLUDED_global_h

#ifndef INCLUDED_config_h
#include "config.h"        /* GLOBALSERVICES */
#define INCLUDED_config_h
#endif

#ifdef GLOBALSERVICES

/*
 * Prototypes
 */

void gs_process(char *nick, char *command);

#endif /* GLOBALSERVICES */

#endif /* INCLUDED_global_h */
