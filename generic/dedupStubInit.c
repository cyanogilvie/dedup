#include "dedup.h"

/* !BEGIN!: Do not edit below this line. */

const DedupStubs dedupStubs = {
    TCL_STUB_MAGIC,
    0,
    Dedup_NewPool, /* 0 */
    Dedup_FreePool, /* 1 */
    Dedup_NewStringObj, /* 2 */
    Dedup_Stats, /* 3 */
};

/* !END!: Do not edit above this line. */


const DedupStubs* DedupStubsPtr = NULL;
MODULE_SCOPE const DedupStubs* const dedupConstStubsPtr;
const DedupStubs* const dedupConstStubsPtr = &dedupStubs;
