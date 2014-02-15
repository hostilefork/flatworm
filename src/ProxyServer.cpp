//
// ProxyServer.cpp
//
// This is the workhorse of the proxying process.  The function proxychild
// gets the proxy object passed by the client, and then most of the work is
// done as methods on the ProxyWorker class.  The handleIncomingRequest is a
// monster of a monolithic function, but it's broken up into scopes and
// should be easy to modularize if-and-when that becomes necessary.
//

#include <memory>
#include <sstream>
#include <iostream>

#include "ProxyServer.h"
#include "parasock/Filter.h"
#include "parasock/DeadFilter.h"
#include "parasock/PassthruFilter.h"
#include "base64.h"

#include "RequestLineFilter.h"
#include "ClientHeaderFilter.h"
#include "ResponseLineFilter.h"
#include "ServerHeaderFilter.h"

#include "PcreDataFilter.h"
#include "FlvFilter.h"

int parsehostname(
	const std::string hostname,
	ProxyWorker *proxy,
	unsigned short port
) {
	size_t sp = std::string::npos;

	if(hostname.empty())
		return 1;
	sp = hostname.find(':');
	proxy->hostname = hostname.substr(
		0, sp != std::string::npos ? sp : std::string::npos
	);
	if (sp != std::string::npos) {
		port = atoi(hostname.c_str() + sp + 1);
	}
	proxy->req.sin_port=htons(port);
	proxy->req.sin_addr.s_addr = getip(proxy->hostname.c_str());
	proxy->sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_addr.s_addr = 0;
	proxy->sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_port = 0;
	return 0;
}


int parseusername(const std::string username, ProxyWorker *proxy, int extpasswd) {
	size_t sb = std::string::npos;
	size_t se = std::string::npos;
	size_t sp = std::string::npos;
	size_t sq = std::string::npos;
	
	if(username.empty())
		return 1;

	sb = username.find(':');
	if (sb != std::string::npos)
		se = username.find(':', sb + 1);
	if (se != std::string::npos)
		sp = username.find(':', se + 1);
	if (!proxy->srv->nouser
		&& (sb != std::string::npos)
		&& (se != std::string::npos)
		&& (!extpasswd || (sp != std::string::npos))
	) {
		proxy->username = username.substr(0, sb);
		if (sb != std::string::npos) {
			proxy->password = username.substr(
				sb+1, se != std::string::npos ? se : std::string::npos
			);
		}
		// default, no default for password?
		proxy->extusername = proxy->username; 
	}
	if (extpasswd) {
		proxy->extusername = username.substr(
			se+1, sp != std::string::npos ? sp : std::string::npos
		);
		if (sp != std::string::npos)
			proxy->extpassword = username.substr(sp + 1, std::string::npos);
	}
	return 0;
}


int parseconnusername(
	const std::string username,
	ProxyWorker *proxy,
	int extpasswd,
	unsigned short port){
	size_t sb = std::string::npos;
	size_t se = std::string::npos;
	if(username.empty())
		return 1;

	if ((sb=username.find(conf.delimchar)) != std::string::npos) {
		if (
			proxy->hostname.empty()
			&& (proxy->sockpair.sockbuf[SockPair::ServerConnection]->sock == INVALID_SOCKET)
		) {
			return 2;
		}
		return parseusername(username, proxy, extpasswd);
	}

	while ((se=username.find(conf.delimchar, sb + 1))) {
		sb=se;
	}

	if (parseusername(username.substr(0, sb), proxy, extpasswd)) {
		return 3;
	}

	if (parsehostname(username.substr(sb+1, std::string::npos), proxy, port)) {
		return 4;
	}

	return 0;
}


void logstdout(ProxyWorker * proxy, const char *s) {
	fprintf(stdout, "%s\n", s); 
}


int myinet_ntoa(struct in_addr in, char * buf) {
	unsigned u = ntohl(in.s_addr);
	return sprintf(
		buf, "%u.%u.%u.%u", 
		((u&0xFF000000)>>24), 
		((u&0x00FF0000)>>16),
		((u&0x0000FF00)>>8),
		((u&0x000000FF))
	);
}


