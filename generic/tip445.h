#ifndef _TIP445_H
#define _TIP445_H

#if TIP445_SHIM

/* Just enough of TIP445 to build rl_json on Tcl 8.6 */

#ifndef Tcl_ObjIntRep
typedef union Tcl_ObjIntRep {
	struct {
		void*	ptr1;
		void*	ptr2;
	} twoPtrValue;
	struct {
		void*			ptr;
		unsigned long	value;
	} ptrAndLongRep;
} Tcl_ObjIntRep;
#endif

#ifndef Tcl_FetchIntRep
#define Tcl_FetchIntRep(obj, type)		(Tcl_ObjIntRep*)(((obj)->typePtr == (type)) ? &(obj)->internalRep : NULL)
#endif

#ifndef Tcl_FreeIntRep
static inline void Tcl_FreeIntRep(Tcl_Obj* obj)
{
	if (obj->typePtr && obj->typePtr->freeIntRepProc)
		obj->typePtr->freeIntRepProc(obj);
}
#endif

#ifndef Tcl_StoreIntRep
static inline void Tcl_StoreIntRep(Tcl_Obj* objPtr, const Tcl_ObjType* typePtr, const Tcl_ObjIntRep* irPtr)
{
	objPtr->typePtr = typePtr;
	memcpy(&objPtr->internalRep, irPtr, sizeof(Tcl_ObjIntRep));
}
#endif

#ifndef Tcl_HasStringRep
#define Tcl_HasStringRep(obj)	((obj)->bytes != NULL)
#endif

#endif

#endif
