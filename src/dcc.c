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
#include "client.h"
#include "conf.h"
#include "config.h"
#include "dcc.h"
#include "flood.h"
#include "hash.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "mystring.h"
#include "operserv.h"
#include "server.h"
#include "settings.h"
#include "sock.h"
#include "sprintf_irc.h"

static int MakeConnection(char *host, int port, struct Luser *lptr);
static int IsAuth(struct DccUser *);
static int RequestIdent(struct DccUser *, struct sockaddr *, socklen_t);
static void LinkDccClient(struct DccUser *dccptr);
static void UnlinkDccClient(struct DccUser *dccptr);
#ifdef HAVE_BROKEN_INT_GETADDRINFO
void DCCNormalizeIP(char *, int);
#endif

/*
 * Global - list of dcc/telnet connections
 */
struct DccUser *connections = NULL;

/*
SendMotd
  args: int sockfd
  purpose: output message of the day to the socket 'sockfd'
  return: none
*/

void
SendMotd(int sockfd)

{
	char line[MAXLINE + 1];
	char *final;
	char *ch;
	FILE *fp;

	if ((fp = fopen(DccMotdFile, "r")) == NULL)
	{
		writesocket(sockfd, "MOTD file missing\r\n");
		return;
	}

	while (fgets(line, sizeof(line), fp))
	{
		if ((ch = strchr(buffer, '\n')) != NULL)
			*ch = '\0';

		if (*line != '\0')
		{
			final = Substitute(NULL, line, sockfd);
			if (final && (final != (char *) -1))
			{
				writesocket(sockfd, final);
				MyFree(final);
			}
		}

		writesocket(sockfd, "\r\n");
	}

	fclose(fp);
} /* SendMotd() */

/*
IsAuth()
 Determine if authorization checks are complete for dccptr.
Such checks include ident request, and correct nickname/password
for telnet clients.
Return: 1 if client is authorized
        0 if not
*/

static int
IsAuth(struct DccUser *dccptr)

{
	if (!dccptr)
		return (0);

	if ((dccptr->socket == NOSOCKET) ||
	        (dccptr->flags & SOCK_NEEDID) ||
	        IsDccPending(dccptr))
		return (0);

	return (1);
} /* IsAuth() */

/*
BroadcastDcc()
 Send the message (specified in 'format') to users who match
the integer 'towho'. 'towho' can be either DCCALL (all users),
or DCCOPS (opers and above only)
 
 return: none
*/

void
BroadcastDcc(int towho, char *format, ...)

{
	char tcmbuf[MAXLINE * 2];
	char buffer[MAXLINE * 2];
	struct DccUser *dccptr;
	va_list args;

	va_start(args, format);

	vsprintf_irc(buffer, format, args);

	va_end(args);

	/*
	 * Construct the string we want to send
	 */
	tcmbuf[0] = '\0';
	if (towho == DCCALL)
	{
		/* Make the tcm buffer look like: (OperServ) Message */
		ircsprintf(tcmbuf, "(%s) %s", n_OperServ, buffer);
	}

	for (dccptr = connections; dccptr; dccptr = dccptr->next)
	{
		/*
		 * Don't broadcast to a client who hasn't entered their
		 * password yet, or who hasn't responded to an ident
		 * request yet
		 */
		if (!IsAuth(dccptr))
			continue;

		switch (towho)
		{
		case DCCALL:
			{
				/* send it to everybody */

				if ((dccptr->flags & SOCK_TCMBOT) && (tcmbuf[0]))
				{
					/*
					 * This client is a tcm bot, so send it tcmbuf[]
					 */

					writesocket(dccptr->socket, tcmbuf);
				}
				else
				{
					/*
					 * This is a user connection, send them buffer[]
					 */
					writesocket(dccptr->socket, buffer);
				}

				break;
			} /* case DCCALL */

		case DCCOPS:
			{
				struct Userlist *tempuser;

				/*
				 * send it only to operators (must have an "o" in their
				 * O: line)
				 */

				/*
				 * Assume dccptr has already been verified to come
				 * from the correct host etc, so allow GetUser()
				 * to match only nicknames if need be (they might
				 * be telnetting from an unknown host, or services
				 * might be squit'd from the network, and they're
				 * dcc chatting)
				 */
				tempuser = DccGetUser(dccptr);

				if (!tempuser)
				{
					/*
					 * One reason tempuser might be null here is if the user
					 * connected to the partyline, but then his/her O: was
					 * commented out of hybserv.conf and rehashed
					 */
					break;
				}

				if (dccptr->flags & SOCK_TCMBOT)
					break;

				if (IsOper(tempuser) &&
				        IsRegistered(NULL, dccptr->socket))
					writesocket(dccptr->socket, buffer);

				break;
			} /* case DCCOPS */

		default:
			break;
		} /* switch (towho) */
	} /* for (dccptr = connections; dccptr; dccptr = dccptr->next) */
} /* BroadcastDcc() */

/*
SendDccMessage()
 Called when a dcc or telnet client sends a message to the partyline.
Send the message to all other connected clients in the format:
 
<Nickname@OperServ> message
 
*/

void
SendDccMessage(struct DccUser *from, char *message)

{
	char final[MAXLINE + 1];
	struct DccUser *dccptr;

	if (!from || !message)
		return;

	ircsprintf(final, "<%s@%s> %s", from->nick, n_OperServ, message);
	strlcat(final, "\r\n", sizeof(final));

	/* now send the string to all clients */
	for (dccptr = connections; dccptr; dccptr = dccptr->next)
	{
		/*
		 * Don't broadcast to a client who hasn't entered their
		 * password yet, or who hasn't responded to an ident
		 * request yet
		 */
		if (!IsAuth(dccptr))
			continue;

		writesocket(dccptr->socket, final);
	}
} /* SendDccMessage() */

/*
SendUmode()
 Send message 'format' to all users on the partyline that
currently have 'umode' set in their usermodes
 
Note to self: don't put debug statements in here or an infinite
loop will occur, since debug() calls SendUmode() to broadcast
to +d people
*/

void
SendUmode(int umode, char *format, ...)

