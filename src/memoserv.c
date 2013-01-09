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
#include "data.h"
#include "config.h"
#include "err.h"
#include "hash.h"
#include "helpserv.h"
#include "match.h"
#include "memoserv.h"
#include "misc.h"
#include "mystring.h"
#include "nickserv.h"
#include "settings.h"
#include "sprintf_irc.h"
#include "timestr.h"

#if defined(NICKSERVICES) && defined(MEMOSERVICES)

/* hash table of memos */
struct MemoInfo *memolist[MEMOLIST_MAX];

static struct MemoInfo *MakeMemoList(void);
static struct Memo *MakeMemo(void);
static void DeleteMemo(struct MemoInfo *mptr, struct Memo *memoptr);
static void AddMemoList(struct MemoInfo *mptr);
static void AddMemo(struct MemoInfo *mptr, struct Memo *memoptr);

static void m_send(struct Luser *, struct NickInfo *, int, char **);
static void m_list(struct Luser *, struct NickInfo *, int, char **);
static void m_read(struct Luser *, struct NickInfo *, int, char **);
static void m_help(struct Luser *, struct NickInfo *, int, char **);
static void m_del(struct Luser *, struct NickInfo *, int, char **);
static void m_undel(struct Luser *, struct NickInfo *, int, char **);
static void m_forward(struct Luser *, struct NickInfo *, int, char **);
static void m_reply(struct Luser *, struct NickInfo *, int, char **);
static void m_purge(struct Luser *, struct NickInfo *, int, char **);

static struct Command memocmds[] =
    {
	    { "SEND", m_send, LVL_IDENT
	    },
	    { "LIST", m_list, LVL_IDENT },
	    { "READ", m_read, LVL_IDENT },
	    { "HELP", m_help, LVL_NONE },
	    { "DEL", m_del, LVL_IDENT },
	    { "UNDEL", m_undel, LVL_IDENT },
	    { "FORWARD", m_forward, LVL_IDENT },
	    { "REPLY", m_reply, LVL_IDENT },
	    { "PURGE", m_purge, LVL_IDENT },
	    { 0, 0, 0 }
    };

/*
ms_process()
  Process command coming from 'nick' directed towards n_MemoServ
*/

void
ms_process(char *nick, char *command)

{
	int acnt;
	char **arv;
	struct Command *mptr;
	struct Luser *lptr;
	struct NickInfo *nptr, *master;

	if (!command || !(lptr = FindClient(nick)))
		return;

	if (Network->flags & NET_OFF)
	{
		notice(n_MemoServ, lptr->nick,
		       "Services are currently \002disabled\002");
		return;
	}

	acnt = SplitBuf(command, &arv);
	if (acnt == 0)
	{
		MyFree(arv);
		return;
	}

	mptr = GetCommand(memocmds, arv[0]);

	if (!mptr || (mptr == (struct Command *) -1))
	{
		notice(n_MemoServ, lptr->nick,
		       "%s command [%s]",
		       (mptr == (struct Command *) -1) ? "Ambiguous" : "Unknown",
		       arv[0]);
		MyFree(arv);
		return;
	}

	/*
	 * Check if the command is for admins only - if so,
	 * check if they match an admin O: line.  If they do,
	 * check if they are EITHER oper'd, or registered with OperServ,
	 * if either of these is true, allow the command
	 */
	if ((mptr->level == LVL_ADMIN) && !(IsValidAdmin(lptr)))
	{
		notice(n_MemoServ, lptr->nick, "Unknown command [%s]",
		       arv[0]);
		MyFree(arv);
		return;
	}

	nptr = FindNick(lptr->nick);
	master = GetMaster(nptr);

	if (!nptr && !master && (mptr->level != LVL_NONE))
	{
		/* the command requires a registered nickname */

		notice(n_MemoServ, lptr->nick,
		       "Your nickname is not registered");
		notice(n_MemoServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_NickServ,
		       "REGISTER");
		MyFree(arv);
		return;
	}

	if (nptr)
	{
		if (master->flags & NS_FORBID)
		{
			notice(n_MemoServ, lptr->nick,
			       "Cannot execute commands for forbidden nicknames");
			MyFree(arv);
			return;
		}

		if (mptr->level != LVL_NONE)
		{
			if (!(nptr->flags & NS_IDENTIFIED))
			{
				notice(n_MemoServ, lptr->nick,
				       "Password identification is required for [\002%s\002]",
				       mptr->cmd);
				notice(n_MemoServ, lptr->nick,
				       "Type \002/MSG %s IDENTIFY <password>\002 and retry",
				       n_NickServ);
				MyFree(arv);
				return;
			}
		}
	} /* if (nptr) */

	/* call mptr->func to execute command */
	(*mptr->func)(lptr, master, acnt, arv);

	MyFree(arv);

	return;
} /* ms_process() */

/*
ms_loaddata()
  Load MemoServ database - return 1 if successful, -1 if not, and -2
if errors are unrecoverable
*/

int
ms_loaddata()

