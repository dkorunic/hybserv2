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
#include <string.h>
#include <assert.h>
#include <netdb.h>

#include "alloc.h"
#include "channel.h"
#include "client.h"
#include "conf.h"
#include "config.h"
#include "data.h"
#include "dcc.h"
#include "gline.h"
#include "hash.h"
#include "hybdefs.h"
#include "jupe.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "operserv.h"
#include "settings.h"
#include "sock.h"
#include "timestr.h"
#include "sprintf_irc.h"

/* Solaris does not provide this by default. Anyway this is wrong approach,
   since -1 is 255.255.255.255 addres which is _valid_! Obviously
   inet_aton() should be used instead. I'll fix that later. -kre */
#ifndef INADDR_NONE
# define INADDR_NONE ((unsigned long)-1)
#endif

static char *getfield (char *newline);

static void AddHostLimit(char *host, int hostnum, int banhost);
static void AddUser(char *host, char *pass, char *nick, char *flags);
static void AddServ(char *hostname, char *password, int port);
static void AddBot(char *nick, char *host, char *pass, int port);
static void AddRemoteBot(char *nick, char *host, char *pass);
static void AddPort(int port, char *host, char *type);
static void ClearConfLines();

static void CheckChans();
static void CheckServers();
static void CheckListenPorts();

static struct Servlist *CreateServer();

#if defined AUTO_ROUTING && defined SPLIT_INFO
void AddReconnect(const char *, const int, const char *, const long);
#endif

#if 0
static void AddAddressToServer(struct Servlist **ptr,
                               char *ip, int length);
#endif /* 0 */

struct rHost *rHostList = NULL;      /* list of restricted hosts */
struct Userlist *UserList = NULL;    /* list of privileged users */
struct Servlist *ServList = NULL;    /* list of hub servers */
struct Chanlist *ChanList = NULL;    /* list of channels to monitor */
struct Botlist *BotList = NULL,      /* list of tcm bots that can be linked to */
               *RemoteBots = NULL;   /* list of tcm bots authorized to connect */
struct PortInfo *PortList = NULL;    /* list of ports to listen on */

struct Userlist *GenericOper = NULL;

#if defined AUTO_ROUTING && defined SPLIT_INFO
struct cHost *cHostList = NULL;      /* list of autoconnecting hosts */
#endif

int HubCount = 0;                    /* number of S: lines in hybserv.conf */

/* 
 * Global - pointer to structure containing the info for the hub
 *          services is currently connected to
 */
struct Servlist *currenthub = NULL;

/*
Rehash()
  args: none
  purpose: clear all old jupe, gline, and user lists and parse
           ConfigFile for new ones.
  return: 0 if successful rehash, 1 if not
*/

int
Rehash()

{
  int RehashError;
  FILE *fp;
  struct Chanlist *tempchan;
  struct Servlist *tempserv;
  struct PortInfo *pptr;

  /*
   * We have to try to open the configuration file before
   * calling ParseConf() in case we no longer have access to
   * it, or it is no longer there, in which case do not
   * kill the old conf lists. Unfortunately this means we'll
   * have to call fopen() on it twice (once again in ParseConf()).
   */
  if (!(fp = fopen(ConfigFile, "r")))
    return (1);

  fclose(fp);

  /* Kill old  conf lists */
  ClearConfLines();

  /* 
   * mark all monitored channels with the delete flag, and "undelete"
   * them as we read in C: lines from the conf - any left over,
   * permanently delete
   */
  for (tempchan = ChanList; tempchan; tempchan = tempchan->next)
    tempchan->flags |= CHAN_DELETE;

  /*
   * mark servers with delete flag and undelete them as we read
   * in S: lines from conf - if we clear the ServList, currenthub
   * will point to garbage, so this makes sure that doesn't happen
   */
  for (tempserv = ServList; tempserv; tempserv = tempserv->next)
    tempserv->flags |= SL_DELETE;

  /*
   * do the same with the P: lines
   */
  for (pptr = PortList; pptr; pptr = pptr->next)
    pptr->type = PRT_DELETE;

  HubCount = 0;

  /*
   * Reload configuration files
   */
  ParseConf(ConfigFile, 1);

  if (GlineFile)
    ParseConf(GlineFile, 1);
  if (JupeFile)
    ParseConf(JupeFile, 1);

  RehashError = 0;

  if (!UserList)
  {
    RehashError = 1;
    putlog(LOG1, "Rehash: No O: lines found in %s",
      ConfigFile);
  }

  if (!ServList)
  {
    RehashError = 1;
    putlog(LOG1, "Rehash: No S: lines found in %s",
      ConfigFile);
  }

  /*
   * PART any channels that have been removed from conf file
   * and JOIN any new channels
   */
  CheckChans();

  /*
   * Delete server structures that no longer have an S: line
   */
  CheckServers();

  /*
   * Close any ports who no longer have a P: line, and start
   * listening on new listen ports
   */
  CheckListenPorts();

#ifdef ALLOW_JUPES
  /*
   * SQUIT any pseudo-servers who no longer have a J: line
   */
  CheckJupes();
#endif

#ifdef GLOBALSERVICES
 /*
  * Re-read LogonNews File
  */

  if (LogonNews)
  {
    Network->LogonNewsFile.filename = LogonNews;
    ReadMessageFile(&Network->LogonNewsFile);
  }

#endif

  /*
   * All users usermodes will have been erased - reload them
   */
  if (os_loaddata() == (-2))
    RehashError = 1;

  /*
   * Rehash settings.conf
   */
  if (LoadSettings(1) == 0)
    RehashError = 1;

  return (RehashError);
} /* Rehash() */