char months[12][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


int scanaddr(const char *s, unsigned long * ip, unsigned long * mask) {
	unsigned d1, d2, d3, d4, m;
	int res = sscanf((char *)s, "%u.%u.%u.%u/%u", &d1, &d2, &d3, &d4, &m);
	if (res < 4) {
		return 0;
	}
	if (mask && res == 4) {
		*mask = 0xFFFFFFFF;
	} else if (mask) {
		*mask = htonl(0xFFFFFFFF << (32 - m));
	}
	*ip = htonl ((d1<<24) ^ (d2<<16) ^ (d3<<8) ^ d4);
	return res;
}


RESOLVFUNC resolvfunc = NULL;

unsigned long getip(const char *name){
	unsigned long retval;
	int i;
	int ndots = 0;
	struct hostent *hp = NULL;

	if (strlen(name) > 255)
		return 0; // used to truncate

	for (i = 0; name[i]; i++){
		if (name[i] == '.'){
			if (++ndots > 3) {
				break;
			}
			continue;
		}

		if ((name[i] < '0') || (name[i] >'9')) {
			break;
		}
	}
	if (!name[i] && (ndots == 3)) {
		unsigned long ip;
		if (scanaddr(name, &ip, NULL) == 4) {
			return ip;
		}
	}
	if (resolvfunc){
		retval = (*resolvfunc)(name);
		if (retval) {
			return retval;
		}
		if (conf.demanddialprog) {
			system(conf.demanddialprog);
		}
		return (*resolvfunc)(name);
	}
	hp = gethostbyname(name);
	if (!hp && conf.demanddialprog) {
		system(conf.demanddialprog);
		hp=gethostbyname(name);
	}
	retval = hp?*(unsigned long *)hp->h_addr:0;
	return retval;
}


void file2url(
	char *sb,
	char *buf,
	unsigned bufsize,
	unsigned int * inbuf,
	int skip255
) {
	for(; *sb; sb++) {
		if ((bufsize - *inbuf) < 16) {
			break;
		}

		if ((*sb=='\r') || (*sb=='\n')) {
			continue;
		}

		if (isallowed(*sb)) {
			buf[(*inbuf)++] = *sb;
		} else if (*sb == '\"') {
			memcpy(buf+*inbuf, "%5C%22", 6);
			(*inbuf) += 6;
		} else if (skip255 && (*sb == 255) && (*(sb+1) == 255)) {
			memcpy(buf + *inbuf, "%ff", 3);
			(*inbuf) += 3;
			sb++;
		} else {
			sprintf((char *)buf+*inbuf, "%%%.2x", (unsigned)*sb);
			(*inbuf) += 3;
		}
	}
}


void * proxychild(ProxyWorker* proxy) {
	// I wasn't sure which of these variables might affect the course of the
	// next loop.  Sort it out later
	std::string lastRequest;
	std::string lastRequestOriginal;
	unsigned ckeepalive = 0;
	size_t prefix = 0;
	bool isconnect = false;
	bool transparent = false;
	bool redirect = false;
	bool firstRequest = true;

	do {
		std::string request;
		std::string requestOriginal;
		bool success = proxy->handleIncomingRequest(
			request,
			requestOriginal,
			ckeepalive,
			prefix,
			isconnect,
			transparent,
			redirect,
			lastRequest,
			lastRequestOriginal
		);
		if (!success) {
			break;
		}

		lastRequest = request;
		lastRequestOriginal = requestOriginal;
		if (!firstRequest) {
			// first request has an implicit keepalive, if we bump ckeepalive
			// then we'll keep it open...
			ckeepalive--; 
		} else {
			firstRequest = false;
		}

		std::cout <<
			"REQUEST END shouldKeepAlive(" << ckeepalive << ")" <<
			" w/requestOriginal: " << 
			(lastRequestOriginal.empty() ? "(empty)" : lastRequestOriginal) << 
			"\r\n" <<
			" w/request: " <<
			(lastRequest.empty() ? "(empty)" : lastRequest) << 
			"\r\n";

	} while (ckeepalive);

	delete proxy;
	return NULL;
}



//
// ProxyWorker methods are run on its own thread.
//

ProxyWorker::ProxyWorker() {
	srv = NULL;
	redirectfunc = NULL;

	service = S_NOSERVICE;

	ctrlsock = 0;

	next = prev = NULL;

	redirtype = R_TCP;

	redirected = 0;

	pwtype = 0;
	threadid = 0;
	weight = 0;
	nolog = 0;

	msec_start = 0;

	memset(&req, 0, sizeof(sockaddr_in));

	extip = 0;

	extport = 0;

	time_start = (time_t)0;
}

ProxyWorker::ProxyWorker(const ProxyWorker* clientproxy) {
	this->srv = clientproxy->srv;
	this->redirectfunc = clientproxy->redirectfunc;

	this->service = clientproxy->service;

	this->ctrlsock = clientproxy->ctrlsock;

	this->next = clientproxy->next;
	this->prev = clientproxy->prev;

	this->redirtype = clientproxy->redirtype;

	this->redirected = clientproxy->redirected;

	this->pwtype = clientproxy->pwtype;
	this->threadid = clientproxy->threadid;
	this->weight = clientproxy->weight;
	this->nolog = clientproxy->nolog;

	this->msec_start = clientproxy->nolog;

	this->req = clientproxy->req;
	
	this->extip = clientproxy->extip;

	this->extport = clientproxy->extport;

	this->time_start = clientproxy->time_start;

	SockBuf* sockbufClient = new SockBuf();
	*sockbufClient = *clientproxy->sockpair.sockbuf[SockPair::ClientConnection];
	this->sockpair.sockbuf[SockPair::ClientConnection].reset(sockbufClient);

	SockBuf* sockbufServer = new SockBuf();
	*sockbufServer = *clientproxy->sockpair.sockbuf[SockPair::ServerConnection];
	this->sockpair.sockbuf[SockPair::ServerConnection].reset(sockbufServer);
}


void ProxyWorker::connectToServer(const int operation) {
	if (
		(sockpair.sockbuf[SockPair::ServerConnection]->sock == INVALID_SOCKET)
		&& (operation != DNSRESOLVE && operation != ADMIN)
	) {
		if (sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_addr.s_addr == 0) {
			if (req.sin_addr.s_addr == 0) {
				// req.sin_addr.s_addr should have been found by parsehost name
				throw &Proxyerror_Host_Not_Found;
			}
			sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_addr.s_addr =
				req.sin_addr.s_addr;
		}

		if(sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_port == 0)
			sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_port = req.sin_port;

		SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
			throw "Error opening socket to server";
		sockpair.sockbuf[SockPair::ServerConnection]->sock = sock;
	}

	if (extip == 0) {
		// we need to bind and optionally connect
		// ...unless the connection was kept alive

		struct linger lg;
		setsockopt(
			sockpair.sockbuf[SockPair::ServerConnection]->sock,
			SOL_SOCKET,
			SO_LINGER,
			reinterpret_cast<char *>(&lg),
			sizeof(lg)
		);

		struct sockaddr_in bindsa;
		memset(&bindsa, 0, sizeof(bindsa)); // ensures sin_zero ends up 0s
		bindsa.sin_family = AF_INET;
		if (
			(extport == 0)
			&& (srv->targetport == 0)
			&& (ntohs(sockpair.sockbuf[SockPair::ClientConnection]->sin.sin_port) > 1023)
		) {
			bindsa.sin_port = sockpair.sockbuf[SockPair::ClientConnection]->sin.sin_port;
		} else {
			bindsa.sin_port = extport;
		}
		bindsa.sin_addr.s_addr = extip;

		if (-1 == bind(
			sockpair.sockbuf[SockPair::ServerConnection]->sock,
			(struct sockaddr*)&bindsa,
			sizeof(bindsa)
		)) {
			int errorno = WSAGetLastError();
			bindsa.sin_port = 0; // try accepting any port?
			if(-1 == bind(
				sockpair.sockbuf[SockPair::ServerConnection]->sock,
				(struct sockaddr*)&bindsa,
				sizeof(bindsa)
			)) {
				errorno = WSAGetLastError();
				throw "Could not bind socket connection from client to server";
			}
		} else {
			Assert(extip == 0);
		}

		sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_family = AF_INET;
		if ((operation >= 256) || (operation & CONNECT)) {
			unsigned long ul;
			int res = connect(
				sockpair.sockbuf[SockPair::ServerConnection]->sock,
				(struct sockaddr *)&sockpair.sockbuf[SockPair::ServerConnection]->sin,
				sizeof(sockpair.sockbuf[SockPair::ServerConnection]->sin)
			);
			if (res != 0) {
				throw "Could not connect bound socket from client to server";
			}

			ioctlsocket(sockpair.sockbuf[SockPair::ServerConnection]->sock, FIONBIO, &ul);

			SASIZETYPE size = sizeof(sockpair.sockbuf[SockPair::ServerConnection]->sin);
			if (-1 == getsockname(
				sockpair.sockbuf[SockPair::ServerConnection]->sock,
				(struct sockaddr *)&bindsa, &size
			)) {
				throw "getsockname on CONNECT or operation >= 256 did not work";
			}
			extip = bindsa.sin_addr.s_addr;
		} else {
			SASIZETYPE size = sizeof(sockpair.sockbuf[SockPair::ServerConnection]->sin);
			if (-1 == getsockname(
				sockpair.sockbuf[SockPair::ServerConnection]->sock,
				(struct sockaddr *)&sockpair.sockbuf[SockPair::ServerConnection]->sin,
				&size
			)) {
				throw "getsockname did not work";
			}
		}

		if (
			(sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_addr.s_addr == srv->intip)
			&& (sockpair.sockbuf[SockPair::ServerConnection]->sin.sin_port == srv->intport)
		) {
			throw &Proxyerror_Recursion_Detected;
		}
		SASIZETYPE sasize = sizeof(struct sockaddr_in);
		if (0 != getpeername(
			sockpair.sockbuf[SockPair::ServerConnection]->sock,
			(struct sockaddr *)&sockpair.sockbuf[SockPair::ServerConnection]->sin,
			&sasize)
		) {
			throw "Get peer name returned non-zero (apparently not good...)";
		}
	}
}


bool ProxyWorker::handleIncomingRequest(
	std::string& requestNonConst,
	std::string& requestOriginalNonConst,
	unsigned& ckeepalive,
	size_t& prefix,
	bool& isconnect,
	bool& transparent,
	bool& redirect,
	const std::string lastRequest,
	const std::string lastRequestOriginal
) {

	try {
		
		// Read and filter the request
		RequestLineFilter requestFilter (
			sockpair,
			ClientToServer,
			ckeepalive,
			lastRequest,
			lastRequestOriginal,
			this
		);
		{
			// Note here I don't know what it means:
			//    "(?i)^\\s*Accept-encoding:*$", "Accept-Encoding:\n"

			size_t readSoFar[FlowDirectionMax];
			{
				FlowDirection whichZero;
				ForEachDirection(whichZero)
					readSoFar[whichZero] = 0;
			}

			DeadFilter deadServerFilter(sockpair, ServerToClient);
			Filter* filter[FlowDirectionMax] = {
				&requestFilter,
				&deadServerFilter
			};
			sockpair.doBidirectionalFilteredProxy(
				readSoFar,
				conf.timeouts[CONNECTION_L],
				filter
			);
		}

		// buffer and req are preserved across loops, for redirects?  hmm.
		int operation = requestFilter.operation;
		isconnect = requestFilter.isconnect;
		requestNonConst = requestFilter.request;
		requestOriginalNonConst = requestFilter.requestOriginal;
		transparent = requestFilter.transparent;
		prefix = requestFilter.prefix;
		redirect = requestFilter.redirect;

		if (requestNonConst.length() < 9) {
			throw "The request is too short";
		}
		if (!lastRequestOriginal.empty()) { 
			// Note: may not be correct semantics, 3Proxy also checked NULL
			std::string reqPrefix = lastRequestOriginal.substr(0, prefix);
			if (
				(requestOriginalNonConst.length() <= prefix)
				|| strncasecmplen(
					requestOriginalNonConst,
					lastRequestOriginal.c_str(),
					0,
					NULL
				)
			) {
				// can't serve requests for anything not on the same server
				// using this same socket!
				ckeepalive = 0; 
				sockpair.sockbuf[SockPair::ServerConnection].reset(new SockBuf);
				redirected = 0;
			} else if (ckeepalive && (sockpair.sockbuf[SockPair::ServerConnection].get() != NULL)) {
				MYPOLLFD fds;

				fds.fd = sockpair.sockbuf[SockPair::ServerConnection]->sock;
				fds.events = POLLIN;
				int resPoll = poll(&fds, 1, Timeout (0));
				if (resPoll < 0) {
					throw "Poll returned negative one.  Hmmm.";
				}

				if (resPoll > 0) {
					ckeepalive = 0;
					sockpair.sockbuf[SockPair::ServerConnection].reset(new SockBuf);
					redirected = 0;
				}
			}
		}	

		// Now read the client header; clientHeaderFilter
		ClientHeaderFilter clientHeaderFilter(
			sockpair,
			ClientToServer,
			/* ref */ requestOriginalNonConst,
			isconnect,
			transparent,
			ckeepalive,
			this
		);
		{
			size_t readSoFar[FlowDirectionMax];
			{
				FlowDirection whichZero;
				ForEachDirection(whichZero)
					readSoFar[whichZero] = 0;
			}

			DeadFilter deadServerFilter (sockpair, ServerToClient);
			Filter* filter[FlowDirectionMax] = {
				&clientHeaderFilter,
				&deadServerFilter
			};
			sockpair.doBidirectionalFilteredProxy(
				readSoFar,
				conf.timeouts[CONNECTION_L],
				filter
			);
		}

		/* BeginSockWatch(sockpair.sockbuf[SockPair::ClientConnection]->sock); */

		bool keepaliveClient = clientHeaderFilter.shouldKeepAlive();
	
		// with hostname/etc. encoded into it
		const std::string requestOriginal = requestOriginalNonConst;

		// adjusted so it makes sense to send to server
		const std::string request = requestNonConst;

		connectToServer(operation);
		
		// For non-HTTP connections, just copy the sockets to each other.
		// This means, of course, that you will not be able to run the
		// filters on HTTPS traffic.  That would require somehow
		// sabotaging the client's browser to accept bad certificates.
		// If that form of hackery interests you, see mitmproxy:
		//
		//     http://mitmproxy.org/doc/howmitmproxy.html
		// 
		if (isconnect && this->redirtype != R_HTTP) {

			// connect wasn't an instruction for the server.  it was an
			// instruction for us, the proxy...so don't pass it along.
			// consume everything.
			requestFilter.consume();
			clientHeaderFilter.consume();

			size_t readSoFar[FlowDirectionMax];
			{
				FlowDirection whichZero;
				ForEachDirection(whichZero)
					readSoFar[whichZero] = 0;
			}

			PassthruFilter passServerToClient (
				sockpair,
				ServerToClient,
				UNKNOWN,
				Proxyerror_Connection_Established.html
			);

			PassthruFilter passClientToServer (sockpair, ClientToServer, UNKNOWN);
			Filter* filter[FlowDirectionMax] = {
				&passClientToServer,
				&passServerToClient
			};
			sockpair.doBidirectionalFilteredProxy(
				readSoFar,
				conf.timeouts[CONNECTION_L],
				filter
			);

			sockpair.cleanCheckpoint();
			return true;
		}
		

		// Send the request to the server, with the modifications we've made
		{ 
			if(requestOriginal.empty() || (redirtype != R_HTTP)) {
				// BUGBUG: We only filtered requestOriginal.  (?)
				sockpair.sockbuf[SockPair::ServerConnection]->fulfillPlaceholder(
					requestFilter.placeholder,
					request
				);
			} else {
				redirect = true;
				sockpair.sockbuf[SockPair::ServerConnection]->fulfillPlaceholder(
					requestFilter.placeholder,
					requestOriginal
				);
			}
			size_t bytesSent = sockpair.doUnidirectionalProxy(
				ServerToClient,
				conf.timeouts[STRING_L]
			);
		}

		// Tack on a few things to the client header before sending
		{ 
			std::ostringstream outputBuf;
			if (keepaliveClient) {
				if (redirect) {
					outputBuf << "Proxy-Connection";
				} else {
					outputBuf << "Connection";
				}
				outputBuf << ": Keep-Alive\r\n";
			}

			if (!extusername.empty()) {
				if (redirect) {
					outputBuf << "Proxy-Authorization";
				} else {
					outputBuf << "Authorization";
				}
				outputBuf << ": basic ";
				std::string username = extusername + ":" + extpassword;
				outputBuf << en64(username.c_str(), username.length());
				outputBuf << "\r\n";
			}

			// Don't accept any encodings.
			outputBuf << "Accept-Encoding:\r\n";

			clientHeaderFilter.getHeaderString() += outputBuf.str();
		}

		// Connect requests are just fed along, with the client and server 
		// copying data to each other.
		{ 
			if(isconnect) {
				// Transfer the header received from the client to the server,
				// since it's not still in the buffer.

				// REVIEW: If we left it in the buffer, this would be automatic
				// in the MapWithoutFiltering

				clientHeaderFilter.fullfillHeaderString();
				size_t bytesSent = sockpair.doUnidirectionalProxy(
					ServerToClient,
					conf.timeouts[STRING_S]
				);

				size_t readSoFar[FlowDirectionMax];
				{
					FlowDirection whichZero;
					ForEachDirection(whichZero)
						readSoFar[whichZero] = 0;
				}

				PassthruFilter passServer(sockpair, ServerToClient, UNKNOWN);
				PassthruFilter passClient(sockpair, ClientToServer, UNKNOWN);
				Filter* filter[FlowDirectionMax] = { &passClient, &passServer };
				sockpair.doBidirectionalFilteredProxy(
					readSoFar,
					conf.timeouts[CONNECTION_L],
					filter
				);

				sockpair.sockbuf[SockPair::ClientConnection]->cleanCheckpoint();
				sockpair.sockbuf[SockPair::ServerConnection]->cleanCheckpoint();
				return true;
			}
		}

		bool clientChunked = false;
		PcreDataFilter clientDataFilter (
			sockpair,
			ClientToServer,
			clientHeaderFilter,
			"the",
			"Flatworm"
		);

		// Fix up content length and send client's header to server
		{ 
			if(clientDataFilter.getContentLengthFiltered().isKnown()) {
				// Note: We do not have to do it this way.  We can set the
				// transfer encoding mode to chunked in HTTP1.1 and above.
				// We can also strip the length if we so choose.

				DeadFilter deadServerFilter (sockpair, ServerToClient);

				// NOTE: until we change to a chunked mode or something
				// that doesn't require reading all the client data, the
				// statements above have already sent the data...
				// There is no more!
				Filter* filterData[FlowDirectionMax];
				filterData[SockPair::ServerConnection] = &deadServerFilter;
				filterData[SockPair::ClientConnection] = &clientDataFilter;

				size_t readSoFar[FlowDirectionMax];
				FlowDirection which;
				ForEachDirection(which)
					readSoFar[which] = 0;

				// only read the client data here if we didn't already, and
				// won't read server data if we're chunking
				sockpair.doBidirectionalFilteredProxy(
					readSoFar,
					conf.timeouts[CONNECTION_S],
					filterData
				);
				clientHeaderFilter.fulfillContentLength(
					clientDataFilter.getContentLengthFiltered().getKnownValue(),
					false
				);
			} else {
				clientHeaderFilter.fulfillContentLength(
					UNKNOWN,
					clientHeaderFilter.getChunkedUnfiltered()
				);
			}
		}

		// Send the client request, with or without a content length...
		{ 
			clientHeaderFilter.fullfillHeaderString();
			size_t bytesSent = sockpair.doUnidirectionalProxy(
				ServerToClient,
				conf.timeouts[STRING_S]
			);
		}

		ResponseLineFilter responseFilter (sockpair, ServerToClient);

		// We want to read the HTTP response, it's just one line.
		// Followed by key/value pairs
		{
			size_t readSoFar[FlowDirectionMax];
			{
				FlowDirection whichZero;
				ForEachDirection(whichZero)
					readSoFar[whichZero] = 0;
			}

			// we actually want to kick in the client data filter here...
			DeadFilter deadClientFilter (sockpair, ClientToServer);
			Filter* filter[FlowDirectionMax] = {
				&deadClientFilter,
				&responseFilter
			};
			sockpair.doBidirectionalFilteredProxy(
				readSoFar,
				conf.timeouts[CONNECTION_L],
				filter
			);
		}

		// okay now we have the key value pairs coming up...
		ServerHeaderFilter serverHeaderFilter (
			sockpair,
			ServerToClient,
			isconnect,
			redirect
		);
		{
			size_t readSoFar[FlowDirectionMax];
			{
				FlowDirection whichZero;
				ForEachDirection(whichZero)
					readSoFar[whichZero] = 0;
			}

			// we actually want to kick in the client data filter here...
			DeadFilter deadClientFilter (sockpair, ClientToServer);
			Filter* filter[FlowDirectionMax] = {
				&deadClientFilter,
				&serverHeaderFilter
			}; 
			sockpair.doBidirectionalFilteredProxy(
				readSoFar,
				conf.timeouts[CONNECTION_L],
				filter);
		}

		/* EndSockWatch(sockpair.sockbuf[SockPair::ClientConnection]->sock); */

		int httpStatusCode = responseFilter.httpStatusCode;
	 	bool authenticate = serverHeaderFilter.authenticate;
		bool keepaliveServer = serverHeaderFilter.shouldKeepAlive();

		PcreDataFilter serverDataFilter(
			sockpair,
			ServerToClient,
			serverHeaderFilter,
			"the",
			"Flatworm"
		);

		if ((httpStatusCode < 200) || (httpStatusCode > 499)) {
			ckeepalive = 0;
		} else if (
			serverDataFilter.getContentLengthFiltered().isUnknown()
			|| clientDataFilter.getContentLengthFiltered().isUnknown()
		) {
			// we have to close the connection if we don't know how long...
			// but... this could be tweaked with chunking, I think!
			ckeepalive = 0; 
		} else {
			ckeepalive += (keepaliveClient || keepaliveServer) ? 1 : 0;
		}

		// If we know the content lengths, go ahead and read the data from 
		// the server and the client
		{ 
			if (
				(operation != HTTP_HEAD)
				&& (httpStatusCode != 204)
				&& (httpStatusCode != 304)
			) {
				size_t readSoFar[FlowDirectionMax];
				{
					FlowDirection whichZero;
					ForEachDirection(whichZero)
						readSoFar[whichZero] = 0;
				}

				DeadFilter deadServerFilter (sockpair, ServerToClient);
				DeadFilter deadClientFilter (sockpair, ClientToServer);

				// NOTE: until we change to a chunked mode or something that
				// doesn't require reading all the client data, the statements
				// above have already sent the data-- there is no more!
				Filter* filterData[FlowDirectionMax];
				if (serverDataFilter.getContentLengthFiltered().isKnown()) {
					filterData[SockPair::ServerConnection] = &serverDataFilter;
				} else {
					filterData[SockPair::ServerConnection] = &deadServerFilter;
				}

				// Server wouldn't respond until we sent the client header.
				// So we had to read client data earlier if we intended to fill
				// in the size, but if we didn't do that then now we attach
				// the client filter
				if (clientDataFilter.getContentLengthFiltered().isUnknown()) {
					// can't do this until we have filter pre-empting of 
					// some kind.  will wait indefinitely on a keep alive conn.
					/* filterData[SockPair::ClientConnection] = &clientDataFilter; */

					filterData[SockPair::ClientConnection] = &deadClientFilter;
				} else {
					filterData[SockPair::ClientConnection] = &deadClientFilter;
				}

				// only read the client data here if we didn't already.
				// Don't do chunking!
				sockpair.doBidirectionalFilteredProxy(
					readSoFar,
					conf.timeouts[CONNECTION_S],
					filterData
				);

				if (serverDataFilter.getContentLengthFiltered().isKnown()) {
					Assert(
						readSoFar[SockPair::ServerConnection]
						== serverDataFilter.getContentLengthFiltered().getKnownValue()
					);
				}
			}
		}

		// Touch up server headers after filtering, before passing on to client
		{	
			if (serverDataFilter.getContentLengthFiltered().isKnown()) {
				serverHeaderFilter.fulfillContentLength(
					serverDataFilter.getContentLengthFiltered().getKnownValue(),
					false
				);
			} else {
				serverHeaderFilter.fulfillContentLength(
					UNKNOWN,
					serverDataFilter.getChunkedFiltered()
				);
			}

			std::ostringstream bufStream;
			if (authenticate && !transparent) {
				bufStream << "Proxy-support: Session-Based-Authentication\r\n"
					<< "Connection: Proxy-support\r\n";
			}
			if (transparent) {
				bufStream << "Connection";
			} else {
				bufStream << "Proxy-Connection";
			}
			bufStream << ": ";
			if (
				serverDataFilter.getContentLengthFiltered().isKnown()
				&& keepaliveServer
			) {
				bufStream << "Keep-Alive";
			} else {
				bufStream << "Close";
			}
			bufStream << "\r\n";
			serverHeaderFilter.getHeaderString() += bufStream.str();
		}

		// Transmit header received from server to client socket.
		{
			serverHeaderFilter.fullfillHeaderString();
		}

		// Handle 204, 304, and HEAD.
		//
		// 204 is No Content:
		//
		// "The server has fulfilled the request but does not need to return 
		// an entity-body, and might want to return updated metainformation. 
		// The response MAY include new or updated metainformation in the
		// form of entity-headers, which if present SHOULD be associated with
		// the requested variant.
		//
		// If the client is a user agent, it SHOULD NOT change its document
		// view from that which caused the request to be sent. This response
		// is primarily intended to allow input for actions to take place
		// without causing a change to the user agent's active document view,
		// although any new or updated metainformation SHOULD be applied to
		// the document currently in the user agent's active view.
		// 
		// The 204 response MUST NOT include a message-body, and thus is
		// always terminated by the first empty line after the header fields."
		//
		// 304 is Not Modified:
		//
		// "If the client has performed a conditional GET request and access
		// is allowed, but the document has not been modified, the server
		// SHOULD respond with this status code. The 304 response MUST NOT
		// contain a message-body, and thus is always terminated by the first
		// empty line after the header fields."
		//
		// HEAD is defined thusly:
		// 
		// "The HEAD method is identical to GET except that the server MUST
		// NOT return a message-body in the response. The metainformation
		// contained in the HTTP headers in response to a HEAD request SHOULD
		// be identical to the information sent in response to a GET request.
		// This method can be used for obtaining metainformation about the
		// entity implied by the request without transferring the entity-body
		// itself. This method is often used for testing hypertext links for
		// validity, accessibility, and recent modification."

		if (
			(httpStatusCode == 204)
			|| (httpStatusCode == 304)
			|| (operation == HTTP_HEAD)
		) {
			size_t readSoFar[FlowDirectionMax];
			{
				FlowDirection whichZero;
				ForEachDirection(whichZero)
					readSoFar[whichZero] = 0;
			}

			// no more to read, don't try.  a kept alive connection would then
			// block indefinitely!
			DeadFilter deadServerFilter (sockpair, ServerToClient);

			// I thought GET could have client data.  But on a keep alive
			// connection, I got subsequent GETs with naught but a line feed
			/* PassthruFilter passClient (sockpair, ClientToServer, UNKNOWN); */ 
			DeadFilter deadClientFilter (sockpair, ClientToServer);

			Filter* filter[FlowDirectionMax] = {
				/* passClient */ &deadClientFilter,
				&deadServerFilter
			};
			sockpair.doBidirectionalFilteredProxy(
				readSoFar,
				conf.timeouts[CONNECTION_L],
				filter
			);

			sockpair.cleanCheckpoint();
			return true;
		}

		// Now if we have chunking to take care of we will
		{
			if (!serverDataFilter.getContentLengthFiltered().isKnown()) {
				size_t readSoFar[FlowDirectionMax];
				{
					FlowDirection whichZero;
					ForEachDirection(whichZero)
						readSoFar[whichZero] = 0;
				}

				DeadFilter deadClientFilter (sockpair, ClientToServer);
				Filter* filter[FlowDirectionMax];
				// we actually want to kick in the client data filter here...
				filter[SockPair::ServerConnection] = &serverDataFilter;
				filter[SockPair::ClientConnection] = &deadClientFilter;
				sockpair.doBidirectionalFilteredProxy(
					readSoFar,
					conf.timeouts[CONNECTION_L],
					filter
				);
			}
		}

		// only necessary if we filled a placeholder and didn't map and filter?
		{
			size_t bytesSentClient = sockpair.doUnidirectionalProxy(
				ClientToServer,
				conf.timeouts[STRING_S]
			);  
			size_t bytesSentServer = sockpair.doUnidirectionalProxy(
				ServerToClient,
				conf.timeouts[STRING_S]
			);
		}

		sockpair.cleanCheckpoint();

	} catch (const char * str) {
		// string error.  improve feedback, wrap as a bug report?
		// "Click here to report bug"

		/* EndSockWatch(sockpair.sockbuf[SockPair::ClientConnection]->sock); */

		sockpair.sockbuf[SockPair::ClientConnection]->failureShutdown(str, conf.timeouts[STRING_S]);
		std::cout << "Exception thrown during [" << requestOriginalNonConst
			<< ": " << str << "\n";
		return false;

	} catch (const ProxyWorkerERROR * proxyerror) { // bigger error page

		// Handling similar to this was in the original 3Proxy.
		// Not sure what it was for (??)
		/* if (sockpair.sockbuf[SockPair::ClientConnection].get() != NULL) {
			if ((this->res>=509 && this->res < 517) || (this->res > 900)) {
				int vvv;
				do {
					vvv = this->sockpair.sockbuf[SockPair::ClientConnection]->GetLineUnfiltered(
						buffer,
						BUFSIZE - 1,
						'\n',
						conf.timeouts[STRING_S]
					);
				} while (vvv >2);
			}
		}*/

		sockpair.sockbuf[SockPair::ClientConnection]->failureShutdown(
			proxyerror->html,
			conf.timeouts[STRING_S]
		);
		return false;
	}

	return true;
}


ProxyWorker::~ProxyWorker() {
	// We used to do this inside the handler when ckeepalive is 0, 
	// but it's actually sensible here.
	//
	// You kill a ProxyWorker when the connection dies I think; which
	// further makes it seem like it may be the right class to consider
	// "the" proxying object...

	if (this->sockpair.sockbuf[SockPair::ServerConnection]->sock != INVALID_SOCKET) {
		this->sockpair.sockbuf[SockPair::ServerConnection].reset();
	}

	if (this->srv) {
		pthread_mutex_lock(&this->srv->counter_mutex);
		if (this->prev) {
			this->prev->next = this->next;
		} else {
			this->srv->child = this->next;
		}
		if (this->next) {
			this->next->prev = this->prev;
		}
		(this->srv->childcount)--;
		pthread_mutex_unlock(&this->srv->counter_mutex);
	}

	if (
		(this->ctrlsock != INVALID_SOCKET)
		&& (this->sockpair.sockbuf[SockPair::ClientConnection].get() != NULL)
		&& (this->ctrlsock != this->sockpair.sockbuf[SockPair::ClientConnection]->sock)
	) {
		shutdown(this->ctrlsock, SHUT_RDWR);
		closesocket(this->ctrlsock);
	}
}
