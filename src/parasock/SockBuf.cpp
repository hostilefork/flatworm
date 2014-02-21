
//
// SockBuf.cpp
//
// Buffered socket abstraction.
//

#include "SockBuf.h"


void SockBuf::shutdownAndClose() {
	disconnected = true;

#ifdef SOCKWATCH
	std::deque<SOCKET>::iterator it = std::find(
		socketwatches.begin(), socketwatches.end(), sock
	);
	if (it != socketwatches.end())
		DebugBreak();
#endif

	printf("SHUT DOWN SOCKET (%d)\n", sock);
	shutdown(sock, SHUT_RDWR);
	closesocket(sock);
	sock = INVALID_SOCKET;
}


void SockBuf::failureShutdown(std::string const message, Timeout timeout) {
	// failure, possibly send a message, don't trigger assertions

	if (this->sock != INVALID_SOCKET) {
		if (!message.empty()) {
			// is same as send 
			socksendto(
				this->sock,
				&this->sin,
				message.c_str(),
				static_cast<int>(message.length()),
				timeout
			);
		}
		shutdownAndClose();
	}
}


void SockBuf::cleanCheckpoint() {
	// check for integrity if no one dropped the connection

	Assert(uncommittedBytes.empty());
	Assert(placeholders.empty());

	// REVIEW: Why is this commented out?
	/* Assert(unfilteredBytes.empty()); */
}


SockBuf::SockBuf() {
	this->sock = INVALID_SOCKET;
	memset(&this->sin, 0, sizeof(sockaddr_in));
	  
	this->sin.sin_family = AF_INET;
	disconnected = false;
}


SockBuf::~SockBuf() {
	if (this->sock != INVALID_SOCKET) {
		shutdownAndClose();
	}
	this->sin.sin_addr.s_addr = 0;
	this->sin.sin_port = 0;
	this->sock = INVALID_SOCKET;
}
