Flatworm
========

Flatworm is a proxy (simpler than a ["Squid"](http://en.wikipedia.org/wiki/Squid_(software)) :-P) that allows for easy bookkeeping while rewriting traffic.  The original goal was to inject frames into YouTube videos as they went by if one were serving as a proxy for a network connection to youtube.com.  It evolved into an attempt to build a general purpose system for mutating http traffic while also mutating the headers to be consistent with the mutated data.

This was an experiment I undertook in 2008.  It started by being derived from version 0.6 of a simple C-based proxy server called 3Proxy, written by Vladimir Dubrovin (3APA3A):

[http://www.3proxy.ru/](http://www.3proxy.ru/)

I was looking for something that "just worked" and was easy enough to get my head around, as my knowledge of HTTP was not that detailed.  I slowly evolved it from C into using C++-isms.  Though I was not yet truly knowledgable about C++ in 2008, and was just starting to use the (now-deprecated) auto_ptr.  Other oddities in there (for instance, I hadn't heard of boost::optional, but invented something similar called "knowable" to serve a similar purpose).

If I were to tackle something like this again today, I would want to build it using [Boost.ASIO](http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio.html) or perhaps a different language paradigm altogether, like [Red](http://red-lang.org).  So I don't really suggest anyone use it, except possibly for educational purposes.  I'm just archiving it on GitHub to get it off my hard drive, and to accompany an article and video.

The code has some pretty nuanced complexities to it, and I doubt there's anything out there that attacks quite the same problem.  However, if this topic interests you, then you will almost certainly want to look at the "Man In The Middle Proxy":

[http://mitmproxy.org/](http://mitmproxy.org/)

In any case, I'll maintain some amount of documentation at the project's homepage, here:

[http://flatworm.hostilefork.com](http://flatworm.hostilefork.com)

Copyright (c) 2008-2014 hostilefork.com

(3Proxy was licensed as BSD.  So should anyone care for some reason, I'm licensing my modifications under those terms as well.)