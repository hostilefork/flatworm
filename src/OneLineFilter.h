//
// OneLineFilter.h
//
// Reads a single line only, inherited by RequestLineFilter and ResponseLineFilter
//

#ifndef __FLATWORM_ONELINEFILTER_H__
#define __FLATWORM_ONELINEFILTER_H__

#include "parasock/Filter.h"

class OneLineFilter : public Filter {

public:
	OneLineFilter (Parasock & parasock, FlowDirection whichInput) : 
		Filter (parasock, whichInput)
	{
	}

	std::auto_ptr<Instruction> firstInstruction() /* override */ {
		return std::auto_ptr<Instruction>(
			new ThruDelimiterInstruction("\r\n", 0)
		);
	}

	virtual void processTheLine(std::string const & line) = 0;

	virtual std::auto_ptr<Instruction> runFilter(
		std::string const & uncommittedBytes,
		size_t newDataOffset,
		size_t readSoFar,
		bool disconnected
	) {
		if (disconnected) {
			throw "Disconnection before line was read.";
		}

		size_t lineFeedPos = uncommittedBytes.find("\r\n");
		Assert(lineFeedPos != std::string::npos);

		if (lineFeedPos == 0) {
			return std::auto_ptr<Instruction>(
				new ThruDelimiterInstruction(
					"\r\n",
					uncommittedBytes.length()
				)
			);
		} 

		processTheLine(uncommittedBytes);

		return std::auto_ptr<Instruction>(
			new QuitFilterInstruction(uncommittedBytes.length())
		);
	}

	virtual ~OneLineFilter() /* override */ {}
};

#endif
