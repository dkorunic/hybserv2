![](https://api.travis-ci.org/dkorunic/hybserv2.svg)
![](https://scan.coverity.com/projects/263/badge.svg)

README
======

INFO
----
Hybserv2 is a new and improved version of the now unsupported Hybserv,
sporting new features, bugfixes, and more.  It is specifically designed
for Hybrid IRC servers, but may work with others as well. It includes
OperServ, NickServ, ChanServ, MemoServ, StatServ, HelpServ, and Global.
Each of the services can be enabled and disabled as necessary.

Hybserv was specifically designed to run with Hybrid6/7 IRCd, although it
*should* work with Ratbox IRCd and any other Hybrid-based IRCd. You may
find the Hybrid IRC daemon at http://ircd-hybrid.com/. These services
(more or less same code) are currently used by several other networks such
as idolNET, BalcanNET, BGNet, BGFree, UniBG, BCNet, GunNet, ShakeIT,
FreeWorld, IRCBG, FreeNode/OPN.

The former Hybserv development team has decided to spin off and focus its
energy towards the Hybserv2 project. Like the name implies, this new
version is even better. If there is something you would like to see added
to Hybserv2, please let us know as we are always looking for fresh ideas.

NOTE: Per Sidewnder's (Cosine's) request - original Hybserv will no longer
      be supported.  So if you have questions or some problems, please
      *upgrade* first to a recent stable or even to a development version.

Hybserv was first coded by Patrick Alken (wnder|AT|underworld|net) and is
now maintained by the Hybserv2 Coding Team, unless otherwise mentioned in
the code or the Credits.


STAFF
-----
Lead developer of Hybserv2:

| IRC nick  | real name  | e-mail  |
|---|---|---|
| kreator | Dinko Korunic | dinko.korunic@gmail.com |


The following people have contributed to Hybserv2 in blood and sweat
(nick alphabetical order):

| IRC nick  | real name  | e-mail  |
|---|---|---|
| adx | Piotr Nizynski | adx@crashnet.pl| 
| alex | Ales Tokic | ales.tokic@gmail.com| 
| args | Antoniu-George Savu | antoniu-george.savu@fr.tiscali.com| 
| asuffield | Andrew Suffield | asuffield@users.sourceforge.net| 
| bane | Dragan Dosen | ddosen@ddosen.net| 
| bbrazil | Brian Brazil | bbrazil@netsoc.tcd.ie| 
| BEER_MAN | Ilian Jovchev | ilian@irc.zonebg.com| 
| Bruns | Brian Bruns | bruns@magenet.net| 
| CoolCold | Roman Ovchinnikov | coolcold@coolcold.org| 
| cosine | Patrick Alken | wnder@uwns.underworld.net| 
| ddb | Ivan Petrov | ddb@xplovdiv.com| 
| decho | Nedelcho Stanev | decho@iname.com| 
| fl_ | Lee Hardy | lee@leeh.co.uk| 
| harly | Tomislav Novak | harly@bofhlet.net| 
| ike | Ivan Krstic | ike@gnjilux.srk.fer.hr | 
| Janos | John Binder | jbinder@kgazd.bme.hu| 
| John-Work | F. John Rowan | john@tdkt.org| 
| knight | Alan Levee | alan.levee@prometheus-designs.net| 
| KrisDuv | Christophe Duverger | krisduv2000@yahoo.fr| 
| mend0za | Vladimir Shahov | mend0za@nsys.by| 
| MOLI | Olivier Molinete | olivier@molinete.org| 
| rhodie | Julian Petrov | rhodie@irchelp.unibg.org| 
| Sarisa | Wendy Campbell | wcampbel@botbay.net| 
| sofit | Stanislav Zahariev | sofit@proshe.bg| 
| t0sh | Todor Dimitrov | todor_p_dimitrov@yahoo.com| 
| toot | Toby Verrall | to7@antipope.fsnet.co.uk| 
| Craig | Kamen Sabeff | ksabeff@gmail.com| 

We are grateful to all these people, as well as all users that have been
reporting bugs last several years. Thank you, without you this project
would be meaningless.

DISTRIBUTION
------------
You can get Hybserv2 from:

1. [GitHub Hybserv2 home](http://github.com/dkorunic/hybserv2) and
      [GitHub Hybserv2 releases](http://github.com/dkorunic/hybserv2/releases)

    NOTE: GitHub copy is always more current than the release, since it is
    working/development version. It is possible to checkout any needed
    release via appropriate tags (for example REL_1_9_5).

INSTALLATION
------------
Please read the INSTALL file. NOW.


RUNNING HYBSERV
---------------
Make sure that each hub server you have specified in your config file has
C/N lines for Hybserv matching the password in the first field of the S:
line and the server name specified in the N: line. Also, if you wish to
enable jupes (#define ALLOW_JUPES), you *MUST* give services an H: line in
ircd.conf.

Configuration examples:

NOTE: Suppose the host name of services is "services.name" with
an ip of 1.2.3.4, and that server accepting the services
has name "server.shomewhere" with ircd class "server" (or
class 1).

Required statements in ircd.conf for Hybrid5/6:

```
C:1.2.3.4:password:services.name::1
N:1.2.3.4:password:services.name::1
H:*:*:services.name
```

NOTE: You can leave out H line if you don't want to use server
jupes and G-Lines.

NOTE: We recommend using services on same server that is your
hub, and then you can use 127.0.0.1 as address in C/N lines
(which will give you some performance, since traffic will
go through loop back device).

However in Hybrid7 to accomplish the same you have to put in ircd.conf
following code:

```
  connect {
    name = "services.name";
    host = "1.2.3.4";
    send_password = "password";
    accept_password = "password";
    compressed = no;
    hub_mask = "*";
    class = "server";
  };
```


In hybserv.conf configuration should be as follows:

```
  S:password:hub.server.somewhere:6667
  N:services.name:Hybrid services
```

When you have compiled Hybserv and edited the necessary files, simply type
./hybserv which should start daemon properly. If it is not in process
list, check hybserv.log which should state reasons of failure. 

Then, go on IRC and type: **/msg OperServ identify <password>**

Assuming OperServ is the OperServNick defined in settings.conf and you
have given yourself a O: line in hybserv.conf. You should be allowed to
give OperServ commands through /msg or DCC CHAT.

For a list of commands do: **/msg OperServ help**

Most commands may also be done through DCC CHAT, and in fact more commands
are available through DCC CHAT. Simply **/dcc chat OperServ** to connect, and
**.help**

I have tried to make Hybserv fully compatible with TCM (linking wise). If
you wish Hybserv to be part of your TCM botnet, read TCM-LINKING for
instructions. If you have no idea what a TCM bot is, don't worry about it
:-)

If you enabled NickServ, ChanServ, MemoServ etc. in config.h, you can get
lists of their commands through **/msg *Serv help**. NickServ and ChanServ
have several commands that can only be executed by administrators. This
means you must match an O: line (with an "a" flag) in hybserv.conf and be
registered with OperServ to use them. This can be done by typing **/msg
OperServ password**, these commands CANNOT be accessed by DCC Chat. 

NOTE: If you use Hybrid or Hybrid-compatible IRC daemon, you should enable
Q-lines (quarantined nickname) for services-reserved nicknames
because of obvious security reasons:

```
  Q:NickServ:This nickname is reserved.
  Q:ChanServ:This nickname is reserved.
  Q:OperServ:This nickname is reserved.
```

However Q lines changed in Hybrid7. Feel free to copy and paste these
lines:

```
  resv {
    # The reason must go first
    reason = "This nickname is reserved";
    nick = "NickServ";
    nick = "ChanServ";
    nick = "OperServ";
  };
```

BUGS
----
Use the appropriate bug reporting and ticketing system: [GitHub Hybserv2 Issues](https://github.com/dkorunic/hybserv2/issues)


Alternative, subscribe to the mailing list (details follow) and post them
there.  We can't fix bugs if no one reports them!

LEGAL STUFF
-----------
This package has absolutely no warranty. Use at your own risk.  The author
will accept no responsibility for any damage, whatsoever, caused by this
program.

This software is released under the terms of the GNU General Public
License (see COPYING). Should you choose to use and/or modify any of code,
please do so under the terms of the GNU General Public License, published
by the Free Software Foundation.

CREDITS
-------
Functions used from other GPL'd sources:
* match() -- from ircd-hybrid source
* HashNick() -- from ircd-hybrid source
* HashChannel() -- from ircd-hybrid source

tools/mkpasswd.c is copyright (C) 1991 Nelson Minar
<minar|AT|reed|edu>, W. Campbell, and Hybrid7 team.

Some help files were used from EsperNet's service package:
[ftp://ftp.dragonfire.net/software/unix/irc](ftp://ftp.dragonfire.net/software/unix/irc)

The ideas for a settings.conf and the shownicks/showchans programs were
inspired from this package as well. See also the beginning of this file.

