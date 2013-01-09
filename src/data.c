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
#include "data.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "memoserv.h"
#include "nickserv.h"
#include "settings.h"
#include "seenserv.h"
#include "misc.h"
#include "sprintf_irc.h"
#include "mystring.h"

static int CopyFile(char *oldfile, char *newfile);

/*
ReloadData()
  Reload nick/chan/memo/stat/oper databases
*/

int
ReloadData()

{
#ifdef NICKSERVICES
	struct NickInfo *temp, *newnick, *nnext;
	int ii;

#ifdef CHANNELSERVICES

	struct ChanInfo *ctemp, *cnext;
#endif

#ifdef MEMOSERVICES

	struct MemoInfo *mtemp, *mnext;
#endif

#endif /* NICKSERVICES */

	/* reopen logs */
	CloseLogFile();
	OpenLogFile();

#ifdef NICKSERVICES

	for (ii = 0; ii < NICKLIST_MAX; ++ii)
		for (temp = nicklist[ii]; temp; temp = temp->next)
			temp->flags |= NS_DELETE;

	if (ns_loaddata() == (-2))
	{
		/*
		 * Reload had fatal errors, so use the old database -
		 * go through and KEEP all structures marked for deletion
		 * and DELETE structures not marked, because they are the
		 * ones that were just added in the failed reload
		 */

		for (ii = 0; ii < NICKLIST_MAX; ++ii)
		{
			for (temp = nicklist[ii]; temp; temp = nnext)
			{
				nnext = temp->next;

				if (!(temp->flags & NS_DELETE))
					DeleteNick(temp);
				else
				{
					/* remove the deletion flag */
					temp->flags &= ~NS_DELETE;
				}
			}
		}

		return 0;
	}
	else
	{
		/*
		 * Reload was ok - now go through and remove the old nick
		 * structures marked for deletion
		 */

		for (ii = 0; ii < NICKLIST_MAX; ++ii)
		{
			for (temp = nicklist[ii]; temp; temp = nnext)
			{
				nnext = temp->next;

				if (temp->flags & NS_DELETE)
				{
					/*
					 * We should try to preserve the old identification
					 * data. Before deleting this nickname, attempt to find
					 * the corresponding one that was just read from the
					 * configuration file, and set it's
					 * ident/collide/split_ts variables correctly. It is
					 * safe to use FindNick() because it will return the
					 * most current entry. If the entry was deleted,
					 * FindNick() will return 'temp' so we're still not
					 * losing anything.
					 */
					if ((newnick = FindNick(temp->nick)))
					{
						newnick->flags |= (temp->flags & NS_IDENTIFIED);
						newnick->collide_ts = temp->collide_ts;
					}

					DeleteNick(temp);

				} /* if (temp->flags & NS_DELETE) */
			} /* for (temp = nicklist[ii]; temp; temp = nnext) */
		} /* for (ii = 0; ii < NICKLIST_MAX; ++ii) */
	}

#ifdef CHANNELSERVICES

	for (ii = 0; ii < CHANLIST_MAX; ++ii)
		for (ctemp = chanlist[ii]; ctemp; ctemp = ctemp->next)
			ctemp->flags |= CS_DELETE;

	if (cs_loaddata() == (-2))
	{
		/*
		 * Reload had fatal errors, so use the old database -
		 * go through and KEEP all structures marked for deletion
		 * and DELETE structures not marked, because they are the
		 * ones that were just added in the failed reload
		 */

		for (ii = 0; ii < CHANLIST_MAX; ++ii)
		{
			for (ctemp = chanlist[ii]; ctemp; ctemp = cnext)
			{
				cnext = ctemp->next;

				if (!(ctemp->flags & CS_DELETE))
					DeleteChan(ctemp);
				else
				{
					/* remove the deletion flag */
					ctemp->flags &= ~CS_DELETE;
				}
			}
		}

		return 0;
	}
	else
	{
		struct Channel *cptr;

		/*
		 * Reload was ok - now go through and remove the old chan
		 * structures marked for deletion
		 */

		for (ii = 0; ii < CHANLIST_MAX; ++ii)
		{
			for (ctemp = chanlist[ii]; ctemp; ctemp = cnext)
			{
				cnext = ctemp->next;

				if (ctemp->flags & CS_DELETE)
					DeleteChan(ctemp);
				else
				{
					/*
					 * It must be a new channel entry, have ChanServ
					 * join the channel
					 */
					if (!(ctemp->flags & CS_FORGET))
					{
						cptr = FindChannel(ctemp->name);
						if (cptr && !IsChannelMember(cptr, Me.csptr))
							cs_join(ctemp);
					}
				}
			}
		}

		/*
		 * Now go through the list and have ChanServ part any old chans
		 */
		if (Me.csptr)
		{
			struct UserChannel *uc, *ucprev;

			ucprev = NULL;
			for (uc = Me.csptr->firstchan; uc; )
			{
				if (!FindChan(uc->chptr->name))
				{
					if (ucprev)
					{
						ucprev->next = uc->next;
						cs_part(uc->chptr);
						uc = ucprev;
					}
					else
					{
						Me.csptr->firstchan = uc->next;
						cs_part(uc->chptr);
						uc = NULL;
					}
				}

				ucprev = uc;

				if (uc)
					uc = uc->next;
				else
					uc = Me.csptr->firstchan;
			}
		}
	} /* if (Me.csptr) */

#endif /* CHANNELSERVICES */

#ifdef MEMOSERVICES

	for (ii = 0; ii < MEMOLIST_MAX; ++ii)
		for (mtemp = memolist[ii]; mtemp; mtemp = mtemp->next)
			mtemp->flags |= MS_RDELETE;

	if (ms_loaddata() == (-2))
	{
		/*
		 * Reload had fatal errors, so use the old database -
		 * go through and KEEP all structures marked for deletion
		 * and DELETE structures not marked, because they are the
		 * ones that were just added in the failed reload
		 */

		for (ii = 0; ii < MEMOLIST_MAX; ++ii)
		{
			for (mtemp = memolist[ii]; mtemp; mtemp = mnext)
			{
				mnext = mtemp->next;

				if (!(mtemp->flags & MS_RDELETE))
					DeleteMemoList(mtemp);
				else
				{
					/* remove the deletion flag */
					mtemp->flags &= ~MS_RDELETE;
				}
			}
		}

		return 0;
	}
	else
	{
		/*
		 * Reload was ok - now go through and remove the old memo
		 * structures marked for deletion
		 */

		for (ii = 0; ii < MEMOLIST_MAX; ++ii)
		{
			for (mtemp = memolist[ii]; mtemp; mtemp = mnext)
			{
				mnext = mtemp->next;

				if (mtemp->flags & MS_RDELETE)
					DeleteMemoList(mtemp);
			}
		}
	}

#endif /* MEMOSERVICES */

#endif /* NICKSERVICES */

#ifdef STATSERVICES

	if (ss_loaddata() == (-2))
		return 0;

#endif /* STATSERVICES */

#ifdef SEENSERVICES

	if (es_loaddata() == (-2))
		return 0;

#endif /* SEENSERVICES */

	if (os_loaddata() == (-2))
		return 0;

	if (ignore_loaddata() == (-2))
		return 0;

	return 1;
} /* ReloadData() */