{
	va_list args;
	char buffer[MAXLINE * 2];
	struct DccUser *dccptr;
	struct Userlist *tempuser;
	struct tm *tmp_tm;
	char timestr[MAXLINE + 1];

	if (ActiveFlood)
		return;

	if (umode != OPERUMODE_D)
		if (IsFlood())
			ActiveFlood = current_ts;

	if (!ActiveFlood)
	{
		va_start(args, format);

		vsprintf_irc(buffer, format, args);

		va_end(args);
	} /* if (!ActiveFlood) */

	tmp_tm = localtime(&current_ts);

	ircsprintf(timestr, "[%02d:%02d:%02d]",
	           tmp_tm->tm_hour, tmp_tm->tm_min, tmp_tm->tm_sec);

	for (dccptr = connections; dccptr; dccptr = dccptr->next)
	{
		if (!IsAuth(dccptr))
			continue;

		if ((tempuser = DccGetUser(dccptr)))
		{
			if (!IsOper(tempuser))
				continue;

			if (!ActiveFlood && tempuser->umodes & umode)
			{
				tosock(dccptr->socket, "%s %s\r\n", timestr, buffer);
			}
		}
	}
} /* SendUmode() */

/*
 * ConnectClient()
 * purpose: accept remote connection on portptr->socket
 * return: none 
 */
void ConnectClient(struct PortInfo *portptr)
{
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	char *resolved;
	int goodid = 1, fd, sock;
	struct DccUser *tempconn;

	if (portptr == NULL)
		return;

	if ((fd = accept(portptr->socket, (struct sockaddr *)&addr,
	                 &addrlen)) == -1)
	{
		putlog(LOG1, "Error in accept on socket %d: %s", portptr->socket,
		       strerror(errno));
		return;
	}

	if (MaxConnections && (Network->TotalConns >= MaxConnections))
	{
		/* maximum users reached */
		writesocket(fd, "Maximum connections reached, try again later\r\n");
		close(fd);
		return;
	}

	resolved = LookupAddress((struct sockaddr *)&addr, addrlen);

	tempconn = MyMalloc(sizeof(struct DccUser));

	tempconn->socket = fd;

	if (resolved != NULL)
		tempconn->hostname = resolved;
	else
	{
		tempconn->hostname = ConvertHostname((struct sockaddr *)&addr,
		                                     addrlen);
	}

	tempconn->username = NULL;
	tempconn->nick = NULL;
	tempconn->idle = current_ts;
	tempconn->port = GetPort((struct sockaddr *)&addr);

	/* not really the smartest to do... */
	SetNonBlocking(tempconn->socket);

	switch (portptr->type)
	{
	case PRT_TCM:
		{
			tempconn->flags = (SOCK_TCMBOT | SOCK_PENDING | SOCK_NEEDID);

			putlog(LOG1, "Received tcm connection from [%s:%d] on port %d",
			       tempconn->hostname, tempconn->port, portptr->port);
			SendUmode(OPERUMODE_B,
			          "*** Received tcm connection from [%s:%d] on port %d",
			          tempconn->hostname, tempconn->port, portptr->port);

			sock = RequestIdent(tempconn, (struct sockaddr *)&addr, addrlen);
			if (sock < 0)
				goodid = 0;

			break;
		}

	case PRT_USERS:
		{
			if (portptr->host)
			{
				/* Check if incoming connection has an authorized host */
				if (match(portptr->host, tempconn->hostname) == 0)
				{
					putlog(LOG1,
					       "Illegal connection attempt on port %d from [%s:%d]",
					       portptr->port, tempconn->hostname, tempconn->port);

					SendUmode(OPERUMODE_B,
					          "*** Unauthorized connection attempt from [%s:%d] on port %d",
					          tempconn->hostname, tempconn->port,
					          portptr->port);

					close(tempconn->socket);
					MyFree(tempconn->hostname);
					MyFree(tempconn);
					return;
				}
			}

			tempconn->flags = SOCK_NEEDID;

			SendUmode(OPERUMODE_Y,
			          "*** Received connection from [%s:%d] on port %d",
			          tempconn->hostname, tempconn->port, portptr->port);

			sock = RequestIdent(tempconn, (struct sockaddr *)&addr, addrlen);
			if (sock < 0)
				goodid = 0;

			break;
		}

	default:
		{
			/* something is really screwed */
			putlog(LOG1,
			       "Invalid connection type specified on port [%d] (%s:%d)",
			       portptr->port, tempconn->hostname, tempconn->port);

			close(tempconn->socket);
			MyFree(tempconn->hostname);
			MyFree(tempconn);
			return;
		}
	}

	if (!goodid)
	{
		/* something went wrong while connecting to their ident port */

		close(tempconn->authfd);
		tempconn->authfd = NOSOCKET;
		tempconn->flags &= ~SOCK_NEEDID;
		tempconn->username = MyStrdup("unknown");

		/* Their username will remain "unknown" from above */

		if (!(tempconn->flags & SOCK_TCMBOT))
			TelnetGreet(tempconn);
	}

	LinkDccClient(tempconn);
} /* ConnectClient() */

/*
readauth()
  dccptr->authfd is active - read the ident reply if there is one
*/

void readauth(struct DccUser *dccptr)
{
	char buffer[MAXLINE + 1], ident[USERLEN + 1];
	int length;

	memset(buffer, 0, sizeof(buffer));
	length = recv(dccptr->authfd, buffer, sizeof(buffer), 0);

	if (length > 0)
	{
		unsigned short remport = 0, locport = 0;
		int num;

		num = sscanf(buffer, "%hu , %hu : USERID : %*[^:]: %10s",
		             &remport, &locport, ident);

		/*
		 * If num != 3, it's possible the remote identd returned
		 * <remoteport> , <localport> : ERROR : NO-USER
		 * so make the ident "unknown"
		 */
		if (num != 3)
			strlcpy(ident, "unknown", sizeof(ident));
	}
	else
	{
		/* connection was closed */
		strlcpy(ident, "unknown", sizeof(ident));
	}

	close(dccptr->authfd);
	dccptr->authfd = NOSOCKET;

	dccptr->flags &= ~SOCK_NEEDID;

	MyFree(dccptr->username);
	dccptr->username = MyStrdup(ident);

	if (!(dccptr->flags & SOCK_TCMBOT))
		TelnetGreet(dccptr);
} /* readauth() */

/*
writeauth()
  Request ident reply for dccptr->authfd
*/

void
writeauth(struct DccUser *dccptr)

{
	char idstr[MAXLINE + 1];
	struct sockaddr_storage local, remote;
	socklen_t len;

	len = sizeof(local);
	if ((getsockname(dccptr->socket, (struct sockaddr *)&local,
	                 &len) == -1) ||
	        (getpeername(dccptr->socket,
	                     (struct sockaddr *)&remote, &len) == -1))
	{
		/* something went wrong */
		putlog(LOG1,
		       "FATAL: get{sock,peer}name error for %d: %s",
		       dccptr->socket, strerror(errno));
		return;
	}

	/* send ident request */
	ircsprintf(idstr, "%u , %u\r\n",
	           (unsigned int)ntohs(GetPort((struct sockaddr *)&remote)),
	           (unsigned int)ntohs(GetPort((struct sockaddr *)&local)));
	writesocket(dccptr->authfd, idstr);

	dccptr->flags &= ~SOCK_WRID;
} /* writeauth() */

