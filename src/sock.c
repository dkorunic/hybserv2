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
#include "alloc.h"
#include "conf.h"
#include "config.h"
#include "dcc.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "operserv.h"
#include "server.h"
#include "sock.h"
#include "sprintf_irc.h"
#include "timer.h"

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
struct sockaddr_storage   LocalAddr;
socklen_t                 LocalAddrSize;

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
char                      spill[BUFSIZE];

/*
 * Index of spill[] where we left off
 */
int                       offset = 0;

#ifdef HIGHTRAFFIC_MODE

int                       HTM = 0;         /* High Traffic Mode */
time_t                    HTM_ts = 0;      /* when HTM was activated */

/*
 * Switch to HTM when the load becomes greater than
 * ReceiveLoad
 */
int                       ReceiveLoad = RECVLOAD;

#endif /* HIGHTRAFFIC_MODE */

struct DccUser            *dccnext;

/* Set this to 1 when exiting ReadSocketInfo (XXX: CRUDE SEMAPHORE HACK!)
 * */
int read_socket_done = 0;

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
		return 0;

	if (writestr == NULL)
		return 0;

#ifdef DEBUGMODE

	fprintf(stderr, "Writing: %s",
	        writestr);
#endif /* DEBUGMODE */

	ii = write(sockfd, writestr, strlen(writestr));

	Network->SendB += strlen(writestr);

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
  toserv() sends 'str' to hub server
*/
void toserv(char *format, ...)
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
		writesocket(HubSock, "\r\n");
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
 * SetupVirtualHost()
 * Initialize virtual hostname
 */
void SetupVirtualHost()
{
	struct addrinfo *res;

	assert(LocalHostName != 0);

	res = LookupHostname(LocalHostName);

	if (res == NULL)
	{
		putlog(LOG1, "FATAL: Unresolvable LocalHostName");
		MyFree(LocalHostName);
	}
	else
	{
		/* use only first address */
		memset(&LocalAddr, 0, sizeof(LocalAddr));
		memcpy(&LocalAddr, res->ai_addr, res->ai_addrlen);
		LocalAddrSize = res->ai_addrlen;
		freeaddrinfo(res);
	}
} /* SetupVirtualHost() */

/*
 * LookupHostname()
 * 
 * Attempt to resolve 'host' into an ip address. 
 * Return: a addrinfo list with the hostname information
*/
struct addrinfo *LookupHostname(const char *host)
{
#if 0
	struct addrinfo hints;
#endif

	struct addrinfo *res = NULL;
	int error;

#if 0

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST;
#endif

	error = getaddrinfo(host, NULL, NULL, &res);

	if (error)
		putlog(LOG1, "Unable to resolve %s: %s", host,
		       gai_strerror(error));

	return res;
} /* LookupHostname() */

/*
 * LookupAddress()
 * 
 * Attempt to resolve 'ip' address into hostname.
 * Return: a addrinfo list with the hostname information
*/
char *LookupAddress(struct sockaddr *ss, socklen_t sslen)
{
	char *name;
	int error;

	name = MyMalloc(HOSTLEN);
	memset(name, 0, HOSTLEN);

	error = getnameinfo(ss, sslen, name, HOSTLEN, NULL, 0, 0);

	if (error)
		putlog(LOG1, "Unable to convert address back to ASCII: %s",
		       gai_strerror(error));

	return name;
} /* LookupAddress() */

/*
 * ConvertHostname()
 * Convert address from sockaddr struct back to char string
 */
char *ConvertHostname(struct sockaddr *ss, socklen_t sslen)
{
	char *name;
	int error;

	name = MyMalloc(HOSTLEN);
	memset(name, 0, HOSTLEN);

	error = getnameinfo(ss, sslen, name, HOSTLEN, NULL, 0,
	                    NI_NUMERICHOST);

	if (error)
	{
		putlog(LOG1, "Unable to convert: %s", gai_strerror(error));
	}

	return name;
} /* ConvertHostname() */

/*
 * Originally from squid
 * IgnoreErrno()
 * Find out if we can ignore network error
 */