/*
CreateDatabase()
  Open file 'name' and return a pointer to it
*/

FILE *
CreateDatabase(char *name, char *info)

{
	FILE *fptr;
	time_t currtime;

	if ((fptr = fopen(name, "w")) == NULL)
	{
		putlog(LOG1,
			   "Error creating database [%s]: %s",
			   name, strerror(errno));
		return NULL;
	}

	/* change the mode to -rw------- */
	if (chmod(name, 0600) == -1)
	{
		putlog(LOG1,
			   "Error changing permissions for database [%s]: %s",
			   name, strerror(errno));
		fclose(fptr);
		return NULL;
	}

	currtime = current_ts;
	fprintf(fptr, "; Hybserv2 %s - %s - created %s",
	        hVersion, info, ctime(&currtime));

	return (fptr);
} /* CreateDatabase() */

/*
BackupDatabases()
 Called from DoTimer() to backup the previous days'
database files. Use a directory structure like:
   backup/YYYYMMDD/database
so we can tell exactly what day the files were backed up.
*/

void
BackupDatabases(time_t unixtime)

{
	struct tm *backup_tm;
	char bpath[MAXLINE + 1],
	temp[MAXLINE + 1];

	/*
	 * First make sure HPath/backup/ exists
	 * Notice that %s/backup/something has to be under 512 characters. -kre
	 */
	ircsprintf(bpath, "%s/backup", HPath);
	/* Function mkdir() returns -1 on failure -kre */
	if (mkdir(bpath, 0700)==-1)
	{
		/* Proceed if errno is set. This usually should not be necessary, but
		 * this code should help me find why Solaris complains -kre */
		if (errno && (errno != EEXIST))
		{
			putlog(LOG1,
			       "Error creating backup directory [%s]: %s",
			       bpath,
			       strerror(errno));
			return;
		}
	}

	backup_tm = localtime(&unixtime);

	ircsprintf(bpath, "%s/backup/%d%02d%02d", HPath, 1900 +
	           backup_tm->tm_year, backup_tm->tm_mon + 1, backup_tm->tm_mday);

	/*
	 * Make the directory permissions: drwx------
	 * Function mkdir() returns -1 on failure -kre
	 */
	if (mkdir(bpath, 0700)==-1)
	{
		/* Proceed if errno is set. This usually should not be necessary, but
		 * this code should help me find why Solaris complains -kre */
		if (errno && errno!=EEXIST)
		{
			putlog(LOG1,
			       "Error creating backup directory [%s]: %s",
			       bpath,
			       strerror(errno));
			return;
		}
	}

	ircsprintf(temp, "%s/%s", bpath, OperServDB);

	CopyFile(OperServDB, temp);

	ircsprintf(temp, "%s/%s", bpath, OperServIgnoreDB);

	CopyFile(OperServIgnoreDB, temp);

#ifdef STATSERVICES

	ircsprintf(temp, "%s/%s", bpath, StatServDB);

	CopyFile(StatServDB, temp);

#endif /* STATSERVICES */

#ifdef NICKSERVICES

	ircsprintf(temp, "%s/%s", bpath, NickServDB);

	CopyFile(NickServDB, temp);

#ifdef CHANNELSERVICES

	ircsprintf(temp, "%s/%s", bpath, ChanServDB);

	CopyFile(ChanServDB, temp);
#endif /* CHANNELSERVICES */

#ifdef MEMOSERVICES

	ircsprintf(temp, "%s/%s", bpath, MemoServDB);

	CopyFile(MemoServDB, temp);
#endif /* MEMOSERVICES */

	/* SeenServDB should be backed up too. -kre */
#ifdef SEENSERVICES

	ircsprintf(temp, "%s/%s", bpath, SeenServDB);

	CopyFile(SeenServDB, temp);
#endif

#endif /* NICKSERVICES */
} /* BackupDatabases() */