/*
 * RequestIdent()
 * Connect to the ident port of dccptr
 */
static int RequestIdent(struct DccUser *dccptr, struct sockaddr *addr,
                        socklen_t addrlen)
{
	int domain = PF_INET;
	struct sockaddr_storage laddr;
	struct sockaddr_storage remote;
	socklen_t laddrlen = sizeof(laddr);

	switch (addr->sa_family)
	{
#ifdef INET6
	case AF_INET6:
		domain = PF_INET6;
		break;
#endif

	case AF_INET:
		domain = PF_INET;
		break;
	default:
		putlog(LOG1, "FATAL: Unknown address family %d", addr->sa_family);
	}

	if ((dccptr->authfd = socket(domain, SOCK_STREAM, 6)) == -1)
	{
		putlog(LOG1, "FATAL: Problem allocating socket: %s",
		       strerror(errno));
		return -1;
	}

	SetSocketOptions(dccptr->authfd);
	SetNonBlocking(dccptr->authfd);

	if (getsockname(dccptr->socket, (struct sockaddr *)&laddr, &laddrlen) == -1)
	{
		putlog(LOG1, "FATAL: Unable to get current address: %s", strerror(errno));
		close(dccptr->authfd);
		return -1;
	}

	SetPort((struct sockaddr *)&laddr, 0);

	if (bind(dccptr->authfd, (struct sockaddr *)&laddr, laddrlen) == -1)
	{
		putlog(LOG1, "FATAL: Unable to bind port tcp/%d: %s",
		       0, strerror(errno));
		close(dccptr->authfd);
		return -1;
	}

	memcpy(&remote, addr, sizeof(remote));
	SetPort((struct sockaddr *)&remote, 113);

	if ((connect(dccptr->authfd, (struct sockaddr *)&remote,
	             addrlen)) == -1)
	{
		if (IgnoreErrno(errno))
		{
			/* probably all alright */
			dccptr->flags |= SOCK_WRID;
			return(dccptr->authfd);
		}
		else
		{
			/* error, can't retry */
			putlog(LOG1, "Error connecting to %s tcp/%d: %s",
			       dccptr->hostname, 113, strerror(errno));

			close(dccptr->authfd);
			return -1;
		}
	}

	/* no problems at all */
	dccptr->flags |= SOCK_WRID;
	return(dccptr->authfd);
} /* RequestIdent() */

/*
TelnetGreet()
  Send new telnet connection version and "enter nickname" lines
*/

void
TelnetGreet(struct DccUser *dccptr)

{
	char ver[MAXLINE + 1];

	if (!dccptr)
		return;

	if (dccptr->socket == NOSOCKET)
		return;

	SetDccPending(dccptr);

	ircsprintf(ver, "\r\nHybserv2 (TS Services version %s)\r\n", hVersion);
	writesocket(dccptr->socket, ver);

	writesocket(dccptr->socket, "\r\nEnter nickname\r\n");
} /* TelnetGreet() */

/*
MakeConnection()
  args: char *host, int port, struct Luser *lptr
  purpose: create a DCC CHAT session for 'lptr->nick'
  return: 1 if connection was successful, 0 if not
*/

static int
MakeConnection(char *host, int port, struct Luser *lptr)

{
	struct Userlist *tempuser;
	struct DccUser *dccptr;

	if (!host || !lptr)
		return 0;

	if (MaxConnections && (Network->TotalConns >= MaxConnections))
	{
		/* maximum users reached */
		notice(n_OperServ, lptr->nick,
		       "Maximum DCC connections have been reached, try again later");
		return 0;
	}

	if (lptr->flags & L_OSREGISTERED)
		tempuser = GetUser(1, lptr->nick, lptr->username, lptr->hostname);
	else
		tempuser = GetUser(0, lptr->nick, lptr->username, lptr->hostname);

	if (!tempuser)
		return 0;

	dccptr = (struct DccUser *) MyMalloc(sizeof(struct DccUser));
	memset(dccptr, 0, sizeof(struct DccUser));

	dccptr->socket = ConnectHost(host, port);
	if (dccptr->socket == -1)
	{
		MyFree(dccptr);
		return 0;
	}

	dccptr->flags = SOCK_DCC;

	SetDccConnect(dccptr);

	dccptr->port = port;
	dccptr->idle = current_ts;
	dccptr->authfd = NOSOCKET;

	dccptr->nick = MyStrdup(lptr->nick);
	dccptr->username = MyStrdup(lptr->username);
	dccptr->hostname = MyStrdup(lptr->hostname);

	if (!(lptr->flags & L_OSREGISTERED))
		SetDccPending(dccptr);

	/* add them to list */
	LinkDccClient(dccptr);

	return 1;
} /* MakeConnection() */

/*
GreetDccUser()
 Called upon a successful connection to a dcc client - give them
the motd etc.
 
Return: 1 if successful
        0 if unsuccessful
*/

int
GreetDccUser(struct DccUser *dccptr)

