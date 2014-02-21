//
// FlvDataFilter.h
//
// Beginnings of a Flash Video filter, that may or may not come to fruition.
//

#ifndef __FLATWORM_FLVFILTER_H__
#define __FLATWORM_FLVFILTER_H__

#include "parasock/Filter.h"
#include "HeaderFilter.h"

#include "flv/flv.h"


// experimental filter for screwing with flash streams as they go by
class FlvFilter : public Filter {
private:
	enum FlvState {
		NoFlv,
		ReadFlvHeader,
		SkipFlvHeader,
		ReadPreviousFlvTagSize,
		ReadFlvTagHeader,
		// should chain to appropriate handler
		// when this is transformed from a test into a DataFilter
		ReadFlvTagBody 
	};

	FlvState nextFlvState(FlvState flvstate) {
		switch (flvstate) {
			case ReadFlvHeader:
				return SkipFlvHeader;
			case SkipFlvHeader:
				return ReadPreviousFlvTagSize;
			case ReadPreviousFlvTagSize:
				return ReadFlvTagHeader;
			case ReadFlvTagHeader:
				return ReadFlvTagBody;
			case ReadFlvTagBody:
				return ReadPreviousFlvTagSize;
			default:
				NotReached();
				return NoFlv;
		}
	}

private:
	Knowable<size_t> totalSize;
	FlvState flvstate;

public:
	FlvFilter (
		Parasock & parasock,
		FlowDirection whichInput,
		HeaderFilter const & filterHeader
	) :
		Filter(parasock, whichInput),
		totalSize (filterHeader.getContentLengthUnfiltered()),
		flvstate (ReadFlvHeader)
	{
	}

	std::auto_ptr<Instruction> FlvFilter::firstInstruction() {
		std::auto_ptr<Instruction> instruction;
		if (totalSize.isKnown()) {
			if (totalSize.isKnownToBe(0)) {
				instruction = std::auto_ptr<Instruction>(
					new QuitFilterInstruction(0)
				);
			} else {
				// 9 bytes for header, known size...
				instruction = std::auto_ptr<Instruction>(
					new BytesExactInstruction(9, 0)
				);
			}
		} else {
			instruction = std::auto_ptr<Instruction>(
				new BytesUnknownInstruction(0)
			);
		}
	
		return instruction;
	}

	std::auto_ptr<Instruction> FlvFilter::runFilter(
		std::string const & uncommittedBytes,
		size_t newDataOffset,
		size_t readSoFar,
		bool disconnected
	) {

		if (disconnected) {
			if (totalSize.isKnown()) {
				Assert(readSoFar < totalSize.getKnownValue());
				throw "Connection dropped with known length data pending.";
			} else {
				// sadly, disconnection is the only way an unknown length that
				// isn't being chunked at us is signaled.  The length is
				// whatever we got, e.g. readSoFar

				// overwriting the variable, probably a bad idea if analyzing
				// after the fact, but...
				totalSize.setKnownValue(readSoFar);
			}
		}

		Assert(!totalSize.isKnown() || readSoFar <= totalSize.getKnownValue());

		std::auto_ptr<Instruction> instruction;
		switch (flvstate) {
			case ReadFlvHeader: {
				// we should have read the mandatory 9 bytes!
				Assert(uncommittedBytes.length() == 9);
				FLV_HEADER const * flvHeader =
					reinterpret_cast<FLV_HEADER const *>(uncommittedBytes.c_str());

				instruction = std::auto_ptr<Instruction>(
					new BytesExactInstruction(
						DecodeEndian(flvHeader->Offset),
						uncommittedBytes.length()
					)
				);
				flvstate = nextFlvState(flvstate);
				break;
			}

			case SkipFlvHeader:
				// don't want these bytes, want size
				instruction = std::auto_ptr<Instruction>(
					new BytesExactInstruction(
						sizeof(FLV_PREVIOUS_TAG_SIZE),
						uncommittedBytes.length()
					)
				);
				flvstate = nextFlvState(flvstate);
				break;

			case ReadPreviousFlvTagSize:
				instruction = std::auto_ptr<Instruction>(
					new BytesExactInstruction(
						sizeof(FLV_TAG_HEADER),
						uncommittedBytes.length()
					)
				);
				break;

			case ReadFlvTagBody:
				break;

			default:
				NotReached();
				break;
		};
		return instruction;
	}

	~FlvFilter() /* override */ {}
};

#endif
