//
// Filter.h
//
// This is a filtering socket abstraction, and may be a subset of capabilities
// of Boost.ASIO (which I did not know about at the time of writing.)
//

#ifndef __PARASOCK_FILTER_H__
#define __PARASOCK_FILTER_H__

#include <deque>
#include <string>
#include <memory>
#include <algorithm>

#include "Helpers.h"
#include "NetUtils.h"

#include "Parasock.h"


#ifdef SOCKWATCH
void BeginSockWatch(SOCKET sock);
void EndSockWatch(SOCKET sock);
#endif


class Filter;

class Instruction {

	friend class SockBuf;
	friend class Parasock;
	friend class Filter;

public:
	enum InstructionType {
		QuitFilter, 
		ThruDelimiter, 
		BytesMax, 
		BytesExact, 
		BytesUnknown
	};

public:
	const InstructionType type;
	size_t commitSize;

protected:
	Instruction (const InstructionType type, size_t commitSize) :
		commitSize (commitSize),
		type (type)
	{
	}

private:
	// Disable copying, C++98 style
	Instruction(const Instruction& other);

public:
	virtual ~Instruction() { }
};


// quit the filter
class QuitFilterInstruction : public Instruction {
public:
	QuitFilterInstruction (size_t commitSize) :
		Instruction (Instruction::QuitFilter, commitSize)
	{ 
	}
};


// read until delimiter is reached
// (delimiter will be last of uncommittedBytes characters)
class ThruDelimiterInstruction : public Instruction {
public:
	std::string delimiter;

public:
	ThruDelimiterInstruction(
		std::string const delimiter,
		size_t commitSize
	) :
		Instruction (Instruction::ThruDelimiter, commitSize),
		delimiter (delimiter)
	{
	}
};


// read up to the given # of bytes, allowing for shorter buffer ranges
// in chunks if they become available.  Probably want the operation to offer
// a minimum too.
class BytesMaxInstruction : public Instruction {
public:
	size_t maxByteCount;

public:
	BytesMaxInstruction(size_t maxByteCount, size_t commitSize) :
		Instruction (Instruction::BytesMax, commitSize),
		maxByteCount (maxByteCount)
	{
		Assert(maxByteCount > 0);
	}
};


// don't call the handler until the exact number of bytes is available
// (or a disconnect?)
class BytesExactInstruction : public Instruction {
public:
	size_t exactByteCount;

public:
	BytesExactInstruction(size_t exactByteCount, size_t commitSize) :
		Instruction (Instruction::BytesExact, commitSize),
		exactByteCount (exactByteCount)
	{
		Assert(exactByteCount > 0);
	}
};


// read an unknown number of bytes until the bytes stop coming
class BytesUnknownInstruction : public Instruction {
public:
	BytesUnknownInstruction (size_t commitSize) :
		Instruction (Instruction::BytesUnknown, commitSize)
	{
	}
};


// Filters are allowed to write to the http stream
// There is therefore no "filter action"
// to censor output, replace it with html code of the censorship message
class Filter {

	friend class SockBuf;
	friend class Parasock;

private:
	Knowable<size_t> lastReadSoFar;
	size_t uncommittedChars;
	std::auto_ptr<Instruction> instruction;
	SockBuf& sockbufInput;
	SockBuf& sockbufOutput;
	bool running;

private:
	std::string bytesRead; // DEBUG

public:
	Filter(Parasock & parasock, FlowDirection whichInput);

protected:
	Instruction const * currentInstruction() {
		return this->instruction.get();
	}

// only derived class may call these.  Must be from within a run method!
// currently public instead of protected due to bad filter chaining methodology
public: 
	virtual void outputString(std::string const sendMe);
	virtual std::auto_ptr<Placeholder> outputPlaceholder();
	virtual void fulfillPlaceholder(
		std::auto_ptr<Placeholder> placeholder,
		std::string const contents
	);

private:
	virtual std::auto_ptr<Instruction> runFilter(
		std::string const & uncommittedBytes,
		size_t newDataOffset,
		size_t readSoFar,
		bool disconnected
	) = 0;
	virtual std::auto_ptr<Instruction> firstInstruction() = 0;

private:
	void setupfirstInstruction() {
		Assert(instruction.get() == NULL);
		Assert(!running);
		running = true;
		instruction = firstInstruction();
		Assert(instruction->commitSize == 0);
		running = false;
	}

private:
	// Putting this all here in the base class interface so the somewhat
	// complex invariants are documented more easily
	void runWrapper(
		std::string const & uncommittedBytes,
		size_t newDataOffset,
		size_t readSoFar,
		bool disconnected
	) {
		Assert((readSoFar > 0) || disconnected);
		Assert(
			!lastReadSoFar.isKnown()
			|| readSoFar >= lastReadSoFar.getKnownValue()
		);
		
		lastReadSoFar = readSoFar;
		size_t newChars = uncommittedBytes.length() - newDataOffset;
		uncommittedChars += newChars;
		Assert(uncommittedBytes.length() == uncommittedChars);
		bytesRead += uncommittedBytes.substr(newDataOffset, std::string::npos);

		Assert(!running);
		running = true;
		std::auto_ptr<Instruction> newInstruction = runFilter(
			uncommittedBytes,
			newDataOffset,
			readSoFar,
			disconnected
		);
		running = false;

		if (disconnected) {
			Assert(newInstruction->type == Instruction::QuitFilter);
		}

		Assert(newInstruction->commitSize <= uncommittedChars);
		uncommittedChars -= newInstruction->commitSize;

		instruction = newInstruction;
	}

public:
	virtual ~Filter() {}
};

#endif
