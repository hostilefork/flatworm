// 
// Parasock.h
//
// Abstraction to represent a pair of sockets used by the proxy
// (a "pair of SOCKs", if you will :-P).
//
// This abstraction is central to the idea of mediating the connection
// and rewriting the data between them, with the attachment of filters.
//

#ifndef __PARASOCK_PARASOCK_H__
#define __PARASOCK_PARASOCK_H__

#include "SockBuf.h"

class Filter;

enum FlowDirection {
	ClientToServer,
	ServerToClient,
	FlowDirectionMax
};
#define ForEachDirection(which) \
	for (which = ClientToServer; \
		which != FlowDirectionMax; \
		which = (which == ClientToServer) ? ServerToClient : FlowDirectionMax)
inline FlowDirection OtherDirection(FlowDirection which) {
	return (which == ClientToServer) ? ServerToClient : ClientToServer;
}


class Parasock {

	friend class Filter;

public:
	enum WhichConnection {
		ClientConnection = 0,
		ServerConnection = 1,
		WhichConnectionMax
	};

public: // Need to work on this to make it private, 3Proxy startup is wily
	std::auto_ptr<SockBuf> sockbuf[WhichConnectionMax];
	Parasock () {
	}

private:
	// Disable copying C++98 style
	Parasock (Parasock const & other);

private:
	void filterHelper(
		FlowDirection which,
		size_t offsetAmount,
		size_t readSoFar,
		Filter & filter,
		bool disconnected
	);

private:
	// sends FILTERED data from OtherDirection(which) to which
	size_t doUnidirectionalProxyCore(
		FlowDirection which,
		bool & timedOut,
		bool & socketClosed,
		Timeout timeout
	); 

public:
	size_t doUnidirectionalProxy(FlowDirection which, Timeout timeout) {
		bool timedOut;
		bool socketClosed;
		size_t bytesSent = doUnidirectionalProxyCore(
			which,
			timedOut,
			socketClosed,
			timeout
		);
		return bytesSent;
	}

private:
	void doBidirectionalFilteredProxyCore(
		size_t (&readSoFar)[FlowDirectionMax],
		size_t (&sentSoFar)[FlowDirectionMax],
		bool (&socketClosed)[FlowDirectionMax],
		bool (&readAZero)[FlowDirectionMax],
		bool & timedOut,
		Timeout timeout,
		Filter* (&filter)[FlowDirectionMax]
	);
public:
	void doBidirectionalFilteredProxyEx(
		size_t (&readSoFar)[FlowDirectionMax],
		Timeout timeout,
		Filter * (&filter)[FlowDirectionMax]
	);

	void doBidirectionalFilteredProxy(
		size_t (&readSoFar)[FlowDirectionMax],
		Timeout timeout,
		Filter * (&filter)[FlowDirectionMax]
	) {
		return doBidirectionalFilteredProxyEx(readSoFar, timeout, filter);
	}

	void cleanCheckpoint() {
		sockbuf[ClientConnection]->cleanCheckpoint();
		sockbuf[ServerConnection]->cleanCheckpoint();
	}

	void failureShutdown(std::string const message, Timeout timeout) {
		sockbuf[ClientConnection]->failureShutdown(message, timeout);
	}
};

#endif