/*
ParseConf()
 Backend for LoadConfig().
 Args: filename - file name to parse
       rehash - set to 1 if this is a rehash
*/

void
ParseConf(char *filename, int rehash)

{
  FILE *fileptr;
  char line[MAXLINE];
  char *key;
  int scnt; /* S: line count */

  if (!filename)
    return;

  if ((fileptr = fopen(filename, "r")) == NULL)
  {
    fprintf(stderr, "Unable to open %s\n", filename);
    putlog(LOG1,
      "Unable to open configuration file: %s %s",
      filename,
      (filename == ConfigFile) ? "(FATAL)" : "");

    /*
     * If the file is the top-level config file, exit.
     * If this is a rehash, do not exit, because Rehash()
     * would NOT have erased the old conf lists, so we
     * can continue normally.
     */
    if (!rehash && (filename == ConfigFile))
      exit(0);

    return;
  }

  scnt = 0;

  while (fgets(line, MAXLINE - 1, fileptr))
  {
    /*
     * you may add comments to config file using a '#'
     */
    if (line[0] == '#')
      continue;         

    key = getfield(line);
    if (!key)
      continue;

    switch (*key)
    {
      case 'a':
      case 'A':
      {
        char *temp;

        /*
         * If this is a rehash, Me.admin should already be
         * set, so make sure it gets freed.
         */
        if (Me.admin)
          MyFree(Me.admin);

        if (!(temp = getfield(NULL)))
          break;

        if (strlen(temp) > REALLEN)
        {
          Me.admin = (char *) MyMalloc(REALLEN);
          memset(Me.admin, 0, REALLEN);
          strncpy(Me.admin, temp, REALLEN - 1);
        }
        else
          Me.admin = MyStrdup(temp);

        break;
      } /* case 'A' */

      case 's':
      case 'S':
      {
        char *pass;
        char *host;
        char *port;
        struct Servlist *servptr;
        
        pass = getfield(NULL);
        host = getfield(NULL);
        port = getfield(NULL);
        if (!host || !pass)
          continue;

        if ((servptr = IsServLine(host, port)))
        {
          servptr->flags &= ~SL_DELETE;
          if (servptr->password)
          {
            /*
             * This must be a rehash, so in case the password
             * was changed, set servptr->password to the new one.
             */
            MyFree(servptr->password);
            servptr->password = MyStrdup(pass);
          }
        }
        else
        {
          AddServ(host, pass, port ? atoi(port) : DefaultHubPort);
        }

        scnt++;
        break;
      } /* case 'S' */

  #ifdef ALLOW_JUPES
      case 'j':
      case 'J':
      {
        char *name;
        char *reason;
        char *who;

        name = getfield(NULL);
        reason = getfield(NULL);
        who = getfield(NULL);
        if (!name || !reason || !who)
          continue;

        AddJupe(name, reason, who);

        break;  
      } /* case 'J' */
  #endif /* ALLOW_JUPES */

  #ifdef ALLOW_GLINES
      case 'g':
      case 'G':
      {
        char *host;
        char *reason;
        char *who;

        host = getfield(NULL);
        reason = getfield(NULL);
        who = getfield(NULL);

        if (!host || !who)
          continue;

        AddGline(host, reason, who, 0);

        break;     
      } /* case 'G': */
  #endif /* ALLOW_GLINES */

      case 'c':
      case 'C':
      {
        char *cname;
        struct Chanlist *chanptr;

        cname = getfield(NULL);
        if (!cname)
          continue;

        if (*cname != '#')
          continue;

        if ((chanptr = IsChannel(cname)))
        {
          /*
           * Since cname is already in the channel list,
           * this must be a rehash. Remove the deletion
           * flag from chanptr since the C: line was
           * not deleted prior to the rehash
           */
          chanptr->flags &= ~CHAN_DELETE;
        }
        else
        {
          /* a new chan was added to the conf */
          AddMyChan(cname);
        }

        break;
      } /* case 'C' */

      case 'o':
      case 'O':
      {
        char *nick;
        char *host;
        char *pass;
        char *flags;

        host = getfield(NULL);
        pass = getfield(NULL);
        nick = getfield(NULL);
        flags = getfield(NULL);
        if (!host || !pass || !nick || !flags)
          continue;

        AddUser(host, pass, nick, flags);

        break;    
      } /* case 'O' */

      case 'n':
      case 'N':
      {
        char *name;
        char *info;

        name = getfield(NULL);
        info = getfield(NULL);
        if (!name || !info)
          continue;

        if (strlen(name) > REALLEN)
        {
          Me.name = (char *) MyMalloc(REALLEN);
          memset(Me.name, 0, REALLEN);
          strncpy(Me.name, name, REALLEN - 1);
        }
        else
          Me.name = MyStrdup(name);

        if (strlen(info) > REALLEN)
        {
          Me.info = (char *) MyMalloc(REALLEN);
          memset(Me.info, 0, REALLEN);
          strncpy(Me.info, info, REALLEN - 1);
        }
        else
          Me.info = MyStrdup(info);

        break;
      } /* case 'N' */

      case 'b':
      case 'B':
      {
        char *name;
        char *host;
        char *pass;
        char *port;

        host = getfield(NULL);
        name = getfield(NULL);
        pass = getfield(NULL);
        port = getfield(NULL);
        if (!name || !host || !pass)
          continue;

        AddBot(name, host, pass, port ? atoi(port) : DefaultTcmPort);

        break;
      } /* case 'B' */

      case 'l':
      case 'L':
      {
        char *name;
        char *host;
        char *pass;

        host = getfield(NULL);
        name = getfield(NULL);
        pass = getfield(NULL);
        if (!name || !host || !pass)
          continue;

        AddRemoteBot(name, host, pass);

        break;
      } /* case 'L' */

      case 'i':
      case 'I':
      {
        char *host, *num, *banhost;

        host = getfield(NULL);
        num = getfield(NULL);
        banhost = getfield(NULL);
        if (!banhost)
          banhost = "0";
        if (!host || !num)
          continue;

        AddHostLimit(host, atoi(num), atoi(banhost));

        break;
      } /* case 'I' */

      case 'p':
      case 'P':
      {
        char *port, *type, *host;

        host = getfield(NULL);
        port = getfield(NULL);
        type = getfield(NULL);

        if (!port)
          continue;
        if (!IsNum(port))
          continue;

        AddPort(atoi(port), host, type);

        break;
      } /* case 'P' */

      case 'v':
      case 'V':
      {
        char *vhost;

        vhost = getfield(NULL);
        if (!vhost)
          continue;

        if (!LocalHostName)
          LocalHostName = MyStrdup(vhost);

        break;
      } /* case 'V' */

#if defined AUTO_ROUTING && defined SPLIT_INFO
      /* This includes routines for auto-connecting in case of split -
       * HybServ will send required CONNECT string and try to connect
       * server if splitted. Usually this is not necessary because of
       * auto-connecting ports, but what if you want to make sure they
       * will be reconnected after some time if admin is not present, ant
       * non-autoconnecting c/N lines exist? -kre 
       */
      case 'm':
      case 'M':
      {
        char *hub, *leaf, *port_str;
        long re_time;
        int port;

        hub=getfield(NULL);
        port_str=getfield(NULL);
        if (!port_str)
          port=DefaultHubPort;
        else
          port=atoi(port_str);
        leaf=getfield(NULL);
        re_time=timestr(getfield(NULL));

        if (!hub || !leaf || !re_time)
          continue;

        AddReconnect(hub, port, leaf, re_time);

        break;
      } /* case 'M' */
#endif /*  AUTO_ROUTING && SPLIT_INFO */

      case '.':
      {
        char *start, *end;
        char *filename;

        if (!ircncmp(line, ".include", 8))
        {
          /*
           * It is a .include statement - meaning they
           * want to include another file to be parsed
           * as a regular configuration file. Normal
           * syntax is: .include "filename"
           */

          if ((start = strchr(line, '"')))
            filename = start + 1;
          else
          {
            /*
             * There are no quotes in the line - bad syntax
             */
            putlog(LOG1,
              "Incorrect config file syntax: %s",
              line);
            continue;
          }

          /*
           * "filename" should now point past the first quote
           * (ie: file") so now set the end quote to a \0
           */
          if ((end = strchr(filename, '"')))
            *end = '\0';
          else
          {
            putlog(LOG1,
              "Incorrect config file syntax: %s",
              line);
            continue;
          }

          /*
           * Now recursively call ParseConf() to parse the new
           * configuration file
           */
          ParseConf(filename, 0);
        } /* if (!ircncmp(line, ".include", 8)) */

        break;
      } /* case '.' */

      default:
        break;
    }
  }

  HubCount += scnt;

  fclose(fileptr);
} /* ParseConf() */

