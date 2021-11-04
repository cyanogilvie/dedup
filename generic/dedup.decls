library dedup
interface dedup

declare 0 generic {
	struct dedup_pool* Dedup_NewPool(Tcl_Interp* interp)
}
declare 1 generic {
	void Dedup_FreePool(struct dedup_pool* p)
}
declare 2 generic {
	Tcl_Obj* Dedup_NewStringObj(struct dedup_pool* p, const char* bytes, int length)
}
declare 3 generic {
	void Dedup_Stats(Tcl_DString* ds, struct dedup_pool* p)
}

# vim: ft=tcl foldmethod=marker foldmarker=<<<,>>>
