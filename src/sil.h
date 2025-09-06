/*
** $Id: sil.h $
** See Copyright Notice at the end of this file
*/

#ifndef sil_h
#define sil_h

#include <stdarg.h>
#include <stddef.h>


#define SIL_COPYRIGHT	SIL_RELEASE "\nCopyright (C) 1994-2025 Lua.org, PUC-Rio"
#define SIL_AUTHORS	"Igor Alexey M."


#define SIL_VERSION_MAJOR_N	1
#define SIL_VERSION_MINOR_N	0

#define SIL_VERSION_NUM  (SIL_VERSION_MAJOR_N * 100 + SIL_VERSION_MINOR_N)

#include "silconf.h"


/* mark for precompiled code ('<esc>Sil') */
#define SIL_SIGNATURE	"\x1bSil"

/* option for multiple returns in 'sil_pcall' and 'sil_call' */
#define SIL_MULTRET	(-1)


/*
** Pseudo-indices
** (-SILI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define SIL_REGISTRYINDEX	(-SILI_MAXSTACK - 1000)
#define sil_upvalueindex(i)	(SIL_REGISTRYINDEX - (i))


/* thread status */
#define SIL_OK		0
#define SIL_YIELD	1
#define SIL_ERRRUN	2
#define SIL_ERRSYNTAX	3
#define SIL_ERRMEM	4
#define SIL_ERRERR	5


typedef struct sil_State sil_State;


/*
** basic types
*/
#define SIL_TNONE		(-1)

#define SIL_TNIL		0
#define SIL_TBOOLEAN		1
#define SIL_TLIGHTUSERDATA	2
#define SIL_TNUMBER		3
#define SIL_TSTRING		4
#define SIL_TTABLE		5
#define SIL_TFUNCTION		6
#define SIL_TUSERDATA		7
#define SIL_TTHREAD		8

#define SIL_NUMTYPES		9



/* minimum SIL stack available to a C function */
#define SIL_MINSTACK	20


/* predefined values in the registry */
/* index 1 is reserved for the reference mechanism */
#define SIL_RIDX_GLOBALS	2
#define SIL_RIDX_MAINTHREAD	3
#define SIL_RIDX_LAST		3


/* type of numbers in SIL */
typedef SIL_NUMBER sil_Number;


/* type for integer functions */
typedef SIL_INTEGER sil_Integer;

/* unsigned integer type */
typedef SIL_UNSIGNED sil_Unsigned;

/* type for continuation-function contexts */
typedef SIL_KCONTEXT sil_KContext;


/*
** Type for C functions registered with SIL
*/
typedef int (*sil_CFunction) (sil_State *L);