/*
LoadConfig()
  args: none
  purpose: parse config file and store server, admin, user, and 
           jupe/gline information
  return: none
*/

void
LoadConfig()

{
  int ConfError;

  ParseConf(ConfigFile, 0);

  /*
   * If settings.conf specified a Gline/Jupe file, parse
   * them as well
   */
  if (GlineFile)
    ParseConf(GlineFile, 0);
  if (JupeFile)
    ParseConf(JupeFile, 0);

  ConfError = 0;

  if (!Me.admin)
  {
    fprintf (stderr,
      "No administrative contact information (A:) found in %s\n",
      ConfigFile);
    ConfError = 1;
  }

  if (!Me.name)
  {
    fprintf(stderr,
      "No server name (N:) specified in %s\n",
      ConfigFile);
    ConfError = 1;
  }

  if (!Me.info)
  {
    fprintf (stderr,
      "No server info (N:) specified in %s\n",
      ConfigFile);
    ConfError = 1;
  }

  if (!ServList)
  {
    fprintf (stderr,
      "No hub servers (S:) specified in %s\n",
      ConfigFile);
    ConfError = 1;
  }
  
  if (!UserList)
  {
    fprintf (stderr,
      "No users (O:) specified for access to services in %s (at least one user should be added)\n",
      ConfigFile);
    ConfError = 1;
  }

  if (ConfError)
    exit(1);
} /* LoadConfig() */