{
	char prefix[MAXLINE + 1],
	sendstr[MAXLINE + 1];
	time_t CurrTime = current_ts;
	struct tm *motd_tm;
	struct Userlist *tempuser;
	int errval;
	socklen_t errlen;

	assert(dccptr != 0);

	ClearDccConnect(dccptr);

	errval = 0;
	errlen = sizeof(errval);
	if (getsockopt(dccptr->socket, SOL_SOCKET, SO_ERROR, (void *)&errval,
	               &errlen) < 0)
	{
		putlog(LOG1, "getsockopt(SO_ERROR) failed: %s", strerror(errno));
		return 0;
	}

	if (errval > 0)
	{
		putlog(LOG1,
		       "Error connecting to %s@%s tcp/%d: %s",
		       dccptr->username, dccptr->hostname, dccptr->port,
		       strerror(errval));

		SendUmode(OPERUMODE_Y,
		          "*** Lost connection to [%s@%s:%d]: %s",
		          dccptr->username, dccptr->hostname, dccptr->port,
		          strerror(errval));

		return 0;
	}

	motd_tm = localtime(&CurrTime);
	ircsprintf(sendstr, "Hybserv2 %s (%d/%d/%d %d:%02d)\r\n",
	           hVersion, 1900 + motd_tm->tm_year, motd_tm->tm_mon + 1,
	           motd_tm->tm_mday, motd_tm->tm_hour, motd_tm->tm_min);
	writesocket(dccptr->socket, sendstr);

	/* give new user the motd */
	SendMotd(dccptr->socket);

	if (IsDccPending(dccptr))
	{
		writesocket(dccptr->socket,
		            "You must .identify before you may use commands\r\n");
		tempuser = GetUser(0, dccptr->nick, dccptr->username, dccptr->hostname);
	}
	else
		tempuser = GetUser(1, dccptr->nick, dccptr->username, dccptr->hostname);

	assert(tempuser != 0);

	if (IsServicesAdmin(tempuser))
		strlcpy(prefix, "Admin(%)", sizeof(prefix));
	else if (IsAdmin(tempuser))
		strlcpy(prefix, "Admin", sizeof(prefix));
	else if (IsOper(tempuser))
		strlcpy(prefix, "Oper", sizeof(prefix));
	else
		strlcpy(prefix, "User", sizeof(prefix));

	/* inform everyone of new user */
	BroadcastDcc(DCCALL, "%s %s (%s@%s) has connected\r\n",
	             prefix, dccptr->nick, dccptr->username, dccptr->hostname);

	return 1;
} /* GreetDccUser() */

/*
onctcp()
  args: char *nick, char *msg
  purpose: handle ctcp message 'msg' sent by 'nick' to 'target'
  return: none
*/

void
onctcp(char *nick, char *target, char *msg)

{
	char temp[MAXLINE + 1];
	int goodDCC;
	struct Luser *lptr;
	struct Userlist *tempuser;

	if (!msg)
		return;

	if (!*++msg)
		return;

	lptr = FindClient(nick);
	if (!lptr)
		return;

	if (!ircncmp(msg, "ACTION", 6))
		return; /* don't log ctcp actions */

	if (ircncmp(msg, "PING", 4) == 0)
	{
		notice(target, nick, "%s", msg - 1);
		SendUmode(OPERUMODE_Y,
		          "%s: CTCP PING received from %s!%s@%s",
		          target,
		          lptr->nick,
		          lptr->username,
		          lptr->hostname);
		return;
	}

	if (ircncmp(msg, "VERSION", 7) == 0)
	{
#ifdef ADMININFO
		struct Luser *ouser = NULL;
#endif /* ADMININFO */

		notice(target, nick,
		       "\001VERSION Hybserv2 TS Services version %s\001",
		       hVersion);
		notice(target, nick,
		       "\001VERSION Compiled with NICKLEN=%d, "
		       "CHANNELLEN=%d, TOPICLEN=%d\001",
		       NICKLEN, CHANNELLEN, TOPICLEN);
		notice(target, nick,
		       "\001VERSION Administrator: %s\001",
		       Me.admin);

#ifdef ADMININFO
		/* Print active operators that have identified to OperServ. This could
		 * be a little performance hit, blah. -kre */
		for (ouser = ClientList; ouser; ouser = ouser->next)
		{
			if (ouser->flags & L_OSREGISTERED)
				notice(target, nick, "\001VERSION Active operators: %s"
				       " (%s@%s)\001", ouser->nick, ouser->username,
				       ouser->hostname);
		}
#endif /* ADMININFO */

		SendUmode(OPERUMODE_Y,
		          "%s: CTCP VERSION received from %s!%s@%s",
		          target,
		          lptr->nick,
		          lptr->username,
		          lptr->hostname);
		return;
	}

	if (ircncmp(msg, "DCC CHAT", 8) == 0)
	{
		int acnt;
		char **av;
		char buff[MAXLINE + 1];

		if (irccmp(target, n_OperServ) != 0)
			return; /* only let n_OperServ accept dcc chats */

		goodDCC = 1;

		if (lptr->flags & L_OSREGISTERED)
		{
			/*
			 * If they have already registered through /msg, allow
			 * them to be from a different host, by passing 1 to
			 * GetUser() so it can only match nicknames and
			 * not hostnames if need be - if they are not registered,
			 * make sure hostnames match, otherwise anyone could just
			 * /nick <Oline-nickname> and gain dcc chat access
			 */
			tempuser = GetUser(1, lptr->nick, lptr->username, lptr->hostname);
		}
		else
			tempuser = GetUser(0, lptr->nick, lptr->username, lptr->hostname);

		if (tempuser)
		{
			if (CanChat(tempuser))
			{
				goodDCC = 1;

				if (OpersOnlyDcc && !IsOper(tempuser))
					goodDCC = 0;
			}
			else
				goodDCC = 0;

			if (IsAdmin(tempuser))
				goodDCC = 1;
		} /* if (tempuser) */
		else
			goodDCC = 0;

		if (goodDCC == 0)
		{
			notice(n_OperServ, nick,
			       "You are not authorized to use this service");

			SendUmode(OPERUMODE_Y,
			          "*** Unauthorized Dcc Chat attempt from %s (%s@%s)",
			          nick,
			          lptr->username,
			          lptr->hostname);

			notice(n_OperServ, nick, "\001DCC REJECT chat chat\001");

			putlog(LOG1, "Unauthorized Dcc Chat attempt from %s!%s@%s",
			       nick,
			       lptr->username,
			       lptr->hostname);

			return;
		}

		strlcpy(temp, msg, sizeof(temp));

		acnt = SplitBuf(temp, &av);
		if (acnt < 5)
		{
			notice(n_OperServ, nick, "DCC CHAT connection failed");
			notice(n_OperServ, nick, "\001DCC REJECT chat chat\001");
			putlog(LOG1, "Invalid DCC string received from [%s]: %s",
			       nick, temp);
			MyFree(av);
			return;
		}

		strlcpy(buff, av[3], sizeof(buff));
#ifdef HAVE_BROKEN_INT_GETADDRINFO

		DCCNormalizeIP(buff, sizeof(buff));
#endif

		if (atoi(av[4]) < 1024)
		{
			notice(n_OperServ, nick,
			       "Invalid DCC CHAT port specified.  Nice Try.");
			notice(n_OperServ, nick, "\001DCC REJECT chat chat\001");
			putlog(LOG1, "Bogus DCC CHAT port (%s) specified by %s!%s@%s",
			       av[4],
			       lptr->nick,
			       lptr->username,
			       lptr->hostname);
		}
		else if (!MakeConnection(buff, atoi(av[4]), lptr))
		{
			notice(n_OperServ, nick, "DCC CHAT connection failed");
			notice(n_OperServ, nick, "\001DCC REJECT chat chat\001");
		}

		MyFree(av);
		return;
	}
	else
	{
		/*
		 * Chop off ending \001
		 */

		strlcpy(temp, msg, sizeof(temp));

		SendUmode(OPERUMODE_Y,
		          "%s: CTCP [%s] received from %s!%s@%s",
		          target, temp, lptr->nick, lptr->username, lptr->hostname);
	}
} /* onctcp() */

