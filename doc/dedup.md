% dedup(3) 0.9 | Tcl\_Obj value deduplicator
% Cyan Ogilvie
% 0.9


# NAME

dedup - Smart value deduplicator


# SYNOPSIS

**package require dedup** ?0.9?

const char \***Dedup_InitStubs**(*interp*)

struct dedup\_pool \***Dedup_NewPool**(*interp*)

void **Dedup_FreePool**(*poolPtr*)

Tcl\_Obj \***Dedup_NewStringObj**(*poolPtr*, *bytes*, *length*)

void **Dedup_Stats**(*dsPtr*, *poolPtr*)


## ARGUMENTS

Tcl\_Interp \**interp*
:   The Tcl interp to associate the pool with.  May be NULL.

struct dedup\_pool \**poolPtr*
:   The pool that caches the recently hot values.

const char \**bytes*
:   The string to return a Tcl\_Obj for.  As for **Tcl_NewStringObj**.

int	*length*
:   The length in bytes of the string pointed to by *bytes*.  If -1
    then find the length with **strlen**.  As for **Tcl_NewStringObj**.

Tcl\_DString \**dsPtr*
:   DString instance to append the pool stats to.


# DESCRIPTION

Tcl provides a reference counting mechanism for sharing existing values
efficiently and freeing the value when the last user goes away, but there often
arise situations when building C extensions for Tcl where new Tcl\_Objs need to
be built from short strings are reused frequently (such as the column names in
a database result row, the keys in JSON objects and the values themselves, the
XML node names, attribute names and their values, etc.).  But these values are
often not known in advance (to use the Literal mechanism), and it's hard to
predict which values will be useful to share, and not feasible to just share
everything (unbounded cache growth).

This extension provides a simple mechanism for automatically maintaining a
cache of recently used short strings without having to predict them in
advance or manage this cache.  The API is exposed to other C extensions
via a stubs interface, no Tcl API is provided (which would be of questionable
utility).  **Dedup_NewStringObj** is a candicate replacement for nearly any
instance where an extension would normally call **Tcl_NewStringObj**, to
speculatively cache short values and reuse them in the future.

const char \***Dedup_InitStubs**(*interp*)
:   Initialize the stubs wiring.  Should be called in your package's Init
    handler, before any API calls are made to Dedup.

struct dedup\_pool \***Dedup_NewPool**(*interp*)
:   Create a new pool of cached values.  Pools should not be shared across
    threads, and care should be taken when sharing them between interps
    in a single thread to avoid unexpected consequences of sharing Tcl_Objs
    between interps.

void **Dedup_FreePool**(*poolPtr*)
:   Free the pool of cached values.  The values in the cache themselves aren't
    freed, just decrefed, so their other users still hold their valid
    references.

Tcl\_Obj \***Dedup_NewStringObj**(*poolPtr*, *bytes*, *length*)
:   Like **Tcl_NewStringObj** except that the returned Tcl\_Obj may be
    shared, and could have an internal representation other than string.  Its
    string rep will always match what was supplied in *bytes* and *length*
    though.

void **Dedup_Stats**(*dsPtr*, *poolPtr*)
:   Append some text describing the cache stats for *poolPtr* to the
    Tcl\_DString \**dsPtr*.  Users should not attempt to parse the result,
    as the format is intended for human consumption and may change in the
    future without notice.


# DISCUSSION

The primary benefit of applying this mechanism to Tcl\_Objs created from
strings by an extension is performance - firstly because (in the case where
an existing cached value is returned) no memory management is performed,
the cached Tcl\_Obj's reference count is just incremented and the pointer
returned.  This can be a substantial win for an extension such as an XML
parser that will spend much of its time creating Tcl\_Objs for the same
set of strings (element names, attribute names, etc.).  Secondly, the
performance of scripts that use the values can be improved because there
is better re-use of internal representations of those values because the
shared Tcl\_Objs in the cache will likely have shimmered to match the
type they represent in the active code.

There is little downside to just always calling **Dedup_NewStringObj**
in place of **Tcl_NewStringObj** as the former will fall through to the
latter in the cases it can't optimize, and the cache management overhead
is minimal so performance is typically the same as **Tcl_NewStringObj**
in the worst case and materially better in the common case.  One caveat
is that, until Tcl acquires a multi-intrep capable Tcl\_Obj, then common
short values that are ambiguous (and are frequently used in script with
different type interpretations), this mechanism can increase the wasteful
shimmering back and forth between competing intreps since it tends to
cause those values to be shared more widely than before.  If this problem
occurs, an approach to mitigate the impact is to create pools for specific
domains (just the column names for database result set rows, for
instance) as this will tend to limit the scope for the sharing of those
ambiguous values to a single context where the interpretation of their
value is consistent.


# EXAMPLES

In an Expat XML parser, create the node element name strings and attribute
name / value pairs with **Dedup_NewStringObj**, since these are very
likely to be repeated throughout the document being parsed.  Error management
is omitted for clarity:

~~~c
struct parse_context {
    Tcl_Interp          *interp;
    Tcl_Obj             *callback;
    struct dedup_pool   *pool;
};

void pcb_start_element(void *userData,
    const XML_Char *name, const XML_Char **attrs)
{
    struct parse_context *cx = (struct parse_context*)userData;
    Tcl_Obj *cb = NULL;
    const XML_Char **attrib = atts;

    /*
     * Make a copy of the callback command prefix, we're
     * about to append the arguments for this event.
     */
    Tcl_IncrRefCount(cb = Tcl_DuplicateObj(cx->callback));

    /*
     * Append the type of event.  Note that here we don't
     * need to specially manage a literal Tcl_Obj to store
     * the name of the callback.  The dedup cache will take
     * care of sharing this value automatically.
     */
    Tcl_ListObjAppendElement(cx->interp, cb,
            Dedup_NewStringObj(cx->pool, "start_element", -1));

    /*
     * Append the element tag name.  This is very likely to
     * be a string duplicated many times in the XML document
     * and will benefit greatly from the deduplication here
     */
    Tcl_ListObjAppendElement(cx->interp, cb,
            Dedup_NewStringObj(cx->pool, name, -1));

    while (*attrib) {
        /*
         * Append the attribute name.  Also very likely
         * to be repeated throughout the document being
         * parsed.
         */
        Tcl_ListObjAppendElement(cx->interp, cb,
                Dedup_NewStringObj(cx->pool, *attrib++, -1));

        /*
         * Append the attribute's value.  In this case these
         * values are also commonly repeated, and additionally
         * will probably benefit by converting to a useful
         * intrep early in the script and be reused for later
         * instances.  For example, the attribute enabled="true",
         * which will, if treated as a boolean by the script,
         * convert to a boolean intrep and all subsequent instances
         * of the string "true" we encounter here will just
         * reference that same Tcl_Obj.
         */
        Tcl_ListObjAppendElement(cx->interp, cb,
                Dedup_NewStringObj(cx->pool, *attrib++, -1));
    }

    Tcl_EvalObjEx(cx->interp, cb, TCL_EVAL_GLOBAL);

    Tcl_DecrRefCount(cb);
    cb = NULL;
}
~~~


# SEE ALSO

Tcl\_NewStringObj(3)


# LICENSE

This package is Copyright 2021 Cyan Ogilvie, and is made available under the
same license terms as the Tcl Core
