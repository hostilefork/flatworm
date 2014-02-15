//
// NetUtils.h
//
// Some of the basic network functions out of 3Proxy isolated from the main
// proxying logic, to make room for all the filtering code.  Added in some
// abstractions used by SockBuf too.
//

#ifndef __NETUTILS_H__
#define __NETUTILS_H__

#include <winsock2.h>


// There are many different ways of specifying timeouts
// This is better for type checking, especially in parameter lists where
// it can be easy to mix up a integer representing a size vs. an integer
// representing a timeout
class Timeout {
private:
	int sec;
	int usec;

public:
	Timeout () :
		sec (-1),
		usec (-1)
	{
	}

	explicit Timeout (int sec, int usec = 0) :
		sec (sec),
		usec (usec)
	{
	}

	DWORD getMilliseconds() const {
		if ((this->sec == -1) || (this->usec == -1)) {
			DebugBreak();
			return 0;
		}
		return sec * 1000 + usec;
	}

	int getSeconds() const {
		if ((this->sec == -1) || (this->usec == -1)) {
			DebugBreak();
			return 0;
		}
		return sec;
	}

	virtual ~Timeout () {}
};


#ifndef EAGAIN
#define EAGAIN WSAEWOULDBLOCK
#endif

#ifndef EINTR
#define EINTR WSAEWOULDBLOCK
#endif

#define SLEEPTIME 1
#define usleep Sleep

#define SASIZETYPE int
#define SHUT_RDWR SD_BOTH

#define UDPBUFSIZE 16384
#define TCPBUFSIZE  4096
#define BUFSIZE TCPBUFSIZE

#ifdef WITH_POLL
#include <poll.h>
#else
struct MYPOLLFD {
 SOCKET    fd;       /* file descriptor */
 short  events;   /* events to look for */
 short  revents;  /* events returned */
};

int mypoll(MYPOLLFD *fds, unsigned int nfds, Timeout timeout);
#ifndef POLLIN
#define POLLIN 1
#endif
#ifndef POLLOUT
#define POLLOUT 2
#endif
#ifndef POLLPRI
#define POLLPRI 4
#endif
#ifndef POLLERR
#define POLLERR 8
#endif
#ifndef POLLHUP
#define POLLHUP 16
#endif
#ifndef POLLNVAL
#define POLLNVAL 32
#endif
#define poll mypoll
#endif

int socksendto(
	SOCKET sock, struct sockaddr_in * sin,
	const char * buf, int bufsize,
	const Timeout timeout
);

int sockrecvfrom(
	SOCKET sock,
	struct sockaddr_in * sin,
	char * buf,
	int bufsize,
	const Timeout timeout
);

#endif
