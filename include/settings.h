/*
 * settings.h
 * Copyright (C) 1999 Patrick Alken
 *
 * $Id$
 */

#ifndef INCLUDED_settings_h
#define INCLUDED_settings_h

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
  } param[PARAM_MAX];
};

/*
 * Prototypes
 */

struct Directive *FindDirective(char *);
int LoadSettings(int rehash);
int SaveSettings();

/*
 * External declarations
 */

extern  struct Directive directives[];

extern  char      *HPath;
extern  char      *ConfigFile;
extern  char      *LogFile;
extern  char      *PidFile;
extern  char      *HelpPath;
extern  char      *MotdFile;
extern  char      *DccMotdFile;

extern  char      *GlineFile;
extern  char      *JupeFile;

extern  char      *LogonNews;

extern  char      *NickServDB;
extern  char      *ChanServDB;
extern  char      *MemoServDB;
extern  char      *StatServDB;
extern  char      *OperServDB;
extern  char      *SeenServDB;

extern  char      *n_OperServ;
extern  char      *n_NickServ;
extern  char      *n_ChanServ;
extern  char      *n_MemoServ;
extern  char      *n_StatServ;
extern  char      *n_HelpServ;
extern  char      *n_Global;
extern  char      *n_SeenServ;

extern  char      *id_OperServ;
extern  char      *id_NickServ;
extern  char      *id_ChanServ;
extern  char      *id_MemoServ;
extern  char      *id_StatServ;
extern  char      *id_HelpServ;
extern  char      *id_Global;
extern  char      *id_SeenServ;

extern  char      *desc_OperServ;
extern  char      *desc_NickServ;
extern  char      *desc_ChanServ;
extern  char      *desc_MemoServ;
extern  char      *desc_StatServ;
extern  char      *desc_HelpServ;
extern  char      *desc_Global;
extern  char      *desc_SeenServ;

extern  char      *ServiceUmodes;

extern  int       RestrictedAccess;
extern  int       AutoOpAdmins;
extern  int       OpersHaveAccess;
extern  int       SmartMasking;

extern  long      NickNameExpire;
extern  long      ChannelExpire;
extern  long      MemoExpire;
extern  long      StatExpire;
extern  long      NSReleaseTimeout;

extern  int       FloodProtection;
extern  int       FloodCount;
extern  long      FloodTime;
extern  long      IgnoreTime;
extern  int       IgnoreOffenses;

extern  int       MaxClones;
extern  char      *MaxClonesWarning;
extern  int       AutoKillClones;

extern  long      ConnectFrequency;
extern  long      ConnectBurst;
extern  int       MaxProxies;
extern  long      AutoLinkFreq;
extern  long      TelnetTimeout;
extern  long      IdentTimeout;
extern  long      BindAttemptFreq;
extern  int       MaxBinds;
extern  int       MaxConnections;
extern  int       OpersOnlyDcc;
extern  long      DatabaseSync;
extern  long      BackupFreq;
extern  int       ReloadDbsOnHup;
extern  int       NonStarChars;
extern  int       DefaultHubPort;
extern  int       DefaultTcmPort;
extern  int       LogLevel;
extern  int       MaxLogs;
extern  int       MaxModes;

extern  int       MaxTrace;
extern  int       MaxChannel;
extern  int       MaxKill;
extern  int       LimitTraceKills;
extern  int       ServerNotices;
extern  int       DoWallops;
extern  int       BCFloodCount;
extern  long      BCFloodTime;

extern  int       NSSetKill;
extern  int       NSSetAutoMask;
extern  int       NSSetPrivate;
extern  int       NSSetSecure;
extern  int       NSSetUnSecure;
extern  int       NSSetAllowMemos;
extern  int       NSSetMemoSignon;
extern  int       NSSetMemoNotify;
extern  int       NSSetHide;
extern  int       NSSetHideEmail;
extern  int       NSSetHideUrl;
extern  int       NSSetHideQuit;
extern  int       LastSeenInfo;
extern  int       NicknameWarn;
extern  long      NickRegDelay;
extern  int       MaxLinks;
extern  int       AllowKillProtection;
extern  int       AllowKillImmed;
extern	int	  AllowGuardChannel;

extern  int       MaxChansPerUser;
extern  int       MaxAkicks;
extern  long      InhabitTimeout;
extern  int       AllowAccessIfSOp;
extern  int       RestrictRegister;
extern  int       GiveNotice;

extern  int       MaxMemos;
extern  long      MemoPurgeFreq;

extern  int       LagDetect;
extern  long      MaxPing;

extern  int       GlobalNotices;
extern  int       SeenMaxRecs;

#endif /* INCLUDED_settings_h */
