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
#include "config.h"
#include "hybdefs.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "settings.h"
#include "timestr.h"
#include "sprintf_irc.h"
#include "mystring.h"

/*
 * These variables, or directives, are set by parsing
 * SETPATH (settings.conf)
 */

char      *HPath;
char      *ConfigFile;
char      *LogFile;
char      *PidFile;
char      *HelpPath;
char      *LogPath;
char      *MotdFile;
char      *DccMotdFile;

char      *GlineFile;
char      *JupeFile;

char      *LogonNews;

char      *NickServDB;
char      *ChanServDB;
char      *MemoServDB;
char      *StatServDB;
char      *OperServDB;
char      *OperServIgnoreDB;
char      *SeenServDB;

char      *n_OperServ;
char      *n_NickServ;
char      *n_ChanServ;
char      *n_MemoServ;
char      *n_StatServ;
char      *n_HelpServ;
char      *n_SeenServ;
char      *n_Global;
char      *id_OperServ;
char      *id_NickServ;
char      *id_ChanServ;
char      *id_MemoServ;
char      *id_StatServ;
char      *id_HelpServ;
char      *id_SeenServ;
char      *desc_SeenServ;
char      *id_Global;
char      *desc_OperServ;
char      *desc_NickServ;
char      *desc_ChanServ;
char      *desc_MemoServ;
char      *desc_StatServ;
char      *desc_HelpServ;
char      *desc_Global;
char      *ServiceUmodes;

int       SecureMessaging;
int       RestrictedAccess;
int       AutoOpAdmins;
int       OpersHaveAccess;
int       SmartMasking;

long      NickNameExpire;
long      ChannelExpire;
long      MemoExpire;
long      StatExpire;
long      BanExpire;
long      NSReleaseTimeout;

int       FloodProtection;
int       FloodCount;
long      FloodTime;
long      IgnoreTime;
int       IgnoreOffenses;

int       MaxClones;
char      *MaxClonesWarning;
int       AutoKillClones;

long      ConnectFrequency;
long      ConnectBurst;
long      AutoLinkFreq;
long      TelnetTimeout;
long      IdentTimeout;
long      BindAttemptFreq;
int       MaxBinds;
int       MaxConnections;
int       OpersOnlyDcc;
long      DatabaseSync;
long      BackupFreq;
int       ReloadDbsOnHup;
int       NonStarChars;
int       DefaultHubPort;
int       DefaultTcmPort;
int       LogLevel;
int       MaxLogs;
int       MaxModes;

int       MaxTrace;
int       MaxChannel;
int       MaxKill;
int       LimitTraceKills;
int       ServerNotices;
int       DoWallops;
int       BCFloodCount;
long      BCFloodTime;

int       NSSetKill;
int       NSSetAutoMask;
int       NSSetPrivate;
int       NSSetSecure;
int       NSSetUnSecure;
int       NSSetAllowMemos;
int       NSSetMemoSignon;
int       NSSetMemoNotify;
int       NSSetHide;
int       NSSetHideEmail;
int       NSSetHideUrl;
int       NSSetHideQuit;
int       NSSetNoLink;
int       NSSetNoAdd;
int       LastSeenInfo;
int       NicknameWarn;
long      NickRegDelay;
int       MaxLinks;
int       AllowKillProtection;
int       AllowKillImmed;
int	      AllowGuardChannel;
long      MinNickAge;
long      NickNameExpireAddTime;   /* How much time to add to NickNameExpire, and ... */
long      NickNameExpireAddPeriod; /* ... how often to increase that time             */
int       MaxChansPerUser;
int       MinChanUsers;
int       MaxAkicks;
long      InhabitTimeout;
int       AllowAccessIfSOp;
int       RestrictRegister;
int       NotifyOpers;
int       GiveNotice;
long      MaxTSDelta;

int       MaxMemos;
long      MemoPurgeFreq;

int       LagDetect;
long      MaxPing;

int       GlobalNotices;
int       SeenMaxRecs;

int       UseMD5;

int       MaxServerCollides;
long      MinServerCollidesDelta;

static int CheckDirectives(void);
static int dparse(char *, int, int);

