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
#include "channel.h"
#include "chanserv.h"
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
#include "nickserv.h"
#include "operserv.h"
#include "seenserv.h"
#include "settings.h"
#include "sock.h"
#include "sprintf_irc.h"

#ifdef CHANNELSERVICES

/*
 * Global - linked list of channels
 */
struct Channel *ChannelList = NULL;

/*
 * AddBan()
 * args: char *who, struct Channel *cptr, char *ban
 * purpose: add 'ban' set by 'who' to channel 'cptr'
 * return: none
 */
void AddBan(const char *who, struct Channel *cptr, const char *ban)
{
	time_t CurrTime = current_ts;
	struct ChannelBan *tempban = NULL;

	if (FindBan(cptr, ban))
		return;

	tempban = MyMalloc(sizeof(struct ChannelBan));
	memset(tempban, 0, sizeof(struct ChannelBan));

	if (who != NULL)
		tempban->who = MyStrdup(who);

	tempban->mask = MyStrdup(ban);
	tempban->when = CurrTime;
	tempban->next = cptr->firstban;
	tempban->prev = NULL;

	if (tempban->next != NULL)
		tempban->next->prev = tempban;
	cptr->firstban = tempban;
} /* AddBan() */

/*
 * DeleteBan()
 * args: struct Channel *cptr, char *ban
 * purpose: remove 'ban' from channel 'cptr'
 * return: none
 */
void DeleteBan(struct Channel *cptr, const char *ban)
{
	struct ChannelBan *bptr = NULL;

	if ((bptr = FindBan(cptr, ban)) == NULL)
		return;

	MyFree(bptr->mask);
	MyFree(bptr->who);

	if (bptr->prev != NULL)
		bptr->prev->next = bptr->next;
	else
	{
		/*
		 * We're deleting the first ban in the list, reset
		 * cptr->firstban
		 */
		cptr->firstban = bptr->next;
	}

	if (bptr->next != NULL)
		bptr->next->prev = bptr->prev;

	MyFree(bptr);
} /* DeleteBan() */

/*
 * AddException()
 * Add hostmask 'mask' set by 'who' to channel exception list
 */
void AddException(const char *who, struct Channel *cptr, const char *mask)
{
	struct Exception *tempe = NULL;

    if (FindException(cptr, mask))
		return;

	tempe = MyMalloc(sizeof(struct Exception));
	memset(tempe, 0, sizeof(struct Exception));

	if (who != NULL)
		tempe->who = MyStrdup(who);

	tempe->mask = MyStrdup(mask);
	tempe->when = current_ts;

	tempe->next = cptr->exceptlist;
	tempe->prev = NULL;
	if (tempe->next != NULL)
		tempe->next->prev = tempe;

	cptr->exceptlist = tempe;
} /* AddException() */

#ifdef HYBRID7
/*
 * AddInviteException()
 * Add hostmask 'mask' set by 'who' to channel invite exception list
 * -Janos
 *
 * XXX: This code is all from AddException(). They have to be merged into
 * single function that has an additional argument which will differ
 * exceptions -kre 
 */
void AddInviteException(const char *who, struct Channel *cptr, const char
		*mask)
{
	struct InviteException *tempinvex = NULL;

    if (FindInviteException(cptr, mask))
		return;

	tempinvex = MyMalloc(sizeof(struct InviteException));
	memset(tempinvex, 0, sizeof(struct InviteException));

	if (who != NULL)
		tempinvex->who = MyStrdup(who);

	tempinvex->mask = MyStrdup(mask);
	tempinvex->when = current_ts;

	tempinvex->next = cptr->inviteexceptlist;
	tempinvex->prev = NULL;
	if (tempinvex->next != NULL)
		tempinvex->next->prev = tempinvex;

	cptr->inviteexceptlist = tempinvex;
} /* AddInviteException() */
#endif /* HYBRID7 */

/*
 * DeleteException()
 * Remove hostmask 'mask' from cptr's exception list
 */
void DeleteException(struct Channel *cptr, const char *mask)
{
	struct Exception *tempe = NULL;

	if ((tempe = FindException(cptr, mask)) == NULL)
		return;

	MyFree(tempe->mask);
	MyFree(tempe->who);

	if (tempe->prev != NULL)
		tempe->prev->next = tempe->next;
	else
		cptr->exceptlist = tempe->next;

	if (tempe->next != NULL)
		tempe->next->prev = tempe->prev;

	MyFree(tempe);
} /* DeleteException() */

#ifdef HYBRID7
/*
 * DeleteInviteException()
 * Remove hostmask 'mask' from cptr's invite exception list
 * -Janos
 * 
 * XXX: Same as the above - they _have_ to go in _same_ function, this way
 * we accumulate useless repeating code. -kre
*/
void DeleteInviteException(struct Channel *cptr, const char *mask)
{
	struct InviteException *tempinvex = NULL;

	if ((tempinvex = FindInviteException(cptr, mask)) == NULL)
		return;

	MyFree(tempinvex->mask);
	MyFree(tempinvex->who);

	if (tempinvex->prev != NULL)
		tempinvex->prev->next = tempinvex->next;
	else
		cptr->inviteexceptlist = tempinvex->next;

	if (tempinvex->next != NULL)
		tempinvex->next->prev = tempinvex->prev;

	MyFree(tempinvex);
} /* DeleteInviteException() */
#endif /* HYBRID7 */

/*
 * MatchBan()
 * Same as FindBan() but use match() to compare bans to allow
 * for wildcards
 */
struct ChannelBan *MatchBan(const struct Channel *cptr, const char *ban)
{
	struct ChannelBan *tempban = NULL;

	tempban = cptr->firstban;
	while ((tempban != NULL) &&
	        (match(ban, tempban->mask) == 0))
		tempban = tempban->next;

	return (tempban);
} /* MatchBan() */

/*
 * FindBan()
 * args: struct Channel *cptr, char *ban
 * purpose: determine if 'ban' is on 'cptr's ban list
 * return: pointer to ban
 */