/*
WriteDatabases()
 Rewrite all databases
 
Return: 1 if successful
        0 if not
*/

int
WriteDatabases()

{

	if (!WriteOpers())
		return (0);

	if (FloodProtection && !WriteIgnores())
		return (0);

#ifdef STATSERVICES

	if (!WriteStats())
		return (0);

#endif /* STATSERVICES */

#ifdef SEENSERVICES

	if (!WriteSeen())
		return (0);

#endif /* SEENSERVICES */

#ifdef NICKSERVICES

	if (!WriteNicks())
		return (0);

#ifdef CHANNELSERVICES

	if (!WriteChans())
		return (0);

#endif /* CHANNELSERVICES */

#ifdef MEMOSERVICES

	if (!WriteMemos())
		return (0);

#endif /* MEMOSERVICES */

#endif /* NICKSERVICES */

	return (1);
} /* WriteDatabases() */

/*
WriteOpers()
  Write OperServDB to disk - just record opers' nicknames and
what umodes they have
 
Format:   Nickname <umodes>
 
 Return 1 if successful, 0 if not
*/

int
WriteOpers()

{
	FILE *fp;
	char tempname[MAXLINE + 1];
	struct Userlist *tempuser;
	struct Luser *luser;
	int ret;

	ircsprintf(tempname, "%s.tmp", OperServDB);
	fp = CreateDatabase(tempname, "OperServ Database");
	if (!fp)
	{
		putlog(LOG1, "Error writing OperServ Database (%s): %s",
		       OperServDB,
		       strerror(errno));
		return 0;
	}

	/* Some O: lines may be for the same * nickname...  */
	for (luser = ClientList; luser != NULL; luser = luser->next)
	{
		if (!(luser->flags & L_OSREGISTERED))
			continue;

		tempuser = GetUser(0, luser->nick, luser->username,
				luser->hostname);

		if (tempuser == NULL || tempuser == GenericOper)
			continue;

		fprintf(fp, "%s!%s@%s %ld\n", tempuser->nick,
				tempuser->username, tempuser->hostname, tempuser->umodes);

#ifdef RECORD_RESTART_TS
		if (tempuser->nick_ts && (tempuser->nick_ts < current_ts - 330))
			fprintf(fp, "->TS %li\n", tempuser->nick_ts);
		if (tempuser->last_nick)
			fprintf(fp, "->LASTNICK %s\n", tempuser->last_nick);
		if (tempuser->last_server)
			fprintf(fp, "->LASTSERVER %s\n", tempuser->last_server);
#endif

	}

	fclose(fp);

	ret = rename(tempname, OperServDB);
	if (ret == -1)
	{
		putlog(LOG1, "Error renaming OperServ Database %s to %s: %s",
			   tempname, OperServDB, strerror(errno));
		return 0;
	}

	putlog(LOG3, "Wrote %s", OperServDB);

	return 1;
} /* WriteOpers() */

/*
 * Name:      WriteIgnores()
 * Operation: Write OperServ ignore data to disk.
 * Format:    mask timeout
 * Return:    1 on success, 0 on failure
 */
