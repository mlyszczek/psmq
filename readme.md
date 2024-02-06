[kursg-meta]: # (order: 1)

About
=====

**psmq** is **p**ublish **s**ubscribe **m**essage **q**ueue. It's a set of
programs and libraries to implement publish/subscribe way of inter process
communication on top of **POSIX** message queue. This program targets mainly
small embedded systems, which do not have access to some more sophisticated IPC
(like unix domain socket). It could also prove useful when you want to realy
heavly on message priority, since every message has set a priority and broker
processes messages from highest ones first. It's main focues is memory and
runtime safety, speed is treated as second class citizen - but of course is not
ignored completely, it's just memory and safety comes first. Always.

Why?
====

Good, question. Why would you want to add more code, to your project, when
you can do all these things using ordinary queues - after all **psmq** is
implemented on top of **mqueue**. Well, if you only communicate processes in
1:1 way, you won't find this library useful - it's easy enough to create 2
queues and send/receive from them. No. Publish subscribe model shines best when
you need to communicate N:N processes. That is for example, you have process1
that handles hardware and reads battery level via **CAN**. Now couple of
processes might be interested in battery level, so you simply publish that
information on "/battery/level" topic and any other processes might
subscribe to it and read it. Some examples who could be interested in such
messages:

* graphical user interface - for nice displaying battery status
* engine control unit - to shut down engine to prevent battery damage
* dignostic module for monitoring battery live and performance.

Note that these processes could also receive multiple battery information (if
there are more than 1 battery). So instead of creating bunch of queues yourself
and sending multiple messages to different processes, remembering who wants
what, you can shift responsibility to receiving end, since application knows
best what it needs, so it can just subscribe to messages, and your client will
just send frame to broker and forget about it. Of course, you could do all of
this in one process, or multiple threads that share such information easily, but
it's poor design.

Library overview with usage explanation can be found in
[psmq_overview](https://psmq.bofc.pl/manuals/psmq_overview.7.html)(7)
manual page.

Documentation
=============

For detailed information about using broker and library, check out
[man pages](https://psmq.bofc.pl/manuals.html).

Examples
========

Example of receiving message is in
[psmq_receive](https://psmq.bofc.pl/manuals/psmq_receive.3.html)(3) manual
page, and example of publishing message is in
[psmq_publish](https://psmq.bofc.pl/manuals/psmq_publish.3.html)(3) manual
page.

You can also study [psmq-pub](https://git.bofc.pl/psmq/tree/src/psmq-pub.c)
and [psmq-sub](https://git.bofc.pl/psmq/tree/src/psmq-sub.c) source code.

Dependencies
============

All code follows **ANSI C** and **POSIX** standard (version 200112).
Project has no hard dependencies. The only optional dependency is
**embedlog** which enables more detailed debug logs (like, log location)
and alows for easy logging onto file or other facilities. Without
**embedlog** logs are still available but are only printed to stderr
using stdio.

Broker and psmq-sub optionally can support embedlog library for
enhanced logging.

* [>=embedlog-0.6](https://embedlog.bofc.pl)
* [=embedlog-0.4](https://embedlog.bofc.pl) (**psmq-0.1.0** only)

Test results
============

Library, broker and companion programs have been tested on following
systems/machines.

operating system tests
----------------------

* nuttx
* aix
* hpux
* freebsd
* netbsd
* solaris
* linux

machine tests
-------------

* aarch64-builder-linux-g
* armv5te926-builder-linux-gnueabihf
* armv6j1136-builder-linux-gnueabihf
* armv7a15-builder-linux-gnueabihf
* armv7a9-builder-linux-gnueabihf
* mips-builder-linux-gnu

sanitizers
----------

* -fsanitize=address
* -fsanitize=leak
* -fsanitize=undefined
* -fsanitize=thread

Compiling and installing
========================

Instruction on how to compile, install and integrate are in
[psmq_building](https://psmq.bofc.pl/manuals/psmq_building.7.html) manual
page.

For building on Unix it's classic "./configure && make install"

Contact
=======

Michał Łyszczek <michal.lyszczek@bofc.pl>

License
=======

Library is licensed under BSD 2-clause license. See
[LICENSE](http://git.bofc.pl/psmq/tree/LICENSE) file for details

See also
========

* [mtest](http://mtest.bofc.pl) unit test framework **psmq** uses
* [git repository](http://git.bofc.pl/psmq) to browse code online
* [continous integration](http://ci.psmq.bofc.pl) for project
* [polarhome](http://www.polarhome.com) nearly free shell accounts for
  virtually any unix there is.
* [pvs studio](https://www.viva64.com/en/pvs-studio) static code analyzer with
  free licenses for open source projects