{
	FILE *fp;
	char line[MAXLINE + 1], **av;
	char *keyword;
	int ac, ret = 1, cnt;
	struct MemoInfo *mi = NULL;
	struct NickInfo *nickptr;

	if (!(fp = fopen(MemoServDB, "r")))
	{
		/* MemoServ data file doesn't exist */
		return -1;
	}

	cnt = 0;
	/* load data into list */
	while (fgets(line, sizeof(line), fp))
	{
		cnt++;
		ac = SplitBuf(line, &av);
		if (!ac)
		{
			/* probably a blank line */
			MyFree(av);
			continue;
		}

		if (av[0][0] == ';')
		{
			/* its a comment */
			MyFree(av);
			continue;
		}

		if (!ircncmp("->", av[0], 2))
		{
			/*
			 * check if there are enough args
			 */
			if (ac < 5)
			{
				fatal(1, "%s:%d Invalid database format (FATAL)",
				      MemoServDB,
				      cnt);
				ret = -2;
				MyFree(av);
				continue;
			}

			/* check if there is no nickname before it */
			if (!mi)
			{
				fatal(1, "%s:%d No nickname associated with data",
				      MemoServDB,
				      cnt);
				if (ret > 0)
					ret = -1;
				MyFree(av);
				continue;
			}

			keyword = av[0] + 2;
			if (!ircncmp(keyword, "TEXT", 4))
			{
				struct Memo *memoptr;

				memoptr = MakeMemo();
				memoptr->sender = MyStrdup(av[1]);
				memoptr->sent = atol(av[2]);
				memoptr->flags = atol(av[3]);
				if (!(memoptr->flags & MS_READ))
					mi->newmemos++;
				memoptr->text = MyStrdup(av[4] + 1);
				memoptr->index = ++mi->memocnt;
				AddMemo(mi, memoptr);
			}

		} /* if (!ircncmp("->", keyword, 2)) */
		else
		{
			if (mi)
			{
				if (!mi->memos)
				{
					fatal(1, "%s:%d No memos for entry [%s] (skipping)",
					      MemoServDB,
					      cnt,
					      mi->name);
					MyFree(mi->name);
					MyFree(mi);
					mi = NULL;
					if (ret > 0)
						ret = -1;
				}
				else
					AddMemoList(mi);
			}

			/*
			 * make sure there are enough args on the line:
			 * <nickname>
			 */
			if (ac < 1)
			{
				fatal(1, "%s:%d Invalid database format (FATAL)",
				      MemoServDB,
				      cnt);
				ret = -2;
				mi = NULL;
				MyFree(av);
				continue;
			}

			if (!(nickptr = FindNick(av[0])))
			{
				fatal(1, "%s:%d Memo entry [%s] is not a registered nickname (skipping)",
				      MemoServDB,
				      cnt,
				      av[0]);
				if (ret > 0)
					ret = -1;
				mi = NULL;
				MyFree(av);
				continue;
			}

#ifdef LINKED_NICKNAMES
			if (nickptr->master)
			{
				/*
				 * nickptr is a leaf nickname - they should not have
				 * memo entries
				 */
				fatal(1, "%s:%d Memo entry [%s] is not a master nickname (skipping)",
				      MemoServDB,
				      cnt,
				      av[0]);
				if (ret > 0)
					ret = (-1);
				mi = NULL;
				MyFree(av);
				continue;
			}
#endif /* LINKED_NICKNAMES */

			mi = MakeMemoList();
			mi->name = MyStrdup(av[0]);
		}

		MyFree(av);
	}

	if (mi)
	{
		if (!mi->memos)
		{
			fatal(1, "%s:%d No memos for entry [%s] (skipping)",
			      MemoServDB,
			      cnt,
			      mi->name);
			MyFree(mi->name);
			MyFree(mi);
			if (ret > 0)
				ret = -1;
		}
		else
			AddMemoList(mi);
	}

	fclose (fp);
	return (ret);
} /* ms_loaddata() */

/*
MakeMemoList()
 Allocate a MemoInfo structure and return a pointer to it
*/

static struct MemoInfo *
			MakeMemoList()

{
	struct MemoInfo *mi;

	mi = (struct MemoInfo *) MyMalloc(sizeof(struct MemoInfo));

	memset(mi, 0, sizeof(struct MemoInfo));

	return (mi);
} /* MakeMemoList() */

/*
MakeMemo()
 Allocate a Memo structure and return a pointer to it
*/

static struct Memo *
			MakeMemo()

{
	struct Memo *memoptr;

	memoptr = (struct Memo *) MyMalloc(sizeof(struct Memo));

	memset(memoptr, 0, sizeof(struct Memo));

	return (memoptr);
} /* MakeMemo() */

/*
DeleteMemoList()
  Remove 'mptr' from memolist[]
*/

void
DeleteMemoList(struct MemoInfo *mptr)

{
	struct Memo *next;
	unsigned int hashv;

	if (!mptr)
		return;

	hashv = MSHashMemo(mptr->name);

	/* kill all their memos (if there are any) */
	while (mptr->memos)
	{
		next = mptr->memos->next;
		MyFree(mptr->memos->sender);
		MyFree(mptr->memos->text);
		MyFree(mptr->memos);
		mptr->memos = next;
	}

	if (mptr->next)
		mptr->next->prev = mptr->prev;
	if (mptr->prev)
		mptr->prev->next = mptr->next;
	else
		memolist[hashv] = mptr->next;

	MyFree(mptr->name);
	MyFree(mptr);

	--Network->TotalMemos;
} /* DeleteMemoList() */

/*
DeleteMemo()
 Remove memoptr from the mptr->memos list
*/

static void
DeleteMemo(struct MemoInfo *mptr, struct Memo *memoptr)

{
	assert(mptr && memoptr);

	MyFree(memoptr->sender);
	MyFree(memoptr->text);

	if (memoptr->next)
		memoptr->next->prev = memoptr->prev;
	else
		mptr->lastmemo = memoptr->prev;

	if (memoptr->prev)
		memoptr->prev->next = memoptr->next;
	else
		mptr->memos = memoptr->next;

	MyFree(memoptr);
} /* DeleteMemo() */

/*
PurgeMemos()
  Go through mptr->memos and remove any marked for deletion
*/

int
PurgeMemos(struct MemoInfo *mptr)

