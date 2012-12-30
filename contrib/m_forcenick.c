/*
 * m_forcenick.c
 * Hybrid7 FORCENICK module
 * Copyright (C) 2002 Dinko 'kreator' Korunic <dinko.korunic@gmail.com>
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
 *  10/2005. update from Brian Brazil to send notices if oper is changing
 *  a remote nickname
 *
 */

#include "stdinc.h"
#include "handlers.h"
#include "client.h"
#include "common.h"
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


static void mo_forcenick(struct Client *client_p, struct Client *source_p,
                         int parc, char *parv[]);
struct Message forcenick_msgtab =
    {
	    "FORCENICK", 0, 0, 3, 0, MFLG_SLOW, 0,
	    {m_unregistered, m_not_oper, mo_forcenick, m_ignore, mo_forcenick, m_ignore}
    };

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&forcenick_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&forcenick_msgtab);
}

char *_version = "$Revision: 1325$";
#endif

/* is_nickname()
 *
 * input        - nickname
 * output       - none
 * side effects - walks through the nickname, returning 0 if erroneous
 */
static int is_nickname(char *nick)
{
	if (nick == NULL)
		return 0;

	/* nicks cant start with a digit or - or be 0 length */
	/* This closer duplicates behaviour of hybrid-6 */
	if (*nick == '-' || IsDigit(*nick) || *nick == '\0')
		return 0;

	for (; *nick; nick++)
	{
		if (!IsNickChar(*nick))
			return 0;
	}

	return 1;
}

/*
 * mo_forcenick()
 *      parv[0] = sender prefix
 *      parv[1] = user to force
 *      parv[2] = nick to force them to
 */
static void mo_forcenick(struct Client *client_p, struct Client *source_p,
                         int parc, char *parv[])
{
	struct Client *target_p;

	if (!is_nickname(parv[2]))
	{
		sendto_one(source_p,
		           ":%s NOTICE %s :*** Notice -- Invalid new nickname %s",
		           me.name, source_p->name, parv[2]);
		return;
	}

	if (strlen(parv[2]) > NICKLEN - 1)
		parv[2][NICKLEN - 1] = '\0';

	if ((target_p = find_client(parv[1])) == NULL)
	{
		sendto_one(source_p,
		           ":%s NOTICE %s :*** Notice -- No such nickname %s",
		           me.name, source_p->name, parv[1]);
		return;
	}

	if (IsOper(source_p)) /* send it normally */
	{
		sendto_realops_flags(UMODE_ALL, L_ALL,
		                     "Received FORCENICK message for '%s'->'%s'. From %s!%s@%s",
		                     target_p->name, parv[2], source_p->name,
		                     source_p->username, source_p->host);
	}
	else
	{
		sendto_realops_flags(UMODE_SKILL, L_ALL,
		                     "Received FORCENICK message for '%s'->'%s'. From %s",
		                     target_p->name, parv[2], parv[0]);
	}

	/* Pass on the message to everyone */
	sendto_server(client_p, source_p, NULL, NOCAPS, NOCAPS,
	              LL_ICLIENT, ":%s FORCENICK %s %s", parv[0], parv[1], parv[2]);

	/* See if the user is ours */
	if (!MyClient(target_p))
		return;

	if (find_client(parv[2]) != NULL)
	{
		sendto_one(source_p,
		           ":%s NOTICE %s :*** Notice -- Nickname %s is in use",
		           me.name, source_p->name, parv[2]);
		return;
	}

	change_local_nick(target_p, target_p, parv[2]);
}
