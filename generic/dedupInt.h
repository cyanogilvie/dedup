#ifndef _DEDUPINT_H
#define _DEDUPINT_H

#include "dedup.h"
#include "tclstuff.h"

#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __builtin_expect
#	define likely(exp)   __builtin_expect(!!(exp), 1)
#	define unlikely(exp) __builtin_expect(!!(exp), 0)
#else
#	define likely(exp)   (exp)
#	define unlikely(exp) (exp)
#endif

#define STRING_DEDUP_MAX	16	// TODO: Maybe make this a parameter when creating a new pool?

#define MAX_CHAR_LEN_DECIMAL_INTEGER(type) (10*sizeof(type)*CHAR_BIT/33 + 2)

struct kc_entry {
	Tcl_Obj			*val;
	unsigned int	hits;
};

/* Find the best BSF (bit-scan-forward) implementation available:
 * In order of preference:
 *    - __builtin_ffsll     - provided by gcc >= 3.4 and clang >= 5.x
 *    - ffsll               - glibc extension, freebsd libc >= 7.1
 *    - ffs                 - POSIX, but only on int
 * TODO: possibly provide _BitScanForward implementation for Visual Studio >= 2005?
 */
#if defined(HAVE___BUILTIN_FFSLL) || defined(HAVE_FFSLL)
#	define FFS_TMP_STORAGE	/* nothing to declare */
#	if defined(HAVE___BUILTIN_FFSLL)
#		define FFS				__builtin_ffsll
#	else
#		define FFS				ffsll
#	endif
#	define FREEMAP_TYPE		long long
#	define KC_ENTRIES		6*8*sizeof(FREEMAP_TYPE)	// Must be an integer multiple of 8*sizeof(FREEMAP_TYPE)
#elif defined(_MSC_VER) && defined(_WIN64) && _MSC_VER >= 1400
#	define FFS_TMP_STORAGE	unsigned long ix;
/* _BitScanForward64 numbers bits starting with 0, ffsll starts with 1 */
#	define FFS(x)			(_BitScanForward64(&ix, x) ? ix+1 : 0)
#	define FREEMAP_TYPE		long long
#	define KC_ENTRIES		6*8*sizeof(FREEMAP_TYPE)	// Must be an integer multiple of 8*sizeof(FREEMAP_TYPE)
#else
#	define FFS_TMP_STORAGE	/* nothing to declare */
#	define FFS				ffs
#	define FREEMAP_TYPE		int
#	define KC_ENTRIES		12*8*sizeof(FREEMAP_TYPE)	// Must be an integer multiple of 8*sizeof(FREEMAP_TYPE)
#endif

struct dedup_pool {
	Tcl_HashTable	kc;
	int				kc_count;
	FREEMAP_TYPE	freemap[(KC_ENTRIES / (8*sizeof(FREEMAP_TYPE)))+1];	// long long for ffsll
	struct kc_entry	kc_entries[KC_ENTRIES];

	// TODO: copy these in from an interp cx
	Tcl_Obj*		tcl_empty;
};

#endif
