/*
 * HybServ TS Services, Copyright (C) 1998-1999 Patrick Alken
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "alloc.h"
#include "conf.h"
#include "config.h"
#include "dcc.h"
#include "defs.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "operserv.h"
#include "server.h"
#include "sock.h"
#include "sprintf_irc.h"
#include "timer.h"

#ifdef HAVE_SOLARIS_THREADS
#include <thread.h>
#include <synch.h>
#else
#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif
#endif

/* Solaris does not provide this by default. Anyway this is wrong approach,
   since -1 is 255.255.255.255 addres which is _valid_! Obviously
   inet_aton() should be used instead. I'll fix that later. -kre */
#ifndef INADDR_NONE
# define INADDR_NONE ((unsigned long)-1)
#endif

static int ReadHub(void);
static int ReadSock(struct DccUser *dccptr);

/*
 * Global - socket file descriptor of current hub
 */
int                       HubSock;

/*
 * Virtual hostname
 */
char                      *LocalHostName = NULL;
struct sockaddr_in        LocalAddr;

/*
 * Contains data read from socket descriptors
 */
char                      buffer[BUFSIZE + 1];

/*
 * Array of pointers to each parameter the hub sends
 */
static char               *param[MAXPARAM + 1];
int                       paramc = 0;           /* parameter count */
char                      *nextparam = NULL;    /* address of next parameter */

/*
 * If recv() fills buffer[] completely, splitting the last
 * line off in the middle somewhere, this array will keep
 * the "spilled over" characters - the partial line that
 * buffer wasn't big enough to hold
 */
char                      spill[MAXLINE * 2];

/*
 * Index of spill[] where we left off
 */
int                       offset = 0;

char                      dccspill[MAXLINE * 2];
int                       dccoffset = 0;

#ifdef HIGHTRAFFIC_MODE

int                       HTM = 0;         /* High Traffic Mode */
time_t                    HTM_ts = 0;      /* when HTM was activated */

/*
 * Switch to HTM when the load becomes greater than
 * ReceiveLoad
 */
int                       ReceiveLoad = RECVLOAD;

#endif /* HIGHTRAFFIC_MODE */

/*
writesocket()
  args: sockfd, writestr
  purpose: write 'writestr' to socket designated by 'sockfd'
  return: amount of bytes written
*/

int
writesocket(int sockfd, char *writestr)

{
  int ii;

  if (sockfd == NOSOCKET)
    return (0);

  /*send(sockfd, writestr, strlen(writestr), 0);*/

#ifdef DEBUGMODE
  fprintf(stderr, "Writing: %s",
    writestr);
#endif /* DEBUGMODE */

  ii = write(sockfd, writestr, strlen(writestr));

  Network->SendB += (writestr ? strlen(writestr) : 0);

  if ((ii < 0) && (errno != EAGAIN))
  {
    struct DccUser *conn;
    /* 
     * EOF writing to the socket - check if we were writing to a 
     * partyline socket, if so, mark the user with SOCK_EOF to
     * kill the connection later
     */
    if ((conn = IsDccSock(sockfd)))
      conn->flags |= SOCK_EOF;
  }

  return (ii);
} /* writesocket() */

/*
toserv()
  sends 'str' to hub server
*/

void
toserv(char *format, ...)

{
  char buf[MAXLINE * 2];
  int ii;
  va_list args;

  if (HubSock == NOSOCKET)
    return;

  va_start(args, format);

  vsprintf_irc(buf, format, args);

  va_end(args);

  /* send the string to the server */
  ii = writesocket(HubSock, buf);

  /*
   * If format contains a trailing \n, but buf was too large
   * to fit 'format', make sure the \n gets through
   */
  if ((format[strlen(format) - 1] == '\n') &&
      (buf[ii - 1] != '\n'))
    writesocket(HubSock, "\n");
} /* toserv() */

/*
tosock()
  sends string to specified socket
*/

void
tosock(int sockfd, char *format, ...)

{
  char buf[MAXLINE * 2];
  va_list args;

  va_start(args, format);

  vsprintf_irc(buf, format, args);

  va_end(args);

  /* send the string to the socket */
  writesocket(sockfd, buf);
} /* tosock() */

/*
SetupVirtualHost()
 Initialize virtual hostname
*/

void
SetupVirtualHost()