{
	int ret;
	struct Memo *memoptr, *next;

	if (!mptr)
		return 0;

	ret = 0;
	for (memoptr = mptr->memos; memoptr; memoptr = next)
	{
		next = memoptr->next;

		if (memoptr->flags & MS_DELETE)
		{
			++ret;
			DeleteMemo(mptr, memoptr);
		}
	}

	mptr->memocnt -= ret;

	if (!mptr->memocnt)
	{
		/*
		 * mptr has no more memos - delete it from the memolist[]
		 * array
		 */
		DeleteMemoList(mptr);
		return (ret);
	}
	else if (ret)
	{
		int cnt;

		/*
		 * its possible that the indices of the memos are now
		 * messed up - if you have 3 memos, and you delete the 2nd one,
		 * you'll have 2 memos, one with an index of 1, and the other
		 * with an index of 3 - check if consecutive indices are
		 * one higher than the previous - if not, fix it
		 */

		cnt = 0;
		for (memoptr = mptr->memos; memoptr; memoptr = memoptr->next)
		{
			if (memoptr->index != (cnt + 1))
				memoptr->index = cnt + 1;
			++cnt;
		}
	}

	return (ret);
} /* PurgeMemos() */

/*
ExpireMemos()
 Delete any memos that have expired
*/

void
ExpireMemos(time_t unixtime)

{
	int ii;
	struct MemoInfo *mptr, *mtemp;
	struct Memo *memoptr;

	if (!MemoExpire)
		return;

	for (ii = 0; ii < MEMOLIST_MAX; ii++)
	{
		for (mptr = memolist[ii]; mptr; )
		{
			for (memoptr = mptr->memos; memoptr; memoptr = memoptr->next)
				if ((unixtime - memoptr->sent) >= MemoExpire)
					memoptr->flags |= MS_DELETE;

			mtemp = mptr->next;
			PurgeMemos(mptr);
			mptr = mtemp;
		}
	}
} /* ExpireMemos() */

/*
PurgeCheck()
 Go through all memos and delete any that have been
marked for deletion - should be called every "MemoPurgeFreq"
seconds from DoTimer()
*/

void
PurgeCheck()

{
	int ii;
	struct MemoInfo *mi, *next;

	for (ii = 0; ii < MEMOLIST_MAX; ii++)
	{
		for (mi = memolist[ii]; mi; )
		{
			/*
			 * Record the next ptr, in case mi has every memo
			 * marked for deletion, in which case the struct itself
			 * will be removed from the list
			 */
			next = mi->next;
			PurgeMemos(mi);
			mi = next;
		}
	}
} /* PurgeCheck() */

/*
AddMemoList()
  Insert 'mptr' into memolist[]
*/

static void
AddMemoList(struct MemoInfo *mptr)

{
	unsigned int hashv;

	hashv = MSHashMemo(mptr->name);

	mptr->next = memolist[hashv];
	mptr->prev = NULL;
	if (mptr->next)
		mptr->next->prev = mptr;
	memolist[hashv] = mptr;

	Network->TotalMemos++;
} /* AddMemoList() */

/*
AddMemo()
  Insert 'memoptr' into mptr->memos
*/

static void
AddMemo(struct MemoInfo *mptr, struct Memo *memoptr)

{
	/*
	 * Add memoptr onto the end of mptr->memos because we want
	 * the most recent memos last for the LIST command
	 */
	memoptr->next = NULL;

	if (!mptr->memos)
	{
		mptr->memos = mptr->lastmemo = memoptr;
		mptr->prev = NULL;
	}
	else
	{
		mptr->lastmemo->next = memoptr;
		memoptr->prev = mptr->lastmemo;
		mptr->lastmemo = memoptr;
	}
} /* AddMemo() */

/*
FindMemoList
  Return a pointer to structure containing memolist for 'name'
*/

struct MemoInfo *
			FindMemoList(char *name)

{
	struct MemoInfo *mi;
	unsigned int hashv;

	if (!name)
		return (NULL);

	hashv = MSHashMemo(name);
	for (mi = memolist[hashv]; mi; mi = mi->next)
	{
		if (!irccmp(mi->name, name))
			return (mi);
	}

	return (NULL);
} /* FindMemoList() */

/*
StoreMemo()
  Record 'text' as a memo for 'target' - assume target is a valid
nickname or channel;
 
Return: 0 if target has reached maximum memo capacity
        otherwise, returns index of new memo
*/

int
StoreMemo(char *target, char *text, struct Luser *lptr)

{
	struct MemoInfo *mi;
	struct Memo *memoptr;

	assert(target && text && lptr);

	if (!(mi = FindMemoList(target)))
	{
		mi = MakeMemoList();
		mi->name = MyStrdup(target);
		AddMemoList(mi);
	}

	if (MaxMemos && (mi->memocnt >= MaxMemos))
	{
		notice(n_MemoServ, lptr->nick,
		       "%s has reached the maximum memo limit, and cannot receive more",
		       target);
		return (0);
	}

	++mi->memocnt;
	++mi->newmemos;

	memoptr = MakeMemo();
	memoptr->sender = MyStrdup(lptr->nick);
	memoptr->sent = current_ts;
	memoptr->index = mi->memocnt;
	memoptr->text = MyStrdup(text);
	AddMemo(mi, memoptr);

	return (memoptr->index);
} /* StoreMemo() */

/*
MoveMemos()
  Copy all of source's memos to dest's memo list, and delete
source's memos when done.
*/

void
MoveMemos(struct NickInfo *source, struct NickInfo *dest)

