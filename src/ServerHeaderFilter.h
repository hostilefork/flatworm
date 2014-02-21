//
// ServerHeaderFilter.h
//
// This reads HTTP headers until a lone CR/LF is reached
// at which point it returns Instruction::Quit
//

#ifndef __FLATWORM_SERVERHEADERFILTER_H__
#define __FLATWORM_SERVERHEADERFILTER_H__

#include "parasock/Filter.h"
#include "HeaderFilter.h"

class ServerHeaderFilter : public HeaderFilter {
private:
	bool redirect;

public:
	bool authenticate;

public:
	ServerHeaderFilter (
		Parasock & parasock,
		FlowDirection whichInput,
		bool isconnect,
		bool redirect
	) : 
		HeaderFilter (parasock, whichInput, isconnect),
		redirect (redirect),
		authenticate (false)
	{
	}

	void processHeaderLine(
		std::string & header,
		std::string const key,
		std::string const value
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