struct ChannelBan *FindBan(const struct Channel *cptr, const char *ban)
{
	struct ChannelBan *tempban = NULL;

	if ((cptr == NULL) || (ban == NULL))
		return NULL;

	tempban = cptr->firstban;
	while ((tempban != NULL) &&
	        (irccmp(tempban->mask, ban) != 0))
		tempban = tempban->next;

	return (tempban);
} /* FindBan() */

/*
 * MatchException()
 * Same as FindException() but use match() to compare bans to allow
 * for wildcards
 */
struct Exception *MatchException(const struct Channel *cptr, const char
		*ban)
{
	struct Exception *tempe = NULL;

	tempe = cptr->exceptlist;
	while ((tempe != NULL) &&
	        (match(ban, tempe->mask) == 0))
		tempe = tempe->next;

	return (tempe);
} /* MatchException() */

/*
 * FindException()
 * Return a pointer to occurence of 'mask' on cptr's ban exception
 * list
 */
struct Exception *FindException(const struct Channel *cptr, const char *mask)
{
	struct Exception *tempe = NULL;

	if ((cptr == NULL) || (mask == NULL))
		return NULL;

	tempe = cptr->exceptlist;
	while ((tempe != NULL)
	        && (irccmp(tempe->mask, mask) != 0))
		tempe = tempe->next;

	return (tempe);
} /* FindException() */

#ifdef HYBRID7
/*
 * FindInviteException()
 * Return a pointer to occurence of 'mask' on cptr's invite exception list
 * -Janos
 *
 * XXX: merge into FindException() -kre
 */
struct InviteException *FindInviteException(const struct Channel *cptr,
		const char *mask)
{
	struct InviteException *tempinvex = NULL;

	if ((cptr == NULL) || (mask == NULL))
		return NULL;

	tempinvex = cptr->inviteexceptlist;
	while ((tempinvex != NULL) && (irccmp(tempinvex->mask, mask) != 0))
		tempinvex = tempinvex->next;

	return (tempinvex);
} /* FindInviteException() */
#endif /* HYBRID7 */

/*
 * AddChannel()
 * args: char **line, int nickcnt, char **nicks
 * purpose: add channel info in 'line' to channel list
 * return: pointer to channel structure
 *
 *   line[2] = channel origin
 *   line[3] = channel name
 *   line[4] = channel modes
 *   line[5] = nicks in channel (or new nick who's joining)
 *
 * s_sjoin() already splits the nicknames up into a nice array, so if
 * nickcnt > 0, use the provided nick array, otherwise create one from the
 * nicks in 'line'
 */
struct Channel *AddChannel(char **line, const int nickcnt, char **nicks)
{
	char *names = NULL;
	char **anames = NULL;
	char *currnick = NULL;
	char modes[MAXLINE + 1];
	struct Channel *chname = NULL;
	struct Channel *cptr = NULL;
	struct Channel *tempchan = NULL;
	int ii, ncnt, acnt;

	ncnt = 5; /* default position for channel nicks, if no limit/key */
	strlcpy(modes, line[4], sizeof(modes));

	if (line[ncnt][0] != ':') /* names list *should* start w/ a : */
	{                         /* if it doesn't, theres a limit and/or key */
		strlcat(modes, " ", sizeof(modes));
		strlcat(modes, line[ncnt], sizeof(modes));
		ncnt++;
		if (line[ncnt][0] != ':')
		{
			strlcat(modes, " ", sizeof(modes));
			strlcat(modes, line[ncnt], sizeof(modes));
			ncnt++;
		}
	}

	if (nickcnt > 0)
	{
		acnt = nickcnt;
		anames = nicks;
	}
	else
	{
		names = line[ncnt];
		names++; /* point past the leading : */

		ii = strlen(names);

		/* kill the \n char on the end */
		if (IsSpace(names[ii - 2]))
			names[ii - 2] = '\0';
		else if (IsSpace(names[ii - 1]))
			names[ii - 1] = '\0';

		acnt = SplitBuf(names, &anames);
	}

	if ((cptr = FindChannel(line[3])) == NULL)
	{
#ifdef BLOCK_ALLOCATION
		tempchan = (struct Channel *) BlockSubAllocate(ChannelHeap);
		memset(tempchan, 0, sizeof(struct Channel));
		strlcpy(tempchan->name, line[3], CHANNELLEN + 1);
#else

		tempchan = MyMalloc(sizeof(struct Channel));
		memset(tempchan, 0, sizeof(struct Channel));
		tempchan->name = MyStrdup(line[3]);
#endif /* BLOCK_ALLOCATION */

		tempchan->since = atol(line[2]);
		tempchan->numusers = acnt;

		tempchan->next = ChannelList;
		tempchan->prev = NULL;
		if (tempchan->next != NULL)
			tempchan->next->prev = tempchan;

		HashAddChan(tempchan);

		ChannelList = tempchan;
		chname = ChannelList;

		++Network->TotalChannels;
#ifdef STATSERVICES

		if (Network->TotalChannels > Network->MaxChannels)
		{
			Network->MaxChannels = Network->TotalChannels;
			Network->MaxChannels_ts = current_ts;

			if ((Network->MaxChannels % 10) == 0)
			{
				/* notify +y people about new max channel count */
				SendUmode(OPERUMODE_Y, "*** New Max Channel Count: %ld",
				          Network->MaxChannels);
				putlog(LOG2, "New Max Channel Count: %ld",
						Network->MaxChannels);
			}
		}
		if (Network->TotalChannels > Network->MaxChannelsT)
		{
			Network->MaxChannelsT = Network->TotalChannels;
			Network->MaxChannelsT_ts = current_ts;
		}
#endif /* STATSERVICES */

	}
	else /* it's an existing channel, but someone has joined it */
	{
		cptr->numusers += acnt;
		chname = cptr;
	}

