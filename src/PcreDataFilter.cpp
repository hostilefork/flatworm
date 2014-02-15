//
// PcreDataFilter.cpp
//
// Very simple example of a Flatworm filter designed to perform substitutions
// on regular expressions.  Built on the Perl Compatible Regular Expressions
// library (PCRE)
//
// Lack of comments/etc. is basically due to copying example code and making
// only minor modifications to get it to work.  It's C, it's error-prone, and
// I definitely would seek to use something better... the library just
// happened to be included in the 3Proxy source.
//

#include "pcre/pcreposix.h"

#include "PcreDataFilter.h"

int wday = 0;
time_t basetime = 0;

#ifndef isnumber
#define isnumber(i_n_arg) ((i_n_arg>='0') && (i_n_arg<='9'))
#endif

PcreDataFilter::PcreDataFilter (
	SockPair& sockpair,
	const DIRECTION whichInput,
	const HeaderFilter& filterHeaderServer,
	const std::string regularExpression,
	const std::string replaceString
) :
	DataFilter (sockpair, whichInput, filterHeaderServer),
	replace (replaceString)
{
	pcre *re = NULL;

	int offset = 4;	
	CodeBlock() {
		const char * errptr;
		this->re = pcre_compile(
			regularExpression.c_str(),
			0 /* pcre_options */,
			&errptr,
			&offset,
			NULL
		);
		if (!this->re) {
			throw "Regular expression compilation error";
		}
	}
}


void PcreDataFilter::filterBuffer(std::string& buf){

	int ovector[48];
	int count = 0;
	
	size_t replen, num;
	std::string replace;
	int nreplaces=0;

	if(!this->re) {
		throw "No regular expression supplied in filterBuffer";
	}

	size_t offset = 0;
	size_t length = buf.length();
	for(; offset < length; nreplaces++){

		count = pcre_exec(
			this->re,
			NULL, buf.c_str(),
			static_cast<int>(length),
			static_cast<int>(offset),
			0,
			ovector,
			48
		);

		if (count <= 0) {
			break;
		}

		replace = this->replace;

		replen = length - ovector[1];
		{
			std::string::iterator it = replace.begin();
			while (it != replace.end()){
				if ((*it == '\\') && (it + 1 != replace.end())) {
					it += 2;
					++replen;
				}
				else if (
					(*it == '$')
					&& (it + 1 != replace.end())
					&& isnumber(*(it+1))
				) {
					it++;
					std::string::iterator itFirst = it;
					while((it != replace.end()) && isnumber(*it)) {
						it++;
					}
					std::string numString(itFirst,it);
					num = atoi(numString.c_str());
					if (num > (static_cast<size_t>(count) - 1)) {
						continue;
					}
					replen += (ovector[(num<<1) + 1] - ovector[(num<<1)]);
				} else {
					it++;
					replen++;
				}
			}
		}

		char* tmpbuf = new char[replen];
		char* target = tmpbuf;
		{
			std::string::iterator it = replace.begin();
			while (it != replace.end() ){
				if (((*it) == '\\') && (it + 1 != replace.end())) {
					*target++ = *(it+1);
					it += 2;
				}
				else if (
					((*it) == '$')
					&& (it + 1 != replace.end())
					&& isnumber(*(it+1))
				) {
					it++;
					std::string::iterator itFirst = it;

					while ((it != replace.end()) && isnumber(*it)) {
						it++;
					}
					std::string numString(itFirst,it);
					num = atoi(numString.c_str());
					if (num > (static_cast<size_t>(count) - 1)) {
						continue;
					}
					memcpy(
						target,
						buf.c_str() + ovector[(num<<1)],
						ovector[(num<<1) + 1] - ovector[(num<<1)]
					);
					target += (ovector[(num<<1) + 1] - ovector[(num<<1)]);
				}
				else {
					*target++ = *it++;
				}
			}
		}
		memcpy(target, buf.c_str() + ovector[1], length - ovector[1]);
		buf.resize(ovector[0]);
		buf.append(tmpbuf, replen);
		/* buf.replace(ovector[0], replen, tmpbuf); */
		delete[] tmpbuf;
		length = ovector[0] + replen;
		if (ovector[0] + replen <= offset) {
			break;
		}
		offset = ovector[0] + this->replace.length();
	}
	// nreplaces is how many replaces we did.  does it matter?
}


std::auto_ptr<Instruction> PcreDataFilter::firstInstructionSubCore() {
	std::auto_ptr<Instruction> instruction;
	if (contentLengthUnfiltered.isKnown()) {
		if (contentLengthUnfiltered.isKnownToBe(0)) {
			instruction.reset(new QuitFilterInstruction(0));
		} else {
			instruction.reset(new BytesMaxInstruction(
				contentLengthUnfiltered.getKnownValue(),
				0
			));
		}
	} else {
		instruction.reset(new BytesUnknownInstruction(0));
	}

	Assert(NULL != instruction.get());
	return instruction;
}


std::auto_ptr<Instruction> PcreDataFilter::runSubCore(
	const std::string& uncommittedBytes,
	const size_t newDataOffset,
	const size_t readSoFar,
	bool disconnected
) {

	Assert(
		!contentLengthUnfiltered.isKnown()
		|| readSoFar <= contentLengthUnfiltered.getKnownValue()
	);

	std::auto_ptr<Instruction> instruction;
	std::string filteredOutput = uncommittedBytes;
	filterBuffer(filteredOutput);
	outputString(filteredOutput);

	if (contentLengthUnfiltered.isKnownToBe(readSoFar)) {
		instruction.reset(new QuitFilterInstruction(uncommittedBytes.length()));
	} else {
		if (contentLengthUnfiltered.isKnown()) {
			instruction.reset(new BytesMaxInstruction(
				SafeSubtractSize(
					contentLengthUnfiltered.getKnownValue(), readSoFar
				),
				uncommittedBytes.length()
			)); 
		} else {
			instruction.reset(
				new BytesUnknownInstruction(uncommittedBytes.length())
			);
		}
	}
	Assert(NULL != instruction.get());
	return instruction;
}


PcreDataFilter::~PcreDataFilter() {
}