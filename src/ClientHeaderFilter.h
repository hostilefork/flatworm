//
// ClientHeaderFilter.h
//
// This filter will process the incoming data for a client header, essentially
// buffering it.
//

#ifndef __CLIENTHEADERFILTER_H__
#define __CLIENTHEADERFILTER_H__

#include "parasock/Filter.h"
#include "HeaderFilter.h"

class ClientHeaderFilter : public HeaderFilter {
public:
	std::string& requestOriginal;
	bool isconnect;
	bool transparent;
	const unsigned ckeepalive;
	ProxyWorker* proxy;
	
public:
	ClientHeaderFilter (
		SockPair& sockpair,
		const DIRECTION whichInput,
		std::string& requestOriginal,
		bool isconnect,
		bool transparent,
		const unsigned ckeepalive,
		ProxyWorker* proxy
	) :
		HeaderFilter (sockpair, whichInput, isconnect),
		requestOriginal (requestOriginal), 
		isconnect (isconnect), 
		transparent (transparent),
		ckeepalive (ckeepalive),
		proxy (proxy)
	{
	}

	void processHeaderLine(
		std::string& header,
		const std::string key,
		const std::string value
	) /* override */ {
		size_t sb = 0;
		if(transparent && !strncasecmplen(key, "Host")) {

			size_t se = value.find('\r', sb);
			if(!ckeepalive) {
				std::string sq = value.substr(0, se);
				parsehostname(sq, proxy, 80);
			}
			std::ostringstream newReqStream;
			size_t sp = requestOriginal.find('/', 1);
			newReqStream << requestOriginal.substr(0, sp) << "http://" <<
				value.substr(
					0, se != std::string::npos ? se : std::string::npos
				) << requestOriginal.substr(sp, std::string::npos);
			requestOriginal = newReqStream.str();
			header += key;
			header += ": ";
			header += value;
			header += "\r\n";

		} else if (!strncasecmplen(key, "accept-encoding")) {
			// zip or gzip data is hard to filter, but apparently there
			// is a streaming interface in zlib with z_stream_s:

			// http://stackoverflow.com/questions/4164037/

			// Of course it's easier to just say we don't accepted compressed
			// encodings, so go with that for starters.  :-/

		} else {

			header += key;
			header += ": ";
			header += value;
			header += "\r\n";
		}
	}

	~ClientHeaderFilter() /* override */ {}
};

#endif