/*
 * HybServ2 Services by HybServ2 team
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 * $Id$
 */

#include "defs.h"

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif

#include "alloc.h"
#include "client.h"
#include "conf.h"
#include "config.h"
#include "data.h"
#include "dcc.h"
#include "hash.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "operserv.h"
#include "settings.h"
#include "sock.h"
#include "sprintf_irc.h"
#include "nickserv.h"
#include "server.h"

#ifdef HAVE_SOLARIS_THREADS
#include <thread.h>
#include <synch.h>
#else
#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif
#endif

extern char *crypt();

/*
 * This is an array of all the service bots. It's useful
 * because it keeps code repitition down. For example, when
 * we want to introduce all the service nicks to the network
 * upon connection, we can just loop through this array, rather
 * than writing out all the introduce() calls. It's also useful
 * for the remote trace and whois queries.
 */

struct aService ServiceBots[] =
    {
      {
        &n_OperServ, &id_OperServ, &desc_OperServ, &Me.osptr
      },

#ifdef NICKSERVICES

      { &n_NickServ, &id_NickServ, &desc_NickServ, &Me.nsptr },

#ifdef CHANNELSERVICES

      { &n_ChanServ, &id_ChanServ, &desc_ChanServ, &Me.csptr },

#endif /* CHANNELSERVICES */

#ifdef MEMOSERVICES

      { &n_MemoServ, &id_MemoServ, &desc_MemoServ, &Me.msptr },

#endif /* MEMOSERVICES */

#endif /* NICKSERVICES */

#ifdef STATSERVICES

      { &n_StatServ, &id_StatServ, &desc_StatServ, &Me.ssptr },

#endif /* STATSERVICES */

#ifdef HELPSERVICES

      { &n_HelpServ, &id_HelpServ, &desc_HelpServ, &Me.hsptr },

#endif /* HELPSERVICES */

#ifdef GLOBALSERVICES

      { &n_Global, &id_Global, &desc_Global, &Me.gsptr },

#endif /* GLOBALSERVICES */

#ifdef SEENSERVICES

      { &n_SeenServ, &id_SeenServ, &desc_SeenServ, &Me.esptr },

#endif /* SEENSERVICES */

      { 0, 0, 0, 0 }
    };

#ifdef CRYPT_PASSWORDS
static char saltChars[] = "abcdefghijklmnopqrstuvwxyz"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "0123456789./";
#endif /* CRYPT_PASSWORDS */

#ifdef CRYPT_PASSWORDS
/* This specific parts of code are from Hybrid7 mkpasswd tool. Nelson
 * Minar (minar@reed.edu) wrote the original tool, and Hybrid7 team did
 * MD5. -kre */
char *hybcrypt(char *source, char *oldpass)
{
  char *salt;

  if (UseMD5)
    salt = make_md5_salt();
  else
    salt = make_des_salt();

  /* We don't do anything with oldpass, we could randomize a bit and feed
   * it as salt, but hey, we have wonderful random() -kre */

  return crypt(source, salt);
}

char *make_des_salt()
{
  static char salt[3];
  char *saltptr = salt;

  /* Saltify */
  salt[0] = saltChars[random() % 64];
  salt[1] = saltChars[random() % 64];
  salt[2] = '\0';

  return saltptr;
}

char *make_md5_salt()
{
  static char salt[13];
  int i;
  char *saltptr = salt;

  /* This is for setup of modular crypt -kre */
  salt[0] = '$';
  salt[1] = '1';
  salt[2] = '$';

  /* Saltify */
  for (i = 3; i <= 10; i++)
    salt[i] = saltChars[random() % 64];

  /* And properly finish modular crypt salt string -kre */
  salt[11] = '$';
  salt[12] = '\0';

  return saltptr;
}
#endif /* CRYPT_PASSWORDS */

/*
debug()
  Output a debug message to stderr
*/

void
debug(char *format, ...)

#ifdef DEBUGMODE

{
  va_list args;

  va_start(args, format);

  fprintf(stderr, "DEBUG> ");
  vfprintf(stderr, format, args);

  va_end(args);
} /* debug() */

#else /* !DEBUGMODE */

{
  va_list args;
  char buf[MAXLINE * 2];

  va_start(args, format);
  vsprintf_irc(buf, format, args);
  va_end(args);

  /* send the string to +d users */
  SendUmode(OPERUMODE_D, "DEBUG: %s", buf);
} /* debug() */

#endif /* DEBUGMODE */

/*
fatal()
  Fatal error occured - log it and exit
*/

