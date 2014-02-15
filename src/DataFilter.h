//
// DataFilter.h
//
// This is a placeholder for a generalized data filter, which would be able to
// call sub-filters that would process the information post-chunk-processing.
// The sub-filtering system is not implemented yet.
//

#ifndef __DATAFILTER_H__
#define __DATAFILTER_H__

#include "HeaderFilter.h"

#include <memory>
#include <vector>


// My filtering idea is that you have a class which holds the state during
// a transaction between server and client.  This class is destroyed at the
// end of the transfer.  Chunking is transparent, so you will not get the
// hex #s.  However, your filter must be able to handle several successive
// calls.  The filter is responsible for pushing bytes out at this time.
class DataFilter : public Filter {

private:
	enum ChunkState {
		NoChunk,
		BeginChunk,
		ReadChunkSize,
		ReadChunkData,
		ReadChunkCrLf
	};

	inline ChunkState nextChunkState(ChunkState chunkState) {
		switch (chunkState) {
			case BeginChunk:
			case ReadChunkSize:
				return ReadChunkData;
			case ReadChunkData:
				return ReadChunkCrLf;
			case ReadChunkCrLf:
				return ReadChunkSize;
			default:
				NotReached();
				return NoChunk;
		}
	}

private:
	// for the moment we don't support inter-chunk placeholders
	// there's one chunk, it just gets big until all placeholders are committed
	class Chunk {

		friend class DataFilter;

	private:
		// can't fulfill until the data is all known
		std::auto_ptr<Placeholder> sizePlaceholder; 
		std::vector<Placeholder*> subPlaceholders;
		size_t charsWrittenWithoutputString;
		bool wasWritten;
		Filter& owningFilter;

	public:
		Chunk (Filter& owningFilter) :
			charsWrittenWithoutputString(0),
			wasWritten (false),
			owningFilter (owningFilter)
		{
			sizePlaceholder = owningFilter.Filter::outputPlaceholder();
			owningFilter.Filter::outputString("\r\n");
		}

		bool readyToWrite() {
			return subPlaceholders.empty();
		}

		size_t FulfillAndReturnChunkSize() {
			Assert(readyToWrite());
			Assert(!wasWritten);
			char smallhex[32];
			sprintf(smallhex, "%x", charsWrittenWithoutputString);
			owningFilter.Filter::fulfillPlaceholder(sizePlaceholder, smallhex);

			owningFilter.Filter::outputString("\r\n");
			wasWritten = true;
			return charsWrittenWithoutputString;
		}
		
		virtual ~Chunk() { }
	};

protected:
	bool chunkedUnfiltered;
	Knowable<size_t> contentLengthUnfiltered;

// Filtering may lead us to choose a different length and chunking
// strategy from the input
protected:
	Knowable<size_t> contentLengthFiltered;
	bool chunkedFiltered;
	size_t filteredCharsSent;

private:
	ChunkState chunkState;
	size_t chunkSoFar;
	Knowable<size_t> chunkSize;
	std::string chunkString;

private:
	std::auto_ptr<Chunk> currentChunk;
	std::auto_ptr<Instruction> subInstruction;
	std::string subUncommitted;
	std::string subUnfiltered;
	size_t subReadSoFar;

public:
	DataFilter(
		SockPair& sockpair,
		const FlowDirection whichInput,
		const HeaderFilter& filterHeaderServer
	);

public:
	Knowable<size_t> getContentLengthFiltered() { 
		return contentLengthFiltered;
	}

	bool getChunkedFiltered() {
		return chunkedFiltered;
	}

// these are overridden so you don't put the output directly on the wire...
protected:
	void outputString(const std::string sendMe) /* override */;
	std::auto_ptr<Placeholder> outputPlaceholder() /* override */;
	void fulfillPlaceholder(
		std::auto_ptr<Placeholder> placeholder, const std::string contents
	) /* override */;

public:
	virtual std::auto_ptr<Instruction> firstInstructionSubCore() = 0;
	virtual std::auto_ptr<Instruction> runSubCore(
		const std::string& uncommittedBytes,
		const size_t newDataOffset,
		const size_t readSoFar,
		bool disconnected
	) = 0;

public:
	std::auto_ptr<Instruction> firstInstruction();
	std::auto_ptr<Instruction> runFilter(
		const std::string& uncommittedBytes,
		const size_t newDataOffset,
		const size_t readSoFar,
		bool disconnected
	) /* override */;
	~DataFilter() /* override */;
};

#endif
