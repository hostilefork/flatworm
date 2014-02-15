//
// Filter.cpp
//
// This is a filtering socket abstraction, and may be a subset of capabilities
// of Boost.ASIO (which I did not know about at the time of writing.)
//

#include "NetUtils.h"

#include "Filter.h"

UnknownType UNKNOWN; // single global unknown


// Debugging tool
#ifdef SOCKWATCH
std::deque<SOCKET> socketwatches;
void BeginSockWatch(SOCKET sock) {
	socketwatches.push_back(sock);
}
void EndSockWatch(SOCKET sock) {
	std::deque<SOCKET>::iterator it = std::find(
		socketwatches.begin(),
		socketwatches.end(),
		sock
	);
	if (it != socketwatches.end())
		socketwatches.erase(it);
}
#endif


// Lazy right now, most defines in the header, couldn't do these that way
// because of forward declaration...

Filter::Filter(SockPair& sockpair, const DIRECTION whichInput) : 
	lastReadSoFar (UNKNOWN),
	sockbufInput (*sockpair.sockbuf[whichInput]),
	sockbufOutput (*sockpair.sockbuf[OtherDirection(whichInput)]),
	running (false)
{
	uncommittedChars = 0;
}


void Filter::outputString(const std::string sendMe) {
	sockbufOutput.outputString(sendMe);
}


std::auto_ptr<Placeholder> Filter::outputPlaceholder() {
	return sockbufOutput.outputPlaceholder();
}


void Filter::fulfillPlaceholder(
	std::auto_ptr<Placeholder> placeholder,
	const std::string contents
) {
	sockbufOutput.fulfillPlaceholder(placeholder, contents);
}