/*
getfield()
  Get next field separated by a : in the conf file
  ** This function is from standard ircd source
*/

static char *
getfield (char *newline)

{
  static char *line = NULL;
  char *end, *field;
  
  if (newline)
    line = newline;

  if (!line)
    return (NULL);

  field = line;
  if ((end = strchr(line, ':')) == NULL)
  {
    line = NULL;
    if ((end = strchr(field, '\n')) == NULL)
      end = field + strlen(field);
  }
  else
    line = end + 1;

  *end = '\0';
  if (strlen(field))
    return (field);
  else
    return (NULL);
} /* getfield() */

/*
IsChannel()
  args: char *chan
  purpose: determine if 'chan' is currently being monitored
  return: pointer to channel in monitored channel list
*/

struct Chanlist *
IsChannel(char *chan)

{
  struct Chanlist *tempchan;

  for (tempchan = ChanList; tempchan; tempchan = tempchan->next)
    if (!irccmp(tempchan->name, chan))
      return(tempchan);

  return (NULL);
} /* IsChannel() */

/*
IsServLine()
  args: char *name, int port
  purpose: determine if there is an S: line for 'name' and 'port'
  return: pointer to server struct in server list
*/

struct Servlist *
IsServLine(char *name, char *port)

{
  struct Servlist *tempserv;
  int portnum;

  if (!name)
    return (NULL);

  if (!port)
    portnum = DefaultHubPort;
  else
    portnum = atoi(port);

  for (tempserv = ServList; tempserv; tempserv = tempserv->next)
    if ((tempserv->port == portnum) &&
        (!irccmp(tempserv->hostname, name)))
      return (tempserv);

  return (NULL);
} /* IsServLine() */

/*
IsListening()
  Determine if we are currently accepting connections on 'portnum'
*/

struct PortInfo *
IsListening(int portnum)

{
  struct PortInfo *pptr;

  for (pptr = PortList; pptr; pptr = pptr->next)
    if ((pptr->port == portnum) &&
        (pptr->socket != NOSOCKET))
      return (pptr);

  return (NULL);
} /* IsListening() */

/*
IsPort()
  Determine if 'portnum' has a P: line in hybserv.conf
*/

struct PortInfo *
IsPort(int portnum)

{
  struct PortInfo *pptr;

  for (pptr = PortList; pptr; pptr = pptr->next)
    if (pptr->port == portnum)
      return (pptr);

  return (NULL);
} /* IsPort() */

/*
DoBinds()
 Called from DoTimer() to bind P: lined ports that we were
unable to bind on startup
*/

void
DoBinds()

{
  struct PortInfo *pptr;

  for (pptr = PortList; pptr; pptr = pptr->next)
  {
    if ((pptr->tries < MaxBinds) && (pptr->socket == NOSOCKET))
    {
      DoListen(pptr);

      if (pptr->tries >= MaxBinds)
      {
        putlog(LOG2,
          "Giving up attempt to bind port [%d]",
          pptr->port);

        SendUmode(OPERUMODE_Y,
          "Giving up attempt to bind port [%d]",
          pptr->port);
      }
    }
  }
} /* DoBinds() */

/*
IsBot()
  args: char *bname
  purpose: determine if 'bname' has a B: line 
  return: pointer to bot structure
*/

struct Botlist *
IsBot(char *bname)

{
  struct Botlist *tempbot;

  if (!bname)
    return (NULL);

  tempbot = BotList;
  while (tempbot && (irccmp(tempbot->name, bname) != 0))
    tempbot = tempbot->next;

  return (tempbot);
} /* IsBot() */

/*
AddHostLimit()
  Add 'host' to restricted hosts list
*/

static void
AddHostLimit(char *host, int hostnum, int banhost)

{
  struct rHost *ptr;
  char *tmp;

  ptr = (struct rHost *) MyMalloc(sizeof(struct rHost));
  memset(ptr, 0, sizeof(struct rHost));

  if ((tmp = strchr(host, '@')))
  {
    *tmp++ = '\0';
    ptr->username = MyStrdup(host);
    ptr->hostname = MyStrdup(tmp);
  }
  else
  {
    ptr->username = MyStrdup("*");
    ptr->hostname = MyStrdup(host);
  }

  ptr->hostnum = hostnum;
#ifdef ADVFLOOD
  ptr->banhost = banhost;
#endif /* ADVFLOOD */
  ptr->next = rHostList;
  rHostList = ptr;
} /* AddHostLimit() */

/*
IsRestrictedHost()
  Return pointer to restricted host structure matching 'host'
*/

struct rHost *
IsRestrictedHost(char *user, char *host)

