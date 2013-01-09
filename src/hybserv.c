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
#include "data.h"
#include "dcc.h"
#include "hash.h"
#include "hybdefs.h"
#include "init.h"
#include "log.h"
#include "operserv.h"
#include "server.h"
#include "settings.h"
#include "show.h"
#include "sock.h"
#include "timer.h"
#include "timestr.h"
#include "misc.h"
#include "sprintf_irc.h"

#if !HAVE_SYS_RESOURCE_H
# undef GIMMECORE
#endif

/* unixtime of when services was started */
time_t                       TimeStarted;

/* offset from UTC time */
long                         gmt_offset;

struct NetworkInfo           *Network;

/* structure containing services' info */
struct MyInfo                Me;

/*
 * Set to 1 "ConnectBurst" seconds after a successful
 * hub connection, so we don't send bursts of client/clone
 * connections to +y users etc, during an initial connect.
 */
int                          SafeConnect = 0;

/* Arguments list */
char **myargv;

int main(int argc, char *argv[])
{
#if !defined DEBUGMODE && !defined DAEMONTOOLS
	pid_t pid; /* pid of this process */
#endif /* !DEBUGMODE && !DAEMONTOOLS */

#ifdef GDB_DEBUG

	int GDBAttached = 0;
#endif /* GDB_DEBUG */

#if defined GIMMECORE || defined DEBUGMODE

	struct rlimit rlim; /* resource limits -kre */
#endif /* GIMMECORE || DEBUGMODE */

	FILE *pidfile; /* to write our pid */
	uid_t uid; /* real user id */
	uid_t euid; /* effective user id */

#if defined HAVE_BOEHM_GC
	GC_INIT();
#endif /* HAVE_BOEHM_GC */

	myargv = argv;

	/* Initialise current TS for services -kre */
	TimeStarted = current_ts = time(NULL);

	/* Be sure, be paranoid, be safe. -kre */
	umask(077);

	fprintf(stderr,
	        "Hybserv2 TS services version %s by Hybserv2 team\n"
#if defined __DATE__ && defined __TIME__
	        "Compiled at %s, %s\n",
#endif
	        hVersion
#if defined __DATE__ && defined __TIME__
	        , __DATE__, __TIME__
#endif
	       );

#ifdef GDB_DEBUG

	while (!GDBAttached)
		sleep(1);
#endif /* GDB_DEBUG */

	/*
	 * Load SETPATH (settings.conf) - this must be done
	 * before the config file is loaded, and before any
	 * putlog() calls are made, since LogFile is specified
	 * in settings.conf
	 */
	if (LoadSettings(0) == 0)
	{
		fprintf(stderr, "Fatal errors encountered parsing %s, exiting\n"
		        "Check logfile %s/%s\n", SETPATH, LogPath ? LogPath : "",
		        LogFile ?  LogFile : "*unknown*");
		return (0);
	}

	/*
	 * If they run ./shownicks or ./showchans rather than ./hybserv
	 * display nicknames/channels
	 */
	if (strstr(argv[0], "shownicks"))
	{
#ifdef NICKSERVICES
		ShowNicknames(argc, argv);
#endif /* NICKSERVICES */

		return (0);
	}
	else if (strstr(argv[0], "showchans"))
	{
#if defined(NICKSERVICES) && defined(CHANNELSERVICES)
		ShowChannels(argc, argv);
#endif /* defined(NICKSERVICES) && defined(CHANNELSERVICES) */

		return 0;
	}

	/* Check for running services -kre */
	if ((pidfile = fopen(PidFile, "r")) == NULL)
		fprintf(stderr, "WARNING: Unable to read pid file %s\n",
		        PidFile);
	else
	{
		pid_t mypid;
		char line[MAXLINE + 1];

		if (fgets(line, sizeof(line), pidfile) != NULL)
		{
			mypid = atoi(line);
			if (mypid && !kill(mypid, 0))
			{
				fprintf(stderr, "FATAL: Services are already running!\n");
				fclose(pidfile);
				exit(EXIT_FAILURE);
			}
		}
		fclose(pidfile);
	}

	uid = getuid(); /* the user id of the user who ran the process */
	euid = geteuid(); /* the effective id (different if setuid) */

	if (!uid || !euid)
	{
		fprintf(stderr,
		        "FATAL: Please don't run services as root. Now exiting.\n");
		exit(EXIT_FAILURE);
	}

	if (chdir(HPath) != 0)
	{
		fprintf(stderr,
		        "HPath is an invalid directory, please check %s\n",
		        SETPATH);
		exit(EXIT_FAILURE);
	}

	putlog(LOG1, "Hybserv2 TS services version %s started", hVersion);

	/* Get the offset from GMT (London time) */
	gmt_offset = GetTZOffset(TimeStarted);

	/*
	 * the Network list must be initialized before the config
	 * file is loaded
	 */
	InitLists();

	/* load server, jupe, gline, user, admin info */
	LoadConfig();

	/* load nick/chan/memo/stat databases */
	LoadData();

#ifdef GLOBALSERVICES

	if (LogonNews)
	{
		Network->LogonNewsFile.filename = LogonNews;
		ReadMessageFile(&Network->LogonNewsFile);
	}

#endif /* GLOBALSERVICES */

	if (LocalHostName)
		SetupVirtualHost();

#if !defined DEBUGMODE && !defined GDB_DEBUG

	/* Daemontools compatibility stuff */
#ifndef DAEMONTOOLS

	pid = fork();
	if (pid == -1)
	{
		printf("Unable to fork(), exiting.\n");
		exit(EXIT_FAILURE);
	}
	if (pid != 0)
	{
		printf("Running in background (pid: %d)\n", (int)pid);
		exit(EXIT_SUCCESS);
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Make current process session leader -kre */
	if (setsid() == -1)
	{
		exit(EXIT_FAILURE);
	}
#else

	printf("Entering foreground debug mode\n");
#endif /* DEBUGMODE */
#endif /* DAEMONTOOLS */

#if defined GIMMECORE || defined DEBUGMODE

	printf("Setting corefile limit... ");
	/* Set corefilesize to maximum - therefore we ensure that core will be
	 * generated, no matter of shell limits -kre */
	getrlimit(RLIMIT_CORE, &rlim);
	rlim.rlim_cur = rlim.rlim_max;
	setrlimit(RLIMIT_CORE, &rlim);
	printf("done.\n");
#endif /* GIMMECORE || DEBUGMODE */

	/* Signals must be set up after fork(), since the parent exits */
	InitSignals();

	/* Initialise random number generator -kre */
	srandom(current_ts);
	srandom((unsigned int)random());

	/* Write our pid to a file */
	if ((pidfile = fopen(PidFile, "w")) == NULL)
		putlog(LOG1, "Unable to open %s", PidFile);
	else
	{
		char line[MAXLINE + 1];

		ircsprintf(line, "%d\n", getpid());
		fputs(line, pidfile);
		fclose(pidfile);
	}

	/* initialize tcm/user listening ports */
	InitListenPorts();

	/* initialize hash tables */
	ClearHashes(1);

#ifdef BLOCK_ALLOCATION

	InitHeaps();
#endif

	HubSock = NOSOCKET;
	CycleServers();

	while (1)
	{
		/* enter loop waiting for server info */
		ReadSocketInfo();

		if (Me.hub)
			SendUmode(OPERUMODE_Y, "*** Disconnected from %s", Me.hub->name);
		else
			SendUmode(OPERUMODE_Y, "*** Disconnected from hub server");

		if (currenthub)
		{
			if (currenthub->realname)
			{
				MyFree(currenthub->realname);
				currenthub->realname = NULL;
			}
			currenthub->connect_ts = 0;
		}

		close(HubSock); /* There was an error */
		HubSock = NOSOCKET;

		/*
		 * whenever Hybserv connects/reconnects to a server, clear
		 * users, servers, and chans
		 */
		ClearUsers();
		ClearChans();
		ClearServs();
		/*
		 * ClearHashes() must be called AFTER ClearUsers(),
		 * or StatServ's unique client counts will be off since
		 * cloneTable[] would be NULL while it was trying to find
		 * clones
		 */
		ClearHashes(0);

		PostCleanup();
	} /* while (1) */

	return 0;
} /* main() */
