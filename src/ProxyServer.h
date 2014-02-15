//
// ProxyServer.h
//
// As Main.cpp is largely the unmodified proxymain from 3Proxy, ProxyServer is
// basically just left alone as what was proxy.h
//

#ifndef __PROXYSERVER_H__
#define __PROXYSERVER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <time.h>
#include <fcntl.h>


#define MAXUSERNAME 128
#define PASSWORD_LEN 256
#define MAXNSERVERS 5


#include <io.h>
#include <process.h>
#include <winsock2.h>



#define pthread_self GetCurrentThreadId
#define getpid GetCurrentProcessId
#define pthread_t unsigned
#define socket(x, y, z) WSASocket(x, y, z, NULL, 0, 0)
#define accept(x, y, z) WSAAccept(x, y, z, NULL, 0)
#define ftruncate chsize

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define strcasecmp stricmp
#define strncasecmp strnicmp

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#ifndef isnumber
#define isnumber(n) ((n) >= '0' && (n) <= '9')
#endif

#ifndef ishex
#define ishex(n) (((n) >= '0' && (n) <= '9') \
	|| ((n) >= 'a' && (n) <='f') \
	|| ((n) >= 'A' && (n) <= 'F'))
#endif

#define isallowed(n) (((n) >= '0' && (n) <= '9') \
	|| ((n) >= 'a' && (n) <= 'z') \
	|| ((n) >= 'A' && (n) <= 'Z') \
	|| ((n) >= '*' && (n) <= '/') \
	|| (n) == '_')


#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <memory>
#include <string>

#include <winsock2.h>

#include "ProxyServerErrors.h"
#include "parasock/Filter.h"

#define CONNECT 	0x00000001
#define BIND		0x00000002
#define UDPASSOC	0x00000004
#define ICMPASSOC	0x00000008	/* reserved */
#define HTTP_GET	0x00000100
#define HTTP_PUT	0x00000200
#define HTTP_POST	0x00000400
#define HTTP_HEAD	0x00000800
#define HTTP_CONNECT	0x00001000
#define HTTP_OTHER	0x00008000
#define HTTP		0x0000EF00	/* all except HTTP_CONNECT */
#define HTTPS		HTTP_CONNECT
#define FTP_GET		0x00010000
#define FTP_PUT		0x00020000
#define FTP_LIST	0x00040000
#define FTP_DATA	0x00080000
#define FTP		0x000F0000
#define DNSRESOLVE	0x00100000
#define IM_ICQ		0x00200000
#define IM_MSN		0x00400000
#define ADMIN		0x01000000


typedef enum {
	S_NOSERVICE,
	S_PROXY,
	S_TCPPM,
	S_POP3P,
	S_SOCKS4 = 4,	/* =4 */
	S_SOCKS5 = 5,	/* =5 */
	S_UDPPM,
	S_SOCKS,
	S_SOCKS45,
	S_ADMIN,
	S_DNSPR,
	S_FTPPR,
	S_SMTPP,
	S_ICQPR,
	S_MSNPR,
	S_ZOMBIE
}ProxyWorkerSERVICE;

struct ProxyWorker;
struct SRVPARAM;

typedef void (*LOGFUNC)(ProxyWorker * proxy, const char *);
typedef void * (*REDIRECTFUNC)(ProxyWorker * proxy);
typedef unsigned long (*RESOLVFUNC)(const char *);
typedef void * (*PTHREADFUNC)(void *);


typedef enum {
	SYS,
	CL,
	CR,
	NT,
	LM,
	UN
} PWTYPE;


typedef enum {
	R_TCP,
	R_CONNECT,
	R_SOCKS4,
	R_SOCKS5,
	R_HTTP,
	R_POP3,
	R_FTP,
	R_CONNECTP,
	R_SOCKS4P,
	R_SOCKS5P,
	R_SOCKS4B,
	R_SOCKS5B,
	R_ADMIN
} REDIRTYPE;


struct EXTPARAM {
	TIMEOUT timeouts[10];
	char * conffile;
	SRVPARAM *services;
	int threadinit, counterd, haveerror, paused,
		maxchild, needreload;
	int authcachetype, authcachetime;
	std::string logname;
	unsigned long intip, extip;
	unsigned short intport, extport;
	LOGFUNC logfunc;
	std::string logtarget;
	FILE *stdlog;
	char* demanddialprog;
	time_t logtime, time;
	unsigned logdumpsrv, logdumpcli;
	char delimchar;
public:
	EXTPARAM() { 
		timeouts[0] = TIMEOUT(1);
		timeouts[1] = TIMEOUT(5);
		timeouts[2] = TIMEOUT(30);
		timeouts[3] = TIMEOUT(60);
		timeouts[4] = TIMEOUT(180);
		timeouts[5] = TIMEOUT(1800);
		timeouts[6] = TIMEOUT(15);
		timeouts[7] = TIMEOUT(60);
		timeouts[8] = TIMEOUT(0);
		timeouts[9] = TIMEOUT(0);

		conffile = NULL;
		services = NULL;
		threadinit = 0;
		counterd = -1;
		haveerror = 0;
		paused = 0;
		maxchild = 100;
		needreload = 0;
	 	authcachetype = 6;
		authcachetime = 600;
		logname = "";
		intip = INADDR_ANY;
		extip = INADDR_ANY; 
		intport = 0;
		extport = 0;
		stdlog = NULL;
		demanddialprog = NULL;
		logtime = (time_t)0;
		time = (time_t)0;
		logdumpsrv = 0;
		logdumpcli = 0;
		delimchar = '@';
	}
	virtual ~EXTPARAM() {
	}
};


