/*
 * settings.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_settings_h
#define INCLUDED_settings_h

#include "stdinc.h"
#include "config.h"

/* Parameter types */
#define   PARAM_STRING      1 /* parameter is a string */
#define   PARAM_INT         2 /* parameter is an integer */
#define   PARAM_TIME        3 /* parameter is in time format */
#define   PARAM_SET         4 /* parameter is configurable on/off */
#define   PARAM_PORT        5 /* parameter is a port number 1..65535 */

/* Directive types */
#define   D_REQUIRED        1 /* directive is required */
#define   D_OPTIONAL        2 /* directive is optional */
#define   D_NORUNTIME       3 /* directive is required and not run-time configurable */

#define   PARAM_MAX         5 /* max parameters on one line */

struct Directive
{
	char *name; /* directive name */
	int flag; /* directive flag - D_* */

	struct
	{
		int type; /* type of variable "ptr" is - PARAM_* */
		void *ptr; /* where to store parameter */
	}
	param[PARAM_MAX];
};

struct Directive *FindDirective(char *);
int LoadSettings(int);
int SaveSettings(void);
void ClearDirectives(int);

extern  struct Directive directives[];
extern	int	      AllowGuardChannel;
extern  char      *ChanServDB;
extern  char      *ConfigFile;
extern  char      *DccMotdFile;
extern  char      *GlineFile;
extern  char      *HPath;
extern  char      *HelpPath;
extern  char      *LogPath;
extern  char      *JupeFile;
extern  char      *LogFile;
extern  char      *LogonNews;
extern  char      *MaxClonesWarning;
extern  char      *MemoServDB;
extern  char      *MotdFile;
extern  char      *NickServDB;
extern  char      *OperServDB;
extern  char      *OperServIgnoreDB;
extern  char      *PidFile;
extern  char      *PipeFile;
extern  char      *SeenServDB;
extern  char      *ServiceUmodes;
extern  char      *StatServDB;
extern  char      *desc_ChanServ;
extern  char      *desc_Global;
extern  char      *desc_HelpServ;
extern  char      *desc_MemoServ;
extern  char      *desc_NickServ;
extern  char      *desc_OperServ;
extern  char      *desc_SeenServ;
extern  char      *desc_StatServ;
extern  char      *id_ChanServ;
extern  char      *id_Global;
extern  char      *id_HelpServ;
extern  char      *id_MemoServ;
extern  char      *id_NickServ;
extern  char      *id_OperServ;
extern  char      *id_SeenServ;
extern  char      *id_StatServ;
extern  char      *n_ChanServ;
extern  char      *n_Global;
extern  char      *n_HelpServ;
extern  char      *n_MemoServ;
extern  char      *n_NickServ;
extern  char      *n_OperServ;
extern  char      *n_SeenServ;
extern  char      *n_StatServ;
extern  int       AllowAccessIfSOp;
extern  int       AllowKillImmed;
extern  int       AllowKillProtection;
extern  int       AutoKillClones;
extern  int       AutoOpAdmins;
extern  int       BCFloodCount;
extern  int       DefaultHubPort;
extern  int       DefaultTcmPort;
extern  int       DoWallops;
extern  int       FloodCount;
extern  int       FloodProtection;
extern  int       NotifyOpers;
extern  int       GiveNotice;
extern  int       GlobalNotices;
extern  int       IgnoreOffenses;
extern  int       LagDetect;
extern  int       LastSeenInfo;
extern  int       LimitTraceKills;
extern  int       LogLevel;
extern  int       MaxAkicks;
extern  int       MaxBinds;
extern  int       MaxChannel;
extern  int       MaxChansPerUser;
extern  int       MinChanUsers;
extern  int       MaxClones;
extern  int       MaxConnections;
extern  int       MaxKill;
extern  int       MaxLinks;
extern  int       MaxLogs;
extern  int       MaxMemos;
extern  int       MaxModes;
extern  int       MaxProxies;
extern  int       MaxServerCollides;
extern  int       MaxTrace;
extern  int       NSSetAllowMemos;
extern  int       NSSetAutoMask;
extern  int       NSSetHide;
extern  int       NSSetHideEmail;
extern  int       NSSetHideQuit;
extern  int       NSSetHideUrl;
extern  int       NSSetKill;
extern  int       NSSetMemoNotify;
extern  int       NSSetMemoSignon;
extern  int       NSSetPrivate;
extern  int       NSSetSecure;
extern  int       NSSetUnSecure;
extern  int       NSSetNoLink;
extern  int       NSSetNoAdd;
extern  int       NicknameWarn;
extern  int       NonStarChars;
extern  int       OpersHaveAccess;
extern  int       OpersOnlyDcc;
extern  int       ReloadDbsOnHup;
extern  int       RestrictRegister;
extern  int       SecureMessaging;
extern  int       RestrictedAccess;
extern  int       SeenMaxRecs;
extern  int       ServerNotices;
extern  int       SmartMasking;
extern  long      AutoLinkFreq;
extern  long      BCFloodTime;
extern  long      BackupFreq;
extern  long      BanExpire;
extern  long      BindAttemptFreq;
extern  long      ChannelExpire;
extern  long      ConnectBurst;
extern  long      ConnectFrequency;
extern  long      DatabaseSync;
extern  long      FloodTime;
extern  long      IdentTimeout;
extern  long      IgnoreTime;
extern  long      InhabitTimeout;
extern  long      MaxPing;
extern  long      MaxTSDelta;
extern  long      MemoExpire;
extern  long      MemoPurgeFreq;
extern  long      MinServerCollidesDelta;
extern  long      NSReleaseTimeout;
extern  long      NickNameExpire;
extern  long      NickRegDelay;
extern  long      StatExpire;
extern  long      TelnetTimeout;
extern  long      MinNickAge;
extern  long      NickNameExpireAddTime;
extern  long      NickNameExpireAddPeriod;

#endif /* INCLUDED_settings_h */
