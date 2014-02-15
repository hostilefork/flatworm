//
// PcreDataFilter.h
//
// Very simple example of a Flatworm filter designed to perform substitutions
// on regular expressions.  Built on the Perl Compatible Regular Expressions
// library (PCRE)
//

#ifndef __PCREDATAFILTER_H__
#define __PCREDATAFILTER_H__

#include "DataFilter.h"

#include "pcre/pcre.h"
#include "pcre/pcreposix.h"

class PcreDataFilter : public DataFilter {
private:
	pcre * re;
	std::string replace;

protected:
	void filterBuffer(std::string& buf);

public:
	PcreDataFilter(
		SockPair& sockpair,
		const DIRECTION whichInput,
		const HeaderFilter& filterHeaderServer,
		const std::string regularExpression,
		const std::string replaceString
	);

public:
	std::auto_ptr<Instruction> firstInstructionSubCore() /* override */;
	std::auto_ptr<Instruction> runSubCore(
		const std::string& uncommittedBytes,
		const size_t newDataOffset,
		const size_t readSoFar,
		bool disconnected
	) /* override */;
	~PcreDataFilter() /* override */;
};

#endif