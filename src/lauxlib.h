/*
** $Id: lauxlib.h $
** Auxiliary functions for building SIL libraries
** See Copyright Notice in sil.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

#include "silconf.h"
#include "sil.h"


/* global table */
#define SIL_GNAME	"_G"


typedef struct silL_Buffer silL_Buffer;


/* extra error code for 'silL_loadfilex' */
#define SIL_ERRFILE     (SIL_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define SIL_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define SIL_PRELOAD_TABLE	"_PRELOAD"


typedef struct silL_Reg {
  const char *name;
  sil_CFunction func;
} silL_Reg;


#define SILL_NUMSIZES	(sizeof(sil_Integer)*16 + sizeof(sil_Number))

SILLIB_API void (silL_checkversion_) (sil_State *L, sil_Number ver, size_t sz);
#define silL_checkversion(L)  \
	  silL_checkversion_(L, SIL_VERSION_NUM, SILL_NUMSIZES)

SILLIB_API int (silL_getmetafield) (sil_State *L, int obj, const char *e);
SILLIB_API int (silL_callmeta) (sil_State *L, int obj, const char *e);
SILLIB_API const char *(silL_tolstring) (sil_State *L, int idx, size_t *len);
SILLIB_API int (silL_argerror) (sil_State *L, int arg, const char *extramsg);
SILLIB_API int (silL_typeerror) (sil_State *L, int arg, const char *tname);
SILLIB_API const char *(silL_checklstring) (sil_State *L, int arg,
                                                          size_t *l);
SILLIB_API const char *(silL_optlstring) (sil_State *L, int arg,
                                          const char *def, size_t *l);
SILLIB_API sil_Number (silL_checknumber) (sil_State *L, int arg);
SILLIB_API sil_Number (silL_optnumber) (sil_State *L, int arg, sil_Number def);

SILLIB_API sil_Integer (silL_checkinteger) (sil_State *L, int arg);
SILLIB_API sil_Integer (silL_optinteger) (sil_State *L, int arg,
                                          sil_Integer def);

SILLIB_API void (silL_checkstack) (sil_State *L, int sz, const char *msg);
SILLIB_API void (silL_checktype) (sil_State *L, int arg, int t);
SILLIB_API void (silL_checkany) (sil_State *L, int arg);

SILLIB_API int   (silL_newmetatable) (sil_State *L, const char *tname);
SILLIB_API void  (silL_setmetatable) (sil_State *L, const char *tname);
SILLIB_API void *(silL_testudata) (sil_State *L, int ud, const char *tname);
SILLIB_API void *(silL_checkudata) (sil_State *L, int ud, const char *tname);

SILLIB_API void (silL_where) (sil_State *L, int lvl);
SILLIB_API int (silL_error) (sil_State *L, const char *fmt, ...);

SILLIB_API int (silL_checkoption) (sil_State *L, int arg, const char *def,
                                   const char *const lst[]);

SILLIB_API int (silL_fileresult) (sil_State *L, int stat, const char *fname);
SILLIB_API int (silL_execresult) (sil_State *L, int stat);


/* predefined references */
#define SIL_NOREF       (-2)
#define SIL_REFNIL      (-1)

SILLIB_API int (silL_ref) (sil_State *L, int t);
SILLIB_API void (silL_unref) (sil_State *L, int t, int ref);

SILLIB_API int (silL_loadfilex) (sil_State *L, const char *filename,
                                               const char *mode);

#define silL_loadfile(L,f)	silL_loadfilex(L,f,NULL)

