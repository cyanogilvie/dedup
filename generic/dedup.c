#include "dedupInt.h"

// Internal API <<<
static int first_free(FREEMAP_TYPE* freemap) //<<<
{
	int	i=0, bit, res;
	FFS_TMP_STORAGE;

	while ((bit = FFS(freemap[i])) == 0) i++;
	res = i * (sizeof(FREEMAP_TYPE)*8) + (bit-1);
	return res;
}

//>>>
static void mark_used(FREEMAP_TYPE* freemap, int idx) //<<<
{
	int	i = idx / (sizeof(FREEMAP_TYPE)*8);
	int bit = idx - (i * (sizeof(FREEMAP_TYPE)*8));
	freemap[i] &= ~(1LL << bit);
}

//>>>
static void mark_free(FREEMAP_TYPE* freemap, int idx) //<<<
{
	int	i = idx / (sizeof(FREEMAP_TYPE)*8);
	int bit = idx - (i * (sizeof(FREEMAP_TYPE)*8));
	freemap[i] |= 1LL << bit;
}

//>>>
void free_dedup_tclempty(ClientData cdata, Tcl_Interp* interp) //<<<
{
	Tcl_DecrRefCount((Tcl_Obj*)cdata);	// AssocData ref
	cdata = NULL;
}

//>>>
static void age_cache(struct dedup_pool* p) //<<<
{
	Tcl_HashEntry*		he;
	Tcl_HashSearch		search;
	struct kc_entry*	e;

	he = Tcl_FirstHashEntry(&p->kc, &search);
	while (he) {
		ptrdiff_t	idx = (ptrdiff_t)Tcl_GetHashValue(he);

		//if (idx >= KC_ENTRIES) Tcl_Panic("age_cache: idx (%ld) is out of bounds, KC_ENTRIES: %d", idx, KC_ENTRIES);
		//printf("age_cache: kc_count: %d", p->kc_count);
		e = &p->kc_entries[idx];

		if (e->hits < 1) {
			Tcl_DeleteHashEntry(he);
			Tcl_DecrRefCount(e->val);
			Tcl_DecrRefCount(e->val);	// Two references - one for the cache table and one on loan to callers' interim processing
			mark_free(p->freemap, idx);
			e->val = NULL;
		} else {
			e->hits >>= 1;
		}
		he = Tcl_NextHashEntry(&search);
	}
	p->kc_count = 0;
}

//>>>
//>>>
// Stubs API <<<
struct dedup_pool* Dedup_NewPool(Tcl_Interp* interp /* may be NULL */) //<<<
{
	struct dedup_pool*	new_pool = NULL;

	new_pool = ckalloc(sizeof(*new_pool));
	Tcl_InitHashTable(&new_pool->kc, TCL_STRING_KEYS);
	new_pool->kc_count = 0;
	memset(&new_pool->freemap, 0xFF, sizeof(new_pool->freemap));
	new_pool->tcl_empty = NULL;

	if (interp)
		replace_tclobj(&new_pool->tcl_empty, Tcl_GetAssocData(interp, "dedup_tclempty", NULL));

	if (new_pool->tcl_empty == NULL) {
		replace_tclobj(&new_pool->tcl_empty, Tcl_NewObj());

		if (interp) {
			Tcl_IncrRefCount(new_pool->tcl_empty);	// AssocData ref
			Tcl_SetAssocData(interp, "dedup_tclempty", free_dedup_tclempty, new_pool->tcl_empty);
		}
	}

	return new_pool;
}

//>>>
void Dedup_FreePool(struct dedup_pool* p) //<<<
{
	Tcl_HashEntry*		he;
	Tcl_HashSearch		search;
	struct kc_entry*	e;

	he = Tcl_FirstHashEntry(&p->kc, &search);
	while (he) {
		ptrdiff_t	idx = (ptrdiff_t)Tcl_GetHashValue(he);

		//if (idx >= KC_ENTRIES) Tcl_Panic("age_cache: idx (%ld) is out of bounds, KC_ENTRIES: %d", idx, KC_ENTRIES);
		//printf("age_cache: kc_count: %d", p->kc_count);
		e = &p->kc_entries[idx];

		Tcl_DeleteHashEntry(he);
		Tcl_DecrRefCount(e->val);
		Tcl_DecrRefCount(e->val);	// Two references - one for the cache table and one on loan to callers' interim processing
		mark_free(p->freemap, idx);
		e->val = NULL;
		he = Tcl_NextHashEntry(&search);
	}
	Tcl_DeleteHashTable(&p->kc);
	p->kc_count = 0;
	replace_tclobj(&p->tcl_empty, NULL);

	ckfree(p); p = NULL;
}

