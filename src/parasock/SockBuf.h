//
// SockBuf.h
//
// Socket buffer.  Throws errors so you don't have to handle -1 cases
// Handles large transmission sizes broken into chunks
// Does filtering
//

#ifndef __SOCKBUF_H__
#define __SOCKBUF_H__

#include <deque>
#include <algorithm>

#include "NetUtils.h"
#include "Helpers.h"

class SockPair;

//
// You pass a std::auto_ptr<Placeholder> into an output stream
// You tell the sockbuf when a placeholder is fulfilled by data
// it then moves ahead the stream
//
class Placeholder {

	friend class SockBuf;
	friend class SockPair;

private:
	std::string contents;
	bool contentsKnown;
	const SockBuf* owner;

public:
	Placeholder() { owner = NULL; contentsKnown = false; }
	virtual ~Placeholder() {  }
};

class Filter;

class SockBuf {

	friend class SockPair;
	friend class Filter;

// Currently public because we let other people connect the sockets
// Really, there's nothing saying it has to be a socket.
public:
	SOCKET sock;
	struct sockaddr_in sin;
private:
	bool disconnected;

// Debugging information!
private:
	std::string bytesWrittenSoFar;
	std::string bytesReadSoFar;

// We read data in chunks into unfilteredBytes until we have enough to
// satisfy the filter.  When we do, enough bytes are moved to the
// uncommittedBytes buffer and the filter is called.  The filter decides how
// many of those it wishes to "consume", but if they are not consumed
// they will still be there for the next call
private:
	std::string unfilteredBytes;
	std::string uncommittedBytes;

private:
	std::deque<Placeholder*> placeholders;

public:
	SockBuf();

	std::auto_ptr<Placeholder> outputPlaceholder() {
		std::auto_ptr<Placeholder> placeholder (new Placeholder());
		Assert(placeholder->owner == NULL);
		placeholder->owner = this;
		placeholders.push_back(placeholder.get());
		return placeholder;
	}

	void fulfillPlaceholder(
		std::auto_ptr<Placeholder> placeholder,
		const std::string contents
	) {
		Assert(placeholder.get() != NULL);
		Assert(!placeholder->contentsKnown);
		Assert(placeholder->owner == this);
		std::deque<Placeholder*>::iterator it = std::find(
			placeholders.begin(),
			placeholders.end(),
			placeholder.get()
		);
		Assert(it != placeholders.end());
		placeholder->contentsKnown = true;
		if (contents.empty()) {	
			placeholders.erase(it);
		} else {
			// hold onto placeholder until its time
			placeholder->contents = contents;
			placeholder.release(); // will free later
		}
	}

	void outputString(const std::string sendMe) {
		if (
			!placeholders.empty()
			&& (placeholders.back()->contentsKnown)
		) {
			std::string& addToContents = placeholders.back()->contents;

			// merge if contents known of last placeholder
			addToContents += sendMe; 
		} else {
			std::auto_ptr<Placeholder> placeholder = outputPlaceholder();
			fulfillPlaceholder(placeholder, sendMe);
		}
	}

	bool hasKnownWritesPending() {
		if (placeholders.empty()) {
			return false;
		}
		return placeholders.front()->contentsKnown;
	}

	bool definitelyHasFutureWrites() {
		std::deque<Placeholder*>::iterator it = placeholders.begin();
		while (it != placeholders.end()) {
			if ((*it)->contentsKnown) {
				return true;
			}
			it++;
		}
		return false;
	}

	void cleanCheckpoint();
	void shutdownAndClose();
	void failureShutdown(const std::string message, Timeout timeout);

	virtual ~SockBuf();
};

#endif