{
  struct hostent *hptr;

  assert(LocalHostName != 0);

  if ((hptr = gethostbyname(LocalHostName)) == (struct hostent *) NULL)
  {
    fprintf(stderr,
      "Unable to resolve virtual host [%s]: Unknown hostname\n",
      LocalHostName);

    MyFree(LocalHostName);
    LocalHostName = NULL;
  }
  else
  {
    memset((void *) &LocalAddr, 0, sizeof(struct sockaddr_in));

    LocalAddr.sin_family = AF_INET;
    LocalAddr.sin_port = 0;

    memcpy((void *) &LocalAddr.sin_addr, (void *) hptr->h_addr,
      hptr->h_length);

    fprintf(stderr, "Using virtual host %s[%s]\n",
      LocalHostName,
      inet_ntoa(LocalAddr.sin_addr));
  }
} /* SetupVirtualHost() */

/*
LookupHostname()
 Attempt to resolve 'host' into an ip address. 'ip_address' is
modified to contain the resolved ip address in unsigned int form.

Return: a hostent pointer to the hostname information
*/

struct hostent *
LookupHostname(char *host, struct in_addr *ip_address)

{
  struct hostent *hp;
  struct in_addr ip;

  /*
   * If 'host' was given in dotted notation (1.2.3.4),
   * inet_addr() will convert it to unsigned int form,
   * otherwise return -1L.
   */
  ip.s_addr = inet_addr(host);

  if (ip.s_addr != INADDR_NONE)
  {
    /*
     * No point in resolving it now
     */
    hp = NULL;
  }
  else
  {
    hp = gethostbyname(host);
    if (hp)
      memcpy(&ip.s_addr, hp->h_addr_list[0], (size_t) hp->h_length);
  }

  if (ip_address)
    *ip_address = ip;

  return (hp);
} /* LookupHostname() */

/*
ConnectHost()
  args: hostname, port
  purpose: bind socket and begin a non-blocking connection to
           hostname at port
  return: socket file descriptor if successful connect, -1 if not
*/

int
ConnectHost(char *hostname, unsigned int port)

{
  struct sockaddr_in ServAddr;
  register struct hostent *hostptr;
  struct in_addr ip;
  int socketfd; /* socket file descriptor */

  memset((void *) &ServAddr, 0, sizeof(struct sockaddr_in));

  hostptr = LookupHostname(hostname, &ip);

  if (hostptr)
  {
    assert(ip.s_addr != INADDR_NONE);

    ServAddr.sin_family = hostptr->h_addrtype;
    ServAddr.sin_addr.s_addr = ip.s_addr;
  }
  else
  {
    /*
     * There is no host entry, but there might be an ip address
     */
    if (ip.s_addr == INADDR_NONE)
    {
    #ifdef DEBUGMODE
      fprintf(stderr,
        "Cannot connect to port %d of %s: Unknown host\n",
        port,
        hostname);
    #endif
      putlog(LOG1,
        "Unable to connect to %s.%d: Unknown hostname",
        hostname,
        port);
      return (-1);
    }

    ServAddr.sin_family = AF_INET;
    ServAddr.sin_addr.s_addr = ip.s_addr;
  }

#ifdef DEBUGMODE
  fprintf(stderr,
    "Connecting to %s[%s].%d\n",
    hostname,
    inet_ntoa(ServAddr.sin_addr),
    port);
#endif

  if ((socketfd = socket(ServAddr.sin_family, SOCK_STREAM, 0)) < 0)
  {
  #ifdef DEBUGMODE
    fprintf(stderr, "Unable to open stream socket\n");
  #endif
    putlog(LOG1,
      "Unable to open stream socket: %s",
      strerror(errno));

    return(-1);
  }

  if (LocalHostName)
  {
    /* bind to virtual host */
    if ((bind(socketfd, (struct sockaddr *) &LocalAddr, sizeof(LocalAddr))) < 0)
    {
      putlog(LOG1, "Unable to bind virtual host %s[%s]: %s",
        LocalHostName,
        inet_ntoa(LocalAddr.sin_addr),
        strerror(errno));
      close (socketfd);
      return (-1);
    }
  }

  if (!SetNonBlocking(socketfd))
  {
    putlog(LOG1,
      "Unable to set socket [%d] non-blocking",
      socketfd);
    close(socketfd);
    return (-1);
  }

  ServAddr.sin_port = (unsigned short) htons((unsigned short) port);

  /* Check for both if return value is -1 and errno is set. This is
   * paranoid, I know, but it was obviously necessary on Solaris -kre */
  if (connect(socketfd, (struct sockaddr *) &ServAddr, sizeof(ServAddr))==-1)
  {
    if (errno && errno!=EINPROGRESS)
    {
    #ifdef DEBUGMODE
      fprintf(stderr,
        "Cannot connect to port %d of %s: %s\n",
        port,
        hostname,
        strerror(errno));
    #endif
      putlog(LOG1,
        "Error connecting to %s[%s].%d: %s",
        hostname,
        hostname,
        port,
        strerror(errno));
      close(socketfd);
      return(-1);
    }
  }

  return (socketfd);
} /* ConnectHost() */