int WriteIgnores()
{
	FILE *fp;
	char tempname[MAXLINE + 1];
	struct Ignore *temp;
	int ret;

	ircsprintf(tempname, "%s.tmp", OperServIgnoreDB);
	fp = CreateDatabase(tempname, "OperServ Ignore Database");
	if (!fp)
	{
		putlog(LOG1, "Error writing OperServ ignore Database (%s): %s",
		       OperServIgnoreDB, strerror(errno));
		return 0;
	}

	for (temp = IgnoreList; temp; temp = temp->next)
	{
		if (temp->expire)
			fprintf(fp, "%s %ld\n", temp->hostmask, (long)(temp->expire -
			        current_ts));
		else
			fprintf(fp, "%s %d\n", temp->hostmask, 0);
	}

	fclose(fp);

	ret = rename(tempname, OperServIgnoreDB);
	if (ret == -1)
	{
		putlog(LOG1, "Error renaming OperServ Ignore Database %s to %s: %s",
			   tempname, OperServIgnoreDB, strerror(errno));
		return 0;
	}

	putlog(LOG3, "Wrote %s", OperServIgnoreDB);

	return 1;
} /* WriteIgnores() */

#ifdef STATSERVICES

/*
WriteStats()
  Write out StatServ statistics for max users/chans etc to
StatServDB
*/

int
WriteStats()

{
	FILE *fp;
	char tempname[MAXLINE + 1];
	int ret;

	ircsprintf(tempname, "%s.tmp", StatServDB);
	fp = CreateDatabase(tempname, "StatServ Database");
	if (!fp)
	{
		putlog(LOG1, "Error writing StatServ Database (%s): %s",
		       StatServDB,
		       strerror(errno));
		return 0;
	}

	fprintf(fp, "->USERS %ld %ld\n",
	        Network->MaxUsers,
	        (long) Network->MaxUsers_ts);

	fprintf(fp, "->OPERS %ld %ld\n",
	        Network->MaxOperators,
	        (long) Network->MaxOperators_ts);

	fprintf(fp, "->CHANS %ld %ld\n",
	        Network->MaxChannels,
	        (long) Network->MaxChannels_ts);

	fprintf(fp, "->SERVS %ld %ld\n",
	        Network->MaxServers,
	        (long) Network->MaxServers_ts);

	fclose(fp);

	ret = rename(tempname, StatServDB);
	if (ret == -1)
	{
		putlog(LOG1, "Error renaming StatServ Database %s to %s: %s",
			   tempname, StatServDB, strerror(errno));
		return 0;
	}

	putlog(LOG3, "Wrote %s", StatServDB);

	return 1;
} /* WriteStats() */

#endif /* STATSERVICES */

#ifdef NICKSERVICES

/*
WriteNicks()
 Write all nickname entries to NickServDB
*/

int
WriteNicks()

