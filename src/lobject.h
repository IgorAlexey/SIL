/*
** $Id: lobject.h $
** Type definitions for SIL objects
** See Copyright Notice in sil.h
*/


#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "sil.h"


/*
** Extra types for collectable non-values
*/
#define SIL_TUPVAL	SIL_NUMTYPES  /* upvalues */
#define SIL_TPROTO	(SIL_NUMTYPES+1)  /* function prototypes */
#define SIL_TDEADKEY	(SIL_NUMTYPES+2)  /* removed keys in tables */



/*
** number of all possible types (including SIL_TNONE but excluding DEADKEY)
*/
#define SIL_TOTALTYPES		(SIL_TPROTO + 2)


/*
** tags for Tagged Values have the following use of bits:
** bits 0-3: actual tag (a SIL_T* constant)
** bits 4-5: variant bits
** bit 6: whether value is collectable
*/

/* add variant bits to a type */
#define makevariant(t,v)	((t) | ((v) << 4))



/*
** Union of all SIL values
*/
typedef union Value {
  struct GCObject *gc;    /* collectable objects */
  void *p;         /* light userdata */
  sil_CFunction f; /* light C functions */
  sil_Integer i;   /* integer numbers */
  sil_Number n;    /* float numbers */
  /* not used, but may avoid warnings for uninitialized value */
  lu_byte ub;
} Value;


/*
** Tagged Values. This is the basic representation of values in SIL:
** an actual value plus a tag with its type.
*/

#define TValuefields	Value value_; lu_byte tt_

typedef struct TValue {
  TValuefields;
} TValue;


#define val_(o)		((o)->value_)
#define valraw(o)	(val_(o))


/* raw type tag of a TValue */
#define rawtt(o)	((o)->tt_)

/* tag with no variants (bits 0-3) */
#define novariant(t)	((t) & 0x0F)

/* type tag of a TValue (bits 0-3 for tags + variant bits 4-5) */
#define withvariant(t)	((t) & 0x3F)
#define ttypetag(o)	withvariant(rawtt(o))

/* type of a TValue */
#define ttype(o)	(novariant(rawtt(o)))


/* Macros to test type */
#define checktag(o,t)		(rawtt(o) == (t))
#define checktype(o,t)		(ttype(o) == (t))


/* Macros for internal tests */

/* collectable object has the same tag as the original value */
#define righttt(obj)		(ttypetag(obj) == gcvalue(obj)->tt)

/*
** Any value being manipulated by the program either is non
** collectable, or the collectable object has the right tag
** and it is not dead. The option 'L == NULL' allows other
** macros using this one to be used where L is not available.
*/
#define checkliveness(L,obj) \
	((void)L, sil_longassert(!iscollectable(obj) || \
		(righttt(obj) && (L == NULL || !isdead(G(L),gcvalue(obj))))))


/* Macros to set values */

/* set a value's tag */
#define settt_(o,t)	((o)->tt_=(t))


/* main macro to copy values (from 'obj2' to 'obj1') */
#define setobj(L,obj1,obj2) \
	{ TValue *io1=(obj1); const TValue *io2=(obj2); \
          io1->value_ = io2->value_; settt_(io1, io2->tt_); \
	  checkliveness(L,io1); sil_assert(!isnonstrictnil(io1)); }

/*
** Different types of assignments, according to source and destination.
** (They are mostly equal now, but may be different in the future.)
*/

/* from stack to stack */
#define setobjs2s(L,o1,o2)	setobj(L,s2v(o1),s2v(o2))
/* to stack (not from same stack) */
#define setobj2s(L,o1,o2)	setobj(L,s2v(o1),o2)
/* from table to same table */
#define setobjt2t	setobj
/* to new object */
#define setobj2n	setobj
/* to table */
#define setobj2t	setobj


/*
** Entries in a SIL stack. Field 'tbclist' forms a list of all
** to-be-closed variables active in this stack. Dummy entries are
** used when the distance between two tbc variables does not fit
** in an unsigned short. They are represented by delta==0, and
** their real delta is always the maximum value that fits in
** that field.
*/
typedef union StackValue {
  TValue val;
  struct {
    TValuefields;
    unsigned short delta;
  } tbclist;
} StackValue;


/* index to stack elements */
typedef StackValue *StkId;


/*
** When reallocating the stack, change all pointers to the stack into
** proper offsets.
*/
typedef union {
  StkId p;  /* actual pointer */
  ptrdiff_t offset;  /* used while the stack is being reallocated */
} StkIdRel;


