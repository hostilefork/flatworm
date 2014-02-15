// 
// SockPair.h
//
// Socket proxying abstraction.
//

#ifndef __SOCKPAIR_H__
#define __SOCKPAIR_H__

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


class SockPair {

	friend class Filter;

public:
	enum WhichConnection {
		ClientConnection = 0,
		ServerConnection = 1
	};

public: // Need to work on this to make it private, 3Proxy startup is wily
	std::auto_ptr<SockBuf> sockbuf[2];
	SockPair () {
	}

private:
	// Disable copying C++98 style
	SockPair (SockPair const & other);

private:
	void filterHelper(
		const FlowDirection which,
		const size_t offsetAmount,
		const size_t readSoFar,
		Filter& filter,
		bool disconnected
	);

private:
	// sends FILTERED data from OtherDirection(which) to which
	size_t doUnidirectionalProxyCore(
		const FlowDirection which,
		bool & timedOut,
		bool & socketClosed,
		const Timeout timeout
	); 

public:
	size_t doUnidirectionalProxy(const FlowDirection which, const Timeout timeout) {
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
		bool (&socketClosed)[2],
		bool (&readAZero)[2],
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

	void failureShutdown(const std::string message, const Timeout timeout) {
		sockbuf[ClientConnection]->failureShutdown(message, timeout);
	}
};

#endif