struct Directive directives[] =
    {
	    /* File Paths */
	    { "HPath", D_NORUNTIME,           { { PARAM_STRING, &HPath } } },
	    { "ConfigFile", D_NORUNTIME,      { { PARAM_STRING, &ConfigFile } } },
	    { "LogFile", D_NORUNTIME,         { { PARAM_STRING, &LogFile } } },
	    { "PidFile", D_NORUNTIME,         { { PARAM_STRING, &PidFile } } },
	    { "HelpPath", D_NORUNTIME,        { { PARAM_STRING, &HelpPath } } },
	    { "LogPath", D_NORUNTIME,         { { PARAM_STRING, &LogPath } } },
	    { "MotdFile", D_NORUNTIME,        { { PARAM_STRING, &MotdFile } } },
	    { "DccMotdFile", D_NORUNTIME,     { { PARAM_STRING, &DccMotdFile } } },

	    { "GlineFile", D_OPTIONAL,       { { PARAM_STRING, &GlineFile } } },
	    { "JupeFile", D_OPTIONAL,        { { PARAM_STRING, &JupeFile } } },

#ifdef GLOBALSERVICES

	    { "LogonNews", D_OPTIONAL,       { { PARAM_STRING, &LogonNews } } },

#endif /* GLOBALSERVICES */

	    /* Database Files */
	    { "NickServDB", D_NORUNTIME,      { { PARAM_STRING, &NickServDB } } },
	    { "ChanServDB", D_NORUNTIME,      { { PARAM_STRING, &ChanServDB } } },
	    { "MemoServDB", D_NORUNTIME,      { { PARAM_STRING, &MemoServDB } } },
	    { "StatServDB", D_NORUNTIME,      { { PARAM_STRING, &StatServDB } } },
	    { "OperServDB", D_NORUNTIME,      { { PARAM_STRING, &OperServDB } } },
	    { "OperServIgnoreDB", D_NORUNTIME,{ { PARAM_STRING, &OperServIgnoreDB } } },
	    { "SeenServDB", D_NORUNTIME,      { { PARAM_STRING, &SeenServDB } } },

	    /* Pseudo-Client Nicknames/Idents/Descriptions and Options */
	    { "OperServNick", D_NORUNTIME,    { { PARAM_STRING, &n_OperServ },
	                                        { PARAM_STRING, &id_OperServ },
	                                        { PARAM_STRING, &desc_OperServ } } },
	    { "NickServNick", D_NORUNTIME,    { { PARAM_STRING, &n_NickServ },
	                                        { PARAM_STRING, &id_NickServ },
	                                        { PARAM_STRING, &desc_NickServ } } },
	    { "ChanServNick", D_NORUNTIME,    { { PARAM_STRING, &n_ChanServ },
	                                        { PARAM_STRING, &id_ChanServ },
	                                        { PARAM_STRING, &desc_ChanServ } } },
	    { "MemoServNick", D_NORUNTIME,    { { PARAM_STRING, &n_MemoServ },
	                                        { PARAM_STRING, &id_MemoServ },
	                                        { PARAM_STRING, &desc_MemoServ } } },
	    { "StatServNick", D_NORUNTIME,    { { PARAM_STRING, &n_StatServ },
	                                        { PARAM_STRING, &id_StatServ },
	                                        { PARAM_STRING, &desc_StatServ } } },
	    { "HelpServNick", D_NORUNTIME,    { { PARAM_STRING, &n_HelpServ },
	                                        { PARAM_STRING, &id_HelpServ },
	                                        { PARAM_STRING, &desc_HelpServ } } },
	    { "GlobalNick", D_NORUNTIME,      { { PARAM_STRING, &n_Global },
	                                        { PARAM_STRING, &id_Global },
	                                        { PARAM_STRING, &desc_Global } } },
	    { "SeenServNick", D_NORUNTIME,    { { PARAM_STRING, &n_SeenServ },
	                                        { PARAM_STRING, &id_SeenServ },
	                                        { PARAM_STRING, &desc_SeenServ } } },

	    { "ServiceUmodes", D_NORUNTIME,   { { PARAM_STRING, &ServiceUmodes } } },

	    /* Security Settings */
		{ "SecureMessaging", D_OPTIONAL, { { PARAM_SET, &SecureMessaging} } } ,
	    { "RestrictedAccess", D_OPTIONAL, { { PARAM_SET, &RestrictedAccess } } },
	    { "AutoOpAdmins", D_OPTIONAL,     { { PARAM_SET, &AutoOpAdmins } } },
	    { "OpersHaveAccess", D_OPTIONAL,  { { PARAM_SET, &OpersHaveAccess } } },
	    { "SmartMasking", D_OPTIONAL,     { { PARAM_SET, &SmartMasking } } },

	    /* Expire Settings */
	    { "NickNameExpire", D_OPTIONAL,   { { PARAM_TIME, &NickNameExpire } } },
	    { "NickNameExpireAddPeriod", D_OPTIONAL,   { { PARAM_TIME, &NickNameExpireAddPeriod } } },
	    { "NickNameExpireAddTime", D_OPTIONAL,   { { PARAM_TIME, &NickNameExpireAddTime } } },
	    { "ChannelExpire", D_OPTIONAL,    { { PARAM_TIME, &ChannelExpire } } },
	    { "MemoExpire", D_OPTIONAL,       { { PARAM_TIME, &MemoExpire } } },
	    { "StatExpire", D_OPTIONAL,       { { PARAM_TIME, &StatExpire } } },
	    { "BanExpire", D_OPTIONAL,        { { PARAM_TIME, &BanExpire  } } },
	    { "NSReleaseTimeout", D_OPTIONAL, { { PARAM_TIME, &NSReleaseTimeout } } },

	    /* Flood Protection */
	    { "FloodProtection", D_OPTIONAL,  { { PARAM_SET, &FloodProtection } } },
	    { "FloodCount", D_OPTIONAL,       { { PARAM_INT, &FloodCount } } },
	    { "FloodTime", D_OPTIONAL,        { { PARAM_TIME, &FloodTime } } },
	    { "IgnoreTime", D_OPTIONAL,       { { PARAM_TIME, &IgnoreTime } } },
	    { "IgnoreOffenses", D_OPTIONAL,   { { PARAM_INT, &IgnoreOffenses } } },

	    /* Clone Control */
	    { "MaxClones", D_OPTIONAL,        { { PARAM_INT, &MaxClones } } },
	    { "MaxClonesWarning", D_OPTIONAL, { { PARAM_STRING, &MaxClonesWarning } } },
	    { "AutoKillClones", D_OPTIONAL,   { { PARAM_SET, &AutoKillClones } } },

	    /* General Configuration */
	    { "ConnectFrequency", D_REQUIRED, { { PARAM_TIME, &ConnectFrequency } } },
	    { "ConnectBurst", D_REQUIRED,     { { PARAM_TIME, &ConnectBurst } } },
	    { "AutoLinkFreq", D_REQUIRED,     { { PARAM_TIME, &AutoLinkFreq } } },
	    { "TelnetTimeout", D_REQUIRED,    { { PARAM_TIME, &TelnetTimeout } } },
	    { "IdentTimeout", D_REQUIRED,     { { PARAM_TIME, &IdentTimeout } } },
	    { "BindAttemptFreq", D_OPTIONAL,  { { PARAM_TIME, &BindAttemptFreq } } },
	    { "MaxBinds", D_OPTIONAL,         { { PARAM_INT, &MaxBinds } } },
	    { "MaxConnections", D_OPTIONAL,   { { PARAM_INT, &MaxConnections } } },
	    { "OpersOnlyDcc", D_OPTIONAL,     { { PARAM_SET, &OpersOnlyDcc } } },
	    { "DatabaseSync", D_REQUIRED,     { { PARAM_TIME, &DatabaseSync } } },
	    { "BackupFreq", D_OPTIONAL,       { { PARAM_TIME, &BackupFreq } } },
	    { "ReloadDbsOnHup", D_OPTIONAL,   { { PARAM_SET, &ReloadDbsOnHup } } },
	    { "NonStarChars", D_OPTIONAL,     { { PARAM_INT, &NonStarChars } } },
	    { "DefaultHubPort", D_REQUIRED,   { { PARAM_PORT, &DefaultHubPort } } },
	    { "DefaultTcmPort", D_REQUIRED,   { { PARAM_PORT, &DefaultTcmPort } } },
	    { "LogLevel", D_REQUIRED,         { { PARAM_INT, &LogLevel } } },
	    { "MaxLogs", D_OPTIONAL,          { { PARAM_INT, &MaxLogs } } },
	    { "MaxModes", D_NORUNTIME,        { { PARAM_INT, &MaxModes } } },
	    { "MaxTSDelta", D_OPTIONAL,       { { PARAM_TIME, &MaxTSDelta } } },

	    /* SeenServ Configuration */
	    { "SeenMaxRecs", D_REQUIRED,      { { PARAM_INT, &SeenMaxRecs } } },

	    /* OperServ Configuration */
	    { "MaxTrace", D_REQUIRED,         { { PARAM_INT, &MaxTrace } } },
	    { "MaxChannel", D_REQUIRED,       { { PARAM_INT, &MaxChannel } } },
	    { "MaxKill", D_REQUIRED,          { { PARAM_INT, &MaxKill } } },
	    { "LimitTraceKills", D_OPTIONAL,  { { PARAM_SET, &LimitTraceKills } } },
	    { "ServerNotices", D_OPTIONAL,    { { PARAM_SET, &ServerNotices } } },
	    { "DoWallops", D_OPTIONAL,        { { PARAM_SET, &DoWallops } } },
	    { "BCFloodCount", D_OPTIONAL,     { { PARAM_INT, &BCFloodCount } } },
	    { "BCFloodTime", D_OPTIONAL,      { { PARAM_TIME, &BCFloodTime } } },

	    /* NickServ Configuration */
	    { "NSSetKill", D_OPTIONAL,        { { PARAM_SET, &NSSetKill } } },
	    { "NSSetAutoMask", D_OPTIONAL,    { { PARAM_SET, &NSSetAutoMask } } },
	    { "NSSetPrivate", D_OPTIONAL,     { { PARAM_SET, &NSSetPrivate } } },
	    { "NSSetSecure", D_OPTIONAL,      { { PARAM_SET, &NSSetSecure } } },
	    { "NSSetUnSecure", D_OPTIONAL,    { { PARAM_SET, &NSSetUnSecure } } },
	    { "NSSetAllowMemos", D_OPTIONAL,  { { PARAM_SET, &NSSetAllowMemos } } },
	    { "NSSetMemoSignon", D_OPTIONAL,  { { PARAM_SET, &NSSetMemoSignon } } },
	    { "NSSetMemoNotify", D_OPTIONAL,  { { PARAM_SET, &NSSetMemoNotify } } },
	    { "NSSetHide", D_OPTIONAL,        { { PARAM_SET, &NSSetHide } } },
	    { "NSSetHideEmail", D_OPTIONAL,   { { PARAM_SET, &NSSetHideEmail } } },
	    { "NSSetHideUrl", D_OPTIONAL,     { { PARAM_SET, &NSSetHideUrl } } },
	    { "NSSetHideQuit", D_OPTIONAL,    { { PARAM_SET, &NSSetHideQuit } } },
	    { "NSSetNoLink", D_OPTIONAL,      { { PARAM_SET, &NSSetNoLink } } },
       { "NSSetNoAdd", D_OPTIONAL,       { { PARAM_SET, &NSSetNoAdd } } },
	    { "LastSeenInfo", D_OPTIONAL,     { { PARAM_SET, &LastSeenInfo } } },
	    { "NicknameWarn", D_OPTIONAL,     { { PARAM_SET, &NicknameWarn } } },
	    { "NickRegDelay", D_OPTIONAL,     { { PARAM_TIME, &NickRegDelay } } },
	    { "MaxLinks", D_OPTIONAL,         { { PARAM_INT, &MaxLinks } } },
	    { "AllowKillProtection", D_OPTIONAL,{ { PARAM_SET, &AllowKillProtection } } },
	    { "AllowKillImmed", D_OPTIONAL,   { { PARAM_SET, &AllowKillImmed } } },

	    /* ChanServ Configuration */
	    { "MaxChansPerUser", D_OPTIONAL,  { { PARAM_INT, &MaxChansPerUser } } },
	    { "MinChanUsers", D_OPTIONAL,     { { PARAM_INT, &MinChanUsers } } },
	    { "MaxAkicks", D_OPTIONAL,        { { PARAM_INT, &MaxAkicks } } },
	    { "InhabitTimeout", D_OPTIONAL,   { { PARAM_TIME, &InhabitTimeout } } },
	    { "AllowAccessIfSOp", D_OPTIONAL, { { PARAM_SET, &AllowAccessIfSOp } } },
	    { "RestrictRegister", D_OPTIONAL, { { PARAM_SET, &RestrictRegister } } },
		{ "NotifyOpers", D_OPTIONAL,      { { PARAM_SET, &NotifyOpers } } },
	    { "GiveNotice", D_OPTIONAL,       { { PARAM_SET, &GiveNotice } } },
	    { "AllowGuardChannel", D_OPTIONAL, { { PARAM_SET, &AllowGuardChannel } } },
		{ "MinNickAge", D_OPTIONAL,       { { PARAM_TIME, &MinNickAge } } },

	    /* MemoServ Configuration */
	    { "MaxMemos", D_OPTIONAL,         { { PARAM_INT, &MaxMemos } } },
	    { "MemoPurgeFreq", D_OPTIONAL,    { { PARAM_TIME, &MemoPurgeFreq } } },

	    /* StatServ Configuration */
	    { "LagDetect", D_OPTIONAL,        { { PARAM_SET, &LagDetect } } },
	    { "MaxPing", D_OPTIONAL,          { { PARAM_TIME, &MaxPing } } },

	    /* Global Configuration */
	    { "GlobalNotices", D_OPTIONAL,    { { PARAM_SET, &GlobalNotices } } },
	    { "UseMD5", D_OPTIONAL,           { { PARAM_SET, &UseMD5 } } },
	    { "MaxServerCollides", D_OPTIONAL,{ { PARAM_INT, &MaxServerCollides } } },
	    { "MinServerCollidesDelta", D_OPTIONAL,{ { PARAM_TIME, &MinServerCollidesDelta } } },
	    { 0, 0,                           { { 0, 0 } } }
    };

