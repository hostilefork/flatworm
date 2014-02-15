//
// DataFilter.cpp
//
// Partial implementation of generalized data filter with subfiltering.
//

#include <string.h>

#include "DataFilter.h"


DataFilter::DataFilter (
	SockPair& sockpair,
	const FlowDirection whichInput,
	const HeaderFilter& filterHeader
) :
	Filter (sockpair, whichInput),

	contentLengthUnfiltered (filterHeader.getContentLengthUnfiltered()),
	chunkedUnfiltered (filterHeader.getChunkedUnfiltered()),

	filteredCharsSent (0),
	chunkSize (UNKNOWN),
	chunkSoFar (0),
	// we can chunk output even if the input isn't chunked!
	chunkedFiltered (filterHeader.getChunkedUnfiltered()),
	contentLengthFiltered (UNKNOWN),
	chunkState (BeginChunk)
{
}


void DataFilter::outputString(const std::string sendMe) {
	if (chunkedFiltered) {
		currentChunk->charsWrittenWithoutputString += sendMe.length();
	}
	Filter::outputString(sendMe);
}


std::auto_ptr<Placeholder> DataFilter::outputPlaceholder() {
	// we should remember the placeholder and its position
	// so we know which chunk size it affects
	// it might output a placeholder and not fulfill it until later
	// right now we keep everything in one chunk
	std::auto_ptr<Placeholder> placeholder = Filter::outputPlaceholder();
	if (chunkedFiltered) {
		currentChunk->subPlaceholders.push_back(placeholder.get());
	}
	return placeholder;
}


void DataFilter::fulfillPlaceholder(
	std::auto_ptr<Placeholder> placeholder,
	const std::string contents
) {
	if (chunkedFiltered) {
		std::vector<Placeholder*>::iterator it =
			std::find(
				currentChunk->subPlaceholders.begin(),
				currentChunk->subPlaceholders.end(),
				placeholder.get()
			);
		Assert(it != currentChunk->subPlaceholders.end());
		currentChunk->charsWrittenWithoutputString += contents.length();
		currentChunk->subPlaceholders.erase(it);
	}

	Filter::fulfillPlaceholder(placeholder, contents);
}


std::auto_ptr<Instruction> DataFilter::firstInstruction() {
	// gets more complicated when we chunk output that was initially
	// unchunked, or...when we don't chunk output that was chunked to
	// begin with.
	Assert(chunkedFiltered == chunkedUnfiltered);

	if (!chunkedUnfiltered)
		return firstInstructionSubCore();

	currentChunk.reset (new Chunk (*this));
	subInstruction = firstInstructionSubCore();
	chunkState = BeginChunk;

	return std::auto_ptr<Instruction>(
		new UntilDelimiterInstruction("\r\n", 0)
	);
}		


