/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in sil.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active SIL function (given call info) */
#define ci_func(ci)		(clLvalue(s2v((ci)->func.p)))


#define resethookcount(L)	(L->hookcount = L->basehookcount)

/*
** mark for entries in 'lineinfo' array that has absolute information in
** 'abslineinfo' array
*/
#define ABSLINEINFO	(-0x80)


/*
** MAXimum number of successive Instructions WiTHout ABSolute line
** information. (A power of two allows fast divisions.)
*/
#if !defined(MAXIWTHABS)
#define MAXIWTHABS	128
#endif


SILI_FUNC int silG_getfuncline (const Proto *f, int pc);
SILI_FUNC const char *silG_findlocal (sil_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
SILI_FUNC l_noret silG_typeerror (sil_State *L, const TValue *o,
                                                const char *opname);
SILI_FUNC l_noret silG_callerror (sil_State *L, const TValue *o);
SILI_FUNC l_noret silG_forerror (sil_State *L, const TValue *o,
                                               const char *what);
SILI_FUNC l_noret silG_concaterror (sil_State *L, const TValue *p1,
                                                  const TValue *p2);
SILI_FUNC l_noret silG_opinterror (sil_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
SILI_FUNC l_noret silG_tointerror (sil_State *L, const TValue *p1,
                                                 const TValue *p2);
SILI_FUNC l_noret silG_ordererror (sil_State *L, const TValue *p1,
                                                 const TValue *p2);
SILI_FUNC l_noret silG_runerror (sil_State *L, const char *fmt, ...);
SILI_FUNC const char *silG_addinfo (sil_State *L, const char *msg,
                                                  TString *src, int line);
SILI_FUNC l_noret silG_errormsg (sil_State *L);
SILI_FUNC int silG_traceexec (sil_State *L, const Instruction *pc);
SILI_FUNC int silG_tracecall (sil_State *L);


#endif