{
	FILE *fp;
	char tempname[MAXLINE + 1];
	int ii, ncnt;
	struct NickInfo *nptr;
	struct NickHost *hptr;
	int islinked, ret;

	ircsprintf(tempname, "%s.tmp", NickServDB);
	fp = CreateDatabase(tempname, "NickServ Database");
	if (!fp)
	{
		putlog(LOG1, "Error writing NickServ Database (%s): %s",
		       NickServDB,
		       strerror(errno));
		return 0;
	}

	ncnt = 0;

#ifdef LINKED_NICKNAMES

	/*
	 * We have to go through the nicklist to write out all master entries
	 * first, because the leaf entries will have a ->LINK <master nick>, so
	 * the next time the database is read, we have to make sure we can find
	 * the master nickname entry or we've got problems :-). Basically, make
	 * sure the leaf entries don't get written out before master entries.
	 * Unfortunately, we have to repeat some code here.
	 */

	for (ii = 0; ii < NICKLIST_MAX; ++ii)
	{
		for (nptr = nicklist[ii]; nptr; nptr = nptr->next)
		{
			if (nptr->master || !nptr->nextlink)
			{
				/* This is not a master nickname */
				continue;
			}

			++ncnt;

			/* write out "nickname flags created last-seen" to file */
			fprintf(fp, "%s %ld %ld %ld\n",
			        nptr->nick,
			        nptr->flags,
			        (long) nptr->created,
			        (long) nptr->lastseen);

			if (nptr->password)
				fprintf(fp, "->PASS %s\n", nptr->password);

			if (nptr->phrase)
				fprintf(fp, "->PHRASE :%s\n", nptr->phrase);

			if (nptr->forbidby)
				fprintf(fp, "->FORBIDBY %s\n", nptr->forbidby);

			if (nptr->forbidreason)
				fprintf(fp, "->FORBIDREASON :%s\n", nptr->forbidreason);

			if (nptr->email)
				fprintf(fp, "->EMAIL %s\n", nptr->email);

			if (nptr->url)
				fprintf(fp, "->URL %s\n", nptr->url);

			if (nptr->gsm)
				fprintf(fp, "->GSM %s\n", nptr->gsm);

			if (nptr->phone)
				fprintf(fp, "->PHONE %s\n", nptr->phone);

			if (nptr->UIN)
				fprintf(fp, "->UIN %s\n", nptr->UIN);

			if (LastSeenInfo)
			{
				if (nptr->lastu && nptr->lasth)
					fprintf(fp, "->LASTUH %s %s\n", nptr->lastu,
					        nptr->lasth);

				if (nptr->lastqmsg)
					fprintf(fp, "->LASTQMSG :%s\n", nptr->lastqmsg);
			}

			for (hptr = nptr->hosts; hptr; hptr = hptr->next)
				fprintf(fp, "->HOST %s\n", hptr->hostmask);

#ifdef RECORD_RESTART_TS
			/* 
			 * Don't save if identified within the last 330 seconds (max
			 * inter-server TS difference + 10% slack)
			 * - Brian Brazil
			 */
			if (nptr->nick_ts && (nptr->nick_ts < current_ts - 330))
				fprintf(fp, "->TS %li\n", nptr->nick_ts);
			if (nptr->last_server)
                fprintf(fp, "->LASTSERVER %s\n", nptr->last_server);
#endif

		} /* for (nptr = nicklist[ii]; nptr; nptr = nptr->next) */
	} /* for (ii = 0; ii < NICKLIST_MAX; ++ii) */

#endif /* LINKED_NICKNAMES */

	/*
	 * Now go through and write out all non-master nickname
	 * entries
	 */

	for (ii = 0; ii < NICKLIST_MAX; ++ii)
	{
		for (nptr = nicklist[ii]; nptr; nptr = nptr->next)
		{
			islinked = 0;

#ifdef LINKED_NICKNAMES

			if (nptr->master)
				islinked = 1;
			else
			{
				/*
				 * If nptr->master is NULL, but nptr->nextlink is not,
				 * this is a master nickname, which was already written,
				 * continue the loop
				 */
				if (nptr->nextlink)
					continue;
			}
#endif /* LINKED_NICKNAMES */

			++ncnt;

			/* write out "nickname flags created last-seen" to file */
			fprintf(fp, "%s %ld %ld %ld\n",
			        nptr->nick, nptr->flags, (long) nptr->created, (long)
			        nptr->lastseen);

			if (nptr->password)
				fprintf(fp, "->PASS %s\n", nptr->password);

			if (nptr->forbidby)
				fprintf(fp, "->FORBIDBY %s\n", nptr->forbidby);

			if (nptr->forbidreason)
				fprintf(fp, "->FORBIDREASON :%s\n", nptr->forbidreason);

			if (nptr->email)
				fprintf(fp, "->EMAIL %s\n", nptr->email);

			if (nptr->url)
				fprintf(fp, "->URL %s\n", nptr->url);

			if (nptr->gsm)
				fprintf(fp, "->GSM %s\n", nptr->gsm);

			if (nptr->phone)
				fprintf(fp, "->PHONE %s\n", nptr->phone);

			if (nptr->UIN)
				fprintf(fp, "->UIN %s\n", nptr->UIN);

			if (LastSeenInfo)
			{
				if (nptr->lastu && nptr->lasth)
					fprintf(fp, "->LASTUH %s %s\n",
					        nptr->lastu,
					        nptr->lasth);

				if (nptr->lastqmsg)
					fprintf(fp, "->LASTQMSG :%s\n",
					        nptr->lastqmsg);
			}

			if (!islinked)
			{
				/*
				 * write out hostmasks only if this nickname is
				 * not linked - if it is, the master nickname
				 * (previously written) has the access list
				 */

				for (hptr = nptr->hosts; hptr; hptr = hptr->next)
					fprintf(fp, "->HOST %s\n",
					        hptr->hostmask);
			}

#ifdef RECORD_RESTART_TS
			if (nptr->nick_ts)
				fprintf(fp, "->TS %li\n", nptr->nick_ts);
			if (nptr->last_server)
				fprintf(fp, "->LASTSERVER %s\n", nptr->last_server);
#endif

#ifdef LINKED_NICKNAMES

#if 0

			assert(nptr != nptr->master);
#endif
			/* Quickfix. Seems unlink is broken atm. But, there is no need to
			 * die here since master was not written because of
			 * nptr->nextlink. Huh. Should fix link copying routines -kre */
			if ((nptr != nptr->master) && nptr->master)
				fprintf(fp, "->LINK %s\n",
				        nptr->master->nick);

#endif /* LINKED_NICKNAMES */

		} /* for (nptr = nicklist[ii]; nptr; nptr = nptr->next) */
	} /* for (ii = 0; ii < NICKLIST_MAX; ++ii) */

	fclose(fp);

	ret = rename(tempname, NickServDB);
	if (ret == -1)
	{
		putlog(LOG1, "Error renaming NickServ Database %s to %s: %s",
			   tempname, NickServDB, strerror(errno));
		return 0;
	}

	putlog(LOG3, "Wrote %s (%d registered nicknames)",
	       NickServDB, ncnt);

	return (1);
} /* WriteNicks() */