/* convert a 'StackValue' to a 'TValue' */
#define s2v(o)	(&(o)->val)



/*
** {==================================================================
** Nil
** ===================================================================
*/

/* Standard nil */
#define SIL_VNIL	makevariant(SIL_TNIL, 0)

/* Empty slot (which might be different from a slot containing nil) */
#define SIL_VEMPTY	makevariant(SIL_TNIL, 1)

/* Value returned for a key not found in a table (absent key) */
#define SIL_VABSTKEY	makevariant(SIL_TNIL, 2)

/* Special variant to signal that a fast get is accessing a non-table */
#define SIL_VNOTABLE    makevariant(SIL_TNIL, 3)


/* macro to test for (any kind of) nil */
#define ttisnil(v)		checktype((v), SIL_TNIL)

/*
** Macro to test the result of a table access. Formally, it should
** distinguish between SIL_VEMPTY/SIL_VABSTKEY/SIL_VNOTABLE and
** other tags. As currently nil is equivalent to SIL_VEMPTY, it is
** simpler to just test whether the value is nil.
*/
#define tagisempty(tag)		(novariant(tag) == SIL_TNIL)


/* macro to test for a standard nil */
#define ttisstrictnil(o)	checktag((o), SIL_VNIL)


#define setnilvalue(obj) settt_(obj, SIL_VNIL)


#define isabstkey(v)		checktag((v), SIL_VABSTKEY)


/*
** macro to detect non-standard nils (used only in assertions)
*/
#define isnonstrictnil(v)	(ttisnil(v) && !ttisstrictnil(v))


/*
** By default, entries with any kind of nil are considered empty.
** (In any definition, values associated with absent keys must also
** be accepted as empty.)
*/
#define isempty(v)		ttisnil(v)


/* macro defining a value corresponding to an absent key */
#define ABSTKEYCONSTANT		{NULL}, SIL_VABSTKEY


/* mark an entry as empty */
#define setempty(v)		settt_(v, SIL_VEMPTY)



/* }================================================================== */


/*
** {==================================================================
** Booleans
** ===================================================================
*/


#define SIL_VFALSE	makevariant(SIL_TBOOLEAN, 0)
#define SIL_VTRUE	makevariant(SIL_TBOOLEAN, 1)

#define ttisboolean(o)		checktype((o), SIL_TBOOLEAN)
#define ttisfalse(o)		checktag((o), SIL_VFALSE)
#define ttistrue(o)		checktag((o), SIL_VTRUE)


#define l_isfalse(o)	(ttisfalse(o) || ttisnil(o))
#define tagisfalse(t)	((t) == SIL_VFALSE || novariant(t) == SIL_TNIL)



#define setbfvalue(obj)		settt_(obj, SIL_VFALSE)
#define setbtvalue(obj)		settt_(obj, SIL_VTRUE)

/* }================================================================== */


/*
** {==================================================================
** Threads
** ===================================================================
*/

#define SIL_VTHREAD		makevariant(SIL_TTHREAD, 0)

#define ttisthread(o)		checktag((o), ctb(SIL_VTHREAD))

#define thvalue(o)	check_exp(ttisthread(o), gco2th(val_(o).gc))

#define setthvalue(L,obj,x) \
  { TValue *io = (obj); sil_State *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(SIL_VTHREAD)); \
    checkliveness(L,io); }

#define setthvalue2s(L,o,t)	setthvalue(L,s2v(o),t)

/* }================================================================== */


/*
** {==================================================================
** Collectable Objects
** ===================================================================
*/

/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
#define CommonHeader	struct GCObject *next; lu_byte tt; lu_byte marked


/* Common type for all collectable objects */
typedef struct GCObject {
  CommonHeader;
} GCObject;


/* Bit mark for collectable types */
#define BIT_ISCOLLECTABLE	(1 << 6)

#define iscollectable(o)	(rawtt(o) & BIT_ISCOLLECTABLE)

/* mark a tag as collectable */
#define ctb(t)			((t) | BIT_ISCOLLECTABLE)

#define gcvalue(o)	check_exp(iscollectable(o), val_(o).gc)

#define gcvalueraw(v)	((v).gc)

#define setgcovalue(L,obj,x) \
  { TValue *io = (obj); GCObject *i_g=(x); \
    val_(io).gc = i_g; settt_(io, ctb(i_g->tt)); }

