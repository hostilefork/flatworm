//
// ServerHeaderFilter.h
//
// This reads HTTP headers until a lone CR/LF is reached
// at which point it returns Instruction::Quit
//

#ifndef __SERVERHEADERFILTER__
#define __SERVERHEADERFILTER__

#include "parasock/Filter.h"
#include "HeaderFilter.h"

class ServerHeaderFilter : public HeaderFilter {
private:
	bool redirect;

public:
	bool authenticate;

public:
	ServerHeaderFilter (
		SockPair& sockpair,
		const DIRECTION whichInput,
		bool isconnect,
		bool redirect
	) : 
		HeaderFilter (sockpair, whichInput, isconnect),
		redirect (redirect),
		authenticate (false)
	{
	}

	void processHeaderLine(
		std::string& header,
		const std::string key,
		const std::string value
	) /* override */ {
		size_t sb = 0;
		if (!strncasecmplen(key, "proxy-", 0, NULL)) {

		} else if (!strncasecmplen(key, "www-authenticate", 0, NULL)) {

			authenticate = true;
			header += key;
			header += ": ";
			header += value;
			header += "\r\n";

		} else {

			header += key;
			header += ": ";
			header += value;
			header += "\r\n";
		}
	}

	~ServerHeaderFilter() /* override */ {}
};

#endif
