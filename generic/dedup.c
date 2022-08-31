#include "dedupInt.h"

#define ASSOC	"dedup_pools"

Tcl_Mutex	g_config_mutex = NULL;
Tcl_Obj*	g_packagedir = NULL;
Tcl_Obj*	g_includedir = NULL;
static Tcl_Config cfg[] = {
	{"libdir,runtime",			DEDUP_LIBRARY_PATH_INSTALL},	// Overwritten by _setdir, must be first
	{"includedir,runtime",		DEDUP_INCLUDE_PATH_INSTALL},	// Overwritten by _setdir, must be second
	{"packagedir,runtime",		DEDUP_PACKAGE_PATH_INSTALL},	// Overwritten by _setdir, must be third
	{"libdir,install",			DEDUP_LIBRARY_PATH_INSTALL},
	{"includedir,install",		DEDUP_INCLUDE_PATH_INSTALL},
	{"packagedir,install",		DEDUP_PACKAGE_PATH_INSTALL},
	{"library",					DEDUP_LIBRARY},
	{"stublib",					DEDUP_STUBLIB},
	{"header",					DEDUP_HEADER},
	{NULL, NULL}
};

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
static void free_interp_cx(ClientData cdata, Tcl_Interp* interp) //<<<
{
	struct interp_cx*	l = cdata;
	Tcl_DictSearch	search;
	Tcl_Obj*		k = NULL;
	Tcl_Obj*		v = NULL;
	int				done;

	if (l) {
		if (l->pools) {
			if (TCL_OK == Tcl_DictObjFirst(interp, l->pools, &search, &k, &v, &done)) {
				for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
					Tcl_WideInt	w;
					struct dedup_pool*	p = NULL;

					if (TCL_OK == Tcl_GetWideIntFromObj(NULL, k, &w)) continue;
					p = (struct dedup_pool*)w;
					Dedup_FreePool(p);
				}
				Tcl_DictObjDone(&search);
			}
			replace_tclobj(&l->pools, NULL);
		}

		replace_tclobj(&l->empty, NULL);

		ckfree(l);
		l = NULL;
	}
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
	new_pool->interp = interp;

	if (interp) {
		struct interp_cx*	l = Tcl_GetAssocData(interp, ASSOC, NULL);
		Tcl_Obj*			poolkey = NULL;

		replace_tclobj(&new_pool->tcl_empty, l->empty);
		replace_tclobj(&poolkey, Tcl_NewWideIntObj((Tcl_WideInt)new_pool));
		if (Tcl_IsShared(l->pools)) replace_tclobj(&l->pools, Tcl_DuplicateObj(l->pools));
		if (TCL_OK != Tcl_DictObjPut(NULL, l->pools, poolkey, l->empty))
			Tcl_Panic("Failed to record pool in interp pool registry");
		replace_tclobj(&poolkey, NULL);
	} else {
		replace_tclobj(&new_pool->tcl_empty, Tcl_NewObj());
	}

	return new_pool;
}

//>>>
void Dedup_FreePool(struct dedup_pool* p) //<<<
{
	Tcl_HashEntry*		he;
	Tcl_HashSearch		search;
	struct kc_entry*	e;

	if (p->interp) {
		struct interp_cx*	l = Tcl_GetAssocData(p->interp, ASSOC, NULL);
		Tcl_Obj*			poolkey = NULL;

		replace_tclobj(&poolkey, Tcl_NewWideIntObj((Tcl_WideInt)p));
		if (Tcl_IsShared(l->pools)) replace_tclobj(&l->pools, Tcl_DuplicateObj(l->pools));
		if (TCL_OK != Tcl_DictObjRemove(NULL, l->pools, poolkey))
			Tcl_Panic("Failed to remove pool from interp pool registry");
		replace_tclobj(&poolkey, NULL);
	}

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

	p->interp = NULL;

	ckfree(p); p = NULL;
}

//>>>
Tcl_Obj* Dedup_NewStringObj(struct dedup_pool* p, const char* bytes, int length) //<<<
{
	char				buf[STRING_DEDUP_MAX + 1];
	const char*			keyname;
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
	int					entries = 0;
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
	numbuf_len = sprintf(numbuf, "%4d", entries);
	Tcl_DStringAppend(ds, numbuf, numbuf_len);
}

//>>>
//>>>
// Script API <<<
#define NS	"::" PACKAGE_NAME

static OBJCMD(setdir_cmd) //<<<
{
	int				code = TCL_OK;
	Tcl_Obj*		buildheader = NULL;
	Tcl_Obj*		generic = NULL;
	Tcl_Obj*		h = NULL;
	Tcl_StatBuf*	stat = NULL;

	CHECK_ARGS(1, "dir");

	Tcl_MutexLock(&g_config_mutex);

	replace_tclobj(&g_packagedir, Tcl_FSGetNormalizedPath(interp, objv[1]));

	replace_tclobj(&generic,	Tcl_NewStringObj("generic", -1));
	replace_tclobj(&h,			Tcl_NewStringObj("dedup.h", -1));
	replace_tclobj(&buildheader, Tcl_FSJoinToPath(g_packagedir, 2, (Tcl_Obj*[]){generic, h}));

	stat = Tcl_AllocStatBuf();
	if (0 == Tcl_FSStat(buildheader, stat)) {
		// Running from build dir
		replace_tclobj(&g_includedir, Tcl_FSJoinToPath(g_packagedir, 1, (Tcl_Obj*[]){generic}));
	} else {
		replace_tclobj(&g_includedir, g_packagedir);
	}

	cfg[0].value = Tcl_GetString(g_packagedir);		// Under global ref
	cfg[1].value = Tcl_GetString(g_includedir);		// Under global ref
	cfg[2].value = Tcl_GetString(g_packagedir);		// Under global ref

	Tcl_RegisterConfig(interp, PACKAGE_NAME, cfg, "utf-8");

//done:
	Tcl_MutexUnlock(&g_config_mutex);
	replace_tclobj(&buildheader, NULL);
	replace_tclobj(&generic, NULL);
	replace_tclobj(&h, NULL);
	if (stat) {
		ckfree(stat);
		stat = NULL;
	}

	return code;
}