int IgnoreErrno(int ierrno)
{
	switch (ierrno)
	{
	case EINPROGRESS:
	case EWOULDBLOCK:
#if EAGAIN != EWOULDBLOCK

	case EAGAIN:
#endif

	case EALREADY:
	case EINTR:
#ifdef ERESTART

	case ERESTART:
#endif

		return 1;
	default:
		return 0;
	}
}

/*
 * GetPort()
 * Get port from sockaddr struct
 */
unsigned short int GetPort(struct sockaddr *addr)
{
	unsigned short port = htons(0);

	switch (addr->sa_family)
	{
#ifdef INET6
	case AF_INET6:
		port = ((struct sockaddr_in6 *)addr)->sin6_port;
		break;
#endif

	case AF_INET:
		port = ((struct sockaddr_in *)addr)->sin_port;
		break;
	default:
		putlog(LOG1, "FATAL: Unknown address family %d", addr->sa_family);
	}

	return ntohs(port);
}

/*
 * SetPort()
 * Set port in sockaddr structure
 */
void SetPort(struct sockaddr *addr, unsigned short int port)
{
	switch (addr->sa_family)
	{
#ifdef INET6
	case AF_INET6:
		((struct sockaddr_in6 *)addr)->sin6_port = htons(port);
		break;
#endif

	case AF_INET:
		((struct sockaddr_in *)addr)->sin_port = htons(port);
		break;
	default:
		putlog(LOG1, "FATAL: Unknown address family %d", addr->sa_family);
	}
}

/*
 * ConnectHost()
 * args: hostname, port
 * purpose: bind socket and begin a non-blocking connection to
 *          hostname at port
 * return: socket file descriptor if successful connect, -1 if not
 */
