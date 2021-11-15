#undef USE_TCL_STUBS
#undef USE_DEDUP_STUBS
#define USE_TCL_STUBS 1
#define USE_DEDUP_STUBS 1

#include "dedup.h"

MODULE_SCOPE const DedupStubs*	dedupStubsPtr;
const DedupStubs*					dedupStubsPtr = NULL;

#undef DedupInitializeStubs
MODULE_SCOPE const char* DedupInitializeStubs(Tcl_Interp* interp)
{
	const char*	got = NULL;
	got = Tcl_PkgRequireEx(interp, PACKAGE_NAME, PACKAGE_VERSION, 0, &dedupStubsPtr);
	return got;
}