#ifdef CHANNELSERVICES

/*
WriteChans()
 Write out all channel entries to ChanServDB
*/

int
WriteChans()

{
	FILE *fp;
	char tempname[MAXLINE + 1];
	struct ChanInfo *cptr, *cnext;
	int ii, ccnt, ret;

	ircsprintf(tempname, "%s.tmp", ChanServDB);
	fp = CreateDatabase(tempname, "ChanServ Database");
	if (!fp)
	{
		putlog(LOG1, "Error writing ChanServ Database (%s): %s",
		       ChanServDB,
		       strerror(errno));
		return 0;
	}

	ccnt = 0;

	for (ii = 0; ii < CHANLIST_MAX; ++ii)
	{
		for (cptr = chanlist[ii]; cptr; cptr = cnext)
		{
			cnext = cptr->next;

			if (!GetLink(cptr->founder) &&
			        !(cptr->flags & (CS_FORGET | CS_FORBID)))
			{
				/*
				 * There is no founder - check if there is a successor.
				 * If so, promote them to founder, otherwise delete the
				 * channel.
				 */
				if (cptr->successor && GetLink(cptr->successor))
				{
					/*
					 * There is a valid successor - promote them to founder
					 */
					PromoteSuccessor(cptr);
				}
				else
				{
					putlog(LOG2,
					       "%s: dropping channel [%s] (no founder)",
					       n_ChanServ,
					       cptr->name);

					DeleteChan(cptr);

					continue;
				}
			}

			if (cptr->successor)
			{
				if (!GetLink(cptr->successor))
				{
					/* successor's nickname has expired - erase it */
					putlog(LOG2,
					       "%s: Successor [%s] for channel [%s] expired, removing",
					       n_ChanServ,
					       cptr->successor,
					       cptr->name);

					MyFree(cptr->successor);
					cptr->successor = NULL;
				}
			}

			++ccnt;

			/*
			 * format: channel-name flags ts_created ts_lastused
			 */
			fprintf(fp, "%s %ld %ld %ld\n",
			        cptr->name,
			        cptr->flags,
			        (long) cptr->created,
			        (long) cptr->lastused);

			if (!(cptr->flags & CS_FORGET))
			{
				struct ChanAccess *ca;
				struct AutoKick *ak;
				int jj;

				/* write founder */
				if (cptr->founder)
					fprintf(fp, "->FNDR %s %ld\n", cptr->founder,
					        (long)cptr->last_founder_active);

				/* write password */
				if (cptr->password)
					fprintf(fp, "->PASS %s\n", cptr->password);

				if (cptr->forbidby)
					fprintf(fp, "->FORBIDBY %s\n", cptr->forbidby);

				if (cptr->forbidreason)
					fprintf(fp, "->FORBIDREASON :%s\n", cptr->forbidreason);

				if (cptr->successor)
					fprintf(fp, "->SUCCESSOR %s %ld\n", cptr->successor,
					        (long)cptr->last_successor_active);

				if (cptr->topic)
					fprintf(fp, "->TOPIC :%s\n",
					        cptr->topic);

				if (cptr->limit)
					fprintf(fp, "->LIMIT %ld\n",
					        cptr->limit);

				if (cptr->key)
					fprintf(fp, "->KEY %s\n",
					        cptr->key);

				if (cptr->expirebans)
					fprintf(fp, "->EXPIREBANS %ld\n",
					        cptr->expirebans);
#ifdef DANCER

				if (cptr->forward)
					fprintf(fp, "->FORWARD %s\n",
					        cptr->forward);
#endif /* DANCER */

				if (cptr->modes_on)
					fprintf(fp, "->MON %d\n",
					        cptr->modes_on);

				if (cptr->modes_off)
					fprintf(fp, "->MOFF %d\n",
					        cptr->modes_off);

				if (cptr->entrymsg)
					fprintf(fp, "->ENTRYMSG :%s\n",
					        cptr->entrymsg);

				if (cptr->email)
					fprintf(fp, "->EMAIL %s\n",
					        cptr->email);

				if (cptr->url)
					fprintf(fp, "->URL %s\n",
					        cptr->url);

				if (cptr->comment)
					fprintf(fp, "->COMMENT :%s\n",
					        cptr->comment);

				fprintf(fp, "->ALVL");
				for (jj = 0; jj <= CA_FOUNDER; ++jj)
					fprintf(fp, " %d",
					        cptr->access_lvl[jj]);
				fprintf(fp, "\n");

				for (ca = cptr->access; ca; ca = ca->next)
					fprintf(fp, "->ACCESS %s %d %ld %ld %s\n", ca->nptr ?
					        ca->nptr->nick : ca->hostmask,
					        ca->level, (long)ca->created, (long)ca->last_used,
						ca->added_by);

				for (ak = cptr->akick; ak; ak = ak->next)
					fprintf(fp, "->AKICK %ld %s :%s\n",
					        (long)ak->expires, ak->hostmask,
					        ak->reason ? ak->reason : "");
			} /* if (!(cptr->flags & CS_FORGET)) */
		} /* for (cptr = chanlist[ii]; cptr; cptr = cnext) */
	} /* for (ii = 0; ii < CHANLIST_MAX; ++ii) */

	fclose(fp);

	ret = rename(tempname, ChanServDB);
	if (ret == -1)
	{
		putlog(LOG1, "Error renaming ChanServ Database %s to %s: %s",
			   tempname, ChanServDB, strerror(errno));
		return 0;
	}

	putlog(LOG3, "Wrote %s (%d registered channels)",
	       ChanServDB,
	       ccnt);

	return (1);
} /* WriteChans() */

