//
// RequestLineFilter.h
//
// Simple class for plugging into the socket filtering system to read 
// an HTTP request from a client.
//
// This code is an attempt to tidy up the original corresponding code
// from 3Proxy and put it into a class, using less "magic number" style
// coding.  A work in progress, obviously.
//

#ifndef __FLATWORM_REQUESTLINEFILTER_H__
#define __FLATWORM_REQUESTLINEFILTER_H__

#include "OneLineFilter.h"

//
// REVIEW: is it necessary to modify input?  The original code did.
//
inline void decodeurl(std::string & input, int allowcr) {
	size_t s = 0;
	size_t d = 0;
	unsigned u;

	 while (s < input.length()) {
		if (
			(input[s] == '%')
			&& (s + 2 < input.length())
			&& ishex(input[s+1])
			&& ishex(input[s+2])
		) {
			sscanf(input.c_str() + s + 1, "%2x", &u);
			if (allowcr && u != '\r') {
				input[d++] = u;
			} else if (u != '\r' && u != '\n') {
				if (u == '\"' || u == '\\') {
					input[d++] = '\\';
				} else if (u == (char)255) {
					input[d++] = (char)255;
				}
				input[d++] = u;
			}
			s += 3;
		}
		else if (!allowcr && (input[s] == '?')) {
			break;
		} else if (input[s] == '+') {
			input[d++] = ' ';
			s++;
		}
		else {
			input[d++] = input[s++];
		}
	 }
	 input.resize(d); // unnecessary?
}


class RequestLineFilter : public OneLineFilter {
public:
	std::string request;
	std::string requestOriginal;
	std::auto_ptr<Placeholder> placeholder;

public:
	int operation;
	bool isconnect;
	size_t prefix;
	bool transparent;
	bool redirect; 
	unsigned& ckeepalive;
	std::string const lastRequest;
	std::string const lastRequestOriginal;
	ProxyWorker * proxy;

public:
	RequestLineFilter (
		Parasock & parasock,
		FlowDirection whichInput,
		unsigned& ckeepalive,
		std::string const lastRequest,
		std::string const lastRequestOriginal,
		ProxyWorker * proxy
	) :
		OneLineFilter (parasock, whichInput),
		ckeepalive (ckeepalive),
		lastRequest (lastRequest),
		lastRequestOriginal (lastRequestOriginal),
		proxy (proxy),
		isconnect (false),
		redirect (false),
		transparent (false),
		operation (0),
		prefix (0)
	{
	}

	void consume() {
		fulfillPlaceholder(placeholder, "");
	}

	void processTheLine(std::string const & line) /* override */
	{
		request = line;
		requestOriginal = line;

		if (request.length() < 10) {
			throw "Insufficient character count in request from client";
		}
		if (!strncasecmplen(request, "CONNECT", 0, NULL)) {
			isconnect = true;
		}
		size_t sb = request.find(' ');
		if (sb == std::string::npos) {
			throw "Can't find space in CONNECT string of request from client.";
		}
		size_t ss = ++sb;
		size_t se = std::string::npos;

		if (!isconnect) {
			if (!strncasecmplen(request, "http://", sb, &sb)) {
				// nothing, just increment sb?
			} else if (request[sb] == '/') {
				transparent = true;
			} else {
				throw "Not an http address in request, we only do http.";
			}
		} else {
			// we only damage string if IS CONNECT
			se = request.find(' ', sb);
			if (se == std::string::npos || sb==se) {
				throw "Malformed spaces in your request string";
			}
		}

		if (!transparent) {
			size_t sg = std::string::npos;
			if (!isconnect) {
				se = request.find('/', sb);
				if (se == std::string::npos) {
					throw "Second slash not found in request";
				}
				if (sb == se) {
					// Er... can this happen?
					// I have a hard time following 3Proxy's parsing logic :-/
					throw "Second and first slash positions equal (how!?)";
				}
				sg = request.find(' ', sb);
				if (sg == std::string::npos) {
					throw "No space found in request string.";
				}
				if (se > sg)
					se = sg;
			}
			prefix = se; // we carry this around to the next cycle
			if (request.find_last_of('@', sb) != std::string::npos) {
				std::string su = request.substr(sb, se - sb);
				decodeurl(su, 0);
				parseconnusername(su, proxy, 1, 80);
			} else {
				std::string su = request.substr(sb, se - sb);
				parsehostname(su, proxy, 80);
			}
			if (!isconnect) {
				if (se==sg)
					request[se--] = ' ';
				request[se] = '/';

				std::string replString = request.substr(
					se, request.length() - (se - sb)
				);
				
				request.resize(ss);
				request += replString;
			}
		}

		if (!strncasecmplen(request, "CONNECT", 0, NULL))
			operation = HTTP_CONNECT;
		else if (!strncasecmplen(request, "GET", 0, NULL))
			operation = HTTP_GET;
		else if (!strncasecmplen(request, "PUT", 0, NULL))
			operation = HTTP_PUT;
		else if (!strncasecmplen(request, "POST", 0, NULL))
			operation = HTTP_POST;
		else if (!strncasecmplen(request, "HEAD", 0, NULL))
			operation = HTTP_HEAD;
		else
			operation = HTTP_OTHER;

		placeholder = outputPlaceholder();
	}

	virtual ~RequestLineFilter() {
	}
};

#endif