{
  struct rHost *temphost;

  for (temphost = rHostList; temphost; temphost = temphost->next)
  {
    if (match(temphost->username, user) &&
        match(temphost->hostname, host))
      return (temphost);
  }

  return NULL;
} /* IsRestrictedHost() */

/*
AddUser()
  Add 'host' to O: line UserList with 'pass' and 'flags'
*/

static void
AddUser(char *host, char *pass, char *nick, char *flags)

{
  struct Userlist *ptr;
  int ii;
  char *tmp;

  ptr = (struct Userlist *) MyMalloc(sizeof(struct Userlist));
  memset(ptr, 0, sizeof(struct Userlist));

  ptr->nick = MyStrdup(nick);

  if ((tmp = strchr(host, '@')))
  {
    *tmp++ = '\0';
    ptr->username = MyStrdup(host);
    ptr->hostname = MyStrdup(tmp);
  }
  else
  {
    ptr->username = MyStrdup("*");
    ptr->hostname = MyStrdup(host);
  }

  ptr->password = MyStrdup(pass);
  ptr->umodes = OPERUMODE_INIT;

  for (ii = 0; ii < strlen(flags); ii++)
  {
    switch (flags[ii])
    {
      case 's':
      case 'S':
      {
        ptr->flags |= (PRIV_SADMIN | PRIV_ADMIN | PRIV_OPER | PRIV_FRIEND | PRIV_CHAT | PRIV_EXCEPTION);
        break;
      }
      case 'a':
      case 'A':
      {
        ptr->flags |= (PRIV_ADMIN | PRIV_OPER | PRIV_FRIEND | PRIV_CHAT);
        break;
      }
      case 'o':
      case 'O':
      {
        ptr->flags |= (PRIV_OPER | PRIV_FRIEND);
        break;
      }
      case 'j':
      case 'J':
      {
        ptr->flags |= PRIV_JUPE;
        break;
      }
      case 'g':
      case 'G':
      {
        ptr->flags |= PRIV_GLINE;
        break;
      }
      case 'd':
      case 'D':
      {
        ptr->flags |= PRIV_CHAT;
        break;
      }
      case 'e':
      case 'E':
      {
        ptr->flags |= PRIV_EXCEPTION;
        break;
      }
      case 'f':
      case 'F':
      {
        ptr->flags |= PRIV_FRIEND;
        break;
      }
      default:
        break;
    } /* switch (flags[ii]) */
  }

  ptr->next = UserList;
  UserList = ptr;
} /* AddUser() */

/*
AddServ()
  Add 'hostname', 'password', and 'port' to list containing S: line
info
*/

static void
AddServ(char *hostname, char *password, int port)

{
  struct Servlist *ptr;

#if 0
  /*
   * maybe someday when I'm really bored I'll fix this
   */
  struct hostent *hp;
  struct in_addr ip;
  char **curraddr;

  /*
   * All S: line hostnames are pre-resolved so when we
   * want to connect to a server, we don't waste time resolving
   * the hostname again and again.
   */
  hp = LookupHostname(hostname, &ip);

  if (hp)
  {
    /*
     * We have a resolving hostname - it is possible it points
     * to more than one ip - add all of them
     */
    ptr = CreateServer();

    for (curraddr = hp->h_addr_list; *curraddr; ++curraddr)
      AddAddressToServer(&ptr, *curraddr, hp->h_length);
  }
  else
  {
    if (ip.s_addr == INADDR_NONE)
    {
      putlog(LOG1,
        "Unresolvable server name: %s",
        hostname);
      return;
    }

    ptr = CreateServer();
    AddAddressToServer(&ptr, (char *) &ip.s_addr, sizeof(ip));
  }
#endif /* 0 */

  ptr = CreateServer();
  ptr->hostname = MyStrdup(hostname);
  ptr->password = MyStrdup(password);
  ptr->port = port;
} /* AddServ() */

/*
CreateServer()
 Allocate memory for a new Servlist structure and return pointer
*/

static struct Servlist *
CreateServer()

{
  struct Servlist *ptr;

  ptr = (struct Servlist *) MyMalloc(sizeof(struct Servlist));
  memset(ptr, 0, sizeof(struct Servlist));

  ptr->sockfd = NOSOCKET;

  ptr->next = ServList;
  ServList = ptr;

  return (ptr);
} /* CreateServer() */

#if 0
/*
AddAddressToServer()
 Add the given ip address to the given server structure
*/

static void
AddAddressToServer(struct Servlist **ptr, char *ip, int length)

{
  int offset;

  ++(*ptr)->numips;
  (*ptr)->ips = (struct in_addr **)
    MyRealloc((*ptr)->ips, sizeof(struct in_addr) * (*ptr)->numips);

  offset = sizeof(struct in_addr) * ((*ptr)->numips - 1);
  memcpy((*ptr)->ips[0] + offset, ip, length);
} /* AddAddressToServer() */
#endif /* 0 */

/*
AddMyChan()
  Add 'channel' to list of C: line channels
*/

void
AddMyChan(char *channel)

{
  struct Chanlist *ptr;

  ptr = (struct Chanlist *) MyMalloc(sizeof(struct Chanlist));
  memset(ptr, 0, sizeof(struct Chanlist));

  ptr->name = MyStrdup(channel);

  ptr->next = ChanList;
  ChanList = ptr;

  Network->MyChans++;
} /* AddMyChan() */