	/* Add the channel to each nick's channel list */
	for (ii = 0; ii < acnt; ii++)
	{
		currnick = GetNick(anames[ii]);
		if (currnick == NULL)
			continue;

		if (!IsChannelMember(chname, FindClient(currnick)))
		{
			/* Use anames[ii] instead of currnick so we get the @/+ flags */
			AddToChannel(chname, anames[ii]);
		}
		else
			chname->numusers--;
	}

	/* finally, add the modes for the channel */
	UpdateChanModes(0, line[0] + 1, chname, modes);

	/* Only free anames[] if there was no nick list given */
	if (nickcnt <= 0)
		MyFree(anames);

	return (chname);
} /* AddChannel() */

/*
 * AddToChannel()
 * args: struct Channel *cptr, char *nick
 * purpose: add 'cptr' to the channels that 'nick' is in
 *          (when 'nick' joins a new channel, keep the channel list
 *          updated for 'nick')
 *          'nick' may contain an @ or a + indicating op/voice status
 * return: none
 */
void AddToChannel(struct Channel *cptr, char *nick)
{
	struct Luser *lptr = NULL;
	struct UserChannel *ptr = NULL;
	struct ChannelUser *ptr2 = NULL;
	char *tmpnick = NULL;

	tmpnick = GetNick(nick);
	lptr = FindClient(tmpnick);

	if ((lptr == NULL) || (cptr == NULL))
		return;

	ptr = MyMalloc(sizeof(struct UserChannel));
	ptr->chptr = cptr;
	ptr->flags = 0;

	if ((nick[0] == '+') || (nick[1] == '+'))
		ptr->flags |= CH_VOICED;
	if ((nick[0] == '@') || (nick[1] == '@'))
		ptr->flags |= CH_OPPED;
#ifdef HYBRID7_HALFOPS
	/* Add halfopped feature - Janos */
	if ((nick[0] == '%') || (nick[1] == '%'))
		ptr->flags |= CH_HOPPED;
#endif /* HYBRID7_HALFOPS */

	ptr->next = lptr->firstchan;
	lptr->firstchan = ptr;

	ptr2 = MyMalloc(sizeof(struct ChannelUser));
	ptr2->lptr = lptr;
	ptr2->flags = 0;

	if ((nick[0] == '+') || (nick[1] == '+'))
		ptr2->flags |= CH_VOICED;
	if ((nick[0] == '@') || (nick[1] == '@'))
		ptr2->flags |= CH_OPPED;
#ifdef HYBRID7_HALFOPS
	/* Add halfopped feature - Janos */
	if ((nick[0] == '%') || (nick[1] == '%'))
		ptr2->flags |= CH_HOPPED;
#endif /* HYBRID7_HALFOPS */

	ptr2->next = cptr->firstuser;
	cptr->firstuser = ptr2;
} /* AddToChannel() */

/*
 * DeleteChannel()
 * args: struct Channel *cptr
 * purpose: remove 'cptr' from channel list
 * return: none
 */
void DeleteChannel(struct Channel *cptr)
{
	struct ChannelUser *cnext = NULL;
	struct ChannelBan *bnext = NULL;
	struct Exception *enext = NULL;
#ifdef GECOSBANS

	struct ChannelGecosBan *gnext = NULL;
#endif /* GECOSBANS */
#ifdef HYBRID7

	struct InviteException *inext = NULL;
#endif /* HYBRID7 */

	if (cptr == NULL)
		return;

	HashDelChan(cptr);

	/* right now, having a loop to free the members of the channel is
	 * unnecessary, because this function is only called either from
	 * RemoveFromChannel(), which already took care of it, or from
	 * ClearChans(), which is called after ClearUsers() - ClearUsers() would
	 * have called RemoveFromChannel() to clear each user's channel list -
	 * thus cptr->firstuser will ALWAYS be null, however, this could
	 * possibly develop into a memory leak, if DeleteChannel() were ever to
	 * be called with users still in the channel, so this algorithm will
	 * take care of it.  */
	while (cptr->firstuser != NULL)
	{
		cnext = cptr->firstuser->next;
		MyFree(cptr->firstuser);
		cptr->firstuser = cnext;
	}

	/* clear channel bans */
	while (cptr->firstban != NULL)
	{
		bnext = cptr->firstban->next;
		MyFree(cptr->firstban->who);
		MyFree(cptr->firstban->mask);
		MyFree(cptr->firstban);
		cptr->firstban = bnext;
	}

#ifdef GECOSBANS
	/* clear channel denies */
	while (cptr->firstgecosban != NULL)
	{
		gnext = cptr->firstgecosban->next;
		MyFree(cptr->firstgecosban->who);
		MyFree(cptr->firstgecosban->mask);
		MyFree(cptr->firstgecosban);
		cptr->firstgecosban = gnext;
	}
#endif /* GECOSBANS */

	/* clear channel exceptions */
	while (cptr->exceptlist != NULL)
	{
		enext = cptr->exceptlist->next;
		MyFree(cptr->exceptlist->who);
		MyFree(cptr->exceptlist->mask);
		MyFree(cptr->exceptlist);
		cptr->exceptlist = enext;
	}

#ifdef HYBRID7
	/* Clear channel invite exceptions -Janos */
	while (cptr->inviteexceptlist != NULL)
	{
		inext = cptr->inviteexceptlist->next;
		MyFree(cptr->inviteexceptlist->who);
		MyFree(cptr->inviteexceptlist->mask);
		MyFree(cptr->inviteexceptlist);
		cptr->inviteexceptlist = inext;
	}
#endif /* HYBRID7 */

#ifndef BLOCK_ALLOCATION
	MyFree(cptr->name);
	MyFree(cptr->key);
#ifdef DANCER

	MyFree(cptr->forward);
#endif /* DANCER */
#endif /* BLOCK_ALLOCATION */

	if (cptr->prev != NULL)
		cptr->prev->next = cptr->next;
	else
		ChannelList = cptr->next;

	if (cptr->next != NULL)
		cptr->next->prev = cptr->prev;

#ifdef BLOCK_ALLOCATION

	BlockSubFree(ChannelHeap, cptr);

#else

	MyFree(cptr);

#endif /* BLOCK_ALLOCATION */

	--Network->TotalChannels;
} /* DeleteChannel() */

