#ifndef WRAPPER_OSARRAY_H
#define WRAPPER_OSARRAY_H

#include <libkern/c++/OSObject.h>
#include <deque>

class OSArray : public OSObject
{
	std::deque <OSObject*> data;

	public:
		static OSArray* withCapacity(int n) {
			OSArray *p = new OSArray;
			p->data.resize(n);
			return p;
		}
		int getCount() const { return data.size(); }
		OSObject * getObject(int i) { return data[i]; }
		void removeObject(int i) { data.erase(data.begin()+i); }
		int setObject(OSObject *o) {
			return -1; // failure
			// TODO: implement me.
		}
};

#endif // WRAPPER_OSARRAY_H