void
fatal(int keepgoing, char *format, ...)

{
  char buf[MAXLINE * 2];
  va_list args;
  int oldlev;

  va_start(args, format);
  vsprintf_irc(buf, format, args);
  va_end(args);

  /*
   * If this is being called from dparse(), it's possible
   * LogLevel hasn't been set yet. So, make sure this
   * error gets logged, whether LogLevel is set or not
   */
  oldlev = LogLevel;
  LogLevel = 1;

  /* log the error */
  if (!keepgoing)
    {
      putlog(LOG1, "FATAL: %s", buf);
      exit(1);
    }
  else
    putlog(LOG1, "Warning: %s", buf);

  LogLevel = oldlev;
} /* fatal() */

/*
 * notice()
 * purpose: send a NOTICE or PRIVMSG to 'nick' with 'msg'
 * return: none
 */
void notice(char *from, char *nick, char *format, ...)
{
  char finalstr[MAXLINE * 2];
  struct NickInfo *nptr;
  va_list args;

  va_start(args, format);
  vsprintf_irc(finalstr, format, args);
  va_end(args);

  nptr = GetLink(nick);
  if (nptr && (nptr->flags & NS_PRIVMSG))
    toserv(":%s PRIVMSG %s :%s\r\n", from, nick, finalstr);
  else
  {
    toserv(":%s NOTICE %s :%s\r\n", ServerNotices ? Me.name : from, nick,
        finalstr);
  }

} /* notice() */

/*
DoShutdown()
  Shut down services
*/

void
DoShutdown(char *who, char *reason)

{
  char sendstr[MAXLINE];
  struct PortInfo *pptr;

  putlog(LOG1, "Shutting down services");

  if (!WriteDatabases())
    putlog(LOG1, "Database update failed");

#if defined(NICKSERVICES) && defined(CHANNELSERVICES)

  toserv(":%s QUIT :%s\r\n", n_ChanServ, "Shutting Down");
#endif

  /* close listening sockets */
  for (pptr = PortList; pptr; pptr = pptr->next)
    if (pptr->socket != NOSOCKET)
      close(pptr->socket);

  if (reason)
    {
      if (who)
        ircsprintf(sendstr, "%s (authorized by %s)", reason, who);
      else
        strcpy(sendstr, reason);
    }
  else
    if (who)
      ircsprintf(sendstr, "Authorized by %s", who);
    else
      sendstr[0] = '\0';

  putlog(LOG1, sendstr);

  /* Instead of SQUIT -kre */
  toserv(":%s ERROR :Shutting down\r\n", Me.name);
  toserv(":%s QUIT\r\n", Me.name);

#ifdef DEBUGMODE
  /* This can be useful in debug mode -kre */
  close(HubSock);
  ClearUsers();
  ClearChans();
  ClearServs();
  ClearHashes(0);
#endif

  /* Hmmh! exit(1) should be called from main() to ensure proper threading
   * system termination. Fix this. -kre */
  exit(1);
} /* DoShutdown() */

/*
HostToMask()
  Convert a userhost like "user@host.name.org" into "*user@*.name.org"
or "user@1.2.3.4" into "*user@1.2.3.*"
*/

char *
HostToMask (char *username, char *hostname)

