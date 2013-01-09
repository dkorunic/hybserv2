/*
 * Hybserv2 Services by Hybserv2 team
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 */

#include "stdinc.h"

#if defined HYBSERV_VERSION
char  hVersion[] = HYBSERV_VERSION;
#elif defined PACKAGE_VERSION
char  hVersion[] = PACKAGE_VERSION;
#else
char  hVersion[] = "unknown";
#endif