/*
AddBot()
  Add new bot info to list of B: line bots
*/

static void
AddBot(char *nick, char *host, char *pass, int port)

{
  struct Botlist *ptr;
  char *tmp;

  ptr = (struct Botlist *) MyMalloc(sizeof(struct Botlist));
  memset(ptr, 0, sizeof(struct Botlist));

  ptr->name = MyStrdup(nick);

  if ((tmp = strchr(host, '@')))
  {
    *tmp++ = '\0';
    ptr->username = MyStrdup(host);
    ptr->hostname = MyStrdup(tmp);
  }
  else
  {
    ptr->username = MyStrdup("*");
    ptr->hostname = MyStrdup(host);
  }

  ptr->password = MyStrdup(pass);
  ptr->port = port;
  ptr->next = BotList;
  BotList = ptr;
} /* AddBot() */

/*
AddRemoteBot()
  Add new bot info to list of L: line bots
*/

static void
AddRemoteBot(char *nick, char *host, char *pass)

{
  struct Botlist *ptr;
  char *tmp;

  ptr = (struct Botlist *) MyMalloc(sizeof(struct Botlist));
  memset(ptr, 0, sizeof(struct Botlist));

  ptr->name = MyStrdup(nick);

  if ((tmp = strchr(host, '@')))
  {
    *tmp++ = '\0';
    ptr->username = MyStrdup(host);
    ptr->hostname = MyStrdup(tmp);
  }
  else
  {
    ptr->username = MyStrdup("*");
    ptr->hostname = MyStrdup(host);
  }

  ptr->password = MyStrdup(pass);

  ptr->next = RemoteBots;
  RemoteBots = ptr;
} /* AddRemoteBot() */

/*
AddPort()
  Add port info
*/

static void
AddPort(int port, char *host, char *type)

{
  struct PortInfo *ptr;

  if ((ptr = IsPort(port)))
  {
    /*
     * The port is already in the list, so this routine was probably
     * called from Rehash() - just change the hostmask and type
     * in case they were changed for the rehash
     */
    if (ptr->host)
      MyFree(ptr->host);
  }
  else
  {
    ptr = (struct PortInfo *) MyMalloc(sizeof(struct PortInfo));
    memset(ptr, 0, sizeof(struct PortInfo));

    ptr->port = port;
    ptr->socket = NOSOCKET;
    ptr->next = PortList;
    PortList = ptr;
  }

  if (host)
  {
    if (strlen(host))
      ptr->host = MyStrdup(host);
    else
      ptr->host = NULL;
  }
  else
    ptr->host = NULL;

  if (!type)
    ptr->type = PRT_USERS;
  else
  {
    ptr->type = 0;

    if (!irccmp(type, "TCM"))
      ptr->type = PRT_TCM;
    if (!irccmp(type, "USERS"))
      ptr->type = PRT_USERS;

    if (!ptr->type)
      ptr->type = PRT_USERS;
  }
} /* AddPort() */

/*
ClearConfLines()
 Called from Rehash() to clear old conf list
*/

static void
ClearConfLines()

{
#ifdef ALLOW_JUPES
  struct Jupe *tempjupe;
#endif
#ifdef ALLOW_GLINES
  struct Gline *tempgline;
#endif
  struct Userlist *tempuser;
/*  struct Servlist *tempserv; */
  struct Botlist *tempbot;
  struct rHost *temphost;
#if defined AUTO_ROUTING && defined SPLIT_INFO
  struct cHost *tempchost;
#endif

#ifdef ALLOW_JUPES
  while (JupeList)
  {
    tempjupe = JupeList->next;
    DeleteJupe(JupeList);
    JupeList = tempjupe;
  }
#endif

#ifdef ALLOW_GLINES
  while (GlineList)
  {
    tempgline = GlineList->next;
    DeleteGline(GlineList);
    GlineList = tempgline;
  }
#endif

  while (UserList)
  {
    tempuser = UserList->next;
    MyFree(UserList->nick);
    MyFree(UserList->username);
    MyFree(UserList->hostname);
    MyFree(UserList->password);
    MyFree(UserList);
    UserList = tempuser;
  }

/*
 * Don't clear the ServList during a rehash because currenthub
 * will point to garbage
  while (ServList)
  {
    MyFree(ServList->hostname);
    MyFree(ServList->password);
    if (ServList->realname)
      MyFree(ServList->realname);
    tempserv = ServList->next;
    MyFree(ServList);
    ServList = tempserv;
  }
*/

  while (BotList)
  {
    MyFree(BotList->name);
    MyFree(BotList->username);
    MyFree(BotList->hostname);
    MyFree(BotList->password);
    tempbot = BotList->next;
    MyFree(BotList);
    BotList = tempbot;
  }

  while (RemoteBots)
  {
    MyFree(RemoteBots->name);
    MyFree(RemoteBots->username);
    MyFree(RemoteBots->hostname);
    MyFree(RemoteBots->password);
    tempbot = RemoteBots->next;
    MyFree(RemoteBots);
    RemoteBots = tempbot;
  }

  while (rHostList)
  {
    MyFree(rHostList->username);
    MyFree(rHostList->hostname);
    temphost = rHostList->next;
    MyFree(rHostList);
    rHostList = temphost;
  }
#if defined AUTO_ROUTING && defined SPLIT_INFO
  while (cHostList)
  {
    MyFree(cHostList->hub);
    MyFree(cHostList->leaf);
    tempchost=cHostList->next;
    MyFree(cHostList);
    cHostList=tempchost;
  }
#endif
} /* ClearConfLines() */