{
	struct MemoInfo *srcmptr, /* pointer to source's memos */
				*dstmptr; /* pointer to dest's memos */
	struct Memo *memoptr, *mtmp;

	if (!source || !dest)
		return;

	if (!(srcmptr = FindMemoList(source->nick)))
		return; /* source has no memos to copy */

	if (!(dstmptr = FindMemoList(dest->nick)))
{
		/*
		 * Create a memo structure for dest
		 */
		dstmptr = MakeMemoList();
		dstmptr->name = MyStrdup(dest->nick);
		AddMemoList(dstmptr);
	}

	/*
	 * Go through source's memo list and create duplicate
	 * structures in dest's list
	 */
	for (memoptr = srcmptr->memos; memoptr; memoptr = memoptr->next)
	{
		mtmp = MakeMemo();
		mtmp->sender = MyStrdup(memoptr->sender);
		mtmp->sent = memoptr->sent;
		mtmp->index = ++dstmptr->memocnt;
		mtmp->flags = memoptr->flags;
		mtmp->text = MyStrdup(memoptr->text);

		/*
		 * Now add mtmp, which is a duplicate of memoptr, to dest's
		 * memo list
		 */
		AddMemo(dstmptr, mtmp);

		if (!(mtmp->flags & MS_READ))
			dstmptr->newmemos++;

		/*
		 * Mark all of source's memos for deletion
		 */
		memoptr->flags |= MS_DELETE;
	}

	/*
	 * Purge source's memos
	 */
	PurgeMemos(srcmptr);
} /* MoveMemos() */

/*
m_send()
  Send memo av[2]- to av[1]
*/

static void
m_send(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	char *memotext, /* memo text */
	*to; /* who the memo is sent to */
	int  index;
	struct NickInfo *master = NULL,
				                          *realptr = NULL;
#ifdef CHANNELSERVICES

	struct ChanInfo *ci = NULL;
#endif

	if (ac < 3)
{
		notice(n_MemoServ, lptr->nick,
		       "Syntax: SEND <nick/channel> <text>");
		notice(n_MemoServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_MemoServ,
		       "SEND");
		return;
	}

	if (*av[1] == '#')
	{
#ifndef CHANNELSERVICES

		notice(n_MemoServ, lptr->nick,
		       "Channel services are disabled");
		return;

#else

		if (!(ci = FindChan(av[1])))
		{
			notice(n_MemoServ, lptr->nick,
			       ERR_CH_NOT_REGGED,
			       av[1]);
			return;
		}

		if (!HasAccess(ci, lptr, CA_AUTOOP))
		{
			notice(n_MemoServ, lptr->nick,
			       "AutoOp access is required to send a memo to [\002%s\002]",
			       ci->name);
			return;
		}

		to = ci->name;

#endif /* CHANNELSERVICES */

	}
	else
	{
		/* it was sent to a nickname */

		if (!(realptr = FindNick(av[1])))
		{
			notice(n_MemoServ, lptr->nick,
			       ERR_NOT_REGGED,
			       av[1]);
			return;
		}

		master = GetMaster(realptr);
		assert(master != 0);

		if (!(master->flags & NS_MEMOS))
		{
			notice(n_MemoServ, lptr->nick,
			       "[\002%s\002] is rejecting all memos",
			       av[1]);
			return;
		}

		to = master->nick;
	}

	/* get the actual memo text now */
	memotext = GetString(ac - 2, av + 2);

	index = StoreMemo(to, memotext, lptr);

	if (index)
	{
		notice(n_MemoServ, lptr->nick,
		       "Memo has been recorded for [\002%s\002]",
		       realptr ? realptr->nick : to);

		if (realptr && master)
		{
			/*
			 * It was sent to a nickname - check if they are online
			 * and identified and optionally notify them
			 *
			 * we should have here linklist traversal and notifying all clients.
			 * however, that could be a bit cpu intensitive to do for every
			 * memo, so i haven't done it so far. -kre
			 */
			if ((master->flags & NS_MEMONOTIFY) &&
			        (realptr->flags & NS_IDENTIFIED) &&
			        FindClient(realptr->nick))
			{
				notice(n_MemoServ, realptr->nick,
				       "You have a new memo from \002%s\002 (#%d)",
				       lptr->nick,
				       index);
				notice(n_MemoServ, realptr->nick,
				       "Type \002/MSG %s READ %d\002 to read it",
				       n_MemoServ,
				       index);
			}
		} /* if (ni) */
#ifdef CHANNELSERVICES
		else
		{
			struct Channel *chptr;
			struct ChanAccess *ca;
			char *newtext;

			/*
			 * It was sent to a channel - notify every AOP or higher
			 */
			if ((ci) && (chptr = FindChannel(to)))
			{
				struct ChannelUser *cu;
				struct NickInfo *tmpn;

				for (cu = chptr->firstuser; cu; cu = cu->next)
				{
					if (FindService(cu->lptr))
						continue;

					tmpn = GetLink(cu->lptr->nick);

					if (HasAccess(ci, cu->lptr, CA_AUTOOP))
					{
						if (tmpn)
						{
							/* don't notify people who don't want memos */
							if (!(tmpn->flags & NS_MEMOS) ||
							        !(tmpn->flags & NS_MEMONOTIFY))
								continue;
						}

						notice(n_MemoServ, cu->lptr->nick,
						       "New channel memo from \002%s\002 (#%d)",
						       lptr->nick,
						       index);
						notice(n_MemoServ, cu->lptr->nick,
						       "Type \002/MSG %s READ %s %d\002 to read it",
						       n_MemoServ,
						       chptr->name,
						       index);
					}
				}
			}

			newtext = (char *) MyMalloc(strlen(memotext) + strlen(ci->name) + 4);
			ircsprintf(newtext, "(%s) %s", ci->name, memotext);
			for (ca = ci->access; ca; ca = ca->next)
			{
				if (ca->nptr)
					StoreMemo(ca->nptr->nick, newtext, lptr);
			}

			MyFree(newtext);

		} /* else */
#endif /* CHANNELSERVICES */

	} /* if (index) */

	MyFree (memotext);
} /* m_send() */