#endif /* CHANNELSERVICES */

#ifdef MEMOSERVICES

/*
WriteMemos()
 Write memo entries to MemoServDB
*/

int
WriteMemos()

{
	FILE *fp;
	char tempname[MAXLINE + 1];
	int ii, mcnt, ret;
	struct MemoInfo *mi;
	struct Memo *memoptr;

	ircsprintf(tempname, "%s.tmp", MemoServDB);
	fp = CreateDatabase(tempname, "MemoServ Database");
	if (!fp)
	{
		putlog(LOG1, "Error writing MemoServ Database (%s): %s",
		       MemoServDB,
		       strerror(errno));
		return 0;
	}

	mcnt = 0;

	for (ii = 0; ii < MEMOLIST_MAX; ++ii)
	{
		for (mi = memolist[ii]; mi; mi = mi->next)
		{
			++mcnt;

			/* write out "target" to file */
			fprintf(fp, "%s\n",
			        mi->name);

			for (memoptr = mi->memos; memoptr; memoptr = memoptr->next)
			{
				/* write out "sender ts-sent flags :text" to file */
				if (!(memoptr->flags & MS_DELETE))
					fprintf(fp, "->TEXT %s %ld %ld :%s\n",
					        memoptr->sender,
					        (long) memoptr->sent,
					        memoptr->flags,
					        memoptr->text);
			}
		} /* for (mi = memolist[ii]; mi; mi = mi->next) */
	} /* for (ii = 0; ii < MEMOLIST_MAX; ++ii) */

	fclose(fp);

	ret = rename(tempname, MemoServDB);
	if (ret == -1)
	{
		putlog(LOG1, "Error renaming MemoServ Database %s to %s: %s",
			   tempname, MemoServDB, strerror(errno));
		return 0;
	}

	putlog(LOG3, "Wrote %s (%d memo entries)",
	       MemoServDB,
	       mcnt);

	return (1);
} /* WriteMemos() */

#endif /* MEMOSERVICES */

#endif /* NICKSERVICES */

/*
LoadData()
  Load databases
*/

void
LoadData()

