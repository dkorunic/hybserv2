
#include "stdinc.h"
#include "handlers.h"
#include "client.h"
#include "hash.h"
#include "fdlist.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_stats.h"
#include "s_user.h"
#include "hash.h"
#include "whowas.h"
#include "s_serv.h"
#include "send.h"
#include "list.h"
#include "channel.h"
#include "s_log.h"
#include "resv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "common.h"
#include "packet.h"

static void ms_svsnick(struct Client *, struct Client *, int, char**);

static int clean_nick_name(char *);


struct Message svsnick_msgtab =
    {
	    "SVSNICK", 0, 0, 3, 0, MFLG_SLOW, 0,
	    {m_unregistered, m_not_oper, ms_svsnick, m_ignore, ms_svsnick, m_ignore}
    };

#ifndef STATIC_MODULES
void
_modinit(void)
{
	mod_add_cmd(&svsnick_msgtab);
}

void
_moddeinit(void)
{
	mod_del_cmd(&svsnick_msgtab);
}

const char *_version = "$Revision$";
#endif

/*
 * ms_svsnick()
 *
 *     parv[0] = sender prefix
 *     parv[1] = oldnick
 *     parv[2] = newnick
 */
static void ms_svsnick(struct Client *client_p, struct Client *source_p,
                       int parc, char *parv[])
{
	char     oldnick[NICKLEN];
	char     newnick[NICKLEN];
	struct   Client *oldnickname;
	struct   Client *newnickname;

	if (!IsServer(source_p))
		return;

	/* XXX BadPtr is needed */
	if(parc < 3 || BadPtr(parv[1]) || BadPtr(parv[2]))
	{
		sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN),
		           me.name, parv[0]);
		return;
	}

	/* terminate nick to NICKLEN */
	strlcpy(oldnick, parv[1], NICKLEN);
	strlcpy(newnick, parv[2], NICKLEN);

	if(!clean_nick_name(oldnick))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME),
		           me.name, parv[0], oldnick);
		return;
	}

	/* check the nickname is ok */
	if(!clean_nick_name(newnick))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME),
		           me.name, parv[0], newnick);
		return;
	}

	if(find_nick_resv(newnick) &&
	        !(IsOper(source_p) && ConfigChannel.oper_pass_resv))
	{
		sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
		           me.name, parv[0], newnick);
		return;
	}
	if(!(oldnickname = find_client(oldnick)))
	{
		sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name, source_p->name, oldnick);
		return;
	}
	if(newnickname = find_client(newnick))
	{
		sendto_one(source_p, form_str(ERR_NICKNAMEINUSE), me.name,
		           parv[0], newnick);
		return;
	}
	if(MyConnect(oldnickname))
	{
		if(!IsFloodDone(oldnickname))
			flood_endgrace(oldnickname);
		if (newnickname = find_client(oldnick))
		{
			if(newnickname == oldnickname)
			{
				/* check the nick isnt exactly the same */
				if(strcmp(oldnickname->name, newnick))
				{
					change_local_nick(oldnickname->servptr, oldnickname, newnick);
					return;
				}
				else
				{
					/* client is doing :old NICK old
					* ignore it..
					*/
					return;
				}
			}
			/* if the client that has the nick isnt registered yet (nick but no
			 * user) then drop the unregged client
			 */
			if(IsUnknown(newnickname))
			{
				/* the old code had an if(MyConnect(target_p)) here.. but I cant see
				 * how that can happen, m_nick() is local only --fl_
				 */

				exit_client(NULL, newnickname, &me, "Overridden");
				change_local_nick(oldnickname->servptr, oldnickname, newnick);
				return;
			}
			else
			{
				sendto_one(source_p, form_str(ERR_NICKNAMEINUSE), me.name,
				           parv[0], newnick);
				return;
			}
		}
		else
		{
			if(!ServerInfo.hub && uplink && IsCapable(uplink, CAP_LL))
			{
				/* The uplink might know someone by this name already. */
				sendto_one(uplink, ":%s NBURST %s %s %s", me.name, newnick,
				           newnick, oldnickname->name);
				return;
			}
			else
			{
				change_local_nick(oldnickname->servptr, oldnickname, newnick);
				return;
			}
		}
	}
	else
	{
		sendto_server(client_p, source_p, NULL, CAP_SVNICK, NOCAPS, NOFLAGS,
		              ":%s SVSNICK %s %s", me.name, oldnick, newnick);
		return;
	}
}
static int
clean_nick_name(char *nick)
{
	assert(nick);
	if(nick == NULL)
		return 0;

	/* nicks cant start with a digit or - or be 0 length */
	/* This closer duplicates behaviour of hybrid-6 */

	if (*nick == '-' || IsDigit(*nick) || *nick == '\0')
		return 0;

	for(; *nick; nick++)
	{
		if(!IsNickChar(*nick))
			return 0;
	}

	return 1;
}