/*
ClearDirectives()
 Zero all of the ptr fields of directives[]
 
If rehash is 1, zero all directives that are NOT required. If
we zero a required directive, and they commented it out, it would
never get reset, which could cause a seg fault down the line. This
way, if they comment out a required directive and rehash, we still
have the old copy to work from. If they change a required directive's
value and rehash, it will simply get set to the new value, so
everything is fine.
*/

void
ClearDirectives(int rehash)

{
	struct Directive *tmp;
	int ii;

	for (tmp = directives; tmp->name; ++tmp)
	{
		if (rehash &&
		        ((tmp->flag == D_NORUNTIME) || (tmp->flag == D_REQUIRED)))
			continue;

		for (ii = 0; ii < PARAM_MAX; ii++)
		{
			if (!tmp->param[ii].type)
				break;

			switch (tmp->param[ii].type)
			{
			case PARAM_STRING:
				{
					MyFree(*(char **) tmp->param[ii].ptr);
					*(char **) tmp->param[ii].ptr = NULL;
					break;
				}

			case PARAM_TIME:
				{
					*(long **) tmp->param[ii].ptr = NULL;
					break;
				}

			case PARAM_INT:
			case PARAM_SET:
			case PARAM_PORT:
				{
					*(int *) tmp->param[ii].ptr = 0;
					break;
				}

			default:
				break;
			} /* switch (tmp->param[ii].type) */
		}
	}
} /* ClearDirectives() */