struct CHILD {
	int argc;
	char **argv;
};

extern char *rotations[];

typedef enum {
	SINGLEBYTE_S,
	SINGLEBYTE_L,
	STRING_S,
	STRING_L,
	CONNECTION_S,
	CONNECTION_L,
	DNS_TO,
	CHAIN_TO
}TIMEOUT_TYPES;


typedef void * (* ProxyWorkerFUNC)(ProxyWorker *);

struct SRVPARAM {
	SRVPARAM *next;
	SRVPARAM *prev;
	ProxyWorker *child;
	ProxyWorkerSERVICE service;
	LOGFUNC logfunc;
	ProxyWorkerFUNC pf;
	SOCKET srvsock;
	int childcount;
	int maxchild;
	int version;
	int usentlm;
	int nouser;
	int silent;
	unsigned bufsize;
	unsigned logdumpsrv, logdumpcli;
	unsigned long intip;
	unsigned long extip;
	CRITICAL_SECTION counter_mutex;
	MYPOLLFD fds;
	FILE *stdlog;
	std::string target;
	MYPOLLFD * srvfds;
	std::string logtarget;
	std::string nonprintable;
	unsigned short intport;
	unsigned short extport;
	unsigned short targetport;
	char replace;
	time_t time_start;
public:
	SRVPARAM() {
		next = NULL;
		prev = NULL;
		child = NULL;
		service = S_NOSERVICE;
		logfunc = NULL;
		pf = NULL;
		srvsock = 0;
		childcount = 0;
		maxchild = 0;
		version = 0;
		usentlm = 0;
		nouser = 0;
		silent = 0;
		bufsize = 0;
		logdumpsrv = 0;
		logdumpcli = 0;
		intip = 0;
		extip = 0;
		InitializeCriticalSection(&counter_mutex);
		// fds
		stdlog = NULL;
		srvfds = NULL;
		intport = 0;
		extport = 0;
		targetport = 0;
		replace = 0;
		time_start = (time_t)0;
	}
	virtual ~SRVPARAM() {
		DeleteCriticalSection(&counter_mutex);
	}
};


struct ProxyWorker {
	ProxyWorker* next;
	ProxyWorker* prev;
	SRVPARAM *srv;
	REDIRECTFUNC redirectfunc;

	ProxyWorkerSERVICE service;

	SockPair sockpair;

	SOCKET ctrlsock;

	REDIRTYPE redirtype;

	int	redirected;

	int pwtype,
		threadid,
		weight,
		nolog;

	std::string hostname,
			username,
			password,
			extusername,
			extpassword;

	unsigned 	
			msec_start;

	struct sockaddr_in	req;

	unsigned long extip;

	unsigned short extport;

	time_t time_start;
public:
	ProxyWorker();

	// review how initialization of defaults is done here
	ProxyWorker(const ProxyWorker* other);

private:
	void connectToServer(const int operation);

public:
	// When this is called, requisite information must already be established
	// (e.g. client socket)
	bool handleIncomingRequest(
		std::string& requestNonConst,
		std::string& requestOriginalNonConst,
		unsigned& ckeepalive,
		size_t& prefix,
		bool& isconnect,
		bool& transparent,
		bool& redirect,
		const std::string lastRequest,
		const std::string lastRequestOriginal
	); 
	virtual ~ProxyWorker();
};

extern RESOLVFUNC resolvfunc;

extern int wday;
extern time_t basetime;

extern EXTPARAM conf;


extern FILE * stdlog;
void logstdout(ProxyWorker * proxy, const char *s);

int nbnameauth(ProxyWorker * proxy);
int alwaysauth(ProxyWorker * proxy);
int ipauth(ProxyWorker * proxy);
int doauth(ProxyWorker * proxy);
int strongauth(ProxyWorker * proxy);
void trafcountfunc(ProxyWorker *param);


int scanaddr(const char *s, unsigned long * ip, unsigned long * mask);
int myinet_ntoa(struct in_addr in, char * buf);
extern unsigned long nservers[MAXNSERVERS];
unsigned long getip(const char *name);
unsigned long myresolver(char *);
unsigned long fakeresolver (char *name);
int initdnshashtable(unsigned nhashsize);

int reload (void);
extern int paused;

char * mycrypt(const char *key, const char *salt, char *buf);
char * ntpwdhash (char *szHash, const char *szPassword, int tohex);

struct hashtable;
void hashadd(
	struct hashtable *ht, const char* name, unsigned long value, time_t expires
);

int parsehostname(const std::string hostname, ProxyWorker *param, unsigned short port);
int parseusername(const  std::string username, ProxyWorker *param, int extpasswd);
int parseconnusername(
	const std::string username, ProxyWorker *param, int extpasswd, unsigned short port
);

void freeconf(EXTPARAM *confp);

#define SERVICES 5

void * proxychild(ProxyWorker * proxy);

extern struct hashtable dns_table;

inline void debugInfoDetail(const char* message) {
#if DEBUGLEVEL > 2
	(*param->srv->logfunc)(param, message);
#endif
}

#endif
