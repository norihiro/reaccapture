#ifndef WRAPPER_IOCOMMANDGATE_H
#define WRAPPER_IOCOMMANDGATE_H

#include <IOKit/IOReturn.h>

typedef void IOCommandGate;

struct IOWorkLoop
{
	// TODO: move these functions to OSObject
	void retain() {}
	void release() {}
	IOReturn addEventSource(IOCommandGate*) { return kIOReturnSuccess; }
};

typedef void IOTimerEventSource;

#endif // WRAPPER_IOCOMMANDGATE_H