//>>>
Tcl_Obj* Dedup_NewStringObj(struct dedup_pool* p, const char* bytes, int length) //<<<
{
	char				buf[STRING_DEDUP_MAX + 1];
	const char			*keyname;
	int					is_new;
	struct kc_entry*	kce;
	Tcl_Obj*			out;
	Tcl_HashEntry*		entry = NULL;

	if (p == NULL)
		return Tcl_NewStringObj(bytes, length);

	if (length == 0) {
		return p->tcl_empty;
	} else if (length < 0) {
		length = strlen(bytes);
	}

	if (length > STRING_DEDUP_MAX)
		return Tcl_NewStringObj(bytes, length);

	if (likely(bytes[length] == 0)) {
		keyname = bytes;
	} else {
		memcpy(buf, bytes, length);
		buf[length] = 0;
		keyname = buf;
	}
	entry = Tcl_CreateHashEntry(&p->kc, keyname, &is_new);

	if (is_new) {
		ptrdiff_t	idx = first_free(p->freemap);

		if (unlikely(idx >= KC_ENTRIES)) {
			// Cache overflow
			Tcl_DeleteHashEntry(entry);
			age_cache(p);
			return Tcl_NewStringObj(bytes, length);
		}

		kce = &p->kc_entries[idx];
		kce->hits = 0;
		out = kce->val = Tcl_NewStringObj(bytes, length);
		Tcl_IncrRefCount(out);	// Two references - one for the cache table and one on loan to callers' interim processing.
		Tcl_IncrRefCount(out);	// Without this, values not referenced elsewhere could reach callers with refCount 1, allowing
								// the value to be mutated in place and corrupt the state of the cache (hash key not matching obj value)

		mark_used(p->freemap, idx);

		Tcl_SetHashValue(entry, (void*)idx);
		p->kc_count++;

		if (unlikely(p->kc_count > (int)(KC_ENTRIES/2.5))) {
			kce->hits++; // Prevent the just-created entry from being pruned
			age_cache(p);
		}
	} else {
		ptrdiff_t	idx = (ptrdiff_t)Tcl_GetHashValue(entry);

		kce = &p->kc_entries[idx];
		out = kce->val;
		if (kce->hits < 255) kce->hits++;
	}

	return out;
}

//>>>
void Dedup_Stats(Tcl_DString* ds, struct dedup_pool* p) //<<<
{
	Tcl_HashEntry*		he;
	Tcl_HashSearch		search;
	struct kc_entry*	e;
	int					entries;
	char				numbuf[MAX_CHAR_LEN_DECIMAL_INTEGER(uint64_t)+1];
	int					numbuf_len;

	he = Tcl_FirstHashEntry(&p->kc, &search);
	while (he) {
		ptrdiff_t	idx = (ptrdiff_t)Tcl_GetHashValue(he);

		//if (idx >= KC_ENTRIES) Tcl_Panic("age_cache: idx (%ld) is out of bounds, KC_ENTRIES: %d", idx, KC_ENTRIES);
		//printf("age_cache: kc_count: %d", p->kc_count);
		e = &p->kc_entries[idx];

		Tcl_DStringAppend(ds, "refCount: ", -1);
		numbuf_len = sprintf(numbuf, "%4d", e->val->refCount);
		Tcl_DStringAppend(ds, numbuf, numbuf_len);
		Tcl_DStringAppend(ds, ", heat: ", -1);
		numbuf_len = sprintf(numbuf, "%4d", e->hits);
		Tcl_DStringAppend(ds, numbuf, numbuf_len);
		Tcl_DStringAppend(ds, ", \"", -1);
		Tcl_DStringAppend(ds, Tcl_GetString(e->val), -1);
		Tcl_DStringAppend(ds, "\"\n", -1);
		he = Tcl_NextHashEntry(&search);
		entries++;
	}
	Tcl_DStringAppend(ds, "entries: ", -1);
	numbuf_len = sprintf(numbuf, "%4d", e->hits);
	Tcl_DStringAppend(ds, numbuf, numbuf_len);
}

//>>>
//>>>

extern const DedupStubs* const dedupConstStubsPtr;

#ifdef __cplusplus
extern "C" {
#endif
DLLEXPORT int Dedup_Init(Tcl_Interp* interp)
{
	int		code = TCL_OK;

#if USE_TCL_STUBS
	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL)
		return TCL_ERROR;
#endif

	//fprintf(stderr, "Dedup_Init, providing package version (%s) with stubsPtr: %p\n", PACKAGE_VERSION, dedupConstStubsPtr);
	code = Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION, dedupConstStubsPtr);
	if (code != TCL_OK) goto finally;

finally:
	return code;
}

#ifdef __cplusplus
}
#endif

// vim: foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4