/*
ConnectToTCM()
  args: char *nick, struct Botlist *bptr
  purpose: attempt to connect to tcm bot bptr
  return: 1 on a successful connection, 0 if not
*/

int
ConnectToTCM(char *nick, struct Botlist *bptr)

{
	char sendstr[MAXLINE + 1];
	struct Luser *lptr;
	struct DccUser *dccptr, *nptr, *tmp;

	if (!nick)
	{
		/*
		 * nick will be null if we are trying to connect to all tcms
		 * at once (AutoLinkFreq)
		 */
		nptr = NULL;
	}
	else
		if (!(nptr = IsOnDcc(nick)))
			return 0;

	dccptr = (struct DccUser *) MyMalloc(sizeof(struct DccUser));
	dccptr->socket = ConnectHost(bptr->hostname, bptr->port);

	if (dccptr->socket == -1)
	{
		SendUmode(OPERUMODE_B,
		          "*** Unable to connect to tcm %s [%s:%d]: %s",
		          bptr->name,
		          bptr->hostname,
		          bptr->port,
		          strerror(errno));

		if (nptr)
		{
			ircsprintf(sendstr, "Unable to connect to port %d of %s: %s\r\n",
			           bptr->port, bptr->hostname, strerror(errno));
			writesocket(nptr->socket, sendstr);
		}
		MyFree(dccptr);
		return (0);
	}

	/*
	  The fields for linking to a tcm are in order:
	  <remote tcm's nick> <connecting bot's nick> <password>
	*/
	ircsprintf(sendstr, "%s %s %s\r\n",
	           bptr->name, n_OperServ, bptr->password);
	writesocket(dccptr->socket, sendstr);

	dccptr->flags = (SOCK_TCMBOT | SOCK_BOTHUB);
	dccptr->authfd = NOSOCKET;
	dccptr->nick = MyStrdup(bptr->name);

	lptr = FindClient(dccptr->nick); /* check if bot is on network */
	if (!lptr)
	{
		/* just make the bot's hostname ip:port */
		dccptr->username = MyStrdup("unknown");

		ircsprintf(sendstr, "%s:%d", bptr->hostname, bptr->port);
		dccptr->hostname = MyStrdup(sendstr);
	}
	else /* give the bot a user@host if possible */
	{
		dccptr->username = MyStrdup(lptr->username);
		dccptr->hostname = MyStrdup(lptr->hostname);
	}

	LinkDccClient(dccptr);

	/* inform everyone of new bot */
	for (tmp = connections; tmp; tmp = tmp->next)
	{
		if (!IsAuth(tmp))
			continue;

		if (tmp == dccptr)
			continue;

		if (tmp->flags & SOCK_TCMBOT)
		{
			ircsprintf(sendstr, "(%s) Linked to %s [%s@%s]\r\n",
			           n_OperServ, bptr->name, dccptr->username, dccptr->hostname);
			writesocket(tmp->socket, sendstr);
		}
		else
		{
			ircsprintf(sendstr, "*** Linked to %s [%s@%s]\r\n",
			           bptr->name, dccptr->username, dccptr->hostname);
			writesocket(tmp->socket, sendstr);
		}
	}

	return (1);
} /* ConnectToTCM */

/*
LinkBots()
 Called from DoTimer() to link all tcm bots who are not
already linked
*/

void
LinkBots()

{
	struct Botlist *bptr;
	struct DccUser *dccptr;
	int goodlink;

	if (BotList)
	{
		goodlink = 1;
		for (bptr = BotList; bptr; bptr = bptr->next)
		{
			for (dccptr = connections; dccptr; dccptr = dccptr->next)
			{
				if ((dccptr->flags & SOCK_TCMBOT) &&
				        (!irccmp(dccptr->nick, bptr->name)))
				{
					/* bot is already linked */
					goodlink = 0;
					break;
				}
			}

			if (goodlink)
				ConnectToTCM(NULL, bptr);
			else
				goodlink = 1;
		}
	} /* if (BotList) */
} /* LinkBots() */

/*
CloseConnection()
  args: struct DccUser *dccptr
  purpose: terminate the connection to dccptr
  return: none
*/

void
CloseConnection(struct DccUser *dccptr)

{
	char prefix[MAXLINE + 1];
	struct Userlist *tempuser;

	if (!dccptr)
		return;

	if ((dccptr->flags & SOCK_DCC) || !IsDccPending(dccptr))
	{
		prefix[0] = '\0';

		if (dccptr->flags & SOCK_TCMBOT)
			strlcpy(prefix, "Bot", sizeof(prefix));
		else
		{
			tempuser = DccGetUser(dccptr);

			if (IsServicesAdmin(tempuser))
				strlcpy(prefix, "Admin(%)", sizeof(prefix));
			else if (IsAdmin(tempuser))
				strlcpy(prefix, "Admin", sizeof(prefix));
			else if (IsOper(tempuser))
				strlcpy(prefix, "Oper", sizeof(prefix));
			else
				strlcpy(prefix, "User", sizeof(prefix));
		}

		BroadcastDcc(DCCALL,
		             "%s %s (%s@%s) has disconnected\r\n",
		             prefix, dccptr->nick, dccptr->username,
		             dccptr->hostname);
	}
	else
	{
		/*
		 * they connected through telnet, but closed the connection
		 * before sending their password
		 */
		SendUmode(OPERUMODE_Y,
		          "*** Lost connection to [%s@%s:%d]",
		          dccptr->username,
		          dccptr->hostname,
		          dccptr->port);
	}

	DeleteDccClient(dccptr);
} /* CloseConnection() */

/*
DeleteDccClient()
 Delete the given client from the connections list
*/

void
DeleteDccClient(struct DccUser *dccptr)

{
	if (dccptr == dccnext)
		dccnext = dccptr->next;

	UnlinkDccClient(dccptr);

	close(dccptr->socket);
	dccptr->socket = NOSOCKET;

	if (dccptr->nick)
		MyFree(dccptr->nick);
	if (dccptr->username)
		MyFree(dccptr->username);
	if (dccptr->hostname)
		MyFree(dccptr->hostname);

	MyFree(dccptr);
} /* DeleteDccClient() */

