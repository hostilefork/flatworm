//
// PassFilter.h
//
// Simple filter that does nothing but pass data through, yet still speaks
// the filtering protocol.  Includes a check to make sure that if you
// think you know how much data will be passed through, that's how much
// is actually transferred.
//

#ifndef __PASSTHRUFILTER_H__
#define __PASSTHRUFILTER_H__

#include "Filter.h"

class PassthruFilter : public Filter {
private:
	Knowable<size_t> totalSize;
	const std::string sendFirst;

public:
	PassthruFilter (
		SockPair& sockpair,
		const FlowDirection whichInput,
		Knowable<size_t> totalSize = UNKNOWN,
		const std::string sendFirst = ""
	) : 
		Filter (sockpair, whichInput), 
		totalSize (totalSize),
		sendFirst (sendFirst)
	{
		if (totalSize.isKnown())
			Assert(!totalSize.isKnownToBe(0));
	}

	std::auto_ptr<Instruction> firstInstruction() /* override */ {
		if (!sendFirst.empty())
			outputString(sendFirst);

		if (!totalSize.isKnown()) {
			return std::auto_ptr<Instruction>(new BytesUnknownInstruction(0));
		} else {
			return std::auto_ptr<Instruction>(
				new BytesMaxInstruction(totalSize.getKnownValue(), 0)
			);
		}
	}

	std::auto_ptr<Instruction> runFilter(
		const std::string& uncommittedBytes,
		const size_t newDataOffset,
		const size_t readSoFar,
		bool disconnected
	) /* override */ {
		if (disconnected) {
			if (totalSize.isKnown()) {
				if (readSoFar < totalSize.getKnownValue()) {
					throw "Connection dropped before total size was received";
				}
			} else {
				// well guess this is all we got.  modify variable?
				totalSize.setKnownValue(readSoFar); 
			}
		}

		Assert(!totalSize.isKnown() || (readSoFar <= totalSize.getKnownValue()));

		outputString(uncommittedBytes);

		std::auto_ptr<Instruction> instruction;
		if (totalSize.isKnownToBe(readSoFar)) {
			instruction = std::auto_ptr<Instruction>(
				new QuitFilterInstruction(uncommittedBytes.length())
			);
		} else {
			if (totalSize.isKnown()) {
				instruction = std::auto_ptr<Instruction>(
					new BytesMaxInstruction(
						totalSize.getKnownValue() - readSoFar,
						uncommittedBytes.length()
					)
				);
			} else {
				instruction = std::auto_ptr<Instruction>(
					new BytesUnknownInstruction(uncommittedBytes.length()));
			}
		}
		return instruction;
	}

	~PassthruFilter() /* override */ {
	}
};

#endif
