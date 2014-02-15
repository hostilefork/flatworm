//
// DeadFilter.h
//
// Simple stock filter that basically registers a lack of interest in data.
// Useful whenever you want to hook on and ignore the information coming
// back from a direction.
//

#ifndef __DEADFILTER_H__
#define __DEADFILTER_H__

#include "Filter.h"

class DeadFilter : public Filter {
public:
	DeadFilter (SockPair& sockpair, const FlowDirection whichInput) : 
		Filter (sockpair, whichInput)
	{
	}

	std::auto_ptr<Instruction> firstInstruction() /* override */ {
		return std::auto_ptr<Instruction>(new QuitFilterInstruction(0));
	}

	std::auto_ptr<Instruction> runFilter(
		const std::string& uncommittedBytes,
		const size_t newDataOffset,
		const size_t readSoFar,
		bool disconnected
	) /* override */ {
		NotReached();
		return std::auto_ptr<Instruction>(new QuitFilterInstruction(0));
	}

	~DeadFilter () /* override */ {
	}
};

#endif