/*
** Type for continuation functions
*/
typedef int (*sil_KFunction) (sil_State *L, int status, sil_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping SIL chunks
*/
typedef const char * (*sil_Reader) (sil_State *L, void *ud, size_t *sz);

typedef int (*sil_Writer) (sil_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*sil_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*sil_WarnFunction) (void *ud, const char *msg, int tocont);


/*
** Type used by the debug API to collect debug information
*/
typedef struct sil_Debug sil_Debug;


/*
** Functions to be called by the debugger in specific events
*/
typedef void (*sil_Hook) (sil_State *L, sil_Debug *ar);


/*
** generic extra include file
*/
#if defined(SIL_USER_H)
#include SIL_USER_H
#endif


/*
** RCS ident string
*/
extern const char sil_ident[];


/*
** state manipulation
*/
SIL_API sil_State *(sil_newstate) (sil_Alloc f, void *ud, unsigned seed);
SIL_API void       (sil_close) (sil_State *L);
SIL_API sil_State *(sil_newthread) (sil_State *L);
SIL_API int        (sil_closethread) (sil_State *L, sil_State *from);

SIL_API sil_CFunction (sil_atpanic) (sil_State *L, sil_CFunction panicf);


SIL_API sil_Number (sil_version) (sil_State *L);


/*
** basic stack manipulation
*/
SIL_API int   (sil_absindex) (sil_State *L, int idx);
SIL_API int   (sil_gettop) (sil_State *L);
SIL_API void  (sil_settop) (sil_State *L, int idx);
SIL_API void  (sil_pushvalue) (sil_State *L, int idx);
SIL_API void  (sil_rotate) (sil_State *L, int idx, int n);
SIL_API void  (sil_copy) (sil_State *L, int fromidx, int toidx);
SIL_API int   (sil_checkstack) (sil_State *L, int n);

SIL_API void  (sil_xmove) (sil_State *from, sil_State *to, int n);


/*
** access functions (stack -> C)
*/

SIL_API int             (sil_isnumber) (sil_State *L, int idx);
SIL_API int             (sil_isstring) (sil_State *L, int idx);
SIL_API int             (sil_iscfunction) (sil_State *L, int idx);
SIL_API int             (sil_isinteger) (sil_State *L, int idx);
SIL_API int             (sil_isuserdata) (sil_State *L, int idx);
SIL_API int             (sil_type) (sil_State *L, int idx);
SIL_API const char     *(sil_typename) (sil_State *L, int tp);

SIL_API sil_Number      (sil_tonumberx) (sil_State *L, int idx, int *isnum);
SIL_API sil_Integer     (sil_tointegerx) (sil_State *L, int idx, int *isnum);
SIL_API int             (sil_toboolean) (sil_State *L, int idx);
SIL_API const char     *(sil_tolstring) (sil_State *L, int idx, size_t *len);
SIL_API sil_Unsigned    (sil_rawlen) (sil_State *L, int idx);
SIL_API sil_CFunction   (sil_tocfunction) (sil_State *L, int idx);
SIL_API void	       *(sil_touserdata) (sil_State *L, int idx);
SIL_API sil_State      *(sil_tothread) (sil_State *L, int idx);
SIL_API const void     *(sil_topointer) (sil_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define SIL_OPADD	0	/* ORDER TM, ORDER OP */
#define SIL_OPSUB	1
#define SIL_OPMUL	2
#define SIL_OPMOD	3
#define SIL_OPPOW	4
#define SIL_OPDIV	5
#define SIL_OPIDIV	6
#define SIL_OPBAND	7
#define SIL_OPBOR	8
#define SIL_OPBXOR	9
#define SIL_OPSHL	10
#define SIL_OPSHR	11
#define SIL_OPUNM	12
#define SIL_OPBNOT	13

SIL_API void  (sil_arith) (sil_State *L, int op);

#define SIL_OPEQ	0
#define SIL_OPLT	1
#define SIL_OPLE	2

SIL_API int   (sil_rawequal) (sil_State *L, int idx1, int idx2);
SIL_API int   (sil_compare) (sil_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
SIL_API void        (sil_pushnil) (sil_State *L);
SIL_API void        (sil_pushnumber) (sil_State *L, sil_Number n);
SIL_API void        (sil_pushinteger) (sil_State *L, sil_Integer n);
SIL_API const char *(sil_pushlstring) (sil_State *L, const char *s, size_t len);
SIL_API const char *(sil_pushexternalstring) (sil_State *L,
		const char *s, size_t len, sil_Alloc falloc, void *ud);
SIL_API const char *(sil_pushstring) (sil_State *L, const char *s);
SIL_API const char *(sil_pushvfstring) (sil_State *L, const char *fmt,
                                                      va_list argp);
SIL_API const char *(sil_pushfstring) (sil_State *L, const char *fmt, ...);
SIL_API void  (sil_pushcclosure) (sil_State *L, sil_CFunction fn, int n);
SIL_API void  (sil_pushboolean) (sil_State *L, int b);
SIL_API void  (sil_pushlightuserdata) (sil_State *L, void *p);
SIL_API int   (sil_pushthread) (sil_State *L);


/*
** get functions (Sil -> stack)
*/
SIL_API int (sil_getglobal) (sil_State *L, const char *name);
SIL_API int (sil_gettable) (sil_State *L, int idx);
SIL_API int (sil_getfield) (sil_State *L, int idx, const char *k);
SIL_API int (sil_geti) (sil_State *L, int idx, sil_Integer n);
SIL_API int (sil_rawget) (sil_State *L, int idx);
SIL_API int (sil_rawgeti) (sil_State *L, int idx, sil_Integer n);
SIL_API int (sil_rawgetp) (sil_State *L, int idx, const void *p);

SIL_API void  (sil_createtable) (sil_State *L, int narr, int nrec);
SIL_API void *(sil_newuserdatauv) (sil_State *L, size_t sz, int nuvalue);
SIL_API int   (sil_getmetatable) (sil_State *L, int objindex);
SIL_API int  (sil_getiuservalue) (sil_State *L, int idx, int n);


/*
** set functions (stack -> SIL)
*/
SIL_API void  (sil_setglobal) (sil_State *L, const char *name);
SIL_API void  (sil_settable) (sil_State *L, int idx);
SIL_API void  (sil_setfield) (sil_State *L, int idx, const char *k);
SIL_API void  (sil_seti) (sil_State *L, int idx, sil_Integer n);
SIL_API void  (sil_rawset) (sil_State *L, int idx);
SIL_API void  (sil_rawseti) (sil_State *L, int idx, sil_Integer n);
SIL_API void  (sil_rawsetp) (sil_State *L, int idx, const void *p);
SIL_API int   (sil_setmetatable) (sil_State *L, int objindex);
SIL_API int   (sil_setiuservalue) (sil_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run SIL code)
*/
SIL_API void  (sil_callk) (sil_State *L, int nargs, int nresults,
                           sil_KContext ctx, sil_KFunction k);
#define sil_call(L,n,r)		sil_callk(L, (n), (r), 0, NULL)

SIL_API int   (sil_pcallk) (sil_State *L, int nargs, int nresults, int errfunc,
                            sil_KContext ctx, sil_KFunction k);
#define sil_pcall(L,n,r,f)	sil_pcallk(L, (n), (r), (f), 0, NULL)

SIL_API int   (sil_load) (sil_State *L, sil_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

SIL_API int (sil_dump) (sil_State *L, sil_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
SIL_API int  (sil_yieldk)     (sil_State *L, int nresults, sil_KContext ctx,
                               sil_KFunction k);
SIL_API int  (sil_resume)     (sil_State *L, sil_State *from, int narg,
                               int *nres);
SIL_API int  (sil_status)     (sil_State *L);
SIL_API int (sil_isyieldable) (sil_State *L);

#define sil_yield(L,n)		sil_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
SIL_API void (sil_setwarnf) (sil_State *L, sil_WarnFunction f, void *ud);
SIL_API void (sil_warning)  (sil_State *L, const char *msg, int tocont);


/*
** garbage-collection options
*/

#define SIL_GCSTOP		0
#define SIL_GCRESTART		1
#define SIL_GCCOLLECT		2
#define SIL_GCCOUNT		3
#define SIL_GCCOUNTB		4
#define SIL_GCSTEP		5
#define SIL_GCISRUNNING		6
#define SIL_GCGEN		7
#define SIL_GCINC		8
#define SIL_GCPARAM		9


/*
** garbage-collection parameters
*/
/* parameters for generational mode */
#define SIL_GCPMINORMUL		0  /* control minor collections */
#define SIL_GCPMAJORMINOR	1  /* control shift major->minor */
#define SIL_GCPMINORMAJOR	2  /* control shift minor->major */

/* parameters for incremental mode */
#define SIL_GCPPAUSE		3  /* size of pause between successive GCs */
#define SIL_GCPSTEPMUL		4  /* GC "speed" */
#define SIL_GCPSTEPSIZE		5  /* GC granularity */

/* number of parameters */
#define SIL_GCPN		6


SIL_API int (sil_gc) (sil_State *L, int what, ...);


/*
** miscellaneous functions
*/

SIL_API int   (sil_error) (sil_State *L);

SIL_API int   (sil_next) (sil_State *L, int idx);

SIL_API void  (sil_concat) (sil_State *L, int n);
SIL_API void  (sil_len)    (sil_State *L, int idx);

#define SIL_N2SBUFFSZ	64
SIL_API unsigned  (sil_numbertocstring) (sil_State *L, int idx, char *buff);
SIL_API size_t  (sil_stringtonumber) (sil_State *L, const char *s);

SIL_API sil_Alloc (sil_getallocf) (sil_State *L, void **ud);
SIL_API void      (sil_setallocf) (sil_State *L, sil_Alloc f, void *ud);

SIL_API void (sil_toclose) (sil_State *L, int idx);
SIL_API void (sil_closeslot) (sil_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define sil_getextraspace(L)	((void *)((char *)(L) - SIL_EXTRASPACE))

#define sil_tonumber(L,i)	sil_tonumberx(L,(i),NULL)
#define sil_tointeger(L,i)	sil_tointegerx(L,(i),NULL)

#define sil_pop(L,n)		sil_settop(L, -(n)-1)

#define sil_newtable(L)		sil_createtable(L, 0, 0)

#define sil_register(L,n,f) (sil_pushcfunction(L, (f)), sil_setglobal(L, (n)))

#define sil_pushcfunction(L,f)	sil_pushcclosure(L, (f), 0)

#define sil_isfunction(L,n)	(sil_type(L, (n)) == SIL_TFUNCTION)
#define sil_istable(L,n)	(sil_type(L, (n)) == SIL_TTABLE)
#define sil_islightuserdata(L,n)	(sil_type(L, (n)) == SIL_TLIGHTUSERDATA)
#define sil_isnil(L,n)		(sil_type(L, (n)) == SIL_TNIL)
#define sil_isboolean(L,n)	(sil_type(L, (n)) == SIL_TBOOLEAN)
#define sil_isthread(L,n)	(sil_type(L, (n)) == SIL_TTHREAD)
#define sil_isnone(L,n)		(sil_type(L, (n)) == SIL_TNONE)
#define sil_isnoneornil(L, n)	(sil_type(L, (n)) <= 0)

#define sil_pushliteral(L, s)	sil_pushstring(L, "" s)

#define sil_pushglobaltable(L)  \
	((void)sil_rawgeti(L, SIL_REGISTRYINDEX, SIL_RIDX_GLOBALS))

#define sil_tostring(L,i)	sil_tolstring(L, (i), NULL)


#define sil_insert(L,idx)	sil_rotate(L, (idx), 1)

#define sil_remove(L,idx)	(sil_rotate(L, (idx), -1), sil_pop(L, 1))

#define sil_replace(L,idx)	(sil_copy(L, -1, (idx)), sil_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(SIL_COMPAT_APIINTCASTS)

#define sil_pushunsigned(L,n)	sil_pushinteger(L, (sil_Integer)(n))
#define sil_tounsignedx(L,i,is)	((sil_Unsigned)sil_tointegerx(L,i,is))
#define sil_tounsigned(L,i)	sil_tounsignedx(L,(i),NULL)

#endif

#define sil_newuserdata(L,s)	sil_newuserdatauv(L,s,1)
#define sil_getuservalue(L,idx)	sil_getiuservalue(L,idx,1)
#define sil_setuservalue(L,idx)	sil_setiuservalue(L,idx,1)

#define sil_resetthread(L)	sil_closethread(L,NULL)

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define SIL_HOOKCALL	0
#define SIL_HOOKRET	1
#define SIL_HOOKLINE	2
#define SIL_HOOKCOUNT	3
#define SIL_HOOKTAILCALL 4


/*
** Event masks
*/
#define SIL_MASKCALL	(1 << SIL_HOOKCALL)
#define SIL_MASKRET	(1 << SIL_HOOKRET)
#define SIL_MASKLINE	(1 << SIL_HOOKLINE)
#define SIL_MASKCOUNT	(1 << SIL_HOOKCOUNT)


SIL_API int (sil_getstack) (sil_State *L, int level, sil_Debug *ar);
SIL_API int (sil_getinfo) (sil_State *L, const char *what, sil_Debug *ar);
SIL_API const char *(sil_getlocal) (sil_State *L, const sil_Debug *ar, int n);
SIL_API const char *(sil_setlocal) (sil_State *L, const sil_Debug *ar, int n);
SIL_API const char *(sil_getupvalue) (sil_State *L, int funcindex, int n);
SIL_API const char *(sil_setupvalue) (sil_State *L, int funcindex, int n);

SIL_API void *(sil_upvalueid) (sil_State *L, int fidx, int n);
SIL_API void  (sil_upvaluejoin) (sil_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

SIL_API void (sil_sethook) (sil_State *L, sil_Hook func, int mask, int count);
SIL_API sil_Hook (sil_gethook) (sil_State *L);
SIL_API int (sil_gethookmask) (sil_State *L);
SIL_API int (sil_gethookcount) (sil_State *L);


struct sil_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Sil', 'C', 'main', 'tail' */
  const char *source;	/* (S) */
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  unsigned char extraargs;  /* (t) number of extra arguments */
  char istailcall;	/* (t) */
  int ftransfer;   /* (r) index of first value transferred */
  int ntransfer;   /* (r) number of transferred values */
  char short_src[SIL_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};

/* }====================================================================== */


#define SILI_TOSTRAUX(x)	#x
#define SILI_TOSTR(x)		SILI_TOSTRAUX(x)

#define SIL_VERSION_MAJOR	SILI_TOSTR(SIL_VERSION_MAJOR_N)
#define SIL_VERSION_MINOR	SILI_TOSTR(SIL_VERSION_MINOR_N)
#define SIL_VERSION	"Sil " SIL_VERSION_MAJOR "." SIL_VERSION_MINOR
#define SIL_RELEASE	SIL_VERSION " (based on Lua 5.5.0 beta)"

#endif

/******************************************************************************
* Original Lua: Copyright (C) 1994-2025 Lua.org, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

