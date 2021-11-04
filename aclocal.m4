#
# Include the TEA standard macro set
#

builtin(include,tclconfig/tcl.m4)

#
# Add here whatever m4 macros you want to define for your package
#
builtin(include,ax_gcc_builtin.m4)
builtin(include,ax_cc_for_build.m4)

# All the best stuff seems to be Linux / glibc specific :(
AC_DEFUN([CHECK_GLIBC], [
	AC_MSG_CHECKING([for GNU libc])
	AC_TRY_COMPILE([#include <features.h>], [
#if ! (defined __GLIBC__ || defined __GNU_LIBRARY__)
#	error "Not glibc"
#endif
], glibc=yes, glibc=no)

	if test "$glibc" = yes
	then
		AC_DEFINE(_GNU_SOURCE, 1, [Always define _GNU_SOURCE when using glibc])
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
	])

AC_DEFUN([CygPath],[`${CYGPATH} $1`])

AC_DEFUN([TEAX_CONFIG_INCLUDE_LINE], [
	eval "$1=\"-I[]CygPath($2)\""
	AC_SUBST($1)
])

AC_DEFUN([TEAX_CONFIG_LINK_LINE], [
	AS_IF([test ${TCL_LIB_VERSIONS_OK} = nodots], [
		eval "$1=\"-L[]CygPath($2) -l$3${TCL_TRIM_DOTS}\""
	], [
		eval "$1=\"-L[]CygPath($2) -l$3${PACKAGE_VERSION}\""
	])
	AC_SUBST($1)
])

AC_DEFUN([TIP445], [
	AC_MSG_CHECKING([whether we need to polyfill TIP 445])
	saved_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $TCL_INCLUDE_SPEC"
	AC_TRY_COMPILE([#include <tcl.h>], [Tcl_ObjIntRep ir;],
	    have_tcl_objintrep=yes, have_tcl_objintrep=no)
	CFLAGS="$saved_CFLAGS"

	if test "$have_tcl_objintrep" = yes; then
		AC_DEFINE(TIP445_SHIM, 0, [Do we need to polyfill TIP 445?])
		AC_MSG_RESULT([no])
	else
		AC_DEFINE(TIP445_SHIM, 1, [Do we need to polyfill TIP 445?])
		AC_MSG_RESULT([yes])
	fi
])

