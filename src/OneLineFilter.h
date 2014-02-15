//
// OneLineFilter.h
//
// Reads a single line only, inherited by RequestLineFilter and ResponseLineFilter
//

#ifndef __ONELINEFILTER_H__
#define __ONELINEFILTER_H__

#include "parasock/Filter.h"

class OneLineFilter : public Filter {

public:
	OneLineFilter (SockPair& sockpair, const FlowDirection whichInput) : 
		Filter (sockpair, whichInput)
	{
	}

	std::auto_ptr<Instruction> firstInstruction() /* override */ {
		return std::auto_ptr<Instruction>(
			new UntilDelimiterInstruction("\r\n", 0)
		);
	}

	virtual void processTheLine(const std::string& line) = 0;

	virtual std::auto_ptr<Instruction> runFilter(
		const std::string& uncommittedBytes,
		const size_t newDataOffset,
		const size_t readSoFar,
		bool disconnected
	) {
		if (disconnected) {
			throw "Disconnection before line was read.";
		}

		size_t lineFeedPos = uncommittedBytes.find("\r\n");
		Assert(lineFeedPos != std::string::npos);

		if (lineFeedPos == 0) {
			return std::auto_ptr<Instruction>(
				new UntilDelimiterInstruction(
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
