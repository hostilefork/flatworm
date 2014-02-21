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
	DeadFilter (Parasock & parasock, FlowDirection whichInput) : 
		Filter (parasock, whichInput)
	{
	}

	std::auto_ptr<Instruction> firstInstruction() /* override */ {
		return std::auto_ptr<Instruction>(new QuitFilterInstruction(0));
	}

	std::auto_ptr<Instruction> runFilter(
		std::string const & uncommittedBytes,
		size_t newDataOffset,
		size_t readSoFar,
		bool disconnected
	) /* override */ {
		NotReached();
		return std::auto_ptr<Instruction>(new QuitFilterInstruction(0));
	}

	~DeadFilter () /* override */ {
	}
};

#endif