/*
CheckDirectives()
 Make sure all required directives were specified (by checking
if any of them have zero values)
Return 1 if they are all ok, 0 if not
*/

static int
CheckDirectives()

{
	struct Directive *tmp;
	int ii,
	ret,
	good;

	ret = 1;
	for (tmp = directives; tmp->name; tmp++)
	{
		if ((tmp->flag == D_REQUIRED) || (tmp->flag == D_NORUNTIME))
		{
			for (ii = 0; ii < PARAM_MAX; ii++)
			{
				if (!tmp->param[ii].type)
					break;

				good = 1;
				switch (tmp->param[ii].type)
				{
				case PARAM_STRING:
					{
						if (!(*(char **) tmp->param[ii].ptr))
							good = 0;

						break;
					}

				case PARAM_TIME:
					{
						if (!(*(long **) tmp->param[ii].ptr))
							good = 0;

						break;
					}

				case PARAM_INT:
				case PARAM_SET:
				case PARAM_PORT:
					{
						if (!(*(int *) tmp->param[ii].ptr))
							good = 0;

						break;
					}

				default:
					{
						/* shouldn't get here */
						good = 0;
						break;
					}
				}

				if (!good)
				{
					ret = 0;
					fatal(1, "%s: Missing required directive [%s]",
					      SETPATH,
					      tmp->name);
				}
			}
		}
	}

	/*
	 * Some variable-specific checks
	 */
	if (FloodProtection &&
	        (!FloodCount || !FloodTime || !IgnoreTime || !IgnoreOffenses))
	{
		fatal(1,
		      "%s: When FloodProtection is enabled, FloodCount, FloodTime, IgnoreTime, and IgnoreOffenses are required",
		      SETPATH);
		ret = 0;
	}

	if (BindAttemptFreq && !MaxBinds)
	{
		fatal(1,
		      "%s: When BindAttempFreq is enabled, MaxBinds is also required",
		      SETPATH);
		ret = 0;
	}

	return (ret);
} /* CheckDirectives() */