{
  char *realhost, /* user@host (without nick!) */
  *temp,
  *final, /* final product */
  *user, /* stores the username */
  *host, /* stores the hostname */
  /*
   * first segment of hostname (ie: if host == "a.b.c",
   * topsegment == "c" (top-level domain)
   */
  *topsegment = NULL;
  char userhost[UHOSTLEN + 2];
  int ii, /* looping */
  cnt,
  len;

  if (!username || !hostname)
    return (NULL);

  ircsprintf(userhost, "%s@%s", username, hostname);

  len = strlen(userhost) + 32;

  final = (char *) MyMalloc(len);
  memset(final, 0, len);

  /* strip off a nick nick!user@host (if there is one) */
  realhost = (host = strchr(userhost, '!')) ? host + 1 : userhost;
  user = realhost;
  if ((host = strchr(realhost, '@')))
    {
      final[0] = '*';
      ii = 1;
      /*
       * only use the last 8 characters of the username if there 
       * are more
       */
      if ((host - realhost) > 10)
        user = host - 8;

      /* now store the username into 'final' */
      while (*user != '@')
        final[ii++] = *user++;

      final[ii++] = '@';

      /* host == "@host.name", so point past the @ */
      host++;
    }
  else
    {
      /* there's no @ in the hostname, just make it *@*.host.com */
      strcpy(final, "*@");
      ii = 2;
      host = userhost;
    }

  /*
   * ii now points to the offset in 'final' of where the 
   * converted hostname should go
   */

  realhost = strchr(host, '.');
  if (realhost)
    topsegment = strchr(realhost + 1, '.');
  if (!realhost || !topsegment)
    {
      /*
       * if realhost is NULL, then the hostname must be a top-level
       * domain; and if topsegment is NULL, the hostname must be
       * 2 parts (ie: blah.org), so don't strip off "blah"
       */
      strcpy(final + ii, host);
    }
  else
    {
      /*
       * topsegment now contains the top-level domain - if it's
       * numerical, it MUST be an ip address, since there are
       * no numerical TLD's =P
       */

      /* advance to the end of topsegment */
      for (temp = topsegment; *temp; temp++)
        ;

      --temp; /* point to the last letter (number) of the TLD */
      if ((*temp >= '0') && (*temp <= '9'))
        {
          /* Numerical IP Address */
          while (*temp != '.')
            --temp;

          /*
           * copy the ip address (except the last .XXX) into the 
           * right spot in 'final'
           */
          strncpy(final + ii, host, temp - host);

          /* stick a .* on the end :-) */
          ii += (temp - host);
          strcpy(final + ii, ".*");
        }
      else
        {
          /* its a regular hostname with >= 3 segments */

          if (SmartMasking)
            {
              /*
               * Pick up with temp from were we left off above.
               * Temp now points to the very last charater of userhost.
               * Go backwards, counting all the periods we encounter.
               * If we find 3 periods, make the hostmask:
               *   *.seg1.seg2.seg3
               * Since some users may have extremely long hostnames
               * because of some weird isp. Also, if they have
               * a second TLD, such as xx.xx.isp.com.au, this
               * routine will make their mask: *.isp.com.au, which
               * is much better than *.xx.isp.com.au
               */
              cnt = 0;
              while (temp--)
                {
                  if (*temp == '.')
                    if (++cnt >= 3)
                      break;
                }

              if (cnt >= 3)
                {
                  /*
                   * We have a hostname with more than 3 segments.
                   * Set topsegment to temp, so the final mask
                   * will be *user@*.seg1.seg2.seg3
                   */
                  topsegment = temp;
                }
            } /* if (SmartMasking) */

          /*
           * topsegment doesn't necessarily point to the TLD.
           * It simply points one segment further than realhost.
           * Check if there is another period in topsegment,
           * and if so use it. Otherwise use realhost
           */
          ircsprintf(final + ii, "*%s",
                     strchr(topsegment + 1, '.') ? topsegment : realhost);
        }
    }

  return (final);
} /* HostToMask() */

/*
Substitute()
  args: char *nick, char *str, int sockfd
  purpose: replace any formatting characters in 'str' with the
           corresponding information.
 
    Formatting characters:
      %O - Nickname of n_OperServ
      %N - Nickname of n_NickServ
      %C - Nickname of n_ChanServ
      %M - Nickname of n_MemoServ
      %T - Nickname of n_StatServ
      %H - Nickname of n_HelpServ
      %G - Nickname of n_Global
      %S - Name of this server
      %A - Administrative info
      %V - current version
      %B - bold character
      %+<flag> - needs <flag> to read the line
 
  return: NULL if its a blank line, -1 if 'nick' doesn't have privs
          to read the line; otherwise a ptr to substituted string
*/

char *
Substitute(char *nick, char *str, int sockfd)

