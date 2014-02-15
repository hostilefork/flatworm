//
// ResponseLineFilter.h
//
// The first line of the HTTP response contains a version number and status
// code.  This is the server's analogue to the request.
//
// Maybe silly to have this in its own class... but, why not?
//

#ifndef __RESPONSELINEFILTER__
#define __RESPONSELINEFILTER__

#include "parasock/Filter.h"
#include "OneLineFilter.h"

class ResponseLineFilter : public OneLineFilter {
public:
	std::string response;
	std::auto_ptr<Placeholder> placeholder;

public:
	// http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
	int httpStatusCode; 

public:
	ResponseLineFilter (SockPair& sockpair, const FlowDirection whichInput) : 
		OneLineFilter (sockpair, whichInput),
		httpStatusCode (0)
	{
	}

	void processTheLine(const std::string& line) /* override */
	{
		if (line.length() <= 9) {
			throw "Too few chars in response for HTTP version and code.";
		}

		response = line;

		httpStatusCode = atoi(response.c_str() + 9);

		outputString(response);
	}

	~ResponseLineFilter() /* override */ {}
};

#endif