/*
m_list()
  List the memos for lptr->nick, or channel av[1] if specified
*/

static void
m_list(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct MemoInfo *mi = NULL;
	struct Memo *memoptr;
	char status[10];

	if (!nptr)
		return;

	if (ac >= 2)
	{
		if (av[1][0] == '#')
		{
#ifdef CHANNELSERVICES
			struct ChanInfo *ci;
#endif

#ifndef CHANNELSERVICES

			notice(n_MemoServ, lptr->nick,
			       "Channel services are disabled");
			return;
#else

			if (!(ci = FindChan(av[1])))
			{
				notice(n_MemoServ, lptr->nick,
				       ERR_CH_NOT_REGGED,
				       av[1]);
				return;
			}

			if (!HasAccess(ci, lptr, CA_AUTOOP))
			{
				notice(n_MemoServ, lptr->nick,
				       "AutoOp access is required to list memos for [\002%s\002]",
				       ci->name);
				return;
			}

			mi = FindMemoList(ci->name);
			if (!mi)
			{
				notice(n_MemoServ, lptr->nick,
				       "There are no recorded memos for [\002%s\002]",
				       ci->name);
				return;
			}
#endif /* CHANNELSERVICES */

		}
	}

	if (!mi)
	{
		if (!(mi = FindMemoList(nptr->nick)))
		{
			notice(n_MemoServ, lptr->nick,
			       "You have no recorded memos");
			return;
		}
	}

	notice(n_MemoServ, lptr->nick,
	       "-- Listing memos for [\002%s\002] --",
	       mi->name);
	notice(n_MemoServ, lptr->nick,
	       "        Idx Sender             Time Sent");
	for (memoptr = mi->memos; memoptr; memoptr = memoptr->next)
	{
		if (memoptr->flags & MS_DELETE)
			strlcpy(status, "(D)", sizeof(status));
		else if (mi->name[0] != '#')
		{
			if (memoptr->flags & MS_READ)
				strlcpy(status, "(R)", sizeof(status));
			else
				strlcpy(status, "(N)", sizeof(status));
			if (memoptr->flags & MS_REPLIED)
				strlcat(status, "(RE)", sizeof(status));
		}
		else
			status[0] = '\0';

		notice(n_MemoServ, lptr->nick,
		       "%7s %-3d %-18s %s ago",
		       status,
		       memoptr->index,
		       memoptr->sender,
		       timeago(memoptr->sent, 1));
	}
	notice(n_MemoServ, lptr->nick,
	       "-- End of list --");
} /* m_list() */

/*
m_read()
  Read a memo index av[1] or av[2] if av[1] is a channel
*/

static void
m_read(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct MemoInfo *mi;
	int index;
	struct Memo *memoptr;
	char istr[10];

	if (!nptr)
		return;

	if (ac < 2)
	{
		notice(n_MemoServ, lptr->nick,
		       "Syntax: READ [channel] <index|all>");
		notice(n_MemoServ, lptr->nick, ERR_MORE_INFO, n_MemoServ, "READ");
		return;
	}

	if (ac >= 3)
	{
#ifdef CHANNELSERVICES
		struct ChanInfo *ci;
#endif

#ifndef CHANNELSERVICES

		notice(n_MemoServ, lptr->nick,
		       "Channel services are disabled");
		return;
#else

		if (!(ci = FindChan(av[1])))
		{
			notice(n_MemoServ, lptr->nick, ERR_CH_NOT_REGGED, av[1]);
			return;
		}

		if (!HasAccess(ci, lptr, CA_AUTOOP))
		{
			notice(n_MemoServ, lptr->nick,
			       "AutoOp access is required to read memos for [\002%s\002]",
			       ci->name);
			return;
		}

		if (!(mi = FindMemoList(ci->name)))
		{
			notice(n_MemoServ, lptr->nick,
			       "There are no recorded memos for [\002%s\002]",
			       ci->name);
			return;
		}
		index = IsNum(av[2]);

#endif /* CHANNELSERVICES */

	}
	else
	{
		if (!(mi = FindMemoList(nptr->nick)))
		{
			notice(n_MemoServ, lptr->nick,
			       "You have no recorded memos");
			return;
		}
		index = IsNum(av[1]);
	}

	if ((index > mi->memocnt) ||
	        (!index && (irccmp(av[(ac >= 3) ? 2 : 1], "ALL") != 0)))
	{
		notice(n_MemoServ, lptr->nick,
		       "[\002%s\002] is an invalid index",
		       av[(ac >= 3) ? 2 : 1]);
		return;
	}

	for (memoptr = mi->memos; memoptr; memoptr = memoptr->next)
	{
		if (!index || (memoptr->index == index))
		{
			notice(n_MemoServ, lptr->nick,
			       "Memo #%d from %s (sent %s ago) [%s%s%s]:",
			       memoptr->index,
			       memoptr->sender,
			       timeago(memoptr->sent, 1),
			       (memoptr->flags & MS_READ) ? "R" : "N",
			       (memoptr->flags & MS_REPLIED) ? "/RE" : "",
			       (memoptr->flags & MS_DELETE)? "/D" : "");
			notice(n_MemoServ, lptr->nick, "%s", memoptr->text);
			if (ac < 3)
			{
				/* only mark nickname memos as read - not channel memos */
				if (!(memoptr->flags & MS_READ))
					mi->newmemos--;
				memoptr->flags |= MS_READ;
			}
		}
	}

	if (index)
		ircsprintf(istr, "%d", index);
	else
		strlcpy(istr, "ALL", sizeof(istr));

	if (ac >= 3)
		notice(n_MemoServ, lptr->nick,
		       "To delete, type \002/MSG %s DEL %s %s",
		       n_MemoServ, av[1], istr);
	else
		notice(n_MemoServ, lptr->nick,
		       "To delete, type \002/MSG %s DEL %s",
		       n_MemoServ, istr);

} /* m_read() */