/*
 * RemoveNickFromChannel()
 * Similar to RemoveFromChannel(), except accept string
 * arguements for the nickname and channel
 */
void RemoveNickFromChannel(const char *channel, char *nickname)
{
	struct Channel *cptr = NULL;
	struct Luser *lptr = NULL;
	const char *tmp;

	tmp = channel;

	if (IsNickPrefix(*tmp))
	{
		if (IsNickPrefix(*(++tmp)))
			++tmp;
	}

	if ((cptr = FindChannel(tmp)) == NULL)
		return;

	tmp = GetNick(nickname);
	if ((lptr = FindClient(tmp)) == NULL)
		return;

	RemoveFromChannel(cptr, lptr);
} /* RemoveNickFromChannel() */

/*
 * RemoveFromChannel()
 * args: struct Channel *cptr, struct Luser *lptr
 * purpose: remove lptr from list of nicks in cptr (when a user
 *          leaves a channel)
 * return: none
*/
void RemoveFromChannel(struct Channel *cptr, struct Luser *lptr)
{
	struct UserChannel *tempchan = NULL;
	struct UserChannel *prev = NULL;
	struct ChannelUser *tempuser = NULL;
	struct ChannelUser *prev2 = NULL;
	struct NickInfo *nptr = NULL;
	struct ChanInfo *ciptr = NULL;

	if ((cptr == NULL) || (lptr == NULL))
		return;

	SendUmode(OPERUMODE_P, "*** Channel part: %s (%s)",
	          lptr->nick, cptr->name);

	/* Is this the founder? */
	if (((nptr = FindNick(lptr->nick)) != NULL)
	        && ((ciptr = FindChan(cptr->name)) != NULL))
	{
		if (nptr->flags & NS_IDENTIFIED)
		{
			if ((ciptr->founder != NULL)
			        && irccmp(lptr->nick, ciptr->founder) == 0)
				/* That's the founder joining. Update activity timer */
				ciptr->last_founder_active = current_ts;
			if ((ciptr->successor != NULL)
			        && irccmp(lptr->nick, ciptr->successor) == 0)
				ciptr->last_successor_active = current_ts;
		}
	}

	/* remove cptr from lptr's chan list */
	for (tempchan = lptr->firstchan; tempchan != NULL;
	        tempchan = tempchan->next)
	{
		if (cptr == tempchan->chptr)
		{
			if (prev != NULL)
				prev->next = tempchan->next;
			else
				lptr->firstchan = tempchan->next;
			MyFree(tempchan);
			tempchan = NULL;
			break;
		}
		prev = tempchan;
	}

	/* remove lptr from cptr's nick list */
	for (tempuser = cptr->firstuser; tempuser != NULL; tempuser =
	            tempuser->next)
	{
		if (lptr == tempuser->lptr)
		{
			if (prev2 != NULL)
				prev2->next = tempuser->next;
			else
				cptr->firstuser = tempuser->next;
			MyFree(tempuser);
			tempuser = NULL;
			--cptr->numusers;
			break;
		}
		prev2 = tempuser;
	}

	if (cptr->numusers == 0)
		DeleteChannel(cptr); /* the last nick left the chan, erase it */
} /* RemoveFromChannel() */

/*
 * SetChannelMode()
 * Set modes on a channel
 * 
 * Inputs: cptr    - channel
 *        add     - 1 if add the mode (+) 0 if subtract mode (-)
 *        type    - mode
 *        lptr    - optional user for o/v/h modes
 *        arg     - optional string for b/e modes
 * 
 * NOTE: This is currently used only for o/v/h modes since the others
 *      are simple enough to handle in UpdateChanModes()
 */
void SetChannelMode(struct Channel *cptr, const int add, const int type,
		const struct Luser *lptr)
{
	struct UserChannel *tempc = NULL;
	struct ChannelUser *tempu = NULL;

	tempu = FindUserByChannel(cptr, lptr);
	tempc = FindChannelByUser(lptr, cptr);

	assert(cptr != 0);

	if (type == MODE_O)
	{
		if ((tempu != NULL) && (tempc != NULL))
		{
			if (add)
			{
				tempu->flags |= CH_OPPED;
				tempc->flags |= CH_OPPED;
			}
			else
			{
				tempu->flags &= ~CH_OPPED;
				tempc->flags &= ~CH_OPPED;
			}
		} /* if (tempu && tempc) */
	}
	else if (type == MODE_V)
	{
		if ((tempu != NULL) && (tempc != NULL))
		{
			if (add)
			{
				tempu->flags |= CH_VOICED;
				tempc->flags |= CH_VOICED;
			}
			else
			{
				tempu->flags &= ~CH_VOICED;
				tempc->flags &= ~CH_VOICED;
			}
		}
	}
#ifdef HYBRID7_HALFOPS
	/* Halfop mode setup - Janos */
	else if (type == MODE_H)
	{
		if ((tempu != NULL) && (tempc != NULL))
		{
			if (add)
			{
				tempu->flags |= CH_HOPPED;
				tempc->flags |= CH_HOPPED;
			}
			else
			{
				tempu->flags &= ~CH_HOPPED;
				tempc->flags &= ~CH_HOPPED;
			}
		} /* if (tempu && tempc) */
	} /* (type == MODE_H) */
#endif /* HYBRID7_HALFOPS */
} /* SetChannelMode() */

/*
 * UpdateChanModes()
 * args: Luser *lptr, char *who, struct Channel *cptr, char *modes
 *       (lptr and who should match if its a user mode change)
 * purpose: keep modes field of chan structure updated when theres
 *          a mode change on a channel
 * return: none
 */