/*
CheckChans()
  purpose: this function is called after a rehash, to check if any
           new channels have been added to the config file, or any
           old channels removed from the config file.  Any new
           channels are join()'d and old channels no longer
           being monitored are part()'d.
  return: none
*/

static void
CheckChans()

{
  struct Chanlist *tempchan, *prev;
  struct Channel *cptr;

  /* 
   * go through channel list and PART any channels that were removed
   * from the conf
   */
  prev = NULL;
  for (tempchan = ChanList; tempchan; )
  {
    if (tempchan->flags & CHAN_DELETE)
    {
      putlog(LOG1, "%s: No longer monitoring %s",
        n_OperServ,
        tempchan->name);

      cptr = FindChannel(tempchan->name);
      if (IsChannelMember(cptr, Me.osptr))
        os_part(cptr);

      MyFree(tempchan->name);

      if (prev)
      {
        prev->next = tempchan->next;
        MyFree(tempchan);
        tempchan = prev;
      }
      else
      {
        ChanList = tempchan->next;
        MyFree(tempchan);
        tempchan = NULL;
      }

      Network->MyChans--;
    } /* if (tempchan->flags & CHAN_DELETE) */

    prev = tempchan;

    if (tempchan)
      tempchan = tempchan->next;
    else
      tempchan = ChanList;
  }

  /*
   * go through channel list and JOIN any new channels that were added 
   * to the conf
   */
  for (tempchan = ChanList; tempchan; tempchan = tempchan->next)
  {
    cptr = FindChannel(tempchan->name);
    if (cptr && !IsChannelMember(cptr, Me.osptr))
    {
      os_join(cptr);
      putlog(LOG1, "%s: Now monitoring channel %s",
        n_OperServ,
        tempchan->name);
    }
  }
} /* CheckChans() */

/*
CheckServers()
 Called after a rehash to remove any servers (S: lines) that are
marked for deletion
*/

static void
CheckServers()

{
  struct Servlist *tempserv, *prev;

  prev = NULL;

  for (tempserv = ServList; tempserv; )
  {
    if (tempserv->flags & SL_DELETE)
    {
      if (tempserv == currenthub)
      {
        /*
         * Someone deleted our current hub server out of
         * hybserv.conf .. just keep it in the list, or it will
         * screw a lot of stuff up :-)
         */
        tempserv->flags &= ~SL_DELETE;
        tempserv = tempserv->next;
        continue;
      }

      MyFree(tempserv->hostname);
      MyFree(tempserv->password);
      if (tempserv->realname)
        MyFree(tempserv->realname);

      if (prev)
      {
        prev->next = tempserv->next;
        MyFree(tempserv);
        tempserv = prev;
      }
      else
      {
        ServList = tempserv->next;
        MyFree(tempserv);
        tempserv = NULL;
      }
    }

    prev = tempserv;
    if (tempserv)
      tempserv = tempserv->next;
    else
      tempserv = ServList;
  }
} /* CheckServers() */

/*
CheckListenPorts()
  Delete any ports who no longer have a P: line, and start listening
on new ports
*/

static void
CheckListenPorts()

{
  struct PortInfo *tempport, *prev;

  prev = NULL;
  for (tempport = PortList; tempport; )
  {
    if (tempport->type == PRT_DELETE)
    {
      putlog(LOG1, "No longer listening on port %d %s%s%s",
        tempport->port,
        tempport->host ? "[" : "",
        tempport->host ? tempport->host : "",
        tempport->host ? "]" : "");

      if (tempport->socket != NOSOCKET)
        close(tempport->socket);

      if (tempport->host)
        MyFree(tempport->host);

      if (prev)
      {
        prev->next = tempport->next;
        MyFree(tempport);
        tempport = prev;
      }
      else
      {
        PortList = tempport->next;
        MyFree(tempport);
        tempport = NULL;
      }
    } /* if (tempport->type == PRT_DELETE) */

    prev = tempport;
    if (tempport)
      tempport = tempport->next;
    else
      tempport = PortList;
  }

  /*
   * Now go through and listen on new ports
   */
  for (tempport = PortList; tempport; tempport = tempport->next)
    if (!IsListening(tempport->port))
      DoListen(tempport);

} /* CheckListenPorts() */

/*
CheckAccess()
  args: struct Userlist *user, char flag
  purpose: determines whether O: line user specified by 'user' has
           the flag 'flag'.
  return: >= 1 if the user has 'flag', 0 if not          
*/

int
CheckAccess(struct Userlist *user, char flag)