/*
CompleteHubConnection()
 Complete second half of non-blocking connect sequence

Return: 1 if successful
        0 if unsuccessful
*/

int
CompleteHubConnection(struct Servlist *hubptr)

{
  int errval,
      errlen;

  assert(hubptr != 0);

  ClearHubConnect(hubptr);

  errval = 0;
  errlen = sizeof(errval);
  if (getsockopt(HubSock, SOL_SOCKET, SO_ERROR, &errval, &errlen) < 0)
  {
    putlog(LOG1,
      "getsockopt(SO_ERROR) failed: %s",
      strerror(errno));
    return 0;
  }

  if (errval > 0)
  {
  #ifdef DEBUGMODE
    fprintf(stderr,
      "Cannot connect to port %d of %s: %s\n",
      hubptr->port,
      hubptr->hostname,
      strerror(errval));
  #endif

    putlog(LOG1,
      "Error connecting to port %d of %s: %s",
      hubptr->port,
      hubptr->hostname,
      strerror(errval));

    return 0;
  }

  signon();

  hubptr->connect_ts = current_ts;

  SendUmode(OPERUMODE_Y,
    "*** Connected to %s:%d",
    hubptr->hostname,
    hubptr->port);

  putlog(LOG1,
    "Connected to %s:%d",
    hubptr->hostname,
    hubptr->port);

  return 1;
} /* CompleteHubConnection() */

/*
ReadSocketInfo()
  args: none
  purpose: wait for information from sockets; once an entire
           line has been sent, call ProcessInfo() to process the line
  return: none
*/

void
ReadSocketInfo(void)

{
  int SelectResult;
  struct timeval TimeOut;

  struct DccUser *dccptr, *dccnext;
  struct PortInfo *tempport;
  fd_set readfds,
         writefds;

#if defined(HIGHTRAFFIC_MODE) || (!defined(HAVE_PTHREADS) \
		&& !defined(HAVE_SOLARIS_THREADS))
  time_t curr_ts;
#endif

#if !defined HAVE_PTHREADS && !defined(HAVE_SOLARIS_THREADS)
  time_t last_ts = current_ts;