/*
FindDirective()
 Return pointer to index of directives[] matching 'directive'
*/

struct Directive *
			FindDirective(char *directive)

{
	struct Directive *tmp;

	if (!directive)
		return (NULL);

	for (tmp = directives; tmp->name; tmp++)
		if (!irccmp(directive, tmp->name))
			return (tmp);

	return (NULL);
} /* FindDirective() */

/*
dparse()
 Parse the given line from the settings file SETPATH
Return: 2 if the line is ok
        1 if the line contained warnings
        0 if the line contained errors
*/

static int
dparse(char *line, int linenum, int rehash)

{
	struct Directive *dptr;
	int larc; /* line arg count */
	char *larv[PARAM_MAX + 1]; /* holds line arguements */
	char *lineptr,
	*tmp;
	int ret,
	ii,
	pcnt;

	ret = 2;

	/*
	 * We can't use SplitBuf() to break up the line, since
	 * it doesn't handle quotes etc. - do it manually
	 */

	larc = 0;

	if (line == NULL)
		return 0;

	for (lineptr = line; *lineptr; lineptr++)
	{
		if (larc >= PARAM_MAX)
		{
			fatal(1, "%s:%d Too many parameters (max: %d)",
			      SETPATH,
			      linenum,
			      PARAM_MAX);
			ret = 0;
			break;
		}

		while (*lineptr && IsSpace(*lineptr))
			lineptr++;

		if (!*lineptr)
			break;

		tmp = lineptr;

		if (*lineptr == '"')
		{
			/* we encountered a quote */

			/* advance past quotation mark */
			tmp++;
			lineptr++;

			/*
			 * Now advance lineptr until we hit another quotation mark,
			 * or the end of the line
			 */
			while (*lineptr && (*lineptr != '"'))
			{
				/*
				 * If they want to include double quotes, they can
				 * put a \" inside the quoted string, so advance
				 * lineptr past it twice, once for the '\' and once
				 * for the '"'.
				 */
				if (*lineptr == '\\')
					lineptr++;

				lineptr++;
			}

			if (!*lineptr)
			{
				/* we hit EOL without reaching another quotation mark */
				fatal(1, "%s:%d Unterminated quote",
				      SETPATH,
				      linenum);
				ret = 0;
				break;
			}

			*lineptr = '\0';
		} /* if (*lineptr == '"') */
		else
		{
			/*
			 * Advance lineptr until we hit a space
			 */
			while (*lineptr && !IsSpace(*lineptr))
				lineptr++;

			if (*lineptr)
				*lineptr = '\0';
		}

		larv[larc++] = tmp;
	} /* for (lineptr = line; *lineptr; lineptr++) */

	if (!ret || !larc)
		return (ret);

	if (!(dptr = FindDirective(larv[0])))
	{
		fatal(1, "%s:%d Unknown directive [%s] (ignoring)",
		      SETPATH,
		      linenum,
		      larv[0]);
		return (1);
	}

	if (rehash && (dptr->flag == D_NORUNTIME))
	{
		/*
		 * SETPATH is being parsed during a rehash, and we
		 * hit a non-run time configurable option, ignore it
		 */
		return (2);
	}

	pcnt = 1;
	for (ii = 0; ii < PARAM_MAX; ii++, pcnt++)
	{
		if (!dptr->param[ii].type)
			break;

		/*
		 * Now assign our variable (dptr->param[ii].ptr) to the
		 * corresponding larv[] slot
		 */
		if ((dptr->param[ii].type != PARAM_SET) && (pcnt >= larc))
		{
			fatal(1, "%s:%d Not enough arguements to [%s] directive",
			      SETPATH, linenum, dptr->name);
			return (0);
		}

#if 0
		/*
		 * There should be code which check for too many arguments for
		 * PARAM_SET - because of bugs in previous Hybserv1 code I am leaving
		 * this code under undef, but it _should_ go into distribution once.
		 * However, I don't wont to break old `save -conf' files. -kre
		 */
		if ((dptr->param[ii].type == PARAM_SET) && (pcnt > 1))
		{
			fatal(1, "%s:%d Too many arguments for [%s] directive",
			      SETPATH, linenum, dptr->name);
			return 0;
		}
#endif

		switch (dptr->param[ii].type)
		{
		case PARAM_STRING:
			{
				if ((*(char **) dptr->param[ii].ptr) != NULL)
				{
					/*
					 * If this is a rehash, make sure we free the old
					 * string before allocating a new one. If this
					 * is NOT a rehash, we should never get here, because
					 * ClearDirectives() would have already zeroed
					 * all ptr fields of directives[]
					 */
					MyFree(*(char **) dptr->param[ii].ptr);
				}

				*(char **) dptr->param[ii].ptr = MyStrdup(larv[pcnt]);
				break;
			}

		case PARAM_INT:
			{
				int value;

				value = IsNum(larv[pcnt]);
				if (!value)
				{
					fatal(1, "%s:%d Invalid integer",
					      SETPATH,
					      linenum);
					ret = 0;
					break;
				}

				/*
				 * We have to call atoi() anyway since IsNum() would
				 * have returned 1 if larv[pcnt] was "0", but IsNum()
				 * is a good way of making sure we have a valid integer
				 */
				*(int *) dptr->param[ii].ptr = atoi(larv[pcnt]);
				break;
			}

		case PARAM_TIME:
			{
				long value;

				value = timestr(larv[pcnt]);
				/* Now, let us consider this a bit. Function timestr() will try to
				 * extract as much data as it can. And sometimes we actually
				 * _want_ to put 0 in time field to turn off that feature. So we
				 * will have it turned off whether it has garbage there, or is 0.
				 * Sounds OK, does it? -kre */
				/*
				       if (!value)
				       {
				         fatal(1, "%s:%d Invalid time format",
				           SETPATH,
				           linenum);
				         ret = 0;
				         break;
				       }
				*/

				*(long *) dptr->param[ii].ptr = value;
				break;
			}

		case PARAM_SET:
			{
				*(int *) dptr->param[ii].ptr = 1;
				break;
			}

		case PARAM_PORT:
			{
				int value;

				value = IsNum(larv[pcnt]);
				if (!value)
				{
					fatal(1, "%s:%d Invalid port number (must be between 1 and 65535)",
					      SETPATH,
					      linenum);
					ret = 0;
					break;
				}

				value = atoi(larv[pcnt]);

				if ((value < 1) || (value > 65535))
				{
					fatal(1, "%s:%d Invalid port number (must be between 1 and 65535)",
					      SETPATH,
					      linenum);
					ret = 0;
					break;
				}

				*(int *) dptr->param[ii].ptr = value;

				break;
			}

		default:
			{
				/* we shouldn't get here */
				fatal(1, "%s:%d Unknown parameter type [%d] for directive [%s]",
				      SETPATH,
				      linenum,
				      dptr->param[ii].type,
				      dptr->name);
				ret = 0;
				break;
			}
		} /* switch (dptr->param[ii].type) */
	} /* for (ii = 0; ii < PARAM_MAX; ii++, pcnt++) */

	return (ret);
} /* dparse() */