/* }================================================================== */


/*
** {==================================================================
** Numbers
** ===================================================================
*/

/* Variant tags for numbers */
#define SIL_VNUMINT	makevariant(SIL_TNUMBER, 0)  /* integer numbers */
#define SIL_VNUMFLT	makevariant(SIL_TNUMBER, 1)  /* float numbers */

#define ttisnumber(o)		checktype((o), SIL_TNUMBER)
#define ttisfloat(o)		checktag((o), SIL_VNUMFLT)
#define ttisinteger(o)		checktag((o), SIL_VNUMINT)

#define nvalue(o)	check_exp(ttisnumber(o), \
	(ttisinteger(o) ? cast_num(ivalue(o)) : fltvalue(o)))
#define fltvalue(o)	check_exp(ttisfloat(o), val_(o).n)
#define ivalue(o)	check_exp(ttisinteger(o), val_(o).i)

#define fltvalueraw(v)	((v).n)
#define ivalueraw(v)	((v).i)

#define setfltvalue(obj,x) \
  { TValue *io=(obj); val_(io).n=(x); settt_(io, SIL_VNUMFLT); }

#define chgfltvalue(obj,x) \
  { TValue *io=(obj); sil_assert(ttisfloat(io)); val_(io).n=(x); }

#define setivalue(obj,x) \
  { TValue *io=(obj); val_(io).i=(x); settt_(io, SIL_VNUMINT); }

#define chgivalue(obj,x) \
  { TValue *io=(obj); sil_assert(ttisinteger(io)); val_(io).i=(x); }

/* }================================================================== */


/*
** {==================================================================
** Strings
** ===================================================================
*/

/* Variant tags for strings */
#define SIL_VSHRSTR	makevariant(SIL_TSTRING, 0)  /* short strings */
#define SIL_VLNGSTR	makevariant(SIL_TSTRING, 1)  /* long strings */

#define ttisstring(o)		checktype((o), SIL_TSTRING)
#define ttisshrstring(o)	checktag((o), ctb(SIL_VSHRSTR))
#define ttislngstring(o)	checktag((o), ctb(SIL_VLNGSTR))

#define tsvalueraw(v)	(gco2ts((v).gc))

#define tsvalue(o)	check_exp(ttisstring(o), gco2ts(val_(o).gc))

#define setsvalue(L,obj,x) \
  { TValue *io = (obj); TString *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(x_->tt)); \
    checkliveness(L,io); }

/* set a string to the stack */
#define setsvalue2s(L,o,s)	setsvalue(L,s2v(o),s)

/* set a string to a new object */
#define setsvalue2n	setsvalue


/* Kinds of long strings (stored in 'shrlen') */
#define LSTRREG		-1  /* regular long string */
#define LSTRFIX		-2  /* fixed external long string */
#define LSTRMEM		-3  /* external long string with deallocation */


/*
** Header for a string value.
*/
typedef struct TString {
  CommonHeader;
  lu_byte extra;  /* reserved words for short strings; "has hash" for longs */
  ls_byte shrlen;  /* length for short strings, negative for long strings */
  unsigned int hash;
  union {
    size_t lnglen;  /* length for long strings */
    struct TString *hnext;  /* linked list for hash table */
  } u;
  char *contents;  /* pointer to content in long strings */
  sil_Alloc falloc;  /* deallocation function for external strings */
  void *ud;  /* user data for external strings */
} TString;


#define strisshr(ts)	((ts)->shrlen >= 0)


/*
** Get the actual string (array of bytes) from a 'TString'. (Generic
** version and specialized versions for long and short strings.)
*/
#define rawgetshrstr(ts)  (cast_charp(&(ts)->contents))
#define getshrstr(ts)	check_exp(strisshr(ts), rawgetshrstr(ts))
#define getlngstr(ts)	check_exp(!strisshr(ts), (ts)->contents)
#define getstr(ts) 	(strisshr(ts) ? rawgetshrstr(ts) : (ts)->contents)


/* get string length from 'TString *ts' */
#define tsslen(ts)  \
	(strisshr(ts) ? cast_sizet((ts)->shrlen) : (ts)->u.lnglen)

/*
** Get string and length */
#define getlstr(ts, len)  \
	(strisshr(ts) \
	? (cast_void((len) = cast_sizet((ts)->shrlen)), rawgetshrstr(ts)) \
	: (cast_void((len) = (ts)->u.lnglen), (ts)->contents))

