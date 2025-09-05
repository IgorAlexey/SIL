/*
** $Id: lcode.h $
** Code generator for SIL
** See Copyright Notice in sil.h
*/

#ifndef lcode_h
#define lcode_h

#include "llex.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)


/*
** grep "ORDER OPR" if you change these enums  (ORDER OP)
*/
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR,
  OPR_SHL, OPR_SHR,
  /* string operator */
  OPR_CONCAT,
  /* comparison operators */
  OPR_EQ, OPR_LT, OPR_LE,
  OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;


/* true if operation is foldable (that is, it is arithmetic or bitwise) */
#define foldbinop(op)	((op) <= OPR_SHR)


#define silK_codeABC(fs,o,a,b,c)	silK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define silK_setmultret(fs,e)	silK_setreturns(fs, e, SIL_MULTRET)

#define silK_jumpto(fs,t)	silK_patchlist(fs, silK_jump(fs), t)

SILI_FUNC int silK_code (FuncState *fs, Instruction i);
SILI_FUNC int silK_codeABx (FuncState *fs, OpCode o, int A, int Bx);
SILI_FUNC int silK_codeABCk (FuncState *fs, OpCode o, int A, int B, int C,
                                            int k);
SILI_FUNC int silK_codevABCk (FuncState *fs, OpCode o, int A, int B, int C,
                                             int k);
SILI_FUNC int silK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
SILI_FUNC void silK_fixline (FuncState *fs, int line);
SILI_FUNC void silK_nil (FuncState *fs, int from, int n);
SILI_FUNC void silK_reserveregs (FuncState *fs, int n);
SILI_FUNC void silK_checkstack (FuncState *fs, int n);
SILI_FUNC void silK_int (FuncState *fs, int reg, sil_Integer n);
SILI_FUNC void silK_dischargevars (FuncState *fs, expdesc *e);
SILI_FUNC int silK_exp2anyreg (FuncState *fs, expdesc *e);
SILI_FUNC void silK_exp2anyregup (FuncState *fs, expdesc *e);
SILI_FUNC void silK_exp2nextreg (FuncState *fs, expdesc *e);
SILI_FUNC void silK_exp2val (FuncState *fs, expdesc *e);
SILI_FUNC void silK_self (FuncState *fs, expdesc *e, expdesc *key);
SILI_FUNC void silK_indexed (FuncState *fs, expdesc *t, expdesc *k);
SILI_FUNC void silK_goiftrue (FuncState *fs, expdesc *e);
SILI_FUNC void silK_goiffalse (FuncState *fs, expdesc *e);
SILI_FUNC void silK_storevar (FuncState *fs, expdesc *var, expdesc *e);
SILI_FUNC void silK_setreturns (FuncState *fs, expdesc *e, int nresults);
SILI_FUNC void silK_setoneret (FuncState *fs, expdesc *e);
SILI_FUNC int silK_jump (FuncState *fs);
SILI_FUNC void silK_ret (FuncState *fs, int first, int nret);
SILI_FUNC void silK_patchlist (FuncState *fs, int list, int target);
SILI_FUNC void silK_patchtohere (FuncState *fs, int list);
SILI_FUNC void silK_concat (FuncState *fs, int *l1, int l2);
SILI_FUNC int silK_getlabel (FuncState *fs);
SILI_FUNC void silK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
SILI_FUNC void silK_infix (FuncState *fs, BinOpr op, expdesc *v);
SILI_FUNC void silK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
SILI_FUNC void silK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
SILI_FUNC void silK_setlist (FuncState *fs, int base, int nelems, int tostore);
SILI_FUNC void silK_finish (FuncState *fs);
SILI_FUNC l_noret silK_semerror (LexState *ls, const char *fmt, ...);


#endif
