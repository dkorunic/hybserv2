/*
 * stdinc.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_stdinc_h
#define INCLUDED_stdinc_h

#include "defs.h"

#include <stdio.h>

#if defined HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if defined HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if defined STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
# include <stdarg.h>
# include <ctype.h>
#else
# if defined HAVE_STDLIB_H
#  include <stdlib.h>
# endif
# if !defined HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr(), *strrchr();
# if !defined HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#if defined HAVE_STRING_H
# if !defined STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
#  include <string.h>
#endif

#if defined HAVE_STRINGS_H
# include <strings.h>
#endif

#if defined HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if defined HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#if defined HAVE_UNISTD_H
# include <unistd.h>
#endif

#if defined TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if defined HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if defined HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#if defined HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
#endif

#if defined HAVE_ERRNO_H
# include <errno.h>
#endif

#if defined HAVE_LIMITS_H
# include <limits.h>
#endif

#if defined HAVE_LIBDMALLOC
# include <dmalloc.h>
#endif

#if defined HAVE_ASSERT_H && !defined NDEBUG
# include <assert.h>
#else
# define assert(x)
#endif

#if defined HAVE_NETDB_H
# include <netdb.h>
#endif

#if defined HAVE_FCNTL_H
# include <fcntl.h>
#endif

#if defined HAVE_SIGNAL_H
# include <signal.h>
#endif

#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#if defined HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#if defined HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif

#if defined HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif

#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#if defined HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if defined HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if defined HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if defined HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#if !defined HAVE_SOCKADDR_STORAGE
struct sockaddr_storage
{
	unsigned char ss_len;
	unsigned char ss_family;
	unsigned char padding[128 - 2];
};
# define HAVE_SOCKADDR_STORAGE
#endif

#if !defined HAVE_GETADDRINFO
#include "../compat/addrinfo.h"
int getaddrinfo(const char *, const char *, const struct addrinfo *,
                struct addrinfo **);
#endif

#if !defined HAVE_GETNAMEINFO
#include "../compat/addrinfo.h"
int getnameinfo(const struct sockaddr *, size_t, char *,
                size_t, char *, size_t, int);
#endif

#if !defined HAVE_INET_NTOA
char *inet_ntoa(struct in_addr);
#endif

#if !defined HAVE_INET_NTOP
const char *inet_ntop(int, const void *, char *, size_t);
#endif

#if !defined HAVE_INET_PTON
int inet_pton(int, const char *, void *);
#endif

#if !defined HAVE_STRTOUL
unsigned long int strtoul(const char *, char **, int);
#endif

#if !defined HAVE_STRDUP
char *strdup(const char *);
#endif

#if !defined HAVE_STRERROR
const char *strerror(int);
#endif

#endif /* INCLUDED_stdinc_h */
