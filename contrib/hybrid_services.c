/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  hybrid_services.c: /nickserv, /chanserv, /memoserv, etc. commands, 
 *                     used for hybrid services (HybServ2) on 
 *                     CARNet IRC Network.
 *
 *  Copyright (C) 2002 by Dragan Dosen and the ircd-hybrid development team
 *  Based on m_services.c, originally from bahamut ircd.
 *
 *  Contact info:
 *
 *    E-mail : ddosen@irc.vef.hr
 *    IRC    : (*) Bane, CARNet IRC Network, irc.vef.hr, #tisina
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1.Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 *  2.Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution. 
 *  3.The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *   $Id$
 */

/* List of ircd includes from ../include/ */
#include "stdinc.h"
#include "handlers.h"
#include "client.h"
#include "common.h"     /* FALSE bleah */
#include "ircd.h"
#include "irc_string.h"
#include "numeric.h"
#include "fdlist.h"
#include "hash.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "channel.h"
#include "channel_mode.h"


#define MAXLINE  512    /* don't change this */

/* Prototypes for functions
 */
static void m_global(struct Client *client_p, struct Client *source_p,
                     int parc, char *parv[]);
static void m_nickserv(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[]);
static void m_chanserv(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[]);
static void m_memoserv(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[]);
static void m_seenserv(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[]);
static void m_operserv(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[]);
static void m_statserv(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[]);
static void m_helpserv(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[]);
static void m_identify(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[]);

/* Structures
 */
struct Message global_msgtab = {
  "GLOBAL", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_global, m_ignore, m_global}
};
struct Message nickserv_msgtab = {
  "NICKSERV", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_nickserv, m_ignore, m_nickserv}
};
struct Message chanserv_msgtab = {
  "CHANSERV", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_chanserv, m_ignore, m_chanserv}
};
struct Message memoserv_msgtab = {
  "MEMOSERV", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_memoserv, m_ignore, m_memoserv}
};
struct Message seenserv_msgtab = {
  "SEENSERV", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_seenserv, m_ignore, m_seenserv}
};
struct Message operserv_msgtab = {
  "OPERSERV", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_operserv, m_ignore, m_operserv}
};
struct Message statserv_msgtab = {
  "STATSERV", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_statserv, m_ignore, m_statserv}
};
struct Message helpserv_msgtab = {
  "HELPSERV", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_helpserv, m_ignore, m_helpserv}
};
struct Message identify_msgtab = {
  "IDENTIFY", 0, 0, 0, 2, MFLG_SLOW, 0,
  {m_unregistered, m_identify, m_ignore, m_identify}
};


/* Aliases only for NickServ, ChanServ and MemoServ:
 * /ns, /cs and /ms
 */
struct Message ns_msgtab = {
  "NS", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_nickserv, m_ignore, m_nickserv}
};
struct Message cs_msgtab = {
  "CS", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_chanserv, m_ignore, m_chanserv}
};
struct Message ms_msgtab = {
  "MS", 0, 0, 1, 0, MFLG_SLOW, 0,
  {m_unregistered, m_memoserv, m_ignore, m_memoserv}
};


#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&global_msgtab);
  mod_add_cmd(&nickserv_msgtab);
  mod_add_cmd(&chanserv_msgtab);
  mod_add_cmd(&memoserv_msgtab);
  mod_add_cmd(&seenserv_msgtab);
  mod_add_cmd(&operserv_msgtab);
  mod_add_cmd(&statserv_msgtab);
  mod_add_cmd(&helpserv_msgtab);
  mod_add_cmd(&identify_msgtab);

  mod_add_cmd(&ns_msgtab);
  mod_add_cmd(&cs_msgtab);
  mod_add_cmd(&ms_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&global_msgtab);
  mod_del_cmd(&nickserv_msgtab);
  mod_del_cmd(&chanserv_msgtab);
  mod_del_cmd(&memoserv_msgtab);
  mod_del_cmd(&seenserv_msgtab);
  mod_del_cmd(&operserv_msgtab);
  mod_del_cmd(&statserv_msgtab);
  mod_del_cmd(&helpserv_msgtab);
  mod_del_cmd(&identify_msgtab);

  mod_del_cmd(&ns_msgtab);
  mod_del_cmd(&cs_msgtab);
  mod_del_cmd(&ms_msgtab);
}

char *_version = "$Revision$";
#endif