SILLIB_API int (silL_loadbufferx) (sil_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
SILLIB_API int (silL_loadstring) (sil_State *L, const char *s);

SILLIB_API sil_State *(silL_newstate) (void);

SILLIB_API unsigned silL_makeseed (sil_State *L);

SILLIB_API sil_Integer (silL_len) (sil_State *L, int idx);

SILLIB_API void (silL_addgsub) (silL_Buffer *b, const char *s,
                                     const char *p, const char *r);
SILLIB_API const char *(silL_gsub) (sil_State *L, const char *s,
                                    const char *p, const char *r);

SILLIB_API void (silL_setfuncs) (sil_State *L, const silL_Reg *l, int nup);

SILLIB_API int (silL_getsubtable) (sil_State *L, int idx, const char *fname);

SILLIB_API void (silL_traceback) (sil_State *L, sil_State *L1,
                                  const char *msg, int level);

SILLIB_API void (silL_requiref) (sil_State *L, const char *modname,
                                 sil_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define silL_newlibtable(L,l)	\
  sil_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define silL_newlib(L,l)  \
  (silL_checkversion(L), silL_newlibtable(L,l), silL_setfuncs(L,l,0))

#define silL_argcheck(L, cond,arg,extramsg)	\
	((void)(sili_likely(cond) || silL_argerror(L, (arg), (extramsg))))

#define silL_argexpected(L,cond,arg,tname)	\
	((void)(sili_likely(cond) || silL_typeerror(L, (arg), (tname))))

#define silL_checkstring(L,n)	(silL_checklstring(L, (n), NULL))
#define silL_optstring(L,n,d)	(silL_optlstring(L, (n), (d), NULL))

#define silL_typename(L,i)	sil_typename(L, sil_type(L,(i)))

#define silL_dofile(L, fn) \
	(silL_loadfile(L, fn) || sil_pcall(L, 0, SIL_MULTRET, 0))

#define silL_dostring(L, s) \
	(silL_loadstring(L, s) || sil_pcall(L, 0, SIL_MULTRET, 0))

#define silL_getmetatable(L,n)	(sil_getfield(L, SIL_REGISTRYINDEX, (n)))

#define silL_opt(L,f,n,d)	(sil_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define silL_loadbuffer(L,s,sz,n)	silL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on sil_Integer values with wrap-around
** semantics, as the SIL core does.
*/
#define silL_intop(op,v1,v2)  \
	((sil_Integer)((sil_Unsigned)(v1) op (sil_Unsigned)(v2)))


/* push the value used to represent failure/error */
#if defined(SIL_FAILISFALSE)
#define silL_pushfail(L)	sil_pushboolean(L, 0)
#else
#define silL_pushfail(L)	sil_pushnil(L)
#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct silL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  sil_State *L;
  union {
    SILI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[SILL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define silL_bufflen(bf)	((bf)->n)
#define silL_buffaddr(bf)	((bf)->b)


#define silL_addchar(B,c) \
  ((void)((B)->n < (B)->size || silL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define silL_addsize(B,s)	((B)->n += (s))

#define silL_buffsub(B,s)	((B)->n -= (s))

SILLIB_API void (silL_buffinit) (sil_State *L, silL_Buffer *B);
SILLIB_API char *(silL_prepbuffsize) (silL_Buffer *B, size_t sz);
SILLIB_API void (silL_addlstring) (silL_Buffer *B, const char *s, size_t l);
SILLIB_API void (silL_addstring) (silL_Buffer *B, const char *s);
SILLIB_API void (silL_addvalue) (silL_Buffer *B);
SILLIB_API void (silL_pushresult) (silL_Buffer *B);
SILLIB_API void (silL_pushresultsize) (silL_Buffer *B, size_t sz);
SILLIB_API char *(silL_buffinitsize) (sil_State *L, silL_Buffer *B, size_t sz);

#define silL_prepbuffer(B)	silL_prepbuffsize(B, SILL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'SIL_FILEHANDLE' and
** initial structure 'silL_Stream' (it may contain other fields
** after that initial structure).
*/

#define SIL_FILEHANDLE          "FILE*"


typedef struct silL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  sil_CFunction closef;  /* to close stream (NULL for closed streams) */
} silL_Stream;

/* }====================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(SIL_COMPAT_APIINTCASTS)

#define silL_checkunsigned(L,a)	((sil_Unsigned)silL_checkinteger(L,a))
#define silL_optunsigned(L,a,d)	\
	((sil_Unsigned)silL_optinteger(L,a,(sil_Integer)(d)))

#define silL_checkint(L,n)	((int)silL_checkinteger(L, (n)))
#define silL_optint(L,n,d)	((int)silL_optinteger(L, (n), (d)))

#define silL_checklong(L,n)	((long)silL_checkinteger(L, (n)))
#define silL_optlong(L,n,d)	((long)silL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif


