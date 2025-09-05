/*
** $Id: ltm.h $
** Tag methods
** See Copyright Notice in sil.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM" and "ORDER OP"
*/
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_LEN,
  TM_EQ,  /* last tag method with fast access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_MOD,
  TM_POW,
  TM_DIV,
  TM_IDIV,
  TM_BAND,
  TM_BOR,
  TM_BXOR,
  TM_SHL,
  TM_SHR,
  TM_UNM,
  TM_BNOT,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_CLOSE,
  TM_N		/* number of elements in the enum */
} TMS;


/*
** Mask with 1 in all fast-access methods. A 1 in any of these bits
** in the flag of a (meta)table means the metatable does not have the
** corresponding metamethod field. (Bit 6 of the flag indicates that
** the table is using the dummy node; bit 7 is used for 'isrealasize'.)
*/
#define maskflags	cast_byte(~(~0u << (TM_EQ + 1)))


/*
** Test whether there is no tagmethod.
** (Because tagmethods use raw accesses, the result may be an "empty" nil.)
*/
#define notm(tm)	ttisnil(tm)

#define checknoTM(mt,e)	((mt) == NULL || (mt)->flags & (1u<<(e)))

#define gfasttm(g,mt,e)  \
  (checknoTM(mt, e) ? NULL : silT_gettm(mt, e, (g)->tmname[e]))

#define fasttm(l,mt,e)	gfasttm(G(l), mt, e)

#define ttypename(x)	silT_typenames_[(x) + 1]

SILI_DDEC(const char *const silT_typenames_[SIL_TOTALTYPES];)


SILI_FUNC const char *silT_objtypename (sil_State *L, const TValue *o);

SILI_FUNC const TValue *silT_gettm (Table *events, TMS event, TString *ename);
SILI_FUNC const TValue *silT_gettmbyobj (sil_State *L, const TValue *o,
                                                       TMS event);
SILI_FUNC void silT_init (sil_State *L);

SILI_FUNC void silT_callTM (sil_State *L, const TValue *f, const TValue *p1,
                            const TValue *p2, const TValue *p3);
SILI_FUNC lu_byte silT_callTMres (sil_State *L, const TValue *f,
                               const TValue *p1, const TValue *p2, StkId p3);
SILI_FUNC void silT_trybinTM (sil_State *L, const TValue *p1, const TValue *p2,
                              StkId res, TMS event);
SILI_FUNC void silT_tryconcatTM (sil_State *L);
SILI_FUNC void silT_trybinassocTM (sil_State *L, const TValue *p1,
       const TValue *p2, int inv, StkId res, TMS event);
SILI_FUNC void silT_trybiniTM (sil_State *L, const TValue *p1, sil_Integer i2,
                               int inv, StkId res, TMS event);
SILI_FUNC int silT_callorderTM (sil_State *L, const TValue *p1,
                                const TValue *p2, TMS event);
SILI_FUNC int silT_callorderiTM (sil_State *L, const TValue *p1, int v2,
                                 int inv, int isfloat, TMS event);

SILI_FUNC void silT_adjustvarargs (sil_State *L, int nfixparams,
                                   struct CallInfo *ci, const Proto *p);
SILI_FUNC void silT_getvarargs (sil_State *L, struct CallInfo *ci,
                                              StkId where, int wanted);


#endif