/* }================================================================== */


/*
** {==================================================================
** Userdata
** ===================================================================
*/


/*
** Light userdata should be a variant of userdata, but for compatibility
** reasons they are also different types.
*/
#define SIL_VLIGHTUSERDATA	makevariant(SIL_TLIGHTUSERDATA, 0)

#define SIL_VUSERDATA		makevariant(SIL_TUSERDATA, 0)

#define ttislightuserdata(o)	checktag((o), SIL_VLIGHTUSERDATA)
#define ttisfulluserdata(o)	checktag((o), ctb(SIL_VUSERDATA))

#define pvalue(o)	check_exp(ttislightuserdata(o), val_(o).p)
#define uvalue(o)	check_exp(ttisfulluserdata(o), gco2u(val_(o).gc))

#define pvalueraw(v)	((v).p)

#define setpvalue(obj,x) \
  { TValue *io=(obj); val_(io).p=(x); settt_(io, SIL_VLIGHTUSERDATA); }

#define setuvalue(L,obj,x) \
  { TValue *io = (obj); Udata *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(SIL_VUSERDATA)); \
    checkliveness(L,io); }


/* Ensures that addresses after this type are always fully aligned. */
typedef union UValue {
  TValue uv;
  SILI_MAXALIGN;  /* ensures maximum alignment for udata bytes */
} UValue;


/*
** Header for userdata with user values;
** memory area follows the end of this structure.
*/
typedef struct Udata {
  CommonHeader;
  unsigned short nuvalue;  /* number of user values */
  size_t len;  /* number of bytes */
  struct Table *metatable;
  GCObject *gclist;
  UValue uv[1];  /* user values */
} Udata;


/*
** Header for userdata with no user values. These userdata do not need
** to be gray during GC, and therefore do not need a 'gclist' field.
** To simplify, the code always use 'Udata' for both kinds of userdata,
** making sure it never accesses 'gclist' on userdata with no user values.
** This structure here is used only to compute the correct size for
** this representation. (The 'bindata' field in its end ensures correct
** alignment for binary data following this header.)
*/
typedef struct Udata0 {
  CommonHeader;
  unsigned short nuvalue;  /* number of user values */
  size_t len;  /* number of bytes */
  struct Table *metatable;
  union {SILI_MAXALIGN;} bindata;
} Udata0;


/* compute the offset of the memory area of a userdata */
#define udatamemoffset(nuv) \
       ((nuv) == 0 ? offsetof(Udata0, bindata)  \
		   : offsetof(Udata, uv) + (sizeof(UValue) * (nuv)))

/* get the address of the memory block inside 'Udata' */
#define getudatamem(u)	(cast_charp(u) + udatamemoffset((u)->nuvalue))

/* compute the size of a userdata */
#define sizeudata(nuv,nb)	(udatamemoffset(nuv) + (nb))

/* }================================================================== */


/*
** {==================================================================
** Prototypes
** ===================================================================
*/

#define SIL_VPROTO	makevariant(SIL_TPROTO, 0)


typedef l_uint32 Instruction;


/*
** Description of an upvalue for function prototypes
*/
typedef struct Upvaldesc {
  TString *name;  /* upvalue name (for debug information) */
  lu_byte instack;  /* whether it is in stack (register) */
  lu_byte idx;  /* index of upvalue (in stack or in outer function's list) */
  lu_byte kind;  /* kind of corresponding variable */
} Upvaldesc;


/*
** Description of a local variable for function prototypes
** (used for debug information)
*/
typedef struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;


/*
** Associates the absolute line source for a given instruction ('pc').
** The array 'lineinfo' gives, for each instruction, the difference in
** lines from the previous instruction. When that difference does not
** fit into a byte, SIL saves the absolute line for that instruction.
** (Sil also saves the absolute line periodically, to speed up the
** computation of a line number: we can use binary search in the
** absolute-line array, but we must traverse the 'lineinfo' array
** linearly to compute a line.)
*/
typedef struct AbsLineInfo {
  int pc;
  int line;
} AbsLineInfo;


/*
** Flags in Prototypes
*/
#define PF_ISVARARG	1
#define PF_FIXED	2  /* prototype has parts in fixed memory */