void UpdateChanModes(struct Luser *lptr, const char *who, struct
		Channel *cptr, char *modes)
{
	int add;
	char ch;
	char *tmp = NULL;
	struct Luser *userptr = NULL;

#if defined NICKSERVICES && defined CHANNELSERVICES

	int cs_deoped = 0; /* was chanserv deoped? */
#endif /* NICKSERVICES && CHANNELSERVICES */
#ifdef SEENSERVICES
	int es_devoiced = 0;
#endif

	char **modeargs; /* arguements to +l/k/o/v modes */
	char tempargs[MAXLINE + 1];
	int argcnt; /* number of arguements */
	int argidx; /* current index in modeargs[] */

	if (cptr == NULL)
		return;

	assert(lptr || who);

	if (lptr != NULL)
	{
		SendUmode(OPERUMODE_M, "*** %s: Mode [%s] by %s!%s@%s", cptr->name,
		          modes, lptr->nick, lptr->username, lptr->hostname);

		putlog(LOG3, "%s: mode change \"%s\" by %s!%s@%s", cptr->name,
		       modes, lptr->nick, lptr->username, lptr->hostname);
	}
	else
	{
		SendUmode(OPERUMODE_M, "*** %s: Mode [%s] by %s", cptr->name, modes,
		          who);

		putlog(LOG3, "%s: mode change \"%s\" by %s", cptr->name, modes, who);
	}

	if ((tmp = strchr(modes, ' ')))
		strlcpy(tempargs, *(tmp + 1) ? tmp + 1 : "", sizeof(tempargs));
	else
		tempargs[0] = '\0';

	argcnt = SplitBuf(tempargs, &modeargs);

	/* This routine parses the given channel modes and keeps the
	 * corresponding lists correctly updated - also make sure OperServ and
	 * ChanServ remain opped */

	add = 0;
	argidx = -1;

	for (tmp = modes; *tmp; ++tmp)
	{
		ch = *tmp;

		if (IsSpace(ch))
			break;

		switch (ch)
		{
		case ' ':
		case '\n':
		case '\r':
			break;

		case '-':
			{
				add = 0;
				break;
			}
		case '+':
			{
				add = 1;
				break;
			}

			/* Op/DeOp */
		case 'o':
			{
				++argidx;
				if (argidx >= argcnt)
				{
					/* there are more 'o' flags than there are nicknames */
					break;
				}

				if ((userptr = FindClient(modeargs[argidx])) == NULL)
					break;

				/* never mark ChanServ/OperServ as deopped -adx */
#if defined CHANNELSERVICES

				if (add || (userptr != Me.csptr && userptr != Me.osptr))
#else
				if (add || userptr != Me.osptr)
#endif /* CHANNELSERVICES */
					SetChannelMode(cptr, add, MODE_O, userptr);

				if (add)
				{
#ifdef STATSERVICES
					if (lptr != NULL)
						++lptr->numops;
#endif /* STATSERVICES */

				} /* if (add) */
				else
				{
					if (userptr == Me.osptr)
					{
						if (!FloodCheck(cptr, lptr, Me.osptr, 0))
						{
#ifdef SAVE_TS
							os_part(cptr);
							os_join(cptr);
#else

							toserv(":%s MODE %s +o %s\r\n", Me.name, cptr->name,
							       n_OperServ);
#endif /* SAVE_TS */

						}

						if (lptr == NULL)
						{
							putlog(LOG1, "%s: %s attempted to deop %s", cptr->name, who,
							       n_OperServ);
						}
						else
						{
							putlog(LOG1, "%s: %s!%s@%s attempted to deop %s",
							       cptr->name, lptr->nick, lptr->username, lptr->hostname,
							       n_OperServ);
						}
					}
#if defined NICKSERVICES && defined CHANNELSERVICES
					else if (userptr == Me.csptr)
					{
						cs_deoped = 1;
					}
#endif /* defined NICKSERVICES && defined CHANNELSERVICES */

#ifdef STATSERVICES
					if (lptr != NULL)
						++lptr->numdops;
#endif /* STATSERVICES */

				} /* else if (!add) */

#if defined NICKSERVICES && defined CHANNELSERVICES
				cs_CheckModes(lptr, FindChan(cptr->name), !add, MODE_O,
				              userptr);
#endif /* NICKSERVICES && CHANNELSERVICES */

				break;
			} /* case 'o' */

			/* Voice/DeVoice */
		case 'v':
			{
				++argidx;
				if (argidx >= argcnt)
					break;

				if ((userptr = FindClient(modeargs[argidx])) == NULL)
					break;
#ifdef SEENSERVICES
				if (add || (userptr != Me.esptr))
#endif
				SetChannelMode(cptr, add, MODE_V, userptr);

				if (add)
				{
#ifdef STATSERVICES
					if (lptr != NULL)
						++lptr->numvoices;
#endif /* STATSERVICES */

				}
				else
				{
#ifdef SEENSERVICES
					if (userptr == Me.esptr)
						es_devoiced = 1;
#endif
#ifdef STATSERVICES
					if (lptr != NULL)
						++lptr->numdvoices;
#endif /* STATSERVICES */

				} /* else if (!add) */

#if defined NICKSERVICES && defined CHANNELSERVICES
				cs_CheckModes(lptr, FindChan(cptr->name), !add, MODE_V,
				              userptr);
#endif /* NICKSERVICES && CHANNELSERVICES */

				break;
			} /* case 'v' */

#ifdef HYBRID7_HALFOPS
			/* HalfOp/DeHalfOp -Janos */
		case 'h':
			{
				++argidx;
				if (argidx >= argcnt)
					break;

				if ((userptr = FindClient(modeargs[argidx])) == NULL)
					break;

				SetChannelMode(cptr, add, MODE_H, userptr);

				if (add)
				{
#ifdef STATSERVICES
					if (lptr != NULL)
						++lptr->numhops;
#endif /* STATSERVICES */

				}
				else
				{
#ifdef STATSERVICES
					if (lptr != NULL)
						++lptr->numdhops;
#endif /* STATSERVICES */

				} /* else if (!add) */

#if defined NICKSERVICES && defined CHANNELSERVICES
				cs_CheckModes(lptr, FindChan(cptr->name), !add, MODE_H, userptr);
#endif /* NICKSERVICES && CHANNELSERVICES */

				break;
			} /* case 'h'*/
#endif /* HYBRID7_HALFOPS */

			/* Channel limit */
		case 'l':
			{
				if (add)
				{
					++argidx;
					if (argidx >= argcnt)
						break;

					cptr->limit = atoi(modeargs[argidx]);
				}
				else
					cptr->limit = 0;

#if defined NICKSERVICES && defined CHANNELSERVICES

				cs_CheckModes(lptr, FindChan(cptr->name), !add, MODE_L, 0);
#endif /* NICKSERVICES && CHANNELSERVICES */

				break;
			} /* case 'l' */

			/* Channel key */
		case 'k':
			{
				++argidx;
				if (argidx >= argcnt)
					break;

#ifndef BLOCK_ALLOCATION

				MyFree(cptr->key);
#endif /* BLOCK_ALLOCATION */

				if (add)
				{
#ifdef BLOCK_ALLOCATION
					strlcpy(cptr->key, modeargs[argidx], KEYLEN + 1);
#else

					cptr->key = MyStrdup(modeargs[argidx]);
#endif /* BLOCK_ALLOCATION */

				}
				else
				{
#ifdef BLOCK_ALLOCATION
					cptr->key[0] = '\0';
#else

					cptr->key = 0;
#endif /* BLOCK_ALLOCATION */

				}

#if defined NICKSERVICES && defined CHANNELSERVICES
				cs_CheckModes(lptr, FindChan(cptr->name), !add, MODE_K, 0);
#endif

				break;
			} /* case 'k' */

#ifdef DANCER
			/*
			 * Channel forwarding target
			 */
		case 'f':
			{
				++argidx;
				if (argidx >= argcnt)
					break;
#ifndef BLOCK_ALLOCATION

				MyFree(cptr->forward);
#endif

				if (add)
				{
#ifdef BLOCK_ALLOCATION
					strlcpy(cptr->forward, modeargs[argidx], CHANNELLEN + 1);
#else

					cptr->forward = MyStrdup(modeargs[argidx]);
#endif /* BLOCK_ALLOCATION */

				}
				else
				{
#ifdef BLOCK_ALLOCATION
					cptr->forward[0] = '\0';
#else

					cptr->forward = 0;
#endif /* BLOCK_ALLOCATION */

				}
#if defined NICKSERVICES && defined CHANNELSERVICES
				cs_CheckModes(lptr, FindChan(cptr->name), !add, MODE_F, 0);
#endif

				break;
			} /* case 'f' */
#endif /* DANCER */

			/* Channel ban */
		case 'b':
			{
				++argidx;
				if (argidx >= argcnt)
					break;

				if (add)
					AddBan(who, cptr, modeargs[argidx]);
				else
					DeleteBan(cptr, modeargs[argidx]);

				break;
			} /* case 'b' */

#ifdef GECOSBANS
			/* Channel deny */
		case 'd':
			{
				++argidx;
				if (argidx >= argcnt)
					break;

				if (add)
					AddGecosBan(who, cptr, modeargs[argidx]);
				else
					DeleteGecosBan(cptr, modeargs[argidx]);

				break;
			} /* case 'd' */
#endif /* GECOSBANS */

			/* Channel exception */
		case 'e':
			{
				++argidx;
				if (argidx >= argcnt)
					break;

				if (add)
					AddException(who, cptr, modeargs[argidx]);
				else
					DeleteException(cptr, modeargs[argidx]);

				break;
			} /* case 'e' */

#ifdef HYBRID7
			/* Channel invite exception -Janos */
		case 'I':
			{
				++argidx;
				if (argidx >= argcnt)
					break;

				if (add)
					AddInviteException(who, cptr, modeargs[argidx]);
				else
					DeleteInviteException(cptr, modeargs[argidx]);
				break;
			} /* case 'I' */
#endif /* HYBRID7 */

		default:
			{
				int modeflag = 0;

				if (ch == 's')
					modeflag = MODE_S;
				else if (ch == 'p')
					modeflag = MODE_P;
				else if (ch == 'n')
					modeflag = MODE_N;
				else if (ch == 't')
					modeflag = MODE_T;
				else if (ch == 'm')
					modeflag = MODE_M;
				else if (ch == 'i')
					modeflag = MODE_I;
#ifdef DANCER

				else if (ch == 'c')
					modeflag = MODE_C;
#endif /* DANCER */

				if (modeflag)
				{
					if (add)
						cptr->modes |= modeflag;
					else
						cptr->modes &= ~modeflag;
				}

#if defined NICKSERVICES && defined CHANNELSERVICES
				if (modeflag)
					cs_CheckModes(lptr, FindChan(cptr->name), !add,
							modeflag, 0);
#endif

				break;
			} /* default: */
		} /* switch (*tmp) */
	} /* for (tmp = modes; *tmp; ++tmp) */

	MyFree(modeargs);

#if defined NICKSERVICES && defined CHANNELSERVICES

	if ((cs_deoped) && (!FloodCheck(cptr, lptr, Me.csptr, 0)))
	{
		/* reop ChanServ */
#ifdef SAVE_TS
		cs_part(cptr);
		cs_join(FindChan(cptr->name));
#else

		toserv(":%s MODE %s +o %s\r\n", Me.name, cptr->name, n_ChanServ);
#endif /* SAVE_TS */

		if (lptr == NULL)
			putlog(LOG1, "%s: %s attempted to deop %s", cptr->name, who,
			       n_ChanServ);
		else
			putlog(LOG1, "%s: %s!%s@%s attempted to deop %s", cptr->name,
			       lptr->nick, lptr->username, lptr->hostname, n_ChanServ);
	}
#endif /* defined NICKSERVICES) && defined CHANNELSERVICES */

#ifdef SEENSERVICES

	if ((es_devoiced) && (!FloodCheck(cptr, lptr, Me.esptr, 0)))
	{
		/* revoice SeenServ */
#ifdef SAVE_TS
		ss_part(cptr);
		ss_join(cptr);
#else

		toserv(":%s MODE %s +v %s\r\n", Me.name, cptr->name, n_SeenServ);
#endif /* SAVE_TS */

		if (lptr == NULL)
			putlog(LOG1, "%s: %s attempted to devoice %s", cptr->name, who,
			       n_SeenServ);
		else
			putlog(LOG1, "%s: %s!%s@%s attempted to devoice %s", cptr->name,
			       lptr->nick, lptr->username, lptr->hostname, n_SeenServ);
	}
#endif /* SEENSERVICES */
} /* UpdateChanModes() */

