/*
 * HybServ2 Services by HybServ2 team
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 * $Id$
 */

#include "defs.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif

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

#ifdef RECORD_SPLIT_TS

                      newnick->split_ts = temp->split_ts;
                      newnick->whensplit = temp->whensplit;
#endif /* RECORD_SPLIT_TS */

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
    return (NULL);

  /* change the mode to -rw------- */
  chmod(name, 0600);

  currtime = current_ts;
  fprintf(fptr, "; HybServ2 %s - %s - created %s",
          hVersion,
          info,
          ctime(&currtime));

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
  char bpath[MAXLINE],
  temp[MAXLINE];

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
      if (errno && errno!=EEXIST)
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
  char tempname[MAXLINE];
  struct Userlist *tempuser;
  char *donestr; /* list of nicks we wrote */
  char temp[MAXLINE];

  ircsprintf(tempname, "%s.tmp", OperServDB);
  fp = CreateDatabase(tempname, "OperServ Database");
  if (!fp)
    {
      putlog(LOG1, "Error writing OperServ Database (%s): %s",
             OperServDB,
             strerror(errno));
      return 0;
    }

  /*
   * The problem here is some O: lines may be for the same
   * nickname, so we need to keep track to make sure we don't
   * write the same nickname twice.  It doesn't matter which
   * Userlist structure we write for each nickname, because
   * all structures for the same nickname will have the
   * exact same value for "umodes", which os_loaddata()
   * or o_umode() did already.
   * Check to make sure the nick we're about to write
   * is not already in 'donestr' - if not, write it
   * and cat it to donestr.
   */
  donestr = (char *) MyMalloc(sizeof(char));
  *donestr = '\0';
  for (tempuser = UserList; tempuser; tempuser = tempuser->next)
    {
      ircsprintf(temp, "*%s*", tempuser->nick);
      if (match(temp, donestr) == 0)
        {
          fprintf(fp, "%s %ld\n", tempuser->nick, tempuser->umodes);
          ircsprintf(temp, "%s ", tempuser->nick);
          donestr = (char *) MyRealloc(donestr, strlen(donestr) + strlen(temp)
                                       + 1);
          strcat(donestr, temp);
        }
    }

  MyFree(donestr);

  fclose(fp);

  rename(tempname, OperServDB);

  putlog(LOG3, "Wrote %s",
         OperServDB);

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
  char tempname[MAXLINE];
  struct Ignore *temp;

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
    fprintf(fp, "%s %ld\n", temp->hostmask, temp->expire - current_ts);
  }
  
  fclose(fp);
  rename(tempname, OperServIgnoreDB);
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
  char tempname[MAXLINE];

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

  rename(tempname, StatServDB);

  putlog(LOG3, "Wrote %s",
         StatServDB);

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
  char tempname[MAXLINE];
  int ii, ncnt;
  struct NickInfo *nptr;
  struct NickHost *hptr;
  int islinked;

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

          /* write out password only if not forbidden! -kre */
          if (nptr->password)
            fprintf(fp, "->PASS %s\n", nptr->password);

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

  rename(tempname, NickServDB);

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
  char tempname[MAXLINE];
  struct ChanInfo *cptr, *cnext;
  int ii,
  ccnt;

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
              !(cptr->flags & CS_FORGET))
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
              fprintf(fp, "->FNDR %s %ld\n", cptr->founder,
                  (long)cptr->last_founder_active);

              /* write password */
              fprintf(fp, "->PASS %s\n",
                      cptr->password);

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

              fprintf(fp, "->ALVL");
              for (jj = 0; jj <= CA_FOUNDER; ++jj)
                fprintf(fp, " %d",
                        cptr->access_lvl[jj]);
              fprintf(fp, "\n");

              for (ca = cptr->access; ca; ca = ca->next)
                fprintf(fp, "->ACCESS %s %d %ld %ld\n", ca->nptr ?
                    ca->nptr->nick : stripctrlsymbols(ca->hostmask),
                    ca->level, (long)ca->created, (long)ca->last_used);

              for (ak = cptr->akick; ak; ak = ak->next)
                fprintf(fp, "->AKICK %s :%s\n",
                        stripctrlsymbols(ak->hostmask),
                        ak->reason ? stripctrlsymbols(ak->reason) : "");
            } /* if (!(cptr->flags & CS_FORGET)) */
        } /* for (cptr = chanlist[ii]; cptr; cptr = cnext) */
    } /* for (ii = 0; ii < CHANLIST_MAX; ++ii) */

  fclose(fp);

  rename(tempname, ChanServDB);

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
  char tempname[MAXLINE];
  int ii,
  mcnt;
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

  rename(tempname, MemoServDB);

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
      exit(1);
    }

  if (ignore_loaddata() == (-2))
    {
      fprintf(stderr, "Fatal errors parsing database (%s)\n",
              OperServIgnoreDB);
      putlog(LOG1, "Fatal errors parsing database (%s)",
             OperServIgnoreDB);
      exit(1);
    }

#ifdef SEENSERVICES
  /* load SeenServ database */
  if (es_loaddata() == (-2))
    {
      fprintf(stderr, "Fatal errors parsing database (%s)\n",
              SeenServDB);
      putlog(LOG1, "Fatal errors parsing database (%s)",
             SeenServDB);
      exit(1);
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
      exit(1);
    }

#ifdef CHANNELSERVICES
  /* load ChanServ database */
  if (cs_loaddata() == (-2))
    {
      fprintf(stderr, "Fatal errors parsing database (%s)\n",
              ChanServDB);
      putlog(LOG1, "Fatal errors parsing database (%s)",
             ChanServDB);
      exit(1);
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
      exit(1);
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
      exit(1);
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
  int oldfd,
  newfd;
  struct stat fst;
  char buffer[MAXLINE];
  int bytes;

  if ((oldfd = open(oldfile, O_RDONLY, 0)) < 0)
    return (-1);

  fstat(oldfd, &fst);
  if (!(fst.st_mode & S_IFREG))
    return (-3);

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
  char tempname[MAXLINE];
  aSeen *seen;

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

  rename(tempname, SeenServDB);

  putlog(LOG3, "Wrote %s",
         SeenServDB);

  return 1;

} /* WriteSeen() */

#endif /* SEENSERVICES */