{
	/* load OperServ database */
	if (os_loaddata() == (-2))
	{
		fprintf(stderr, "Fatal errors parsing database (%s)\n",
		        OperServDB);
		putlog(LOG1, "Fatal errors parsing database (%s)",
		       OperServDB);
		exit(EXIT_FAILURE);
	}

	if (ignore_loaddata() == (-2))
	{
		fprintf(stderr, "Fatal errors parsing database (%s)\n",
		        OperServIgnoreDB);
		putlog(LOG1, "Fatal errors parsing database (%s)",
		       OperServIgnoreDB);
		exit(EXIT_FAILURE);
	}

#ifdef SEENSERVICES
	/* load SeenServ database */
	if (es_loaddata() == (-2))
	{
		fprintf(stderr, "Fatal errors parsing database (%s)\n",
		        SeenServDB);
		putlog(LOG1, "Fatal errors parsing database (%s)",
		       SeenServDB);
		exit(EXIT_FAILURE);
	}
#endif /* SEENSERVICES */

#ifdef NICKSERVICES
	/* load NickServ database */
	if (ns_loaddata() == (-2))
	{
		fprintf(stderr, "Fatal errors parsing database (%s)\n",
		        NickServDB);
		putlog(LOG1, "Fatal errors parsing database (%s)",
		       NickServDB);
		exit(EXIT_FAILURE);
	}

#ifdef CHANNELSERVICES
	/* load ChanServ database */
	if (cs_loaddata() == (-2))
	{
		fprintf(stderr, "Fatal errors parsing database (%s)\n",
		        ChanServDB);
		putlog(LOG1, "Fatal errors parsing database (%s)",
		       ChanServDB);
		exit(EXIT_FAILURE);
	}
#endif

#ifdef MEMOSERVICES
	/* load MemoServ database */
	if (ms_loaddata() == (-2))
	{
		fprintf(stderr, "Fatal errors parsing database (%s)\n",
		        MemoServDB);
		putlog(LOG1, "Fatal errors parsing database (%s)",
		       MemoServDB);
		exit(EXIT_FAILURE);
	}
#endif

#endif /* NICKSERVICES */

#ifdef STATSERVICES
	/* load StatServ database */
	if (ss_loaddata() == (-2))
	{
		fprintf(stderr, "Fatal errors parsing database (%s)\n",
		        StatServDB);
		putlog(LOG1, "Fatal errors parsing database (%s)",
		       StatServDB);
		exit(EXIT_FAILURE);
	}
#endif /* STATSERVICES */

#ifdef NICKSERVICES

	putlog(LOG1,
	       "Databases loaded (%d registered nicknames, %d registered channels, %d memo entries)",
	       Network->TotalNicks,

#ifdef CHANNELSERVICES

	       Network->TotalChans,

#else

	       0,

#endif

#ifdef MEMOSERVICES

	       Network->TotalMemos);

#else

	       0);

#endif

#endif /* NICKSERVICES */
} /* LoadData() */

/*
CopyFile()
 Copy 'oldfile' to 'newfile'
Return:
  0  - successful copy
  -1 - cannot open old file
  -2 - cannot open new file
  -3 - old file isn't normal
  -4 - ran out of disk space
*/

static int
CopyFile(char *oldfile, char *newfile)

{
	int oldfd, newfd, ret, bytes;
	struct stat fst;
	char buffer[MAXLINE + 1];

	if ((oldfd = open(oldfile, O_RDONLY, 0)) < 0)
		return (-1);

	ret = fstat(oldfd, &fst);
	if (ret == -1)
	{
		close(oldfd);
		return -3;
	}

	if (!(fst.st_mode & S_IFREG))
	{
		close(oldfd);
		return (-3);
	}

	if ((newfd = creat(newfile, (int) (fst.st_mode & 0700))) < 0)
	{
		close(oldfd);
		return (-2);
	}

	/*
	 * Now start copying
	 */
	for (bytes = 1; bytes > 0; )
	{
		bytes = read(oldfd, buffer, sizeof(buffer));
		if (bytes > 0)
		{
			if (write(newfd, buffer, bytes) < bytes)
			{
				/* disk space is full */
				close(oldfd);
				close(newfd);
				unlink(newfile); /* delete the file we created */
				return (-4);
			}
		}
		/*
		 * When bytes == 0 (EOF), we will drop out of the loop
		 */
	}

	/* successful copy */
	close(oldfd);
	close(newfd);
	return (0);
} /* CopyFile() */

#ifdef SEENSERVICES

/*
WriteSeen()
  Write out SeenServ statistics for users quits/changing nicks to
SeenServDB
*/

int
WriteSeen()

{
	FILE *fp;
	char tempname[MAXLINE + 1];
	aSeen *seen;
	int ret;

	ircsprintf(tempname, "%s.tmp", SeenServDB);
	fp = CreateDatabase(tempname, "SeenServ Database");
	if (!fp)
	{
		putlog(LOG1, "Error writing SeenServ Database (%s): %s",
		       SeenServDB,
		       strerror(errno));
		return 0;
	}

	for (seen = seenb; seen; seen = seen->next)
	{
		if (seen)
			switch(seen->type)
			{
			case 1:
				fprintf(fp, "->QUIT %s %s %ld :%s\n", seen->nick, seen->userhost, (long) seen->time, seen->msg);
				break;
			case 2:
				fprintf(fp, "->NICK %s %s %ld\n", seen->nick, seen->userhost, (long) seen->time);
				break;
			default:
				break;
			}
	}

	fclose(fp);

	ret = rename(tempname, SeenServDB);
	if (ret == -1)
	{
		putlog(LOG1, "Error renaming SeenServ Database %s to %s: %s",
			   tempname, SeenServDB, strerror(errno));
		return 0;
	}

	putlog(LOG3, "Wrote %s", SeenServDB);

	return 1;
} /* WriteSeen() */

#endif /* SEENSERVICES */