/*
m_help()
  Give help to lptr->nick
*/

static void
m_help(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	if (ac >= 2)
	{
		char str[MAXLINE + 1];
		struct Command *cptr;

		for (cptr = memocmds; cptr->cmd; cptr++)
			if (!irccmp(av[1], cptr->cmd))
				break;

		if (cptr->cmd)
			if ((cptr->level == LVL_ADMIN) &&
			        !(IsValidAdmin(lptr)))
			{
				notice(n_MemoServ, lptr->nick,
				       "No help available on \002%s\002",
				       av[1]);
				return;
			}

		ircsprintf(str, "%s", av[1]);

		GiveHelp(n_MemoServ, lptr->nick, str, NODCC);
	}
	else
		GiveHelp(n_MemoServ, lptr->nick, NULL, NODCC);
} /* m_help() */

/*
m_del()
  Mark memo index av[1] for deletion - DoTimer() does the actual
deleting every hour on the hour
*/

static void
m_del(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct MemoInfo *mi;
	struct Memo *memoptr;
	char istr[MAXLINE + 1];
	int index;

	if (!nptr)
		return;

	if (ac < 2)
	{
		notice(n_MemoServ, lptr->nick,
		       "Syntax: DEL [channel] <index|all>");
		notice(n_MemoServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_MemoServ,
		       "DEL");
		return;
	}

	if (ac >= 3)
	{
#ifdef CHANNELSERVICES
		struct ChanInfo *ci;
#endif

#ifndef CHANNELSERVICES

		notice(n_MemoServ, lptr->nick,
		       "Channel services are disabled");
		return;
#else

		if (!(ci = FindChan(av[1])))
		{
			notice(n_MemoServ, lptr->nick,
			       ERR_CH_NOT_REGGED,
			       av[1]);
			return;
		}

		if (!HasAccess(ci, lptr, CA_SUPEROP))
		{
			notice(n_MemoServ, lptr->nick,
			       "SuperOp access is required to delete memos for [\002%s\002]",
			       ci->name);
			return;
		}

		if (!(mi = FindMemoList(ci->name)))
		{
			notice(n_MemoServ, lptr->nick,
			       "There are no recorded memos for [\002%s\002]",
			       ci->name);
			return;
		}
		index = IsNum(av[2]);

#endif /* CHANNELSERVICES */

	}
	else
	{
		if (!(mi = FindMemoList(nptr->nick)))
		{
			notice(n_MemoServ, lptr->nick,
			       "You have no recorded memos");
			return;
		}
		index = IsNum(av[1]);
	}

	if ((index > mi->memocnt) ||
	        (!index && (irccmp(av[(ac >= 3) ? 2 : 1], "ALL") != 0)))
	{
		notice(n_MemoServ, lptr->nick,
		       "[\002%s\002] is an invalid index",
		       av[(ac >= 3) ? 2 : 1]);
		return;
	}

	for (memoptr = mi->memos; memoptr; memoptr = memoptr->next)
	{
		if (!index || (memoptr->index == index))
			memoptr->flags |= MS_DELETE;
	}

	if (index)
		ircsprintf(istr, "Memo #%d has", index);
	else
		strlcpy(istr, "All memos have", sizeof(istr));

	if (ac >= 3)
		notice(n_MemoServ, lptr->nick,
		       "%s been marked for deletion for [\002%s\002]",
		       istr,
		       av[1]);
	else
		notice(n_MemoServ, lptr->nick,
		       "%s been marked for deletion",
		       istr);
} /* m_del() */

/*
m_undel()
  Undelete index av[1] or av[2] if av[1] is a channel
*/

static void
m_undel(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct MemoInfo *mi;
	struct Memo *memoptr;
	char istr[MAXLINE + 1];
	int index;

	if (!nptr)
		return;

	if (ac < 2)
	{
		notice(n_MemoServ, lptr->nick,
		       "Syntax: UNDEL [channel] <index|all>");
		notice(n_MemoServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_MemoServ,
		       "UNDEL");
		return;
	}

	if (ac >= 3)
	{
#ifdef CHANNELSERVICES
		struct ChanInfo *ci;
#endif

#ifndef CHANNELSERVICES

		notice(n_MemoServ, lptr->nick,
		       "Channel services are disabled");
		return;
#else

		if (!(ci = FindChan(av[1])))
		{
			notice(n_MemoServ, lptr->nick,
			       ERR_CH_NOT_REGGED,
			       av[1]);
			return;
		}

		if (!HasAccess(ci, lptr, CA_SUPEROP))
		{
			notice(n_MemoServ, lptr->nick,
			       "SuperOp access is required to undelete memos for [\002%s\002]",
			       ci->name);
			return;
		}

		if (!(mi = FindMemoList(ci->name)))
		{
			notice(n_MemoServ, lptr->nick,
			       "There are no recorded memos for [\002%s\002]",
			       ci->name);
			return;
		}
		index = IsNum(av[2]);
#endif /* CHANNELSERVICES */

	}
	else
	{
		if (!(mi = FindMemoList(nptr->nick)))
		{
			notice(n_MemoServ, lptr->nick,
			       "You have no recorded memos");
			return;
		}
		index = IsNum(av[1]);
	}

	if ((index > mi->memocnt) ||
	        (!index && (irccmp(av[(ac >= 3) ? 2 : 1], "ALL") != 0)))
	{
		notice(n_MemoServ, lptr->nick,
		       "[\002%s\002] is an invalid index",
		       av[(ac >= 3) ? 2 : 1]);
		return;
	}

	for (memoptr = mi->memos; memoptr; memoptr = memoptr->next)
	{
		if (!index || (memoptr->index == index))
			memoptr->flags &= ~MS_DELETE;
	}

	if (index)
		ircsprintf(istr, "Memo #%d has", index);
	else
		strlcpy(istr, "All memos have", sizeof(istr));

	if (ac >= 3)
		notice(n_MemoServ, lptr->nick,
		       "%s been unmarked for deletion for [\002%s\002]",
		       istr,
		       av[1]);
	else
		notice(n_MemoServ, lptr->nick,
		       "%s been unmarked for deletion",
		       istr);
} /* m_undel() */

