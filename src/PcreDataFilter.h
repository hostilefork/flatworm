//
// PcreDataFilter.h
//
// Very simple example of a Flatworm filter designed to perform substitutions
// on regular expressions.  Built on the Perl Compatible Regular Expressions
// library (PCRE)
//

#ifndef __FLATWORM_PCREDATAFILTER_H__
#define __FLATWORM_PCREDATAFILTER_H__

#include "DataFilter.h"

#include "pcre/config.h"
#include "pcre/pcre.h"
#include "pcre/pcreposix.h"

class PcreDataFilter : public DataFilter {
private:
	pcre * re;
	std::string replace;

protected:
	void filterBuffer(std::string & buf);

public:
	PcreDataFilter(
		Parasock & parasock,
		FlowDirection whichInput,
		HeaderFilter const & headerFilterServer,
		std::string const regularExpression,
		std::string const replaceString
	);

public:
	std::auto_ptr<Instruction> firstInstructionSubCore() /* override */;
	std::auto_ptr<Instruction> runSubCore(
		std::string const & uncommittedBytes,
		size_t newDataOffset,
		size_t readSoFar,
		bool disconnected
	) /* override */;
	~PcreDataFilter() /* override */;
};

#endif