/*
LinkDccClient()
  Insert 'dccptr' into connections list
*/

static void
LinkDccClient(struct DccUser *dccptr)

{
	dccptr->prev = NULL;
	dccptr->next = connections;
	if (dccptr->next)
		dccptr->next->prev = dccptr;
	connections = dccptr;

	Network->TotalConns++;
} /* LinkDccClient() */

/*
UnlinkDccClient()
 Unlink the given client from the connections list
*/

static void
UnlinkDccClient(struct DccUser *dccptr)

{
	assert(dccptr != 0);

	Network->TotalConns--;

	if (dccptr->next)
		dccptr->next->prev = dccptr->prev;

	if (dccptr->prev)
		dccptr->prev->next = dccptr->next;
	else
		connections = dccptr->next;
} /* UnlinkDccClient() */

/*
ExpireIdent()
 Check if any ident requests have timed out - if so, make
their username "unknown", and allow them to connect
*/

void
ExpireIdent(time_t unixtime)

{
	struct DccUser *dccptr;

	for (dccptr = connections; dccptr; dccptr = dccptr->next)
	{
		if ((dccptr->flags & SOCK_NEEDID) &&
		        (dccptr->authfd >= 0) &&
		        !(dccptr->flags & SOCK_DCC) &&
		        ((unixtime - dccptr->idle) >= IdentTimeout))
		{
			close(dccptr->authfd);
			dccptr->authfd = NOSOCKET;
			dccptr->flags &= ~SOCK_NEEDID;

			if (dccptr->username)
				MyFree(dccptr->username);
			dccptr->username = MyStrdup("unknown");

			/* reset idle time for TelnetTimeout */
			dccptr->idle = current_ts;

			writesocket(dccptr->socket, "\r\nIdent-request timed out\r\n");
			if (!(dccptr->flags & SOCK_TCMBOT))
				TelnetGreet(dccptr);
		}
	}
} /* ExpireIdent() */

/*
ExpireTelnet()
 Go through dcc list and determine if any clients have taken
longer than TelnetTimeout seconds to enter their password -
if so, close the connection
*/

void
ExpireTelnet(time_t unixtime)

{
	struct DccUser *dccptr, *prev;

	prev = NULL;
	for (dccptr = connections; dccptr; )
	{
		if (IsDccPending(dccptr) &&
		        !(dccptr->flags & SOCK_DCC) &&
		        ((unixtime - dccptr->idle) >= TelnetTimeout))
		{
			if (prev)
			{
				prev->next = dccptr->next;
				CloseConnection(dccptr);
				dccptr = prev;
			}
			else
			{
				/*
				 * We're deleting the first element in the list
				 */
				connections = dccptr->next;
				CloseConnection(dccptr);
				dccptr = NULL;
			}
		}

		prev = dccptr;

		/*
		 * If dccptr is NULL, we deleted the first element in
		 * the list, and connections was set to connections->next,
		 * so set dccptr to the new connections, to check what
		 * was previously the second element, but now is the first
		 */

		if (dccptr)
			dccptr = dccptr->next;
		else
			dccptr = connections;
	}
} /* ExpireTelnet() */

/*
CheckEOF()
 Called from DoTimer() to close any connections that have
EOF'd
*/

void
CheckEOF()

{
	struct DccUser *dccptr, *prev;

	prev = NULL;
	for (dccptr = connections; dccptr; )
	{
		if (dccptr->flags & SOCK_EOF)
		{
			if (prev)
			{
				prev->next = dccptr->next;
				CloseConnection(dccptr);
				dccptr = prev;
			}
			else
			{
				connections = dccptr->next;
				CloseConnection(dccptr);
				dccptr = NULL;
			}
		}

		prev = dccptr;

		if (dccptr)
			dccptr = dccptr->next;
		else
			dccptr = connections;
	}
} /* CheckEOF() */

/*
GoodTCM()
  args: struct DccUser *dccptr
  purpose: determine if dccptr matches an L: line for TCM bots
  return: pointer to bot structure in list
*/

struct Botlist *
			GoodTCM(struct DccUser *dccptr)

{
	struct Botlist *tempbot;
	char *user,
	*host;

	if (!dccptr)
		return (NULL);

	if (dccptr->flags & SOCK_NEEDID)
		user = NULL;
	else
		user = dccptr->username;

	host = dccptr->hostname;

	for (tempbot = RemoteBots; tempbot; tempbot = tempbot->next)
	{
		if (user && tempbot->username)
			if (match(tempbot->username, user) == 0)
				continue;

		if (match(tempbot->hostname, host))
			return (tempbot);
	}

	return (NULL);
} /* GoodTCM() */

struct DccUser *
			GetBot(char *bname)

{
	struct DccUser *botptr;

	for (botptr = connections; botptr; botptr = botptr->next)
		if ((botptr->flags & SOCK_TCMBOT) &&
		        (!irccmp(botptr->nick, bname)))
			return(botptr);

	return (NULL);
} /* GetBot() */

/*
IsDccSock()
  args: int sockfd
  purpose: determine if 'sockfd' is being used by a DCC CHATter
  return: ptr to structure in list connections
*/

struct DccUser *
			IsDccSock(int sockfd)

{
	struct DccUser *tmp;

	for (tmp = connections; tmp; tmp = tmp->next)
		if (tmp->socket == sockfd)
			return (tmp);

	return (NULL);
} /* IsDccSock() */

/*
IsOnDcc()
  args: char *nick
  purpose: determine if 'nick' is currently on DCC CHAT
  return: ptr to structure containing 'nick'
*/

struct DccUser *
			IsOnDcc(char *nick)

{
	struct DccUser *tmp;

	if (!nick)
		return (NULL);

	for (tmp = connections; tmp; tmp = tmp->next)
	{
		if ((IsDccPending(tmp) && !(tmp->flags & SOCK_DCC)) ||
		        (tmp->flags & SOCK_NEEDID))
			continue;

		if (!irccmp(tmp->nick, nick))
			return (tmp);
	}

	return (NULL);
} /* IsOnDcc() */

/*
DccGetUser()
 Return O: line structure matching dccptr->nick/username/hostname
*/

struct Userlist *
			DccGetUser(struct DccUser *dccptr)