/*
m_forward()
  Forward a memo to a nick/channel
*/

static void
m_forward(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct MemoInfo *from, *target;
	struct Memo *memoptr = NULL;
	struct Memo *fromptr;
	char *to; /* who the memo is sent to */
	int index, cnt;
	char buf[MAXLINE + 1];
	struct NickInfo *master,
				*realptr;
#ifdef CHANNELSERVICES

	struct ChanInfo *ci = NULL;
#endif

	if (!nptr)
		return;

	if (ac < 3)
{
		notice(n_MemoServ, lptr->nick,
		       "Syntax: FORWARD <index|ALL> <nick/channel>");
		notice(n_MemoServ, lptr->nick,
		       ERR_MORE_INFO,
		       n_MemoServ,
		       "FORWARD");
		return;
	}

	if (!(from = FindMemoList(nptr->nick)))
	{
		notice(n_MemoServ, lptr->nick,
		       "You have no recorded memos");
		return;
	}

	index = IsNum(av[1]);
	if ((index > from->memocnt) ||
	        (!index && (irccmp(av[1], "ALL") != 0)))
	{
		notice(n_MemoServ, lptr->nick,
		       "[\002%s\002] is an invalid index",
		       av[1]);
		return;
	}

	master = realptr = NULL;

	if (*av[2] == '#')
	{
#ifndef CHANNELSERVICES
		notice(n_MemoServ, lptr->nick,
		       "Channel services are disabled");
		return;
#else

		if (!(ci = FindChan(av[2])))
		{
			notice(n_MemoServ, lptr->nick,
			       ERR_CH_NOT_REGGED,
			       av[2]);
			return;
		}

		if (!HasAccess(ci, lptr, CA_AUTOOP))
		{
			notice(n_MemoServ, lptr->nick,
			       "AutoOp access is required to forward a memo to [\002%s\002]",
			       ci->name);
			return;
		}

		to = ci->name;

#endif

	}
	else
	{
		/* it was sent to a nickname */

		if (!(realptr = FindNick(av[2])))
		{
			notice(n_MemoServ, lptr->nick,
			       ERR_NOT_REGGED,
			       av[2]);
			return;
		}

		master = GetMaster(realptr);
		assert(master != 0);

		if (!(master->flags & NS_MEMOS))
		{
			notice(n_MemoServ, lptr->nick,
			       "[\002%s\002] is rejecting all memos",
			       realptr->nick);
			return;
		}

		to = master->nick;
	}

	if (!(target = FindMemoList(to)))
	{
		target = MakeMemoList();
		target->name = MyStrdup(to);
		AddMemoList(target);
	}
	else if (from == target)
	{
		/*
		 * If we let someone forward memos to themselves,
		 * the below loop will never end, eventually malloc()'ing too
		 * much and crashing - head it off at the pass
		 */
		notice(n_MemoServ, lptr->nick,
		       "You cannot forward memos to yourself");
		return;
	}

	if (MaxMemos && (target->memocnt >= MaxMemos))
	{
		notice(n_MemoServ, lptr->nick,
		       "%s has reached the maximum memo limit, and cannot receive more",
		       to);
		return;
	}

	cnt = 0;
	for (fromptr = from->memos; fromptr; fromptr = fromptr->next)
	{
		if (!index || (fromptr->index == index))
		{
			buf[0] = '\0';

			target->memocnt++;
			target->newmemos++;
			cnt++;

			memoptr = MakeMemo();
			memoptr->sender = MyStrdup(lptr->nick);
			memoptr->sent = current_ts;
			memoptr->index = target->memocnt;

			strlcpy(buf, "[Fwd]: ", sizeof(buf));
			strlcat(buf, fromptr->text, MAXLINE - 8);
			memoptr->text = MyStrdup(buf);

			AddMemo(target, memoptr);

			if (MaxMemos && (target->memocnt >= MaxMemos))
				break;
		}
	}

	if (!index)
		ircsprintf(buf, "All memos have");
	else
		ircsprintf(buf, "Memo #%d has", index);

	notice(n_MemoServ, lptr->nick,
	       "%s been forwarded to [\002%s\002]",
	       buf,
	       target->name);

	if ((master != NULL) && (realptr != NULL) && (memoptr != NULL))
	{
		/*
		 * It was sent to a nickname - check if they are online
		 * and notify them
		 */
		if ((master->flags & NS_MEMONOTIFY) &&
		        (realptr->flags & NS_IDENTIFIED) &&
		        FindClient(realptr->nick))
		{
			notice(n_MemoServ, realptr->nick,
			       "You have %d new forwarded memo%s from \002%s\002",
			       cnt,
			       (cnt == 1) ? "" : "s",
			       memoptr->sender);
			notice(n_MemoServ, realptr->nick,
			       "Type \002/MSG %s LIST\002 to view %s",
			       n_MemoServ,
			       (cnt == 1) ? "it" : "them");
		}
	} /* if (master && realptr) */
#ifdef CHANNELSERVICES
	else
	{
		struct Channel *chptr;

		/*
		 * It was sent to a channel - notify every AOP or higher
		 */
		if ((ci) && (chptr = FindChannel(target->name)))
		{
			struct ChannelUser *cu;
			struct NickInfo *tmpn;

			for (cu = chptr->firstuser; cu; cu = cu->next)
			{
				if (FindService(cu->lptr))
					continue;

				tmpn = GetLink(cu->lptr->nick);

				if (HasAccess(ci, cu->lptr, CA_AUTOOP))
				{
					if (tmpn)
					{
						if (!(tmpn->flags & NS_MEMOS) ||
						        !(tmpn->flags & NS_MEMONOTIFY))
							continue;
					}

					if (memoptr == NULL)
						continue;

					notice(n_MemoServ, cu->lptr->nick,
					       "%d new forwarded channel memo%s from \002%s\002",
					       cnt,
					       (cnt == 1) ? "" : "s",
					       memoptr->sender);

					notice(n_MemoServ, cu->lptr->nick,
					       "Type \002/MSG %s LIST %s\002 to view %s",
					       n_MemoServ,
					       chptr->name,
					       (cnt == 1) ? "it" : "them");
				}
			}
		}
	} /* else */
#endif /* CHANNELSERVICES */
} /* m_forward() */