//>>>
static void delete_poolinst(ClientData cdata) //<<<
{
	struct dedup_pool*	p = cdata;

	Dedup_FreePool(p);
}

//>>>
static OBJCMD(poolinst_cmd) //<<<
{
	int					code = TCL_OK;
	static const char*	ops[] = {
		"get",
		"stats",
		NULL
	};
	enum {
		POOLINST_GET,
		POOLINST_STATS
	};
	int					op;
	struct dedup_pool*	p = cdata;

	if (objc < 2)
		CHECK_ARGS(1, "op arg...");

	TEST_OK_LABEL(done, code, Tcl_GetIndexFromObj(interp, objv[1], ops, "op", TCL_EXACT, &op));
	switch (op) {
		case POOLINST_GET:
			{
				enum args {
					A_STR=2,
					A_objc
				};

				if (objc != A_objc) {
					Tcl_WrongNumArgs(interp, 2, objv, "str");
					code = TCL_ERROR;
					goto done;
				}

				int			len;
				const char*	str = Tcl_GetStringFromObj(objv[A_STR], &len);

				Tcl_SetObjResult(interp, Dedup_NewStringObj(p, str, len));
			}
			break;
		case POOLINST_STATS:
			{
				Tcl_DString	stats;

				Tcl_DStringInit(&stats);
				Dedup_Stats(&stats, p);
				Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_DStringValue(&stats), Tcl_DStringLength(&stats)));
				Tcl_DStringFree(&stats);
			}
			break;
	}

done:
	return code;
}

//>>>
static OBJCMD(pool_cmd) //<<<
{
	int					code = TCL_OK;
	struct dedup_pool*	p = NULL;

	CHECK_ARGS(1, "pool_name");

	p = Dedup_NewPool(interp);

	Tcl_CreateObjCommand(interp, Tcl_GetString(objv[1]), poolinst_cmd, p, delete_poolinst);

done:
	return code;
}

//>>>

static struct cmd {
	char*			name;
	Tcl_ObjCmdProc*	proc;
} cmds[] = {
	{NS "::_setdir",	setdir_cmd},
	{NS "::_pool",		pool_cmd},
	{NULL, NULL}
};
// Script API >>>

extern const DedupStubs* const dedupConstStubsPtr;

#ifdef __cplusplus
extern "C" {
#endif
DLLEXPORT int Dedup_Init(Tcl_Interp* interp) //<<<
{
	int			code = TCL_OK;
	struct cmd*	c = NULL;

#if USE_TCL_STUBS
	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL)
		return TCL_ERROR;
#endif

	c = cmds;
	while (c->name) {
		if (NULL == Tcl_CreateObjCommand(interp, c->name, c->proc, NULL, NULL)) {
			Tcl_SetObjResult(interp, Tcl_ObjPrintf("Could not create command %s", c->name));
			code = TCL_ERROR;
			goto finally;
		}
		c++;
	}

	struct interp_cx* l = ckalloc(sizeof *l);
	*l = (struct interp_cx){0};
	Tcl_SetAssocData(interp, ASSOC, free_interp_cx, l);
	replace_tclobj(&l->pools, Tcl_NewDictObj());
	replace_tclobj(&l->empty, Tcl_NewObj());

	TEST_OK_LABEL(finally, code, Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION, dedupConstStubsPtr));

finally:
	return code;
}

//>>>
DLLEXPORT int Dedup_Unload(Tcl_Interp* interp, int flags) //<<<
{
	int					code = TCL_OK;
	Tcl_Namespace*		ns = NULL;

	ns = Tcl_FindNamespace(interp, NS, NULL, TCL_GLOBAL_ONLY);

	Tcl_DeleteAssocData(interp, ASSOC);

	if (flags == TCL_UNLOAD_DETACH_FROM_PROCESS) {
		Tcl_MutexLock(&g_config_mutex);
		replace_tclobj(&g_packagedir, NULL);
		replace_tclobj(&g_includedir, NULL);
		Tcl_MutexUnlock(&g_config_mutex);
		Tcl_MutexFinalize(&g_config_mutex);
	}

	if (ns) {
		Tcl_RegisterConfig(interp, PACKAGE_NAME, (Tcl_Config[]){{NULL, NULL}}, "utf-8");
		Tcl_DeleteNamespace(ns);
		ns = NULL;
	}

	return code;
}

//>>>
#ifdef __cplusplus
}
#endif

// vim: foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4