/*
 * FindChannelByUser()
 * return a pointer to channel 'channel' in lptr's channel list
 */
struct UserChannel *FindChannelByUser(const struct Luser *lptr, const
		struct Channel *chptr)
{
	struct UserChannel *tempchan = NULL;

	if ((lptr == NULL) || (chptr == NULL))
		return NULL;

	for (tempchan = lptr->firstchan; tempchan != NULL; tempchan =
	            tempchan->next)
		if (tempchan->chptr == chptr)
			return (tempchan);

	return NULL;
} /* FindChannelByUser() */

/*
 * FindUserByChannel()
 * return pointer to client 'lptr' in channel chptr's nick list
*/
struct ChannelUser *FindUserByChannel(const struct Channel *chptr, const
		struct Luser *lptr)
{
	struct ChannelUser *tempuser = NULL;

	if ((chptr == NULL) || (lptr == NULL))
		return NULL;

	for (tempuser = chptr->firstuser; tempuser != NULL; tempuser =
	            tempuser->next)
		if (tempuser->lptr == lptr)
			return (tempuser);

	return NULL;
} /* FindUserByChannel() */

/*
 * IsChannelVoice()
 *  args: struct Channel *chptr, struct Luser *lptr
 *  purpose: determine if 'lptr' is currently voiced on 'chptr'
 *  return: 1 if voiced, 0 if not
 */