#endif

  spill[0] = '\0';
  dccspill[0] = '\0';

  for (;;)
  {

#if !defined HAVE_PTHREADS && !defined HAVE_SOLARIS_THREADS
    /*
     * If pthreads are not enabled, DoTimer() will be called from this
     * code, and so we need curr_ts here
     *
     * And we also need time setup here -kre
     */
    curr_ts = current_ts = time(NULL);
#endif /* !(HAVE_PTHREADS || HAVE_SOLARIS_THREADS) */

#ifdef HIGHTRAFFIC_MODE

#if defined HAVE_PTHREADS || defined HAVE_SOLARIS_THREADS
    /*
     * Since pthreads are enabled, HTM mode is the only thing that needs
     * curr_ts, since DoTimer() is called from a separate thread
     */
    curr_ts = current_ts;
#endif /* HAVE_PTHREADS || HAVE_SOLARIS_THREADS */

    if (HTM)
    {
      int htm_count;

#if !defined HAVE_PTHREADS && !defined HAVE_SOLARIS_THREADS

      if (last_ts != curr_ts)
      {
        /*
         * So DoTimer() doesn't get called a billion times when HTM is
         * turned off
         */
        last_ts = curr_ts;

        /*
         * DoTimer() won't do anything other than update
         * Network->CheckRecvB since we are in HTM
         */
        DoTimer(curr_ts);
      } /* if (last_ts != curr_ts) */

#endif /* HAVE_PTHREADS || HAVE_SOLARIS_THREADS */

      /* Check if HTM should be turned off */
      if ((curr_ts - HTM_ts) >= HTM_TIMEOUT)
      {
        float    currload;

        currload = ((float) (Network->RecvB - Network->CheckRecvB) / 
                    (float) 1024) / (float) HTM_INTERVAL;

        if (currload >= (float) ReceiveLoad)
        {
          HTM_ts = curr_ts;
          putlog(LOG1, "Continuing high-traffic mode (%0.2f K/s)",
            currload);
          SendUmode(OPERUMODE_Y,
            "*** Continuing high-traffic mode (%0.2f K/s)",
            currload);
        }
        else
        {
          HTM = HTM_ts = 0;
          putlog(LOG1, "Resuming standard traffic operations (%0.2f K/s)",
            currload);
          SendUmode(OPERUMODE_Y,
            "*** Resuming standard traffic operations (%0.2f K/s)",
            currload);
        }
      }

      for (htm_count = 0; htm_count < HTM_RATIO; htm_count++)
        if (!ReadHub())
          return;

    } /* if (HTM) */
#if !defined HAVE_PTHREADS && !defined HAVE_SOLARIS_THREADS
    else
#endif /* ! (HAVE_PTHREADS || HAVE_SOLARIS_THREADS) */

#endif /* HIGHTRAFFIC_MODE */

    #if !defined HAVE_PTHREADS && !defined HAVE_SOLARIS_THREADS

      if (last_ts != curr_ts)
      {
        if (curr_ts > (last_ts + 1))
        {
          time_t  delta;
          /*
           * we skipped at least 1 time interval - make up for it
           * by calling DoTimer() how ever many intervals we missed
           * (ie: if the TS went from 5 to 8, call DoTimer() twice
           * since we missed 6 and 7)
           */
          for (delta = (last_ts + 1); delta < curr_ts; delta++)
            DoTimer(delta);
        }
        last_ts = curr_ts;
        DoTimer(curr_ts);
      }

    #endif /* HAVE_PTHREADS */

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    TimeOut.tv_sec = 1;
    TimeOut.tv_usec = 0L;

    if (currenthub && (HubSock != NOSOCKET))
    {
      if (IsHubConnect(currenthub))
        FD_SET(HubSock, &writefds);
      else
        FD_SET(HubSock, &readfds);
    }

    for (dccptr = connections; dccptr; dccptr = dccptr->next)
    {
      if (IsDccConnect(dccptr))
        FD_SET(dccptr->socket, &writefds);
      else
      {
        FD_SET(dccptr->socket, &readfds);
        if (dccptr->authfd != NOSOCKET)
        {
          FD_SET(dccptr->authfd, &readfds);
          if (dccptr->flags & SOCK_WRID)
            FD_SET(dccptr->authfd, &writefds);
        }
      }
    }

    for (tempport = PortList; tempport; tempport = tempport->next)
      if (tempport->socket != NOSOCKET)
        FD_SET(tempport->socket, &readfds);

    SelectResult = select(FD_SETSIZE, &readfds, &writefds, NULL, &TimeOut);
    if (SelectResult > 0)
    {
      /* something happened */

      if (currenthub && (HubSock != NOSOCKET))
      {
        /* This is for info coming from the hub server */

        if (IsHubConnect(currenthub) && FD_ISSET(HubSock, &writefds))
        {
          if (!CompleteHubConnection(currenthub))
          {
            close(HubSock);
            currenthub->sockfd = HubSock = NOSOCKET;
          }
        }
        else if (FD_ISSET(HubSock, &readfds))
        {
          if (!ReadHub())
            return; /* something bad happened */
        }
      } /* if (currenthub && (HubSock != NOSOCKET)) */

      for (tempport = PortList; tempport; tempport = tempport->next)
      {
        if (tempport->socket == NOSOCKET)
          continue;

        if (FD_ISSET(tempport->socket, &readfds))
        {
        #ifdef HAVE_SOLARIS_THREADS
	  thread_t clientid;
        #else
        #ifdef HAVE_PTHREADS
          pthread_t clientid;
          pthread_attr_t attr;
        #endif
        #endif

          /*
           * a client has connected on one of our listening ports
           * (P: lines) - accept their connection, and perform
           * ident etc
           */

	#ifdef HAVE_SOLARIS_THREADS

	  thr_create(NULL, 0, (void *)&ConnectClient, (void *)tempport,
			  THR_DETACHED, &clientid);

	#else

        #ifdef HAVE_PTHREADS

          /* this way gethostbyaddr() will be non-blocking */
          pthread_attr_init(&attr);
          pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
          pthread_create(&clientid, &attr, (void *) &ConnectClient, (void *) tempport);

        #else

          ConnectClient(tempport);

        #endif /* HAVE_PTHREADS */
        #endif
        }
      }

      /* this is for info coming from a dcc/telnet/tcm client */
      for (dccptr = connections; dccptr; dccptr = dccnext)
      {
        dccnext = dccptr->next;

        assert(dccptr->socket != NOSOCKET);

        if (IsDccConnect(dccptr) && FD_ISSET(dccptr->socket, &writefds))
        {
          if (!GreetDccUser(dccptr))
            DeleteDccClient(dccptr);

          continue;
        }

        if (dccptr->authfd != NOSOCKET)
        {
          if (FD_ISSET(dccptr->authfd, &writefds) &&
              (dccptr->flags & SOCK_WRID))
            writeauth(dccptr); /* send ident request */

          if (FD_ISSET(dccptr->authfd, &readfds))
            readauth(dccptr); /* read ident reply */
        }

        if (dccptr->flags & SOCK_NEEDID)
          continue;

        if (FD_ISSET(dccptr->socket, &readfds))
        {
          /* read and parse any data from the socket */
          if (!ReadSock(dccptr))
          {
            CloseConnection(dccptr);
            continue;
          }
        } /* if (FD_ISSET(dccptr->socket, &readfds)) */
      } /* for (dccptr = connections; dccptr; dccptr = dccptr->next) */

    } /* if (SelectResult > 0) */
    else
    {
      /* Also check whether is errno set at all.. -kre */
      if ((SelectResult == (-1)) && errno && errno!=EINTR)
      {
      #ifdef DEBUGMODE      
        fprintf(stderr,
          "Connection closed: %s\n",
          strerror(errno));
      #endif

        putlog(LOG1, "Lost connection to %s:%d (%s)",
          currenthub->hostname,
          currenthub->port,
          strerror(errno));

        return;
      }
    }
  } /* for (;;) */
} /* ReadSocketInfo() */