/*
LoadSettings()
 Load directives in SETPATH, and make sure all required
directives are specified. Return 1 if successful, 0 if not
 If (rehash == 1), it indicates this function is being
called from a rehash, so don't call ClearDirectives() or
CheckDirectives()
*/

int
LoadSettings(int rehash)

{
	FILE *fp;
	char buffer[MAXLINE + 1];
	int goodread;
	int cnt;

	if ((fp = fopen(SETPATH, "r")) == NULL)
	{
		fprintf(stderr, "Unable to open SETPATH (%s)\n",
		        SETPATH);

		if (rehash)
			return (0);
		else
			exit(EXIT_SUCCESS);
	}

	/*
	 * If rehash is 0, clear all directives, otherwise clear
	 * only PARAM_SET directives in case they comment out a
	 * SET directive, in which case we wouldn't be able to
	 * detect it, unless it was already zero'd
	 */
	ClearDirectives(rehash);

	cnt = 0;
	goodread = 1;

	while (fgets(buffer, sizeof(buffer), fp))
	{
		cnt++;
		if (buffer[0] != '#') /* comment line */
		{
			int tmp;

			tmp = dparse(buffer, cnt, rehash);

			if ((goodread > 0) && (tmp == 0))
				goodread = 0;
		}
	}

	fclose(fp);

	if (!rehash)
	{
		/*
		 * Now check to make sure all required directives
		 * were specified
		 */
		if (CheckDirectives() == 0)
			goodread = 0;
	}

	return (goodread);
} /* LoadSettings() */

