#ifndef WRAPPER_IORETURN_H
#define WRAPPER_IORETURN_H

enum IOReturn_e {
	// TODO: what is success?
	kIOReturnSuccess = 0,
	kIOReturnAborted,
	kIOReturnBadArgument,
	kIOReturnError,
	kIOReturnInternalError,
	kIOReturnInvalid,
	kIOReturnNoMemory,
};
typedef enum IOReturn_e IOReturn;

#endif // WRAPPER_IORETURN_H
