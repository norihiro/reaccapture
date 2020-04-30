#ifndef WRAPPER_OSOBJECT_H
#define WRAPPER_OSOBJECT_H

#include <cstdio>

class OSObject {
	int refcnt;
	public:
		OSObject() { refcnt=1; }
		void retain() { refcnt++; }
		void release() { if(--refcnt<=0) free(); }
		void free() { delete this; }
};

#define IOLog(...) do { fprintf(stderr, __VA_ARGS__); } while (0)

#define OSDeclareDefaultStructors(T) public: T();
#define OSDeclareFinalStructors(T) public: T();
#define OSDefineMetaClassAndStructors(T, S) T::T() { S:T(); }

#endif // WRAPPER_OSOBJECT_H
