// 
// SockPair.h
//
// Socket proxying abstraction.
//

#ifndef __SOCKPAIR_H__
#define __SOCKPAIR_H__

#include "SockBuf.h"

class Filter;

typedef enum {
	CLIENT,
	SERVER,
	DIRECTION_MAX
} DIRECTION;
#define ForEachDirection(which) \
	for (which = CLIENT; \
		which != DIRECTION_MAX; \
		which = (which == CLIENT) ? SERVER : DIRECTION_MAX)
inline DIRECTION OtherDirection(DIRECTION which) {
	return (which == CLIENT) ? SERVER : CLIENT;
}


class SockPair {

	friend class Filter;

private:
	enum WhichConnection {
		ClientConnection = 0,
		ServerConnection = 1
	};

public: // Need to work on this to make it private, 3Proxy startup is wily
	std::auto_ptr<SockBuf> sockbuf[2];

private:
	// Disable copying C++98 style
	SockPair (SockPair const & other);

private:
	void filterHelper(
		const DIRECTION which,
		const size_t offsetAmount,
		const size_t readSoFar,
		Filter& filter,
		bool disconnected
	);

private:
	// sends FILTERED data from OtherDirection(which) to which
	size_t doUnidirectionalProxyCore(
		const DIRECTION which,
		bool &timedOut,
		bool& socketClosed,
		const TIMEOUT timeout
	); 

public:
	size_t doUnidirectionalProxy(const DIRECTION which, const TIMEOUT timeout) {
		bool timedOut;
		bool socketClosed;
		size_t charsSent = doUnidirectionalProxyCore(
			which,
			timedOut,
			socketClosed,
			timeout
		);
		return charsSent;
	}

private:
	void doBidirectionalFilteredProxyCore(
		size_t (&readSoFar)[DIRECTION_MAX],
		size_t (&sentSoFar)[DIRECTION_MAX],
		bool (&socketClosed)[2], bool (&readAZero)[2],
		bool& timedOut,
		const TIMEOUT timeo,
		Filter* (&filter)[DIRECTION_MAX]
	);
public:
	void doBidirectionalFilteredProxyEx(
		size_t (&readSoFar)[DIRECTION_MAX],
		const TIMEOUT timeo,
		Filter* (&filter)[DIRECTION_MAX]
	);

	void doBidirectionalFilteredProxy(
		size_t (&readSoFar)[DIRECTION_MAX],
		const TIMEOUT timeo,
		Filter* (&filter)[DIRECTION_MAX]
	) {
		return doBidirectionalFilteredProxyEx(readSoFar, timeo, filter);
	}

	void cleanCheckpoint() {
		sockbuf[CLIENT]->cleanCheckpoint();
		sockbuf[SERVER]->cleanCheckpoint();
	}

	void failureShutdown(const std::string message, const TIMEOUT timeout) {
		sockbuf[CLIENT]->failureShutdown(message, timeout);
	}
};

#endif