{
	struct Userlist *userptr;

	if (!dccptr)
		return (NULL);

	/*
	 * Give the arguement 1 to match nicknames if it can't
	 * find a hostname match, because the client may be
	 * a telnet client from a different host, but they
	 * still entered the correct username/password
	 */
	userptr = GetUser(1, dccptr->nick, dccptr->username, dccptr->hostname);

	if (userptr && !(dccptr->flags & SOCK_DCC))
	{
		/*
		 * dccptr is a telnet client, not dcc
		 */

		/*
		 * Check if dccptr->nick matches userptr->nick. If it does
		 * not, the problem is most likely the following scenario:
		 * There are two O: lines:
		 *        UserA *@a.com
		 *        UserB *@b.com
		 * Now, UserA connects to Hybserv (telnet) from *@b.com,
		 * so the above call to GetUser() would have returned
		 * UserB's structure, since hostnames are a higher
		 * priority than nicknames. However, since this is a
		 * telnet connection, the nicknames must also match since
		 * the user is forced to enter their nickname when
		 * they connect. Thus, call GetUser() again, telling
		 * it to match only nicknames, since matching hostnames
		 * has already failed. Otherwise UserA would incorrectly
		 * get UserB's privileges.
		 */
		if (irccmp(dccptr->nick, userptr->nick) != 0)
			userptr = GetUser(1, dccptr->nick, NULL, NULL);
	}

	return (userptr);
} /* DccGetUser() */

/*
telnet_check()
  Check if any characters of 'buf' are telnet codes, and respond
accordingly.  Return 1 if telnet codes found, 0 if not
 
 Telnet Codes
 Type  Decimal Octal
 
 IAC:  255     377
 WILL: 251     373     
 DO:   253     375
 AYT:  246     366
*/

int
telnet_check(int socket, char *buf)

{
	unsigned int ii;
	int    ret = 0;

	if (!buf)
		return 0;

	for (ii = 0; ii < strlen(buf); ii++)
	{
		if ((buf[ii] == '\377') ||
		        (buf[ii] == '\373') ||
		        (buf[ii] == '\375'))
		{
			ret = 1;
		}
		if (buf[ii] == '\366')
		{
			/* Are you there? */
			writesocket(socket, "\r\nAYT Received\r\n");
			ret = 1;
			break;
		}
	}

	return (ret);
} /* telnet_check() */

/*
DccProcess()
  args: int cindex, char *command
  purpose: process 'command' sent by DCC client specified by 'dccptr'
  return: none
*/

void
DccProcess(struct DccUser *dccptr, char *command)

{
	if (!dccptr || !command)
		return;

	Network->RecvB += strlen(command);

	/* reset idle time */
	dccptr->idle = current_ts;

	if (!(dccptr->flags & SOCK_DCC) && IsDccPending(dccptr))
	{
		struct Userlist *tempuser;
		char prefix[MAXLINE + 1];
		int goodconn; /* are they allowed to connect? */

		/*
		 * dccptr is an incoming telnet connection - they should
		 * either be entering their nickname or password right now
		 */

		if (!dccptr->nick)
		{
			/*
			 * they should be entering their nickname now
			 */
			dccptr->nick = MyStrdup(command);

			/*
			 * \377\373\001 (IAC WILL ECHO) - turn off remote telnet echo
			 * for them to enter their password
			 */
			writesocket(dccptr->socket, "Enter password\377\373\001\r\n");

			return;
		}

		if (telnet_check(dccptr->socket, command))
			return;

		/* Check if they have an O: line */
		if (!(tempuser = DccGetUser(dccptr)))
		{
			writesocket(dccptr->socket, "Invalid password\r\n");

			SendUmode(OPERUMODE_Y,
			          "*** Unauthorized connection attempt from %s@%s:%d [%s]",
			          dccptr->username, dccptr->hostname, dccptr->port,
			          dccptr->nick);
			CloseConnection(dccptr);
			return;
		}

		if (CanChat(tempuser))
		{
			goodconn = 1;

			if (OpersOnlyDcc && !IsOper(tempuser))
				goodconn = 0;
		}
		else
			goodconn = 0;

		if (goodconn == 0)
		{
			writesocket(dccptr->socket,
			            "You are not authorized to use this service\r\n");

			SendUmode(OPERUMODE_Y,
			          "Unauthorized connection attempt from %s@%s:%d [%s]",
			          dccptr->username, dccptr->hostname, dccptr->port,
			          tempuser->nick);
			CloseConnection(dccptr);
			return;
		}

		/* check if 'command' matches their password */

		if (!operpwmatch(tempuser->password, command))
		{
			writesocket(dccptr->socket, "Invalid password\r\n");
			SendUmode(OPERUMODE_Y,
			          "*** Invalid password on connection from %s@%s:%d [%s]",
			          dccptr->username,
			          dccptr->hostname,
			          dccptr->port,
			          tempuser->nick);
			CloseConnection(dccptr);
			return;
		}
		else
		{
			MyFree(dccptr->nick);
			dccptr->nick = MyStrdup(tempuser->nick);

			ClearDccPending(dccptr);

			if (IsServicesAdmin(tempuser))
				strlcpy(prefix, "Admin(%)", sizeof(prefix));
			else if (IsAdmin(tempuser))
				strlcpy(prefix, "Admin", sizeof(prefix));
			else if (IsOper(tempuser))
				strlcpy(prefix, "Oper", sizeof(prefix));
			else
				strlcpy(prefix, "User", sizeof(prefix));

			/* turn ECHO back on */
			writesocket(dccptr->socket, "\377\374\001\r\n");

			/* send them the motd */
			SendMotd(dccptr->socket);

			/* inform everyone of new user */
			BroadcastDcc(DCCALL, "%s %s (%s@%s) has connected\r\n",
			             prefix, dccptr->nick, dccptr->username,
			             dccptr->hostname);
		}

		return;
	}

	if (!(dccptr->flags & SOCK_DCC))
		if (telnet_check(dccptr->socket, command))
			return;

	if (*command == '.')
	{
		int clength;
		char *tmp;

		/* its a command, not a message to the partyline */

		++command;
		if (IsRegistered(NULL, dccptr->socket))
		{
			os_process(dccptr->nick, command, dccptr->socket);
			return;
		}

		clength = 0;
		for (tmp = command; *tmp; tmp++)
		{
			if (*tmp == ' ')
				break;
			clength++;
		}

		if ((!ircncmp(command, "REGISTER", clength)) ||
		        (!ircncmp(command, "IDENTIFY", clength)) ||
		        (!ircncmp(command, "HELP", clength)))
			os_process(dccptr->nick, command, dccptr->socket);
		else
			writesocket(dccptr->socket, "You must identify first\r\n");

	}
	else
	{
		/* it was not a command, broadcast it to dcc users */
		if (strlen(command))
			SendDccMessage(dccptr, command);
	}
} /* DccProcess() */