/*
ReadHub()
  Read and parse any data from the hub server - return 1 if successful,
0 if not; return 2 if the socket is ok, but there's no data to be read
*/

static int
ReadHub()

{
  int length; /* number of bytes we read */
  register char *ch;
  register char *linech;
#ifdef EXTREMEDEBUG
  FILE *fp;
#endif

  if (HubSock == NOSOCKET)
    return 0;

  /* read in a line */
  length = recv(HubSock, buffer, BUFSIZE, 0);

  if ((length == (-1)) && ((errno == EWOULDBLOCK) || (errno == EAGAIN)))
    return 2; /* no error - there's just nothing to read */

  if (length <= 0)
  {
    putlog(LOG1, "Read error from server: %s",
      strerror(errno));
    return 0; /* the connection was closed */
  }

  Network->RecvB += length;

  /*
   * buffer may not be big enough to read the whole last line
   * so it may contain only part of it
   */
  buffer[length] = '\0';

#ifdef EXTREMEDEBUG
  if ((fp = fopen("hybserv.hubinfo", "a+")))
  {
    fprintf(fp, "%s", buffer);
    fclose(fp);
  }
#endif

  /*
   * buffer could possibly contain several lines of info, especially if
   * this is the inital connect burst, so go through, and record each line
   * (until we hit a \n) and process it separately
   */

  ch = buffer;
  linech = spill + offset;

  /*
   * The following routine works something like this: buffer may contain
   * several full lines, and then a partial line. If this is the case,
   * loop through buffer, storing each character in 'spill' until we hit a
   * \n or \r.  When we do, process the line. When we hit the end of
   * buffer, spill will contain the partial line that buffer had, and
   * offset will contain the index of spill where we left off, so the next
   * time we recv() from the hub, the beginning characters of buffer will
   * be appended to the end of spill, thus forming a complete line. If
   * buffer does not contain a partial line, then linech will simply point
   * to the first index of 'spill' (offset will be zero) after we process
   * all of buffer's lines, and we can continue normally from there.
   */

  while (*ch)
  {
    register char tmp;
  #ifdef DEBUGMODE
    int    ii;
  #endif

    tmp = *ch;
    if (IsEOL(tmp))
    {
      *linech = '\0';

      if (nextparam)
      {
        /*
         * It is possible nextparam will not be NULL here if there is a
         * line like:
         * PASS password :TS
         * where the text after the colon does not have any spaces, so we
         * reach the \n and do not execute the code below which sets the
         * next index of param[] to nextparam. Do it here.
         */
        param[paramc++] = nextparam;
      }

    #ifdef DEBUGMODE
      for (ii = 0; ii < paramc; ii++)
        fprintf(stderr, "%s ", param[ii]);
      fprintf(stderr, "\n");
    #endif

      /*
       * Make sure paramc is non-zero, because if the line starts with a
       * \n, we will immediately come here, without initializing param[0]
       */
      if (paramc)
      {
        /* process the line */
        ProcessInfo(paramc, param);
      }

      linech = spill;
      offset = 0;
      paramc = 0;
      nextparam = (char *) NULL;

      /*
       * If the line ends in \r\n, then this algorithm would have only
       * picked up the \r. We don't want an entire other loop to do the
       * \n, so advance ch here.
       */
      if (IsEOL(*(ch + 1)))
        ch++;
    }
    else
    {
      /* make sure we don't overflow spill[] */
      if (linech >= (spill + (sizeof(spill) - 1)))
      {
        ch++;
        continue;
      }

      *linech++ = tmp;
      if (tmp == ' ')
      {
        /*
         * Only set the space character to \0 if this is the very first
         * parameter, or if nextparam is not NULL. If nextparam is NULL,
         * then we've hit a parameter that starts with a colon (:), so
         * leave it as a whole parameter.
         */
        if (nextparam || (paramc == 0))
          *(linech - 1) = '\0';

        if (paramc == 0)
        {
          /*
           * Its the first parameter - set it to the beginning of spill
           */
          param[paramc++] = spill;
          nextparam = linech;
        }
        else
        {
          if (nextparam)
          {
            param[paramc++] = nextparam;
            if (*nextparam == ':')
            {
              /*
               * We've hit a colon, set nextparam to NULL, so we know not
               * to set any more spaces to \0
               */
              nextparam = (char *) NULL;

              /*
               * Unfortunately, the first space has already been set to \0
               * above, so reset to to a space character
               */
              *(linech - 1) = ' ';
            }
            else
              nextparam = linech;

            if (paramc >= MAXPARAM)
              nextparam = (char *) NULL;
          }
        }
      }
      offset++;
    }

    /*
     * Advance ch to go to the next letter in the buffer
     */
    ch++;
  }

  return 1;
} /* ReadHub() */