{
  if (!user)
    return 0;

  switch (flag)
  {
    case '\0':
      return (1);
    case 'a':
    case 'A':
      return (IsAdmin(user));
    case 'd':
    case 'D':
      return (CanChat(user));
    case 'e':
    case 'E':
      return (IsProtected(user));
    case 'f':
    case 'F':
      return (IsFriend(user));
    case 'g':
    case 'G':
      return (CanGline(user));
    case 'j':
    case 'J':
      return (CanJupe(user));
    case 'o':
    case 'O':
      return (IsOper(user));
    case 's':
    case 'S':
      return (IsServicesAdmin(user));

    default:
      return 0;
  }
} /* CheckAccess() */

/*
IsProtectedHost()
  Determines if userhost matches an exception O: line -
returns 1 if so, 0 if not
*/

int
IsProtectedHost(char *username, char *hostname)

{
  struct Userlist *tempuser;

  if (!username || !hostname)
    return 0;

  for (tempuser = UserList; tempuser; tempuser = tempuser->next)
  {
    if (!(tempuser->flags & PRIV_EXCEPTION))
      continue;

    if (match(username, tempuser->username) &&
        match(hostname, tempuser->hostname))
      return 1;
  }

  return 0;
} /* IsProtectedHost() */

int
MatchesAdmin(char *mask)

{
  struct Userlist *tempuser;
  char tempstr[MAXLINE];

  if (!mask)
    return (0);

  for (tempuser = UserList; tempuser; tempuser = tempuser->next)
  {
    if (IsAdmin(tempuser))
    {
      ircsprintf(tempstr, "*!%s@%s", tempuser->username,
          tempuser->hostname);

      if (match(mask, tempstr))
        return (1);
    }
  }

  return (0);
} /* MatchesAdmin() */

/*
GetUser()
  Find O: line entry containing nickname.  If 'user' and 'host' are
given, try to find best match in case there are O: lines with the
same nickname; if hostname is given, but nickonly is 0, do not just
match nicknames - make sure the user@host matches as well
*/

struct Userlist *
GetUser(int nickonly, char *nickname, char *user, char *host)

{
  struct Userlist *tempuser,
            /* we found a hostname match, but not a nick match */
            *hostmatch, 
            /* we found a nick match, but not a hostname match */
            *nickmatch;

  if (!nickname)
    return (NULL);
  
  hostmatch = nickmatch = NULL;
  
  /*
   * First, attempt to match both nickname and hostname, if possible.
   * If we don't find a match, try to match just hostnames, because
   * it is possible the user is simply using another nickname
   */

  for (tempuser = UserList; tempuser; tempuser = tempuser->next)
  {
    if (!irccmp(tempuser->nick, nickname))
    {
      if (nickonly && (!user && !host))
      {
        /*
         * We found a nickname match - nickonly is 1, and userhost
         * is not supplied, return the pointer
         */
        return(tempuser);
      }
      else
      {
        /*
         * We have a nickname match, but nickonly is 0, so try
         * to match userhost's as well
         */
        if (match(tempuser->username, user) &&
            match(tempuser->hostname, host))
          return (tempuser);
      }

      /*
       * Either nickonly is 0, and the userhosts don't match, or
       * nickonly is 1, but a userhost was still given and does
       * not match tempuser's userhost.  In either case, we have
       * a nickname match, so store this user structure into
       * 'nickmatch', so if we don't find a match later on,
       * with a nickname and hostname, we will return 'nickmatch'
       * but only if nickonly is 1, meaning we are allowed to
       * match only nicknames
       */
      if (!nickmatch)
        nickmatch = tempuser;
    }
    else if (user && host)
    {
      if (match(tempuser->username, user) &&
          match(tempuser->hostname, host))
      {
        /*
         * The nicknames do not match, but the userhosts do.
         * Record this user structure as a hostname match; in
         * case we don't find an exact match with both
         * nickname and hostname, this is still a pretty good
         * match
         */
        if (!hostmatch)
          hostmatch = tempuser;
      }
    }
  }

  if (hostmatch)
  {
    /*
     * The given userhost matches an O: line, but the nickname
     * does not - it is still a good match since the user
     * may be using a different nickname than usual -
     * return the hostname pointer
     */
    return (hostmatch);
  }

  if (nickonly && nickmatch)
  {
    /*
     * the given userhost doesn't have an O: line, but we did
     * have a nickname match, and since nickonly is 1, we are
     * allowed to disregard hostnames, so return the nickname
     * match we found
     */
    return (nickmatch);
  }

  if (OpersHaveAccess)
  {
    if (IsOperator(FindClient(nickname)))
      return (GenericOper);
  }

  return (NULL);
} /* GetUser() */

#if defined AUTO_ROUTING && defined SPLIT_INFO
void AddReconnect(const char *hub, const int port, const char *leaf,
    const long re_time)
{
  struct cHost *ptr;
  ptr=(struct cHost *)MyMalloc(sizeof(struct cHost));
  memset(ptr, 0, sizeof(struct cHost));
  ptr->hub=MyStrdup(hub);
  ptr->port=port;
  ptr->leaf=MyStrdup(leaf);
  ptr->re_time=re_time;

  ptr->next=cHostList;
  cHostList=ptr;
} /* AddReconnect() */
#endif