std::auto_ptr<Instruction> DataFilter::runFilter(
	const std::string& uncommittedBytes,
	const size_t newDataOffset,
	const size_t readSoFar,
	bool disconnected
) {

	// gets more complicated when we chunk output that was initially
	// unchunked, or...when we don't chunk output that was chunked to 
	// begin with.
	Assert(chunkedFiltered == chunkedUnfiltered);

	if (disconnected) {
		if (contentLengthUnfiltered.isKnown()) {
			Assert(readSoFar < contentLengthUnfiltered.getKnownValue());
			throw "Dropped connection with pending known data.";
		} else {
			if (chunkedFiltered) {
				throw "Dropped chunked connection with pending known data.";
			} else {
				// sadly, disconnection is the only way an unknown length that
				// isn't being chunked at us is signaled.  The length is
				// whatever we got, e.g. readSoFar

		 		// overwriting the variable, probably a bad idea if
		 		// analyzing after the fact, but...
				contentLengthUnfiltered.setKnownValue(readSoFar);
			}
		}
	}

	if (!chunkedUnfiltered)
		return runSubCore(
			uncommittedBytes,
			newDataOffset,
			readSoFar,
			disconnected
	);

	// Simple and dangerous, but just testing to see if we can chunk in
	// intermediate bits...
	/*
	if (!totalSize.isKnown() || (readSoFar < totalSize.getKnownValue())) {
		commitSize = 0;
		return;
	} */
	std::auto_ptr<Instruction> instruction;

	switch (chunkState) {

	case ReadChunkSize: 
		// technically we can put out chunks even when we haven't received
		// a chunk boundary, think about tuning this
		if (chunkString.length() > 0) { 
			subUnfiltered += chunkString;

			const size_t buflenInitial = subUncommitted.length();

			bool getMoreData = false;
			while (!getMoreData) {
				switch (subInstruction->type) {

					case Instruction::QuitFilter:
						// filter needs to eat all the input for now.
						NotReached(); 
						break;

					case Instruction::UntilDelimiter: {
						UntilDelimiterInstruction* inst =
							dynamic_cast<UntilDelimiterInstruction*>(subInstruction.get());
						size_t delimPos = subUnfiltered.find(inst->delimiter);
						if (delimPos != std::string::npos) {
							size_t offset = subUncommitted.length();
							subUncommitted +=
								subUnfiltered.substr(0, delimPos);
							subUnfiltered.erase(0, delimPos);
							subReadSoFar += delimPos;
							subInstruction = runSubCore(
								subUncommitted,
								offset,
								subReadSoFar,
								disconnected
							);
							Assert(subInstruction->commitSize <= subUncommitted.length());
							subUncommitted.erase(0, subInstruction->commitSize);
						} else 
							getMoreData = true;
						break;
					};

					case Instruction::BytesExact: {
						BytesExactInstruction* inst =
							dynamic_cast<BytesExactInstruction*>(subInstruction.get());
						if (subUnfiltered.length() >= inst->exactByteCount) {
							size_t offset = subUncommitted.length();
							subUncommitted += 
								subUnfiltered.substr(0, inst->exactByteCount);
							subUnfiltered.erase(0, inst->exactByteCount);
							subReadSoFar += inst->exactByteCount;
							subInstruction = runSubCore(
								subUncommitted,
								offset,
								subReadSoFar,
								disconnected
							);
							Assert(subInstruction->commitSize <= subUncommitted.length());
							subUncommitted.erase(0, subInstruction->commitSize);
						} else
							getMoreData = true;
						break;
					}

					case Instruction::BytesMax: {
						BytesMaxInstruction* inst =
							dynamic_cast<BytesMaxInstruction*>(subInstruction.get());

						if (subUnfiltered.length() > 0) {
							size_t offset = subUncommitted.length();
							if (subUnfiltered.length() > inst->maxByteCount) {
								subUncommitted += subUnfiltered.substr(
									0, inst->maxByteCount
								);
								subUnfiltered.erase(0, inst->maxByteCount);
								subReadSoFar += inst->maxByteCount;
								subInstruction = runSubCore(
									subUncommitted,
									offset,
									subReadSoFar,
									disconnected
								);
								Assert(subInstruction->commitSize <= subUncommitted.length());
								subUncommitted.erase(0, subInstruction->commitSize);
							} else {
								subUncommitted += subUnfiltered;
								subReadSoFar += subUnfiltered.length();
								subUnfiltered.erase();
								subInstruction = runSubCore(
									subUncommitted,
									offset,
									subReadSoFar,
									disconnected
								);
								Assert(subInstruction->commitSize <= subUncommitted.length());
								subUncommitted.erase(0, subInstruction->commitSize);
							}
						} else
							getMoreData = true;
						break;
					}

					case Instruction::BytesUnknown: {
						size_t offset = subUncommitted.length();
						subUncommitted += subUnfiltered;
						subReadSoFar += subUnfiltered.length();
						subUnfiltered.clear();
						subInstruction = runSubCore(
							subUncommitted,
							offset,
							subReadSoFar,
							disconnected
						);
						Assert(subInstruction->commitSize <= subUncommitted.length());
						subUncommitted.erase(0, subInstruction->commitSize);
						getMoreData = true;
						break;
					}

					default: {
						NotReached();
					}
				}
			}
			
			// should output a string; we need to know how big that string is

			if (currentChunk->readyToWrite()) {
				filteredCharsSent += currentChunk->FulfillAndReturnChunkSize();
				currentChunk.reset(new Chunk(*this));
			}
		}
		FallThrough();

	case BeginChunk: {
		Assert(uncommittedBytes.find("\r\n") != std::string::npos);
		Assert(uncommittedBytes[uncommittedBytes.length()-1] == '\n');
		Assert(uncommittedBytes[uncommittedBytes.length()-2] == '\r');
		const std::string& line = uncommittedBytes;
		int chunkSizeUnfilteredTemp = 0;
		if(uncommittedBytes.length() < 3)
				throw "Length field for chunked data less than for 0+CR+LF";
		sscanf(line.c_str(), "%x", &chunkSizeUnfilteredTemp);

		if (chunkSizeUnfilteredTemp == 0) {
			// chunk-size CRLF chunk-data CRLF
			// first is in the placeholder.  second we spit out here

			contentLengthUnfiltered.setKnownValue(subReadSoFar);

			subInstruction = runSubCore(
				subUncommitted,
				subReadSoFar,
				subReadSoFar,
				true
			);

			// REVIEW: What if we disconnected?

			Assert(subInstruction->commitSize <= subUncommitted.length());
			subUncommitted.erase(0, subInstruction->commitSize);

			Assert(subInstruction->type == Instruction::QuitFilter);
			Assert(subUnfiltered.empty());
			Assert(subUncommitted.empty());

			Assert(currentChunk->readyToWrite());
			filteredCharsSent += currentChunk->FulfillAndReturnChunkSize();
			currentChunk.reset(NULL);

			Filter::outputString("0\r\n"); /* need an additional \r\n? */

			contentLengthFiltered.setKnownValue(filteredCharsSent);

			instruction = std::auto_ptr<Instruction>(
				new QuitFilterInstruction(uncommittedBytes.length())
			);

		} else {
			// another chunk is coming...

			chunkSoFar = 0;
			chunkSize = static_cast<size_t>(chunkSizeUnfilteredTemp);

			// Should be able to do this exactly, trying that instead...
			/* instruction = Instruction::BytesMax(
				chunkSize.getKnownValue(),
				uncommittedBytes.length()
			); */

			instruction = std::auto_ptr<Instruction>(
				new BytesExactInstruction(
					chunkSize.getKnownValue(),
					uncommittedBytes.length()
				)
			);
		}

		chunkState = nextChunkState(chunkState);
		break;
	}		

	case ReadChunkData: {
		// just data, add it up into our "chunkString" and commit it
		// need a way to say "read up to" bytes vs read exactly...
		// current semantics is "read up to"

		Assert(chunkSize.isKnown());
		chunkString += uncommittedBytes;
		{
			// new test... can we read EXACT bytes?
			Assert(uncommittedBytes.length() == chunkSize.getKnownValue());
			instruction = std::auto_ptr<Instruction>(
				new UntilDelimiterInstruction(
					"\r\n",
					uncommittedBytes.length()
				)
			);
			chunkState = nextChunkState(chunkState);
		}

		// Here's another way of doing it that was disabled...
		// REVIEW: Why wasn't this code just deleted?
		/*
		chunkSoFar += uncommittedBytes.length();
		Assert(chunkSoFar <= chunkSize.getKnownValue());
		if (chunkSoFar == chunkSize.getKnownValue()) {
			instruction = Instruction::UntilDelimiter(
				"\r\n",
				uncommittedBytes.length()
			);
			chunkState = nextChunkState(chunkState);
			Assert(chunkString.length() == chunkSize.getKnownValue());
		} else {
			instruction = Instruction::BytesMax(
				SafeSubtractSize(chunkSize.getKnownValue(), chunkSoFar),
				uncommittedBytes.length()
			);
		}
		*/

		break;
	}

	case ReadChunkCrLf: {
		Assert(uncommittedBytes == "\r\n");

		instruction = std::auto_ptr<Instruction>(new 
			UntilDelimiterInstruction(
				"\r\n",
				uncommittedBytes.length()
			)
		);
		chunkState = nextChunkState(chunkState);
		break;
	}

	default:
		NotReached();
		break;
	}

	return instruction;
}


DataFilter::~DataFilter() {
}