/*
m_reply()
  Reply to memo number av[1] with text av[2]-
*/

static void
m_reply(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct MemoInfo *from;
	struct Memo *memoptr;
	struct NickInfo *master,
				*realptr;
	char memotext[MAXLINE + 1];
	int index, ii;

	if (!nptr)
		return;

	if (ac < 3)
{
		notice(n_MemoServ, lptr->nick, "Syntax: REPLY <index> <text>");
		notice(n_MemoServ, lptr->nick, ERR_MORE_INFO, n_MemoServ, "REPLY");
		return;
	}

	if (!(from = FindMemoList(nptr->nick)))
	{
		notice(n_MemoServ, lptr->nick, "You have no recorded memos");
		return;
	}

	index = IsNum(av[1]);
	if (!index || (index > from->memocnt))
	{
		notice(n_MemoServ, lptr->nick, "[\002%s\002] is an invalid index",
		       av[1]);
		return;
	}

	master = realptr = NULL;
	for (memoptr = from->memos; memoptr; memoptr = memoptr->next)
	{
		if (memoptr->index == index)
		{
			if (!(realptr = FindNick(memoptr->sender)))
			{
				notice(n_MemoServ, lptr->nick, ERR_NOT_REGGED, av[2]);
				return;
			}

			master = GetMaster(realptr);
			assert(master != 0);

			if (!(master->flags & NS_MEMOS))
			{
				notice(n_MemoServ, lptr->nick,
				       "[\002%s\002] is rejecting all memos",
				       master->nick);
				return;
			}

			break;
		}
	}

	if (!master || !realptr)
		return;

	ircsprintf(memotext, "[Re]: %s", av[2]);
	ii = 3;
	while (ii < ac)
	{
		strlcat(memotext, " ", sizeof(memotext));
		strlcat(memotext, av[ii], sizeof(memotext));
		ii++;
	}

	index = StoreMemo(master->nick, memotext, lptr);

	if (index)
	{
		notice(n_MemoServ, lptr->nick,
		       "Memo has been recorded for [\002%s\002]",
		       master->nick);
		memoptr->flags |= MS_REPLIED;

		/*
		 * It was sent to a nickname - check if they are online
		 * and notify them
		 */
		if ((master->flags & NS_MEMONOTIFY) &&
		        (realptr->flags & NS_IDENTIFIED) &&
		        FindClient(realptr->nick))
		{
			notice(n_MemoServ, realptr->nick,
			       "You have a new memo from \002%s\002 (#%d)",
			       lptr->nick, index);
			notice(n_MemoServ, realptr->nick,
			       "Type \002/MSG %s READ %d\002 to read it",
			       n_MemoServ, index);
		}
	} /* if (index) */
} /* m_reply() */

/*
m_purge()
  Delete all memos that were marked for deletion via the DEL command
*/

static void
m_purge(struct Luser *lptr, struct NickInfo *nptr, int ac, char **av)

{
	struct MemoInfo *mi;
	int cnt;

	if (!nptr)
		return;

	if (ac >= 2)
	{
#ifdef CHANNELSERVICES
		struct ChanInfo *ci;
#endif

#ifndef CHANNELSERVICES

		notice(n_MemoServ, lptr->nick,
		       "Channel services are disabled");
		return;

#else

		if (!(ci = FindChan(av[1])))
		{
			notice(n_MemoServ, lptr->nick,
			       ERR_CH_NOT_REGGED,
			       av[1]);
			return;
		}

		if (!HasAccess(ci, lptr, CA_SUPEROP))
		{
			notice(n_MemoServ, lptr->nick,
			       "SuperOp access is required to purge memos for [\002%s\002]",
			       ci->name);
			return;
		}

		if (!(mi = FindMemoList(ci->name)))
		{
			notice(n_MemoServ, lptr->nick,
			       "There are no recorded memos for [\002%s\002]",
			       ci->name);
			return;
		}

		cnt = PurgeMemos(mi);

		notice(n_MemoServ, lptr->nick,
		       "Memos marked for deletion on [\002%s\002] have been purged (%d found)",
		       ci->name,
		       cnt);

		return;

#endif /* CHANNELSERVICES */

	}
	else if (!(mi = FindMemoList(nptr->nick)))
	{
		notice(n_MemoServ, lptr->nick,
		       "You have no recorded memos");
		return;
	}

	cnt = PurgeMemos(mi);

	notice(n_MemoServ, lptr->nick,
	       "Your memos marked for deletion have been purged (%d found)",
	       cnt);
} /* m_purge() */

#endif /* defined(NICKSERVICES) && defined(MEMOSERVICES) */
