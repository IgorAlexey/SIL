/*
** $Id: lvm.h $
** SIL virtual machine
** See Copyright Notice in sil.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#if !defined(SIL_NOCVTN2S)
#define cvt2str(o)	ttisnumber(o)
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(SIL_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define SIL_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(SIL_FLOORN2I)
#define SIL_FLOORN2I		F2Ieq
#endif


/*
** Rounding modes for float->integer coercion
 */
typedef enum {
  F2Ieq,     /* no rounding; accepts only integral values */
  F2Ifloor,  /* takes the floor of the number */
  F2Iceil    /* takes the ceiling of the number */
} F2Imod;


/* convert an object to a float (including string coercion) */
#define tonumber(o,n) \
	(ttisfloat(o) ? (*(n) = fltvalue(o), 1) : silV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
	(ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
	(ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : silV_tointeger(o,i,SIL_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : silV_tointegerns(o,i,SIL_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define silV_rawequalobj(t1,t2)		silV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable'
*/
#define silV_fastget(t,k,res,f, tag) \
  (tag = (!ttistable(t) ? SIL_VNOTABLE : f(hvalue(t), k, res)))


/*
** Special case of 'silV_fastget' for integers, inlining the fast case
** of 'silH_getint'.
*/
#define silV_fastgeti(t,k,res,tag) \
  if (!ttistable(t)) tag = SIL_VNOTABLE; \
  else { silH_fastgeti(hvalue(t), k, res, tag); }


#define silV_fastset(t,k,val,hres,f) \
  (hres = (!ttistable(t) ? HNOTATABLE : f(hvalue(t), k, val)))

#define silV_fastseti(t,k,val,hres) \
  if (!ttistable(t)) hres = HNOTATABLE; \
  else { silH_fastseti(hvalue(t), k, val, hres); }


/*
** Finish a fast set operation (when fast set succeeds).
*/
#define silV_finishfastset(L,t,v)	silC_barrierback(L, gcvalue(t), v)


/*
** Shift right is the same as shift left with a negative 'y'
*/
#define silV_shiftr(x,y)	silV_shiftl(x,intop(-, 0, y))



SILI_FUNC int silV_equalobj (sil_State *L, const TValue *t1, const TValue *t2);
SILI_FUNC int silV_lessthan (sil_State *L, const TValue *l, const TValue *r);
SILI_FUNC int silV_lessequal (sil_State *L, const TValue *l, const TValue *r);
SILI_FUNC int silV_tonumber_ (const TValue *obj, sil_Number *n);
SILI_FUNC int silV_tointeger (const TValue *obj, sil_Integer *p, F2Imod mode);
SILI_FUNC int silV_tointegerns (const TValue *obj, sil_Integer *p,
                                F2Imod mode);
SILI_FUNC int silV_flttointeger (sil_Number n, sil_Integer *p, F2Imod mode);
SILI_FUNC lu_byte silV_finishget (sil_State *L, const TValue *t, TValue *key,
                                                StkId val, lu_byte tag);
SILI_FUNC void silV_finishset (sil_State *L, const TValue *t, TValue *key,
                                             TValue *val, int aux);
SILI_FUNC void silV_finishOp (sil_State *L);
SILI_FUNC void silV_execute (sil_State *L, CallInfo *ci);
SILI_FUNC void silV_concat (sil_State *L, int total);
SILI_FUNC sil_Integer silV_idiv (sil_State *L, sil_Integer x, sil_Integer y);
SILI_FUNC sil_Integer silV_mod (sil_State *L, sil_Integer x, sil_Integer y);
SILI_FUNC sil_Number silV_modf (sil_State *L, sil_Number x, sil_Number y);
SILI_FUNC sil_Integer silV_shiftl (sil_Integer x, sil_Integer y);
SILI_FUNC void silV_objlen (sil_State *L, StkId ra, const TValue *rb);

#endif