/*
SaveSettings()
 Save all current settings to SETPATH.
Return: 1 if successful
        0 if not
*/
int SaveSettings()
{
	FILE *fp;
	struct Directive *dptr;
	int ii, ret;
	char buffer[MAXLINE + 1],
	tmp[MAXLINE + 1],
	tempname[MAXLINE + 1];

	/* MMkay, this should write safe config files so that they won't get
	 * b0rked if something happens when writing. -kre */
	ircsprintf(tempname, "%s.tmp", SETPATH);

	if (!(fp = fopen(tempname, "w")))
	{
		putlog(LOG1,
		       "SaveSettings(): Unable to open %s: %s",
		       SETPATH,
		       strerror(errno));
		return 0;
	}

	for (dptr = directives; dptr->name; dptr++)
	{
		buffer[0] = 0;
		for (ii = 0; ii < PARAM_MAX; ii++)
		{
			if (!dptr->param[ii].type)
				break;

			switch (dptr->param[ii].type)
			{
			case PARAM_STRING:
				{
					/* Try to write out string only if non-null, ie is set -kre */
					if (*(char **)dptr->param[ii].ptr)
					{
						ircsprintf(tmp, "\"%s\" ",
						           *(char **) dptr->param[ii].ptr);
						strlcat(buffer, tmp, sizeof(buffer));
					}

					break;
				} /* case PARAM_STRING */

			case PARAM_TIME:
				{
					ircsprintf(tmp, "%s ",
					           timeago(*(long *) dptr->param[ii].ptr, 4));
					strlcat(buffer, tmp, sizeof(buffer));

					break;
				} /* case PARAM_TIME */

			case PARAM_INT:
			case PARAM_PORT:
				{
					ircsprintf(tmp, "%d ", *(int *) dptr->param[ii].ptr);
					strlcat(buffer, tmp, sizeof(buffer));

					break;
				}

			case PARAM_SET:
				{
					/*
					 * Only write out SETs if they are non null
					 *
					 * IMHO if dptr->param[ii].type == PARAM_SET there is no reason
					 * to write numeric value because it is ignored in the first
					 * way.
					 * -kre
					 */
					if (*(int *) dptr->param[ii].ptr)
					{
#if 0
						ircsprintf(tmp, "%d ",
						           *(int *) dptr->param[ii].ptr);
						strlcat(buffer, tmp, sizeof(buffer));
#endif
						/* Fill in buffer.. */
						strlcat(buffer, " ", sizeof(buffer));
						/* and then do continue -kre */
					}
					else
						/* Any of SETs in param[] was empty, so there is no need to
						 * write directive -kre */
						buffer[0] = 0;

					break;
				} /* case PARAM_SET */
			} /* switch (dptr->param[ii].type) */
		}

		if (*buffer)
			fprintf(fp, "%-20s %s\n",
			        dptr->name,
			        buffer);
	}

	fclose(fp);

	ret = rename(tempname, SETPATH);

	return (ret == 0) ? 1 : 0;
} /* SaveSettings() */