/*
ReadSock()
  Similar to ReadHub(), but read a dcc/telnet/tcm socket instead and
parse any data
  return 1 if successful, 0 if not; return 2 if the socket is ok,
but there's no data to be read
*/

static int
ReadSock(struct DccUser *connptr)

{
  int length;
  register char *ch;
  register char *linech;

  if (!connptr)
    return 0;

  length = recv(connptr->socket, buffer, BUFSIZE, 0);

  if ((length == (-1)) && ((errno == EWOULDBLOCK) || (errno == EAGAIN)))
    return 2; /* no error - there's just nothing to read */

  if (length <= 0)
  {
    /* dcc client closed connection */
    return 0;
  }

  /*
   * recv() may have cut off the line in the middle
   * if buffer wasn't big enough
   */
  buffer[length] = '\0';

  if (telnet_check(connptr->socket, buffer))
    return 1;

  ch = buffer;
  linech = dccspill + dccoffset;

  while (*ch)
  {
    register char tmp;

    tmp = *ch;
    if (IsEOL(tmp))
    {
      *linech = '\0';

      if (*dccspill && IsEOL(*dccspill))
      {
        ch++;
        continue;
      }

      /* process the line */
      if (connptr->flags & SOCK_TCMBOT)
        BotProcess(connptr, dccspill); /* process line from bot */
      else
        DccProcess(connptr, dccspill); /* process line from client */

      linech = dccspill;
      dccoffset = 0;

      /*
       * If the line ends in \r\n, advance past the \n
       */
      if (IsEOL(*(ch + 1)))
        ch++;
    }
    else
    {
      /* make sure we don't overflow dccspill */
      if (linech < (dccspill + (sizeof(dccspill) - 1)))
      {
        *linech++ = tmp;
        dccoffset++;
      }
    }
    ch++;
  }

  return 1;
} /* ReadSock() */