{
  char tempstr[MAXLINE], key;
  char *finalstr = NULL;
  int tcnt, fcnt;
  struct Luser *lptr;

  lptr = FindClient(nick);

  strcpy(tempstr, str);
  finalstr = (char *) MyMalloc(MAXLINE);
  memset(finalstr, 0, MAXLINE);
  fcnt = 0;
  tcnt = 0;
  while (tcnt < MAXLINE)
    {
      if (IsEOL(tempstr[tcnt]))
        break;

      if (tempstr[tcnt] == '%')
        {
          key = tempstr[++tcnt];
          switch (key)
            {
            case 'o':
            case 'O':
              {
                strcat(finalstr, n_OperServ);
                fcnt += strlen(n_OperServ) - 1;
                break;
              }
            case 'n':
            case 'N':
              {
                strcat(finalstr, n_NickServ);
                fcnt += strlen(n_NickServ) - 1;
                break;
              }
            case 'c':
            case 'C':
              {
                strcat(finalstr, n_ChanServ);
                fcnt += strlen(n_ChanServ) - 1;
                break;
              }
            case 'e':
            case 'E':
              {
                strcat(finalstr, n_SeenServ);
                fcnt += strlen(n_SeenServ) - 1;
                break;
              }

            case 'm':
            case 'M':
              {
                strcat(finalstr, n_MemoServ);
                fcnt += strlen(n_MemoServ) - 1;
                break;
              }
            case 't':
            case 'T':
              {
                strcat(finalstr, n_StatServ);
                fcnt += strlen(n_StatServ) - 1;
                break;
              }
            case 'h':
            case 'H':
              {
                strcat(finalstr, n_HelpServ);
                fcnt += strlen(n_HelpServ) - 1;
                break;
              }
            case 'g':
            case 'G':
              {
                strcat(finalstr, n_Global);
                fcnt += strlen(n_Global) - 1;
                break;
              }
            case 's':
            case 'S':
              {
                strcat(finalstr, Me.name);
                fcnt += strlen(Me.name) - 1;
                break;
              }
            case 'b':
            case 'B':
              {
                strcat(finalstr, "\002");
                break;
              }
            case 'v':
            case 'V':
              {
                strcat(finalstr, hVersion);
                fcnt += strlen(hVersion) - 1;
                break;
              }
            case 'a':
            case 'A':
              {
                strcat(finalstr, Me.admin);
                fcnt += strlen(Me.admin) - 1;
                break;
              }
            case '+':
              {
                char flag;
                char *cptr, *finstr;
                struct Userlist *tempuser = NULL;

                flag = tempstr[tcnt + 1];
                if (!nick)
                  tempuser = DccGetUser(IsDccSock(sockfd));
                else if (lptr)
                  tempuser = GetUser(1, lptr->nick, lptr->username, lptr->hostname);
                else
                  tempuser = GetUser(1, nick, NULL, NULL);

                if ((CheckAccess(tempuser, flag)))
                  {
                    if (!IsRegistered(lptr, sockfd))
                      return ((char *) -1);

                    tcnt += 2;
                    cptr = &tempstr[tcnt];
                    finstr = Substitute(nick, cptr, sockfd);
                    if (!finstr)
                      {
                        /* its a line like "%+a" with no text, so send a \r\n */
                        MyFree(finstr);
                        finstr = MyStrdup("\r\n");
                      }
                    return (finstr);
                  }
                else
                  return ((char *) -1); /* user doesn't have privs to read line */
                break;
              }

            default:
              {
                strcat(finalstr, "%");
                finalstr[++fcnt] = tempstr[tcnt];
                break;
              }
            } /* switch (key) */
        }
      else
        finalstr[fcnt] = tempstr[tcnt];

      fcnt++;
      tcnt++;
    }

  if (finalstr[0])
    {
      finalstr[fcnt++] = '\n';
      finalstr[fcnt] = '\0';
      return (finalstr);
    }
  else
    {
      MyFree(finalstr);
      return (NULL);
    }
} /* Substitute() */

/*
GetService
  Returns a pointer to luser structure containing n_*Nick
if 'name' matches a Service Bot
*/

struct Luser *
      GetService(char *name)

  {
    struct aService *sptr;

    if (!name)
      return (NULL);

    for (sptr = ServiceBots; sptr->name; ++sptr)
      {
        if (!irccmp(name, *(sptr->name)))
          return (*(sptr->lptr));
      }

    return (NULL);
  } /* GetService() */

/*
FindService
  Similar to GetService(), except accept a struct Luser * pointer
and attempt to find a service match. Returns a pointer
to appropriate Luser structure if 'lptr' is a service nick
*/

struct Luser *
      FindService(struct Luser *lptr)

  {
    struct aService *sptr;

    if (!lptr)
      return (NULL);

    for (sptr = ServiceBots; sptr->name; ++sptr)
      {
        if (lptr == *(sptr->lptr))
          return (*(sptr->lptr));
      }

    return (NULL);
  } /* FindService() */

/*
pwmatch()
  Check if 'password' matches 'chkpass' - return 1 if so, 0 if not
Assumes you're trying to match passwords from *Serv databases
*/

int
pwmatch(char *password, char *chkpass)

{
#ifdef CRYPT_PASSWORDS
  char *encr;
  int cmpresult;
#endif

  if (!password || !chkpass)
    return 0;

#ifdef CRYPT_PASSWORDS

  encr = crypt(chkpass, password);
  assert(encr != 0);

  cmpresult = strcmp(encr, password);

#ifdef BORKPASSWD

  memset(chkpass, 0, strlen(chkpass));
#endif /* BORKPASSWD */

  if (!cmpresult)
    return 1;
  else
    return 0;

#else

  if (!strcmp(password, chkpass))
    return 1;
  else
    return 0;

#endif
} /* pwmatch() */