/*
** Function Prototypes
*/
typedef struct Proto {
  CommonHeader;
  lu_byte numparams;  /* number of fixed (named) parameters */
  lu_byte flag;
  lu_byte maxstacksize;  /* number of registers needed by this function */
  int sizeupvalues;  /* size of 'upvalues' */
  int sizek;  /* size of 'k' */
  int sizecode;
  int sizelineinfo;
  int sizep;  /* size of 'p' */
  int sizelocvars;
  int sizeabslineinfo;  /* size of 'abslineinfo' */
  int linedefined;  /* debug information  */
  int lastlinedefined;  /* debug information  */
  TValue *k;  /* constants used by the function */
  Instruction *code;  /* opcodes */
  struct Proto **p;  /* functions defined inside the function */
  Upvaldesc *upvalues;  /* upvalue information */
  ls_byte *lineinfo;  /* information about source lines (debug information) */
  AbsLineInfo *abslineinfo;  /* idem */
  LocVar *locvars;  /* information about local variables (debug information) */
  TString  *source;  /* used for debug information */
  GCObject *gclist;
} Proto;

/* }================================================================== */


/*
** {==================================================================
** Functions
** ===================================================================
*/

#define SIL_VUPVAL	makevariant(SIL_TUPVAL, 0)


/* Variant tags for functions */
#define SIL_VLCL	makevariant(SIL_TFUNCTION, 0)  /* SIL closure */
#define SIL_VLCF	makevariant(SIL_TFUNCTION, 1)  /* light C function */
#define SIL_VCCL	makevariant(SIL_TFUNCTION, 2)  /* C closure */

#define ttisfunction(o)		checktype(o, SIL_TFUNCTION)
#define ttisLclosure(o)		checktag((o), ctb(SIL_VLCL))
#define ttislcf(o)		checktag((o), SIL_VLCF)
#define ttisCclosure(o)		checktag((o), ctb(SIL_VCCL))
#define ttisclosure(o)         (ttisLclosure(o) || ttisCclosure(o))


#define isLfunction(o)	ttisLclosure(o)

#define clvalue(o)	check_exp(ttisclosure(o), gco2cl(val_(o).gc))
#define clLvalue(o)	check_exp(ttisLclosure(o), gco2lcl(val_(o).gc))
#define fvalue(o)	check_exp(ttislcf(o), val_(o).f)
#define clCvalue(o)	check_exp(ttisCclosure(o), gco2ccl(val_(o).gc))

#define fvalueraw(v)	((v).f)

#define setclLvalue(L,obj,x) \
  { TValue *io = (obj); LClosure *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(SIL_VLCL)); \
    checkliveness(L,io); }

#define setclLvalue2s(L,o,cl)	setclLvalue(L,s2v(o),cl)

#define setfvalue(obj,x) \
  { TValue *io=(obj); val_(io).f=(x); settt_(io, SIL_VLCF); }

#define setclCvalue(L,obj,x) \
  { TValue *io = (obj); CClosure *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(SIL_VCCL)); \
    checkliveness(L,io); }


/*
** Upvalues for SIL closures
*/
typedef struct UpVal {
  CommonHeader;
  union {
    TValue *p;  /* points to stack or to its own value */
    ptrdiff_t offset;  /* used while the stack is being reallocated */
  } v;
  union {
    struct {  /* (when open) */
      struct UpVal *next;  /* linked list */
      struct UpVal **previous;
    } open;
    TValue value;  /* the value (when closed) */
  } u;
} UpVal;



#define ClosureHeader \
	CommonHeader; lu_byte nupvalues; GCObject *gclist

typedef struct CClosure {
  ClosureHeader;
  sil_CFunction f;
  TValue upvalue[1];  /* list of upvalues */
} CClosure;


typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;
  UpVal *upvals[1];  /* list of upvalues */
} LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;


#define getproto(o)	(clLvalue(o)->p)

/* }================================================================== */


/*
** {==================================================================
** Tables
** ===================================================================
*/

#define SIL_VTABLE	makevariant(SIL_TTABLE, 0)

#define ttistable(o)		checktag((o), ctb(SIL_VTABLE))

#define hvalue(o)	check_exp(ttistable(o), gco2t(val_(o).gc))

#define sethvalue(L,obj,x) \
  { TValue *io = (obj); Table *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(SIL_VTABLE)); \
    checkliveness(L,io); }

#define sethvalue2s(L,o,h)	sethvalue(L,s2v(o),h)