int ConnectHost(const char *hostname, unsigned int port)
{
	struct addrinfo *res, *res_o;
	int socketfd; /* socket file descriptor */
	char *resolved = NULL;

	res_o = res = LookupHostname(hostname);

	while (res != NULL)
	{
		resolved = ConvertHostname(res->ai_addr, res->ai_addrlen);

		putlog(LOG1, "Connecting to %s[%s] tcp/%d",
		       hostname, resolved, port);

		if ((socketfd = socket(res->ai_family, SOCK_STREAM, 6)) == -1)
		{
			MyFree(resolved);
			res = res->ai_next;
			continue;
		}

		SetSocketOptions(socketfd);
		SetPort((struct sockaddr *)res->ai_addr, port);

		if (LocalHostName)
		{
			/* bind to virtual host */
			if (bind(socketfd, (struct sockaddr *)&LocalAddr,
			         LocalAddrSize) == -1)
			{
				char *resolved2;
				resolved2 = ConvertHostname((struct sockaddr *)&LocalAddr,
				                            LocalAddrSize);

				putlog(LOG1, "FATAL: Unable to bind virtual host %s[%s]: %s",
				       LocalHostName, resolved2, strerror(errno));

				MyFree(resolved2);
			}
		}

		if ((connect(socketfd, (struct sockaddr *)res->ai_addr,
		             res->ai_addrlen)) == -1)
		{
			putlog(LOG1, "Error connecting to %s[%s] tcp/%d: %s",
			       hostname, resolved, port, strerror(errno));

			close(socketfd);
			MyFree(resolved);
			res = res->ai_next;
			continue;
		}

		/* nope, no error whatsoever */
		freeaddrinfo(res_o);
		MyFree(resolved);
		/* not really the smartest to do... */
		SetNonBlocking(socketfd);
		return socketfd;
	}

	if (res_o != NULL)
		freeaddrinfo(res_o);

	return -1;
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
	int errval;
	socklen_t errlen;

	assert(hubptr != 0);

	ClearHubConnect(hubptr);

	errval = 0;
	errlen = sizeof(errval);

	if (getsockopt(HubSock, SOL_SOCKET, SO_ERROR,
	               (void *)&errval, &errlen) == -1)
	{
		putlog(LOG1, "getsockopt(SO_ERROR) failed: %s", strerror(errno));
		return 0;
	}

	if (errval > 0)
	{
		putlog(LOG1,
		       "Error connecting to %s tcp/%d: %s",
		       hubptr->hostname, hubptr->port, strerror(errval));

		return 0;
	}

	signon();

	hubptr->connect_ts = current_ts;

#ifdef RECORD_RESTART_TS
	most_recent_sjoin = current_ts;
#endif

	SendUmode(OPERUMODE_Y, "*** Connected to %s tcp/%d",
	          hubptr->hostname, hubptr->port);
	putlog(LOG1, "Connected to %s tcp/%d", hubptr->hostname, hubptr->port);

	burst_complete = 0;

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

	struct DccUser *dccptr;
	struct PortInfo *tempport;
	fd_set readfds,
	writefds;

	time_t curr_ts;
	time_t last_ts = current_ts;

	spill[0] = '\0';

	for (;;)
	{

		/*
		 * DoTimer() will be called from this code, and so we need curr_ts
		 * here. And we also need time setup here -kre
		 */
		curr_ts = current_ts = time(NULL);

#ifdef HIGHTRAFFIC_MODE

		if (HTM)
		{
			int htm_count;

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
				{
					read_socket_done = 1;
					return;
				}

		} /* if (HTM) */
		else

#endif /* HIGHTRAFFIC_MODE */

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

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		TimeOut.tv_sec = 0L;
		TimeOut.tv_usec = 20000L;

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
					{
						read_socket_done = 1;
						return; /* something bad happened */
					}
				}
			} /* if (currenthub && (HubSock != NOSOCKET)) */

			for (tempport = PortList; tempport; tempport = tempport->next)
			{
				if (tempport->socket == NOSOCKET)
					continue;

				if (FD_ISSET(tempport->socket, &readfds))
				{
					/*
					 * a client has connected on one of our listening ports
					 * (P: lines) - accept their connection, and perform
					 * ident etc
					 */

					ConnectClient(tempport);
				}
			}

			/* this is for info coming from a dcc/telnet/tcm client */
			for (dccptr = connections; dccptr != NULL; dccptr = dccnext)
			{
				assert(dccptr->socket != NOSOCKET);
				dccnext = dccptr->next;

				if (IsDccConnect(dccptr)
				        && FD_ISSET(dccptr->socket, &writefds))
				{
					if (!GreetDccUser(dccptr))
						DeleteDccClient(dccptr);

					continue;
				}

				if (dccptr->authfd >= 0)
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

			dccnext = NULL;   /* XXX */
		} /* if (SelectResult > 0) */
		else
		{
			/* Also check whether is errno set at all.. -kre */
			if ((SelectResult == (-1)) && errno && (errno != EINTR))
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
	char *ch;
	char *linech;
#ifdef EXTREMEDEBUG

	FILE *fp;
#endif

	if (HubSock == NOSOCKET)
		return 0;

	/* read in a line */
	length = recv(HubSock, buffer, BUFSIZE, 0);

	/* hub closed connection */
	if (length == 0)
		return 0;

	/* error in read */
	if (length == -1)
	{
			if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
				/* there's just nothing to read */
				return 2;
			else
			{
				/* the connection was closed */
				putlog(LOG1, "Read error from server: %s",
			       strerror(errno));
				return 0;
			}
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
		char tmp;
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
			nextparam = NULL;

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
							nextparam = NULL;

							/*
							 * Unfortunately, the first space has already been set to \0
							 * above, so reset to to a space character
							 */
							*(linech - 1) = ' ';
						}
						else
							nextparam = linech;

						if (paramc >= MAXPARAM)
							nextparam = NULL;
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
	char *ch;
	char *linech;

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
	linech = connptr->spill + connptr->offset;

	while (*ch)
	{
		char tmp;

		tmp = *ch;
		if (IsEOL(tmp))
		{
			*linech = '\0';

			if (IsEOL(*connptr->spill))
			{
				ch++;
				continue;
			}

			/* process the line */
			if (connptr->flags & SOCK_TCMBOT)
			{
				/* process line from bot */
				if (BotProcess(connptr, connptr->spill))
					return 1;
			}
			else
				/* process line from client */
				DccProcess(connptr, connptr->spill);

			linech = connptr->spill;
			connptr->offset = 0;

			/*
			 * If the line ends in \r\n, advance past the \n
			 */
			if (IsEOL(*(ch + 1)))
				ch++;
		}
		else
		{
			/* make sure we don't overflow spill */
			if (linech < (connptr->spill + (sizeof(connptr->spill) - 1)))
			{
				*linech++ = tmp;
				connptr->offset++;
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
	int option = 1;

	/*
	 * SO_REUSEADDR will enable immediate local address reuse of
	 * the port this socket is bound to
	 */
	if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&option,
	               sizeof(option)) == -1)
	{
		putlog(LOG1, "SetSocketOptions: setsockopt(SO_REUSEADDR) failed: %s",
		       strerror(errno));
		return;
	}

	/* disable Nagle, if possible */
	if (setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *)&option,
	               sizeof(option)) == -1)
	{
		putlog(LOG1, "SetSocketOptions: setsockopt(TCP_NODELAY) failed: %s",
		       strerror(errno));
		return;
	}
} /* SetSocketOptions() */