/*
 * GetString()
 *
 * Reverse the array parv back into a normal string assuming there are
 * 'parc' indices in the array. Space is allocated for the new string.
 * Thanks to kre.
 */
char *GetString(int parc, char *parv[])
{
  char *final;
  char temp[MAXLINE];
  int ii = 0;

  final = MyMalloc(sizeof(char));
  *final = '\0';

  while (ii < parc)
  {
    ircsprintf(temp, "%s%s", parv[ii], ((ii + 1) >= parc) ? "" : " ");
    final = MyRealloc(final,  strlen(final) + strlen(temp) +
        sizeof(char));
    strcat(final, temp);
    ++ii;
  }

  return (final);
} /* GetString() */


/*
 * m_identify()
 *
 * Short identify command.
 */
static void m_identify(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[])
{
  struct Client *acptr;

  if (parc == 3)
  {
    acptr = find_person("ChanServ");
    if (!acptr || IsServer(acptr))
    {
      sendto_one(source_p,
          ":%s NOTICE %s :*** Notice -- Services are currently down. "
          "Please wait a few moments, and then try again.",
          me.name, parv[0]);
      return;
    }
    else
      sendto_one(acptr, ":%s PRIVMSG ChanServ :IDENTIFY %s %s",
	         parv[0], parv[1], parv[2]);
  }
  else
  if (parc == 2)
  {
    acptr = find_person("NickServ");
    if (!acptr || IsServer(acptr))
    {
      sendto_one(source_p,
          ":%s NOTICE %s :*** Notice -- Services are currently down. "
          "Please wait a few moments, and then try again.",
		      me.name, parv[0]);
      return;
    }
    else
      sendto_one(acptr, ":%s PRIVMSG NickServ :IDENTIFY %s",
                 parv[0], parv[1]);
  }
  else
  /* WTF? -kre */
  /* if (parc == 0 || parv[1] == '\0') */
  {
    sendto_one(source_p,
        ":%s NOTICE %s :Syntax:  IDENTIFY <password>            - for nickname",
               me.name, parv[0]);
    sendto_one(source_p,
        ":%s NOTICE %s :Syntax:  IDENTIFY <channel> <password>  - for channel",
               me.name, parv[0]);
  }

} /* m_identify() */


/*
 * deliver_services_msg()
 *
 *   parv[0]    = sender prefix
 *   servmsg    = message for services (using GetString() function).
 *                I think this is better. -ddosen
 */
static void deliver_services_msg(char *service, char *command, struct
    Client *client_p, struct Client *source_p, int parc, char *parv[])
{
  struct Client *acptr;
  char *servmsg;

  /* first of all, make sure the nick is online.
   */
  acptr = find_client(service);

  if (!acptr || IsServer(acptr))
  {
    sendto_one(source_p,
        ":%s NOTICE %s :*** Notice -- Services are currently down. "
        "Please wait a few moments, and then try again.",
        me.name, parv[0]);
  }
  else 
  {
    if (parc == 1 || parv[1] == '\0')
    {
      sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
          command);
      return;
    }

    servmsg = GetString(parc - 1, parv + 1);

    /* deliver the message.
     * now, it's more secure (using user@host)
     */
    sendto_one(acptr, ":%s PRIVMSG %s :%s", parv[0], acptr->name,
        servmsg);
    }
} /* deliver_services_msg() */


/* SERV_FUNC
 * Macro for generating service's function.
 */
#define SERV_FUNC(a,b,c) static void a (struct Client *client_p, struct \
    Client *source_p, int parc, char *parv[]) \
{ return deliver_services_msg (b, c, client_p, source_p, parc, parv); }

/* let's generate :-)
 *
 * Fields are:  function_name, services nickname, command name
 */
SERV_FUNC(m_global, "Global", "GLOBAL")
SERV_FUNC(m_nickserv, "NickServ", "NICKSERV")
SERV_FUNC(m_chanserv, "ChanServ", "CHANSERV")
SERV_FUNC(m_memoserv, "MemoServ", "MEMOSERV")
SERV_FUNC(m_seenserv, "SeenServ", "SEENSERV")
SERV_FUNC(m_operserv, "OperServ", "OPERSERV")
SERV_FUNC(m_statserv, "StatServ", "STATSERV")
SERV_FUNC(m_helpserv, "HelpServ", "HELPSERV")