/*
** Nodes for Hash tables: A pack of two TValue's (key-value pairs)
** plus a 'next' field to link colliding entries. The distribution
** of the key's fields ('key_tt' and 'key_val') not forming a proper
** 'TValue' allows for a smaller size for 'Node' both in 4-byte
** and 8-byte alignments.
*/
typedef union Node {
  struct NodeKey {
    TValuefields;  /* fields for value */
    lu_byte key_tt;  /* key type */
    int next;  /* for chaining */
    Value key_val;  /* key value */
  } u;
  TValue i_val;  /* direct access to node's value as a proper 'TValue' */
} Node;


/* copy a value into a key */
#define setnodekey(node,obj) \
	{ Node *n_=(node); const TValue *io_=(obj); \
	  n_->u.key_val = io_->value_; n_->u.key_tt = io_->tt_; }


/* copy a value from a key */
#define getnodekey(L,obj,node) \
	{ TValue *io_=(obj); const Node *n_=(node); \
	  io_->value_ = n_->u.key_val; io_->tt_ = n_->u.key_tt; \
	  checkliveness(L,io_); }



typedef struct Table {
  CommonHeader;
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */
  lu_byte lsizenode;  /* log2 of number of slots of 'node' array */
  unsigned int asize;  /* number of slots in 'array' array */
  Value *array;  /* array part */
  Node *node;
  struct Table *metatable;
  GCObject *gclist;
} Table;


/*
** Macros to manipulate keys inserted in nodes
*/
#define keytt(node)		((node)->u.key_tt)
#define keyval(node)		((node)->u.key_val)

#define keyisnil(node)		(keytt(node) == SIL_TNIL)
#define keyisinteger(node)	(keytt(node) == SIL_VNUMINT)
#define keyival(node)		(keyval(node).i)
#define keyisshrstr(node)	(keytt(node) == ctb(SIL_VSHRSTR))
#define keystrval(node)		(gco2ts(keyval(node).gc))

#define setnilkey(node)		(keytt(node) = SIL_TNIL)

#define keyiscollectable(n)	(keytt(n) & BIT_ISCOLLECTABLE)

#define gckey(n)	(keyval(n).gc)
#define gckeyN(n)	(keyiscollectable(n) ? gckey(n) : NULL)


/*
** Dead keys in tables have the tag DEADKEY but keep their original
** gcvalue. This distinguishes them from regular keys but allows them to
** be found when searched in a special way. ('next' needs that to find
** keys removed from a table during a traversal.)
*/
#define setdeadkey(node)	(keytt(node) = SIL_TDEADKEY)
#define keyisdead(node)		(keytt(node) == SIL_TDEADKEY)

/* }================================================================== */



/*
** 'module' operation for hashing (size is always a power of 2)
*/
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast_uint(s) & cast_uint((size)-1))))


#define twoto(x)	(1u<<(x))
#define sizenode(t)	(twoto((t)->lsizenode))


/* size of buffer for 'silO_utf8esc' function */
#define UTF8BUFFSZ	8


/* macro to call 'silO_pushvfstring' correctly */
#define pushvfstring(L, argp, fmt, msg)	\
  { va_start(argp, fmt); \
  msg = silO_pushvfstring(L, fmt, argp); \
  va_end(argp); \
  if (msg == NULL) silD_throw(L, SIL_ERRMEM);  /* only after 'va_end' */ }


SILI_FUNC int silO_utf8esc (char *buff, l_uint32 x);
SILI_FUNC lu_byte silO_ceillog2 (unsigned int x);
SILI_FUNC lu_byte silO_codeparam (unsigned int p);
SILI_FUNC l_mem silO_applyparam (lu_byte p, l_mem x);

SILI_FUNC int silO_rawarith (sil_State *L, int op, const TValue *p1,
                             const TValue *p2, TValue *res);
SILI_FUNC void silO_arith (sil_State *L, int op, const TValue *p1,
                           const TValue *p2, StkId res);
SILI_FUNC size_t silO_str2num (const char *s, TValue *o);
SILI_FUNC unsigned silO_tostringbuff (const TValue *obj, char *buff);
SILI_FUNC lu_byte silO_hexavalue (int c);
SILI_FUNC void silO_tostring (sil_State *L, TValue *obj);
SILI_FUNC const char *silO_pushvfstring (sil_State *L, const char *fmt,
                                                       va_list argp);
SILI_FUNC const char *silO_pushfstring (sil_State *L, const char *fmt, ...);
SILI_FUNC void silO_chunkid (char *out, const char *source, size_t srclen);


#endif

