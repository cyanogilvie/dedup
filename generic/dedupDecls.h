
/* !BEGIN!: Do not edit below this line. */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Exported function declarations:
 */

/* 0 */
EXTERN struct dedup_pool* Dedup_NewPool(Tcl_Interp*interp);
/* 1 */
EXTERN void		Dedup_FreePool(struct dedup_pool*p);
/* 2 */
EXTERN Tcl_Obj*		Dedup_NewStringObj(struct dedup_pool*p,
				const char*bytes, int length);
/* 3 */
EXTERN void		Dedup_Stats(Tcl_DString*ds, struct dedup_pool*p);

typedef struct DedupStubs {
    int magic;
    void *hooks;

    struct dedup_pool* (*dedup_NewPool) (Tcl_Interp*interp); /* 0 */
    void (*dedup_FreePool) (struct dedup_pool*p); /* 1 */
    Tcl_Obj* (*dedup_NewStringObj) (struct dedup_pool*p, const char*bytes, int length); /* 2 */
    void (*dedup_Stats) (Tcl_DString*ds, struct dedup_pool*p); /* 3 */
} DedupStubs;

extern const DedupStubs *dedupStubsPtr;

#ifdef __cplusplus
}
#endif

#if defined(USE_DEDUP_STUBS)

/*
 * Inline function declarations:
 */

#define Dedup_NewPool \
	(dedupStubsPtr->dedup_NewPool) /* 0 */
#define Dedup_FreePool \
	(dedupStubsPtr->dedup_FreePool) /* 1 */
#define Dedup_NewStringObj \
	(dedupStubsPtr->dedup_NewStringObj) /* 2 */
#define Dedup_Stats \
	(dedupStubsPtr->dedup_Stats) /* 3 */

#endif /* defined(USE_DEDUP_STUBS) */

/* !END!: Do not edit above this line. */
