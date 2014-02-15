//
// Main.cpp
//
// Mostly unmodified code from 3Proxy's proxymain file, except for formatting.
//
// Although there weren't comments and it was flat C, it wasn't the part
// I was interested in looking at... and it worked well enough.  So I left it!
//

#include "ProxyServer.h"
#include "parasock/Filter.h"

#include "DataFilter.h"

EXTPARAM conf;

int main(int argc, char** argv) {

	SOCKET sock = INVALID_SOCKET;
	int i = 0;
	SASIZETYPE size;
	pthread_t thread;
	ProxyWorker defparam;
	SRVPARAM srv;
	ProxyWorker * newparam;
	int error = 0;
	unsigned sleeptime;
	char buf[256];
	char *hostname = NULL;
	int opt = 1;
	int isudp = 1;
	FILE *fp = NULL;
	int nlog = 5000;

	char loghelp[] =
	" -u never ask for username\n"
	" -fFORMAT logging format (see documentation)\n"
	" -l log to stderr\n"
	" -lFILENAME log to FILENAME\n"
	" -bBUFSIZE size of network buffer (default 4096 for TCP, 16384 for UDP)\n"
	" -t be silent (do not log service start/stop)\n"
	" -iIP ip address or internal interface (clients are expected to connect)\n"
	" -eIP ip address or external interface (outgoing connection will have this)\n";

	unsigned long ul;

	struct linger lg;
	HANDLE h;
	WSADATA wd;
	WSAStartup(MAKEWORD( 1, 1 ), &wd);

	srv.silent = 0;
	srv.logfunc = &logstdout;
	srv.version = conf.paused;
	srv.usentlm = 1;
	srv.maxchild = conf.maxchild;
	srv.time_start = time(NULL);
	srv.logtarget = conf.logtarget;
	srv.srvsock = INVALID_SOCKET;
	srv.logdumpsrv = conf.logdumpsrv;
	srv.logdumpcli = conf.logdumpcli;
	defparam.srv = &srv;
	defparam.sockpair.sockbuf[SockPair::ServerConnection].reset(new SockBuf());
	defparam.sockpair.sockbuf[SockPair::ClientConnection].reset(new SockBuf());
	defparam.ctrlsock = INVALID_SOCKET;
	defparam.req.sin_family = AF_INET;
	defparam.service = S_PROXY;

	srv.pf = proxychild;
	isudp = false;
	srv.service = S_PROXY;

	srv.nouser = 1;

	for (i = 1; i < argc; i++) {
		if (*argv[i] == '-') {
			switch(argv[i][1]) {
			case 'l':
				srv.logfunc = logstdout;
				srv.logtarget = (char*)argv[i] + 2;
				if (argv[i][2]) {
					if (argv[i][2]=='@') {
						// ???
						// There was no code here.
					}
					else {
						fp = fopen(argv[i] + 2, "a");
						if (fp) {
							srv.stdlog = fp;
							fseek(fp, 0L, SEEK_END);
						}
					}

				}
				break;
			case 'i':
				srv.intip = getip((char *)argv[i]+2);
				break;
			case 'e':
				srv.extip = getip((char *)argv[i]+2);
				break;
			case 'p':
				srv.intport = htons(atoi(argv[i]+2));
				break;
			case 'b':
				srv.bufsize = atoi(argv[i]+2);
				break;
			case 'n':
				srv.usentlm = 0;
				break;
			case 't':
				srv.silent = 1;
				break;
			case 'h':
				hostname = argv[i] + 2;
				break;
			case 'u':
				srv.nouser = 1;
				break;
			default:
				error = 1;
				break;
			}
		} else {
			break;
		}
	}

	if (error || (i != argc)) {
		conf.threadinit = 0;
		fprintf(stderr, "%s\n"
			"Usage: %s options\n"
			"Available options are:\n"
			"%s"
			" -pPORT - service port to accept connections\n"
			"\tExample: %s -i127.0.0.1\n\n",
			argv[0], argv[0], loghelp, argv[0]
		);

		return (1);
	}


	if(!srv.logtarget.empty())
		srv.logtarget = srv.logtarget;

	if(!srv.intip) {
		srv.intip = conf.intip;
	}

	defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin.sin_addr.s_addr = srv.intip;
	defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin.sin_port = srv.intport;
	if (!srv.extip) {
		srv.extip = conf.extip;
	}

	defparam.extip = srv.extip;
	defparam.sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_addr.s_addr = srv.extip;
	if (!srv.extport) {
		srv.extport = htons(conf.extport);
	}
	defparam.extport = srv.extport;
	defparam.sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_port = srv.extport;

	if (!srv.intport) {
		srv.intport = htons(3128);
	}
	if (!defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin.sin_port) {
		defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin.sin_port = htons(3128);
	}
	if (hostname) {
		parsehostname(hostname, &defparam, 3128);
	}

	conf.threadinit = 0;

	if(srv.srvsock == INVALID_SOCKET) {
		if (!isudp) {
			lg.l_onoff = 1;
			lg.l_linger = conf.timeouts[STRING_L].getSeconds();
			sock=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		} else {
			sock=socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
		}

		if (sock == INVALID_SOCKET) {
			perror("socket()");
			return -2;
		}

		ioctlsocket(sock, FIONBIO, &ul);
		srv.srvsock = sock;
		if (setsockopt(
			sock,
			SOL_SOCKET,
			SO_REUSEADDR,
			(char *)&opt,
			sizeof(int)
		)) {
			perror("setsockopt()");
		}

#ifdef SO_REUSEPORT
		setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(int));
#endif
	}

	size = sizeof(defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin);

	sleeptime = SLEEPTIME * 100;
	for (;;) {
		if (-1 != bind(
			sock,
			(struct sockaddr*)&defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin,
			size
		)) {
			break;
		}

		int errorno = WSAGetLastError();
		sprintf((char *)buf, "bind(): %s", strerror(errorno));

		if (!srv.silent) {
			(*srv.logfunc)(&defparam, buf);
		}

		sleeptime = (sleeptime << 1);	
	
		if(!sleeptime) {
			closesocket(sock);
			return -3;
		}

		usleep(sleeptime);
	}

	if (!isudp) {
		if (listen(sock, 1 + (srv.maxchild >> 4)) == -1) {
			int errorno = WSAGetLastError();
			sprintf((char *)buf, "listen(): %s", strerror(errorno));
			if (!srv.silent) {
				(*srv.logfunc)(&defparam, buf);
			}
			return -4;
		}
	} else { 
		defparam.sockpair.sockbuf[SockPair::ClientConnection]->sock = sock;
	}

	if (!srv.silent) {
		sprintf(
			(char *)buf,
			"Accepting connections [%u/%u]",
			(unsigned)getpid(),
			(unsigned)pthread_self()
		);
		(*srv.logfunc)(&defparam, buf);
	}

	defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin.sin_addr.s_addr = 0;
	defparam.sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_addr.s_addr = 0;

	defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin.sin_port = 0;
	defparam.sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_port = 0;

	srv.fds.fd = sock;
	srv.fds.events = POLLIN;


	for (;;) {
		for (;;) {
			while(
				(conf.paused == srv.version)
				&& (srv.childcount >= srv.maxchild)
			) {
				nlog++;			
				if(nlog > 5000) {
					sprintf(
						(char *)buf,
						"Warning: too many connected clients (%d/%d)",
						srv.childcount,
						srv.maxchild
					);
					if (!srv.silent) {
						(*srv.logfunc)(&defparam, buf);
					}
					nlog = 0;
				}
				usleep(SLEEPTIME);
			}
			if (conf.paused != srv.version) {
				break;
			}
			if (srv.fds.events & POLLIN) {
				error = poll(&srv.fds, 1, Timeout (1));
			}
			else {
				usleep(SLEEPTIME);
				continue;
			}
			if (error >= 1) {
				break;
			}
			if (error == 0) {
				continue;
			}

			int errorno = WSAGetLastError();
			if ((errorno != EAGAIN) && (errorno != EINTR)) {
				sprintf(
					(char *)buf,
					"poll(): %s/%d",
					strerror(errorno),
					errorno
				);
				if (!srv.silent) {
					(*srv.logfunc)(&defparam, buf);
				}
				break;
			}
			continue;
		}

		if ((conf.paused != srv.version) || (error < 0)) {
			break;
		}

		SOCKET new_sock = INVALID_SOCKET;
		if(!isudp) {
			size = sizeof(defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin);
			new_sock = accept(
				sock,
				(struct sockaddr*)&defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin,
				&size
			);
			if(new_sock == INVALID_SOCKET) {
				int errorno = WSAGetLastError();
				sprintf((char *)buf, "accept(): %s", strerror(errorno));
				if (!srv.silent) {
					(*srv.logfunc)(&defparam, buf);
				}
				continue;
			}
			ioctlsocket(new_sock, FIONBIO, &ul);
			setsockopt(
				new_sock,
				SOL_SOCKET,
				SO_LINGER,
				(char *)&lg,
				sizeof(lg)
			);
			setsockopt(
				new_sock,
				SOL_SOCKET,
				SO_OOBINLINE,
				(char *)&opt,
				sizeof(int)
			);
		} else { 
			srv.fds.events = 0;
		}
		newparam = new ProxyWorker(&defparam);
		if (!defparam.hostname.empty()) {
			newparam->hostname= defparam.hostname;
		}
		if (!isudp) {
			newparam->sockpair.sockbuf[SockPair::ClientConnection]->sock = new_sock;
		}

		newparam->prev = newparam->next = NULL;
		pthread_mutex_lock(&srv.counter_mutex);
		if (!srv.child){
			srv.child = newparam;
		} else {
			newparam->next = srv.child;
			srv.child = srv.child->prev = newparam;
		}

		h = (HANDLE)_beginthreadex(
			(LPSECURITY_ATTRIBUTES)NULL,
			(unsigned)16384,
			(BEGINTHREADFUNC)srv.pf,
			(void *)newparam,
			0,
			&thread
		);
		newparam->threadid = (unsigned)thread;
		if (h) {
			srv.childcount++;
			CloseHandle(h);
		}
		else {
			delete(newparam);
		}
		pthread_mutex_unlock(&srv.counter_mutex);
		memset(
			&defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin,
			0,
			sizeof(defparam.sockpair.sockbuf[SockPair::ClientConnection]->sin)
		);
		if(isudp) {
			while(!srv.fds.events)usleep(SLEEPTIME);
		}
	}

	if (!srv.silent) {
		srv.logfunc(&defparam, "Exiting thread");
	}

	if (fp) {
		fclose(fp);
	}

	if (srv.srvsock != INVALID_SOCKET) {
		closesocket(srv.srvsock);
	}

	srv.srvsock = INVALID_SOCKET;
	srv.service = S_ZOMBIE;
	while(srv.child) {
 		usleep(SLEEPTIME * 100);
 	}

	return 0;
}


void freeconf(EXTPARAM *confp){

	confp->intip = confp->extip = 0;
	confp->intport = confp->extport = 0;
	confp->maxchild = 100;
	resolvfunc = NULL;
	confp->logtime = confp->time = 0;

	usleep(SLEEPTIME);
}