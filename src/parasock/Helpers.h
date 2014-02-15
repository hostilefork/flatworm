//
// Helpers.h
//
// Some basic helper routines for Proxlets.
//

#ifndef __HELPERS_H__
#define __HELPERS_H__

#include <winsock2.h>
#include <string>
#include <memory>

inline void Assert(bool condition) {
	if (!condition)
		DebugBreak();
}

inline void NotReached() {
	Assert(false);
}

#define CodeBlock() if(1)
#define REVIEW() if (0)
#define FallThrough() ;
#define Noop() ;

inline size_t SafeSubtractSize(size_t bigger, size_t smaller) {
	Assert(smaller <= bigger);
	return bigger - smaller;
}


inline int strncasecmplen(
	std::string const buf,
	char const * cmp,
	size_t offset = 0,
	size_t * newOffset = NULL
) {
	// no really easy way to do this with std::string due to case insensitity
	// had to write this using strncasecmp

	size_t len = strlen(cmp);
	if (buf.length() - offset < len)
		return -1; // not long enough for substring to exist
	std::string substring = buf.substr(offset, len);
	int ret = _strnicmp(cmp, substring.c_str(), len);
	if (ret == 0 && newOffset != NULL)
		*newOffset = offset + len;
	return ret;
}


// http://www.codeguru.com/forum/archive/index.php/t-321849.html
inline std::string TrimStr(const std::string& Src, const std::string& c = " \r\n")
{
	size_t p2 = Src.find_last_not_of(c);
	if (p2 == std::string::npos)
		return std::string();
	size_t p1 = Src.find_first_not_of(c);
	if (p1 == std::string::npos)
			p1 = 0;
	return Src.substr(p1, (p2-p1)+1);
}


class UnknownType {
public:
	UnknownType() {}
	~UnknownType() {}
};
extern UnknownType UNKNOWN;


// There are many places where lengths can be "known" or "unknown"
// Using -1 to indicate an unknown size makes sizes signed when they
// really should be positive values.
//
// This is safer because at least you will get caught if you try to use
// an "unknown" size in a place where a known size was needed.
//
// (2014 update: I did not know about boost::optional when I came up with
// this, six years prior!)
//
template<class T> class Knowable {
private:
	T t;
	bool known;
public:
	Knowable(T t) : t (t), known (true) {}
	Knowable(UnknownType& dummy) : known (false) {}
	bool isKnown() const { return known; }
	bool isUnknown() const { return !known; }
	bool isKnownToBe(T t) const {
		if (!known) {
			return false;
		}
		return this->t == t;
	} 
	T getKnownValue() const {
		if (known) {
			return t; 
		}
		NotReached();
		return t;
	}
	void setKnownValue(T t) { known = true; this->t = t;}
	void forget() { known = false; }
	~Knowable() { }
};


#define pthread_mutex_lock(x) EnterCriticalSection(x)
#define pthread_mutex_unlock(x) LeaveCriticalSection(x)
typedef unsigned (__stdcall *BEGINTHREADFUNC)(void *);
#pragma warning (disable : 4996)

#endif
