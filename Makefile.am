
bin_PROGRAMS = reaccapture


# TODO: should I include REACConnection?
#	mac-reacdriver/REACConnection.cpp

reaccapture_SOURCES = \
	mac-reacdriver/REACConstants.cpp \
	src/reaccapture.cc \
	src/reacsocket.cc \
	src/ringbuf.cc \
	src/pcaploader.cc

nobase_include_HEADERS = \
	src/settings.h \
	src/reacsocket.h \
	src/ringbuf.h \
	src/constants.h \
	mac-reacdriver/REACConstants.h \
	mac-reacdriver/REACDataStream.h \
	mac-reacdriver/EthernetHeader.h \
	mac-wrapper/libkern/OSTypes.h \
	mac-wrapper/libkern/c++/OSObject.h \
	mac-wrapper/libkern/c++/OSArray.h \
	mac-wrapper/IOKit/IOReturn.h

AM_CPPFLAGS = -I$(top_srcdir)/mac-wrapper