int IsChannelVoice(const struct Channel *chptr, const struct Luser *lptr)
{
	struct UserChannel *tempchan = NULL;

	if ((chptr == NULL) || (lptr == NULL))
		return 0;

	for (tempchan = lptr->firstchan; tempchan != NULL; tempchan =
	            tempchan->next)
		if (tempchan->chptr == chptr)
			return (tempchan->flags & CH_VOICED);

	return 0;
} /* IsChannelVoice() */

/*
 * IsChannelOp()
 *  args: char *channel, char *nick
 *  purpose: determine if 'nick' is currently oped on 'channel'
 *  return: 1 if oped, 0 if not
 */
int IsChannelOp(const struct Channel *chptr, const struct Luser *lptr)
{
	struct UserChannel *tempchan = NULL;

	if ((chptr == NULL) || (lptr == NULL))
		return 0;

	for (tempchan = lptr->firstchan; tempchan != NULL; tempchan =
	            tempchan->next)
		if (tempchan->chptr == chptr)
			return (tempchan->flags & CH_OPPED);

	return 0;
} /* IsChannelOp() */

/*
 * IsChannelHOp()
 *  args: char *channel, char *nick
 *  purpose: determine if 'nick' is currently halfoped on 'channel'
 *  return: 1 if oped, 0 if not
 *  -Janos
 *
 *  XXX: possibly merge this into IsChannelOp() -kre
 */
#ifdef HYBRID7_HALFOPS
int IsChannelHOp(const struct Channel *chptr, const struct Luser *lptr)
{
	struct UserChannel *tempchan = NULL;

	if ((chptr == NULL) || (lptr == NULL))
		return 0;

	for (tempchan = lptr->firstchan; tempchan != NULL; tempchan =
	            tempchan->next)
		if (tempchan->chptr == chptr)
			return (tempchan->flags & CH_HOPPED);

	return 0;
} /* IsChannelHOp() */
#endif /* HYBRID7_HALFOPS */

/*
 * DoMode()
 *  Set 'modes' on 'chptr'
 *  if 'joinpart' is != 1, do not join/part because the channel is being
 *  secured - so there might be more modes to come.
 */
void DoMode(struct Channel *chptr, char *modes, const int joinpart)
{
#ifdef SAVE_TS
	struct Chanlist *chanptr = NULL;
#endif /* SAVE_TS */

	if ((chptr == NULL) || (modes == NULL))
		return;

#ifdef SAVE_TS

	chanptr = IsChannel(chptr->name);

	if (joinpart && (chanptr == NULL))
		/* make sure its not already a monitored channel */
		os_join(chptr);

	toserv(":%s MODE %s %s\r\n", n_OperServ, chptr->name, modes);
	UpdateChanModes(Me.osptr, n_OperServ, chptr, modes);

	if (joinpart && (chanptr == NULL))
		os_part(chptr);

#else /* SAVE_TS */

	toserv(":%s MODE %s %s\r\n", Me.name, chptr->name, modes);
	UpdateChanModes(0, Me.name, chptr, modes);

#endif /* SAVE_TS */
} /* DoMode() */

/*
 * IsChannelMember()
 *  args: struct Channel *cptr, struct Luser *lptr
 *  purpose: determine if 'lptr' is currently on channel 'cptr'
 *  return: 1 if nick is on the channel, 0 if not
 */
int IsChannelMember(const struct Channel *cptr, const struct Luser *lptr)
{
	struct ChannelUser *tempuser = NULL;

	if ((cptr == NULL) || (lptr == NULL))
		return 0;

	for (tempuser = cptr->firstuser; tempuser != NULL; tempuser =
	            tempuser->next)
	{
		if (tempuser->lptr == lptr)
			return 1;
	}

	return 0;
} /* IsChannelMember() */

/*
 * SetModes()
 *  Set mode +/-<mode> <args> on <channel> from <source>
 */
