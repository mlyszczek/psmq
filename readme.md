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

Good, question. Why would you want to add more code, to your project, when you
can do all this thing using ordinary queues - after all **psmq** is implemented on
top of **mqueue**. Well, if you only communicate processes in 1:1 way, you won't
find this library useful - it's easy enough to create 2 queues and send/receive
from them. No, publish subscribe shines best when you need to communicate N:N
processes. That is for example, you have process 1 that handles **CAN** hardware
and reads battery level. Now couple of processes might be interested in battery
level, so you simply publish that information on "/battery/level" topic and any
other processes might subscribe to it and read it. Some examples who could be
interested in such messages:

* graphical user interface - for nice displaying battery status
* engine control unit - to shut down engine to prevent battery damage
* disgnostic module for monitoring battery live and performance.

Note that these processes could also receive multiple battery information (if
there are more than 1 battery). So instead of creating bunch of queues yourself
and sending multiple messages to different processes, remembering who wants
what, you can shift responsibility to receiving end, since application knows
best what it needs, so it can just subscribe to messages, and your client will
just send frame to broker and forget about it. Of course, you could do all of
this in one process, or multiple threads that share such information easily, but
it's poor design.

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
External dependencies are:

* [>=embedlog-0.6](https://embedlog.bofc.pl)

**psmq-0.1.0** requires
* [=embedlog-0.4](https://embedlog.bofc.pl)

Test results
============

NOTE: these represent test results from **master** branch and may, from time to
time, show failures. Tagged versions **always** pass all tests on all
architectures. No exceptions.

operating system tests
----------------------

* arm-cortex-m3-nuttx-7.28 (manual) ![test-result-svg][fsan]
* power4-polarhome-aix-7.1 ![test-result-svg][p4aix]
* i686-builder-freebsd-11.1 ![test-result-svg][x32fb]
* i686-builder-netbsd-8.0 ![test-result-svg][x32nb]
* x86_64-builder-solaris-11.3 ![test-result-svg][x64ss]
* i686-builder-linux-gnu-4.9 ![test-result-svg][x32lg]
* i686-builder-linux-musl-4.9 ![test-result-svg][x32lm]
* i686-builder-linux-uclibc-4.9 ![test-result-svg][x32lu]
* x86_64-builder-linux-gnu-4.9 ![test-result-svg][x64lg]
* x86_64-builder-linux-musl-4.9 ![test-result-svg][x64lm]
* x86_64-builder-linux-uclibc-4.9 ![test-result-svg][x64lu]
* x86_64-bofc-debian-9 ![test-result-svg][x64debian9]
* x86_64-bofc-centos-7 ![test-result-svg][x64centos7]
* x86_64-bofc-fedora-28 ![test-result-svg][x64fedora28]
* x86_64-bofc-opensuse-15 ![test-result-svg][x64suse15]
* x86_64-bofc-rhel-7 ![test-result-svg][x64rhel7]
* x86_64-bofc-slackware-14.2 ![test-result-svg][x64slackware142]
* x86_64-bofc-ubuntu-18.04 ![test-result-svg][x64ubuntu1804]

machine tests
-------------

* aarch64-builder-linux-gnu ![test-result-svg][a64lg]
* armv5te926-builder-linux-gnueabihf ![test-result-svg][armv5]
* armv6j1136-builder-linux-gnueabihf ![test-result-svg][armv6]
* armv7a15-builder-linux-gnueabihf ![test-result-svg][armv7a15]
* armv7a9-builder-linux-gnueabihf ![test-result-svg][armv7a9]
* mips-builder-linux-gnu ![test-result-svg][m32lg]

sanitizers
----------

* -fsanitize (address, leak, undefined, thread) ![test-result-svg][fsan]

Compiling and installing
========================

Instruction on how to compile, install and integrate are in
[psmq_building](https://psmq.bofc.pl/manuals/psmq_building.7.html) manual
page.

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

[a64lg]: http://ci.psmq.bofc.pl/badges/aarch64-builder-linux-gnu-tests.svg
[armv5]: http://ci.psmq.bofc.pl/badges/armv5te926-builder-linux-gnueabihf-tests.svg
[armv6]: http://ci.psmq.bofc.pl/badges/armv6j1136-builder-linux-gnueabihf-tests.svg
[armv7a15]: http://ci.psmq.bofc.pl/badges/armv7a15-builder-linux-gnueabihf-tests.svg
[armv7a9]: http://ci.psmq.bofc.pl/badges/armv7a9-builder-linux-gnueabihf-tests.svg
[x32fb]: http://ci.psmq.bofc.pl/badges/i686-builder-freebsd-tests.svg
[x32lg]: http://ci.psmq.bofc.pl/badges/i686-builder-linux-gnu-tests.svg
[x32lm]: http://ci.psmq.bofc.pl/badges/i686-builder-linux-musl-tests.svg
[x32lu]: http://ci.psmq.bofc.pl/badges/i686-builder-linux-uclibc-tests.svg
[x32nb]: http://ci.psmq.bofc.pl/badges/i686-builder-netbsd-tests.svg
[m32lg]: http://ci.psmq.bofc.pl/badges/mips-builder-linux-gnu-tests.svg
[x64lg]: http://ci.psmq.bofc.pl/badges/x86_64-builder-linux-gnu-tests.svg
[x64lm]: http://ci.psmq.bofc.pl/badges/x86_64-builder-linux-musl-tests.svg
[x64lu]: http://ci.psmq.bofc.pl/badges/x86_64-builder-linux-uclibc-tests.svg
[x64ss]: http://ci.psmq.bofc.pl/badges/x86_64-builder-solaris-tests.svg
[p4aix]: http://ci.psmq.bofc.pl/badges/power4-polarhome-aix-tests.svg
[x64debian9]: http://ci.psmq.bofc.pl/badges/x86_64-debian-9-tests.svg
[x64centos7]: http://ci.psmq.bofc.pl/badges/x86_64-centos-7-tests.svg
[x64fedora28]: http://ci.psmq.bofc.pl/badges/x86_64-fedora-28-tests.svg
[x64suse15]: http://ci.psmq.bofc.pl/badges/x86_64-opensuse-15-tests.svg
[x64rhel7]: http://ci.psmq.bofc.pl/badges/x86_64-rhel-7-tests.svg
[x64slackware142]: http://ci.psmq.bofc.pl/badges/x86_64-slackware-142-tests.svg
[x64ubuntu1804]: http://ci.psmq.bofc.pl/badges/x86_64-ubuntu-1804-tests.svg
[fsan]: http://ci.psmq.bofc.pl/badges/fsanitize.svg