/*
CycleServers()
 Go through server list trying to get a connection to a hub
server
*/

void
CycleServers()

{
  if (HubSock == NOSOCKET)
  {
    if (currenthub)
    {
      if (!currenthub->next)
        currenthub = ServList;
      else
        currenthub = currenthub->next;
    }
    else
      currenthub = ServList;

    SendUmode(OPERUMODE_Y,
      "*** Cycling server list");

    while (currenthub)
    {
      /* Begin connection to server */
      if ((HubSock = ConnectHost(currenthub->hostname, currenthub->port)) >= 0) 
      {
        currenthub->sockfd = HubSock;

        SetHubConnect(currenthub);

        break;
      }
      else
      {
        SendUmode(OPERUMODE_Y,
          "*** Unable to connect to %s:%d (%s)",
          currenthub->hostname,
          currenthub->port,
          strerror(errno));

        HubSock = NOSOCKET;
        currenthub = currenthub->next;
      }
    } /* while (currenthub) */
  } /* if (HubSock == NOSOCKET) */
} /* CycleServers() */

/*
SetNonBlocking()
 Mark a socket file descriptor as non-blocking

Return: 1 if successful
        0 if unsuccessful
*/

int
SetNonBlocking(int socket)

{
  int flags = 0;

  if ((flags = fcntl(socket, F_GETFL, 0)) == -1)
  {
    putlog(LOG1,
      "SetNonBlocking: fcntl(%d, F_GETFL, 0) failed: %s",
      socket,
      strerror(errno));
    return 0;
  }
  else
  {
    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1)
    {
      putlog(LOG1,
        "SetNonBlocking: fcntl(%d, F_SETFL, %d) failed: %s",
        socket,
        flags | O_NONBLOCK,
        strerror(errno));
      return 0;
    }
  }

  return 1;
} /* SetNonBlocking() */

/*
SetSocketOptions()
  set various socket options on 'socket'
*/

void
SetSocketOptions(int socket)

{
  int option;

  option = 1;

  /*
   * SO_REUSEADDR will enable immediate local address reuse of
   * the port this socket is bound to
   */
  if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *) &option, sizeof(option)) < 0)
  {
    putlog(LOG1, "SetSocketOptions: setsockopt(SO_REUSEADDR) failed: %s",
      strerror(errno));
    return;
  }
} /* SetSocketOptions() */

/*
DoListen()
*/

void
DoListen(struct PortInfo *portptr)

{
  struct sockaddr_in socketname;

  memset((void *) &socketname, 0, sizeof(struct sockaddr));
  socketname.sin_family = AF_INET;
  socketname.sin_addr.s_addr = INADDR_ANY;
  socketname.sin_port = htons(portptr->port);
  
  portptr->tries++;

  if ((portptr->socket = socket(PF_INET, SOCK_STREAM, 6)) < 0)
  {
    putlog(LOG1, "Unable to create stream socket: %s",
      strerror(errno));
    close(portptr->socket);
    portptr->socket = NOSOCKET;
    return;
  }

  /* set various socket options */
  SetSocketOptions(portptr->socket);

  if (bind(portptr->socket, (struct sockaddr *)&socketname, sizeof(socketname)) < 0)
  {
    putlog(LOG1, "Unable to bind port %d: %s",
      portptr->port,
      strerror(errno));
    close(portptr->socket);
    portptr->socket = NOSOCKET;
    return;
  }

  if (listen(portptr->socket, 4) < 0)
  {
    putlog(LOG1, "Unable to listen on port %d: %s",
      portptr->port,
      strerror(errno));
    close(portptr->socket);
    portptr->socket = NOSOCKET;
    return;
  }
} /* DoListen() */

/*
signon()
  args: none
  purpose: send the PASS / CAPAB / SERVER handshake
  return: none
*/

void
signon()

{
  toserv("PASS %s :TS\nCAPAB :EX"
#ifdef GECOSBANS
      /* Send gecosbans capab -Janos */
      " DE"
#endif
#ifdef HYBRID7
      /* Send most of Hybrid7 CAPABS -kre && Janos */
      " KLN GLN HOPS IE HUB AOPS"
#endif /* HYBRID7 */
      "\nSERVER %s 1 :%s\n", 
    currenthub->password, 
    Me.name, 
    Me.info); 
} /* signon() */
