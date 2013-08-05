ZNC Log Search
==============

Adds a search utility as a companion to the standard ZNC log module.

Installation
============

Download log_search.cpp to your ZNC server and build it like so:

    znc-buildmod log_search.cpp

To install, copy the resulting *.so file to your ZNC modules directory.

Usage
=====

    /msg *status loadmodule log_search
    /msg *log_search search <channel or nick> <query>

Limitations
===========

First, this shells out to grep and assumes POSIX paths, so Windows is
(currently) unsupported. Pull requests welcome.

Second, results are currently capped at 10 to avoid the potential for
extraordinary spam.

Third, though the module will only send you the first 10 results, all
matches are temporarily stored when the grep command is issued. So be
careful if you have a ton of logs- you could over-tax your server by
either (or both!) memory or I/O.

Look for fixes to these limitations in future releases.