/*
operpwmatch()
  Check if 'password' matches 'chkpass' - return 1 if so, 0 if not
Assumes you're trying to match passwords from O: lines in hybserv.conf
*/

int
operpwmatch(char *password, char *chkpass)

{
#ifdef CRYPT_OPER_PASSWORDS
  char *encr;
  int cmpresult;
#endif

  if (!password || !chkpass)
    return 0;

#ifdef CRYPT_OPER_PASSWORDS

  encr = crypt(chkpass, password);
  assert(encr != 0);

  cmpresult = strcmp(encr, password);

#ifdef BORKPASSWD

  memset(chkpass, 0, strlen(chkpass));
#endif /* BORKPASSWD */

  if (!cmpresult)
    return 1;
  else
    return 0;

#else

  if (!strcmp(password, chkpass))
    return 1;
  else
    return 0;

#endif
} /* operpwmatch() */

/*
IsInNickArray()
 Determine if 'nickname' is in the array 'nicks'
Return 1 if so, 0 if not
*/

int
IsInNickArray(int nickcnt, char **nicks, char *nickname)

{
  int ii;
  char *ntmp;

  for (ii = 0; ii < nickcnt; ii++)
    {
      ntmp = GetNick(nicks[ii]);
      if (!ntmp)
        continue;

      if (!irccmp(ntmp, nickname))
        return (1);
    }

  return (0);
} /* IsInNickArray() */

/*
IsNum()
  Determine if 'str' is a number - return 0 if not, the number if it is
*/

int
IsNum(char *str)

{
  int result = 0;
  char tmp[MAXLINE];
  char *tmp2;

  if (!str)
    return 0;

  strcpy(tmp, str);
  tmp2 = tmp;
  while (*tmp2)
    {
      if (IsDigit(*tmp2))
        {
          result *= 10;
          result += ((*tmp2) & 0xF);
          tmp2++;
        }
      else
        return 0;
    }

  /* if 'str' actually contains the number 0, return 1 */
  if (result == 0)
    result = 1;

  return (result);
} /* IsNum() */

/*
GetCommand()
 Attempt to find the command "name" in the list "cmdlist".
Return a pointer to the index containing "name" if found,
otherwise NULL.  If the command is found, but there is
more than 1 match (ambiguous), return (struct Command *) -1.
*/

struct Command *
      GetCommand(struct Command *cmdlist, char *name)

  {
    struct Command *cmdptr, *tmp;
    int matches; /* number of matches we've had so far */
    unsigned clength;

    if (!cmdlist || !name)
      return (NULL);

    tmp = NULL;
    matches = 0;
    clength = strlen(name);
    for (cmdptr = cmdlist; cmdptr->cmd; cmdptr++)
      {
        if (!ircncmp(name, cmdptr->cmd, clength))
          {
            if (clength == strlen(cmdptr->cmd))
              {
                /*
                 * name and cmdptr->cmd are the same length, so it
                 * must be an exact match, don't search any further
                 */
                matches = 0;
                break;
              }
            tmp = cmdptr;
            matches++;
          }
      }

    /*
     * If matches > 1, name is an ambiguous command, so the
     * user needs to be more specific
     */
    if ((matches == 1) && (tmp))
      cmdptr = tmp;

    if (cmdptr->cmd)
      return (cmdptr);

    if (matches == 0)
      return (NULL); /* no matches found */
    else
      return ((struct Command *) -1); /* multiple matches found */
  } /* GetCommand() */

char* stripctrlsymbols( char * source )
{
  char *p = NULL;

  if( source == NULL )
    return NULL;

  for (p = source; *p; p++)
    {
      if (*p == 0x04 && (*(p + 1) == 0x08 ))
        {
          *(p+1) = '*';
          source = p+1;
          continue;
        }
      if( *p < 0x21 )
        {
          *(p) = '*';
          source = p;
        }
    }
  return source;
}

char* stripformatsymbols( char * source )
{
  char *p ;

  if( source == NULL )
    return NULL;

  for (p = source; *p; p++)
    if (*p == '%' )
      *p = '*';
  return source;
}

int checkforproc( char* source )
{

  char * p;
  if( source == NULL )
    return 0;

  for (p = source; *p; p++)
    if (*p == '%' )
      return 1;
  return 0;

}