void SetModes(const char *source, const int plus, const char mode, struct
		Channel *chptr, const char *args)
{
	int acnt, mcnt, ii;
	char done[MAXLINE + 1], sendstr[MAXLINE + 1];
	char **av, *temp = NULL, *mtmp = NULL;

	if ((source == NULL) || (chptr == NULL) || (args == NULL))
		return;

	/* sanity check: empty list of targets, don't do anything */
	if (*args == '\0')
		return;

	temp = MyStrdup(args);
	acnt = SplitBuf(temp, &av);
	done[0] = '\0';
	mcnt = 1;
	for (ii = 0; ii < acnt; ii++)
	{
		strlcat(done, av[ii], sizeof(done));
		if (mcnt != MaxModes)
			strlcat(done, " ", sizeof(done));
		else
		{
			mcnt = 0;
			mtmp = modestr(MaxModes, mode);
			ircsprintf(sendstr, "%s%s %s", plus ? "+" : "-", mtmp, done);
			toserv(":%s MODE %s %s\r\n", source, chptr->name, sendstr);
			UpdateChanModes(0, source, chptr, sendstr);
			MyFree(mtmp);
			done[0] = '\0';
		}
		mcnt++;
	}

	if (done[0] != '\0')
	{
		mtmp = modestr(mcnt - 1, mode);
		ircsprintf(sendstr, "%s%s %s", plus ? "+" : "-", mtmp, done);
		toserv(":%s MODE %s %s\r\n", source, chptr->name, sendstr);
		UpdateChanModes(0, source, chptr, sendstr);
		MyFree(mtmp);
	}
	MyFree(temp);
	MyFree(av);
} /* SetModes() */

/*
 * KickBan()
 *  KickBan 'nicks' on 'channel'
 */
void KickBan(const int ban, const char *source, struct Channel
		*channel, const char *nicks, const char *reason)
{
	char *mask = NULL, *tempnix = NULL, **av;
	char temp[MAXLINE + 1];
	int ac, ii;
	struct Luser *lptr = NULL;

	if ((source == NULL) || (channel == NULL) || (nicks == NULL))
		return;

	tempnix = MyStrdup(nicks);
	ac = SplitBuf(tempnix, &av);

	if (ban)
	{
		char bans[MAXLINE + 1];
		int jj = 1;
		bans[0] = '\0';

		for (ii = 0; ii < ac; ii++)
		{
			if ((lptr = FindClient(av[ii])) == NULL)
				continue;
			mask = HostToMask(lptr->username, lptr->hostname);
			ircsprintf(temp, "*!%s", mask);
			MyFree(mask);

			if (jj * (MAXUSERLEN + 1) >= sizeof(bans))
			{
				SetModes(source, 1, 'b', channel, bans);
				bans[0] = '\0';
				jj = 1;
			}

			strlcat(bans, temp, sizeof(bans));
			strlcat(bans, " ", sizeof(bans));
			++jj;
		}

		SetModes(source, 1, 'b', channel, bans);
	}

	for (ii = 0; ii < ac; ii++)
	{
		toserv(":%s KICK %s %s :%s\r\n", source, channel->name, av[ii],
		       reason ? reason : "");
		RemoveFromChannel(channel, FindClient(av[ii]));
	}

	MyFree(tempnix);
	MyFree(av);
} /* KickBan() */

#ifdef GECOSBANS
/*
 * AddGecosBan()
 *  args: char *who, struct Channel *cptr, char *ban
 *  purpose: add gecos 'ban' set by 'who' to channel 'cptr'
 *  return: none
 */
void AddGecosBan(const char *who, struct Channel *cptr, const char *ban)
{
	time_t CurrTime = current_ts;
	struct ChannelGecosBan *tempban = NULL;

    if (FindGecosBan(cptr, ban))
		return;

	tempban = MyMalloc(sizeof(struct ChannelGecosBan));
	memset(tempban, 0, sizeof(struct ChannelGecosBan));

	if (who != NULL)
		tempban->who = MyStrdup(who);

	tempban->mask = MyStrdup(ban);
	tempban->when = CurrTime;
	tempban->next = cptr->firstgecosban;
	tempban->prev = NULL;

	if (tempban->next != NULL)
		tempban->next->prev = tempban;
	cptr->firstgecosban = tempban;
} /* AddGecosBan() */

/*
 * DeleteGecosBan()
 *  args: struct Channel *cptr, char *ban
 *  purpose: remove gecos 'ban' from channel 'cptr'
 *  return: none
 */
void DeleteGecosBan(struct Channel *cptr, const char *ban)
{
	struct ChannelGecosBan *bptr = NULL;

	if ((bptr = FindGecosBan(cptr, ban)) == NULL)
		return;

	MyFree(bptr->mask);
	MyFree(bptr->who);

	if (bptr->prev != NULL)
		bptr->prev->next = bptr->next;
	else
	{
		/* deleting the first ban in the list, reset cptr->firstgecosban */
		cptr->firstgecosban = bptr->next;
	}

	if (bptr->next != NULL)
		bptr->next->prev = bptr->prev;

	MyFree(bptr);
} /* DeleteBan() */

/*
 * FindGecosBan()
 *  args: struct Channel *cptr, char *ban
 *  purpose: determine if 'ban' is on 'cptr's ban list
 *  return: pointer to ban
 */
struct ChannelGecosBan *FindGecosBan(const struct Channel *cptr, const
		char *ban)
{
	struct ChannelGecosBan *tempban = NULL;

	if ((cptr == NULL) || (ban == NULL))
		return NULL;

	tempban = cptr->firstgecosban;
	while ((tempban != NULL) && (irccmp(tempban->mask, ban) != 0))
		tempban = tempban->next;

	return (tempban);
} /* FindGecosBan() */

/*
 * MatchGecosBan()
 * Same as FindBan() but use match() to compare bans to allow
 * for wildcards
 */
struct ChannelGecosBan * MatchGecosBan(const struct Channel *cptr, const
		char *ban)
{
	struct ChannelGecosBan *tempban = NULL;

	tempban = cptr->firstgecosban;
	while ((tempban != NULL) && (match(ban, tempban->mask) == 0))
		tempban = tempban->next;

	return (tempban);
} /* MatchGecosBan() */
#endif /* GECOSBANS */

#endif /* CHANNELSERVICES */