/*
BotProcess()
  args: int bindex, char *line
  purpose: send 'line' to all other bots/users on botnet coming from
           botptr
  return: 1 -> failure (forced close, forget about spill)
          0 -> success
*/

int BotProcess(struct DccUser *botptr, char *line)
{
	struct DccUser *tmp;
	char buf[MAXLINE + 1];

	if (!line)
		return 0;

	Network->RecvB += strlen(line);

	if (IsDccPending(botptr))
	{
		char *mynick;
		char *tcmnick;
		char *password;
		struct Botlist *bptr;
		int goodconn = 1;
		char sendstr[MAXLINE + 1];

		if (!(bptr = GoodTCM(botptr)))
		{
			goodconn = 0;
			SendUmode(OPERUMODE_B,
			          "*** Unauthorized tcm connection received from %s@%s:%d",
			          botptr->username, botptr->hostname, botptr->port);
			putlog(LOG1,
			       "Unauthorized tcm connection received from %s@%s:%d",
			       botptr->username, botptr->hostname, botptr->port);
		}

		if (goodconn)
		{
			while (1)
			{
				/*
				 * this loop is needed so we can break if the tcm gives
				 * an incorrect pass/myname etc
				 */

				mynick = strtok(line, " ");
				tcmnick = strtok(NULL, " ");
				password = strtok(NULL, "\r\n");

				if (!mynick || (irccmp(mynick, n_OperServ) != 0))
				{
					SendUmode(OPERUMODE_B,
					          "*** Invalid myname on tcm connection attempt from %s@%s:%d",
					          botptr->username,
					          botptr->hostname,
					          botptr->port);
					goodconn = 0;
					break;
				}

				if (!tcmnick || (irccmp(tcmnick, bptr->name) != 0))
				{
					SendUmode(OPERUMODE_B,
					          "*** Invalid bot name specified on tcm connection attempt from %s@%s:%d",
					          botptr->username,
					          botptr->hostname,
					          botptr->port);
					goodconn = 0;
					break;
				}

				if (!password || (irccmp(password, bptr->password) != 0))
				{
					SendUmode(OPERUMODE_B,
					          "*** Invalid password specified on tcm connection attempt from %s@%s:%d",
					          botptr->username,
					          botptr->hostname,
					          botptr->port);
					goodconn = 0;
					break;
				}

				botptr->nick = MyStrdup(tcmnick);

				ClearDccPending(botptr);

				SendUmode(OPERUMODE_B,
				          "*** Connection with %s [%s@%s] established",
				          botptr->nick,
				          botptr->username,
				          botptr->hostname);

				for (tmp = connections; tmp; tmp = tmp->next)
				{
					if (tmp == botptr)
						continue;

					if (tmp->flags & SOCK_TCMBOT)
					{
						ircsprintf(sendstr, "(%s) Linked to %s [%s@%s]\r\n",
						           n_OperServ, botptr->nick, botptr->username,
						           botptr->hostname);
						writesocket(tmp->socket, sendstr);
					}
					else
					{
						ircsprintf(sendstr, "*** Linked to %s [%s@%s]\r\n",
						           botptr->nick, botptr->username, botptr->hostname);
						writesocket(tmp->socket, sendstr);
					}
				}
				break;
			} /* while (1) */
		} /* if (goodconn) */

		if (!goodconn)
		{
			struct DccUser *prv;

			close(botptr->socket);
			botptr->socket = NOSOCKET;
			MyFree(botptr->username);
			MyFree(botptr->hostname);

			if (connections)
			{
				if (connections == botptr)
				{
					connections = connections->next;
					MyFree(botptr);
				}
				else
				{
					prv = connections;
					for (tmp = connections->next; tmp; )
					{
						if (tmp == botptr)
						{
							prv->next = tmp->next;
							MyFree(tmp);
							break;
						}
						else
						{
							prv = tmp;
							tmp = tmp->next;
						}
					}
				}
			} /* if (connections) */

			Network->TotalConns--;
			return 1;
		}

		return 0;
	}

	if (*line == '<')
	{
		/* <nick@bot> said something */

		ircsprintf(buf, "%s\r\n", line);
		for (tmp = connections; tmp; tmp = tmp->next)
		{
			if (botptr == tmp)
				continue; /* don't send it back to the bot it came from */

			writesocket(tmp->socket, buf);
		}
	}
	return 0;
} /* BotProcess() */

/*
ServReboot()
  Close DCC connections, reconnect to the network
*/

void
ServReboot()

{
	struct DccUser *tempconn;

	while (connections)
	{
		tempconn = connections->next;
		close(connections->socket);
		connections->socket = NOSOCKET;

		MyFree(connections->nick);
		MyFree(connections->username);
		MyFree(connections->hostname);
		MyFree(connections);

		connections = tempconn;
	}
	connections = NULL;

#if defined(NICKSERVICES) && defined(CHANNELSERVICES)

        toserv(":%s QUIT :%s\r\n", n_ChanServ, "Restarting");
#endif
#ifdef SEENSERVICES
        toserv(":%s QUIT :%s\r\n", n_SeenServ, "Restarting");
#endif
        toserv(":%s QUIT :%s\r\n", n_OperServ, "Restarting");

	/* Instead of SQUIT -kre */
	toserv(":%s ERROR :Restarting\r\n", Me.name);
	toserv(":%s QUIT\r\n", Me.name);

	/* kill old connection and clear out user/chan lists etc */
	close(HubSock);

	ClearUsers();
	ClearChans();
	ClearServs();

	ClearHashes(0);

	HubSock = NOSOCKET;

	currenthub->connect_ts = 0;
} /* ServReboot() */

/*
 * DCCStr2IP()
 * This code is ugly and generally should not be used. Unfortunately,
 * getaddrinfo() seems broken on some libc implementations so we'll use
 * this approach for a while
 *
 * Thanks Alan LeVee
 */
#ifdef HAVE_BROKEN_INT_GETADDRINFO
void DCCNormalizeIP(char *name, int len)
{
	if (strchr(name, ':') == NULL)
	{
		unsigned long int addr;
		struct in_addr ipaddr;

		/* normal IPv4 address in 32bit number form */
		addr = strtoul(name, NULL, 10);
		ipaddr.s_addr = htonl(addr);
		strlcpy(name, inet_ntoa(ipaddr), len);
	}
}
#endif