/*
  DoListen()
*/
void DoListen(struct PortInfo *portptr)
{
	struct addrinfo hints;
	struct addrinfo *res, *res_o;
	int error;
	char port[MAXLINE];

	/* can't use standard LookupHostname */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	ircsprintf(port, "%d", portptr->port);
	error = getaddrinfo(LocalHostName, port, &hints, &res);

	res_o = res;

	if (error)
	{
		putlog(LOG1,
		       "Unable to get local addresses, this should never happen: %s",
		       gai_strerror(error));
		return;
	}

	while (res != NULL)
	{
		if ((portptr->socket = socket(res->ai_family, SOCK_STREAM, 6)) == -1)
		{
			res = res->ai_next;
			portptr->socket = NOSOCKET;
			continue;
		}

		/* set various socket options */
		SetSocketOptions(portptr->socket);
		SetPort((struct sockaddr *)res->ai_addr, portptr->port);

		if (bind(portptr->socket, res->ai_addr, res->ai_addrlen) == -1)
		{
			putlog(LOG1, "FATAL: Unable to bind port tcp/%d: %s",
			       portptr->port, strerror(errno));
			close(portptr->socket);
			portptr->socket = NOSOCKET;
		}
		else
#ifdef SOMAXCONN
			if (listen(portptr->socket, SOMAXCONN) == -1)
#else

			if (listen(portptr->socket, HYBSERV_SOMAXCONN) == -1)
#endif

			{
				putlog(LOG1, "Unable to listen on port tcp/%d: %s",
				       portptr->port, strerror(errno));
				close(portptr->socket);
				portptr->socket = NOSOCKET;
			}
			else
			{
				putlog(LOG1,
				       "Listener successfully started on host [%s] port tcp/%d",
				       (LocalHostName != NULL) ? LocalHostName : "*",
				       portptr->port);
			}

		res = res->ai_next;
	}

	portptr->tries++;

	if (res_o != NULL)
		freeaddrinfo(res_o);
} /* DoListen() */

/*
 * signon()
 *
 * args: none
 * purpose: send the PASS / CAPAB / SERVER handshake
 * return: none
 */
void signon(void)
{
	/* Hybrid6 and 7 handshake -kre */
#ifdef HYBRID_ONLY
	toserv("PASS %s :TS\r\nCAPAB :EX"
#ifdef DANCER
	       " DNCR SRV"
#endif /* DANCER */
#ifdef GECOSBANS
	       /* Send gecosbans capab -Janos */
	       " DE"
#endif /* GECOSBANS */
#ifdef HYBRID7
	       /* Send most of Hybrid7 CAPABS -kre && Janos */
	       " KLN GLN HOPS IE HUB AOPS"
#endif /* HYBRID7 */
	       "\r\n", currenthub->password);
#endif /* HYBRID_ONLY */

	toserv("SERVER %s 1 :%s\r\n", Me.name, Me.info);
} /* signon() */
