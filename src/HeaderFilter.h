//
// HeaderFilter.h
//
// Simple generalized base class from which ClientHeaderFilter and
// ServerHeaderFilter are derived.
//
// Might seem silly to make a class for this, but it does make it easier
// to keep with changes to the filtering API and not have to update
// both filters...
//

#ifndef __HEADERFILTER_H__
#define __HEADERFILTER_H__

#include "ProxyServer.h"
#include <sstream>

class HeaderFilter : public Filter {
private:
	std::string header;
	std::auto_ptr<Placeholder> placeholder;
	std::auto_ptr<Placeholder> contentLengthPlaceholder;
	std::auto_ptr<Placeholder> crlfPlaceholder;
	Knowable<size_t> contentLengthUnfiltered;
	bool chunkedUnfiltered; // http://httpwatch.com/httpgallery/chunked/
	bool keepAlive;
	bool transparent;
	bool isconnect;
	
public:
	HeaderFilter (
		SockPair& sockpair,
		const FlowDirection whichInput,
		bool isconnect
	) :
		Filter (sockpair, whichInput),
		keepAlive (false),
		contentLengthUnfiltered (UNKNOWN),
		chunkedUnfiltered (false),
		transparent (false),
		isconnect (isconnect)
	{
	}

	std::auto_ptr<Instruction> firstInstruction() /* override */ {
		return std::auto_ptr<Instruction>(
			new UntilDelimiterInstruction("\r\n", 0)
		);
	}

	Knowable<size_t> getContentLengthUnfiltered() const {
		return contentLengthUnfiltered;
	}

	bool getChunkedUnfiltered() const {
		return chunkedUnfiltered;
	}

	bool shouldKeepAlive() const {
		return keepAlive;
	}

	std::string& getHeaderString() {
		return header;
	}

	void consume() {
		fulfillPlaceholder(placeholder, "");
		fulfillPlaceholder(contentLengthPlaceholder, "");
		fulfillPlaceholder(crlfPlaceholder, "");
	}

	void fullfillHeaderString() {
		fulfillPlaceholder(placeholder, header);
	}

	void fulfillContentLength(
		Knowable<size_t> contentLengthFiltered,
		bool chunkedFiltered
	) {

		std::ostringstream bufStream;
		if (contentLengthFiltered.isKnown()) {
			Assert(!chunkedFiltered);
			bufStream << "Content-Length: ";
			bufStream << contentLengthFiltered.getKnownValue();
			bufStream << "\r\n";
		} else {
			if (chunkedFiltered) {
				bufStream << "Transfer-Encoding: chunked\r\n";
			} else {
				// bufstream should be empty, to indicate unknown content 
				// length and not chunked.  the connection will need to be
				// closed to indicate the end of the transfer
			}
		}

		fulfillPlaceholder(contentLengthPlaceholder, bufStream.str());

		// instruction to signal end of header
		fulfillPlaceholder(crlfPlaceholder, "\r\n");
	}

	// return the line to add, if empty no line
	virtual void processHeaderLine(
		std::string& header,
		const std::string key,
		const std::string value
	) = 0;

	std::auto_ptr<Instruction> /* override */ runFilter(
		const std::string& uncommittedBytes,
		const size_t newDataOffset,
		const size_t readSoFar,
		bool disconnected
	) {
		if (disconnected) {
			throw "Socket disconnected before full header could be read.";
		}

		if ((uncommittedBytes.length() <= 2)) {
			if (uncommittedBytes.length() == 0) {
				NotReached();
			}

			if (uncommittedBytes.length() == 1) {
				Assert(uncommittedBytes[0] == '\n');
			}

			if (uncommittedBytes.length() == 2) {
				Assert(uncommittedBytes[0] == '\r');
				Assert(uncommittedBytes[1] == '\n');
			}

			// placeholder for all of "header"
			placeholder = outputPlaceholder();

			// always fill in content length and encoding or "" for no length
			contentLengthPlaceholder = outputPlaceholder();

			// this will signal the end of the header
			crlfPlaceholder = outputPlaceholder();

			return std::auto_ptr<Instruction>(
				new QuitFilterInstruction(uncommittedBytes.length())
			);
		}

		Assert(uncommittedBytes[uncommittedBytes.length()-1] == '\n');
		size_t colonPosition = uncommittedBytes.find(':');
		if (colonPosition == std::string::npos) {
			throw "Malformed header, expected colon after attribute name.";
		}

		std::string key = TrimStr(uncommittedBytes.substr(0, colonPosition));
		std::string value = TrimStr(
			uncommittedBytes.substr(colonPosition+1, std::string::npos)
		);

		if (!isconnect && (
			(!transparent && !strncasecmplen(key, "proxy-connection")) 
			|| (transparent && (!strncasecmplen(key, "connection"))))
		) {

			if (!strncasecmplen(value, "keep-alive")) {
				keepAlive = true;
			}

		} else if (!strncasecmplen(key, "content-length")) {

			if (value.empty())
				throw "Invalid content-length in header.";

			std::istringstream lengthStream (value.c_str());
			int length;
			lengthStream >> length;
			if (length >= 0) {
				contentLengthUnfiltered.setKnownValue(length);
			}

			// We will add our own content-length to the header

		} else if (!strncasecmplen(key, "transfer-encoding")) {

			if (value.empty()) {
				throw "Invalid transfer encoding in header.";
			}

			if (!strncasecmplen(value, "chunked")) {
				chunkedUnfiltered = true;
			}

		} else {

			// dispatch to virtual method
			processHeaderLine(header, key, value); 
		}

		return std::auto_ptr<Instruction>(
			new UntilDelimiterInstruction(
				"\r\n",
				uncommittedBytes.length()
			)
		);
	}

	~HeaderFilter() /* override */ {}
};

#endif