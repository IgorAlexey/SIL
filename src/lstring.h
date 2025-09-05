/*
** $Id: lstring.h $
** String table (keep all strings handled by SIL)
** See Copyright Notice in sil.h
*/

#ifndef lstring_h
#define lstring_h

#include "lgc.h"
#include "lobject.h"
#include "lstate.h"


/*
** Memory-allocation error message must be preallocated (it cannot
** be created after memory is exhausted)
*/
#define MEMERRMSG       "not enough memory"


/*
** Maximum length for short strings, that is, strings that are
** internalized. (Cannot be smaller than reserved words or tags for
** metamethods, as these strings must be internalized;
** #("function") = 8, #("__newindex") = 10.)
*/
#if !defined(SILI_MAXSHORTLEN)
#define SILI_MAXSHORTLEN	40
#endif


/*
** Size of a short TString: Size of the header plus space for the string
** itself (including final '\0').
*/
#define sizestrshr(l)  \
	(offsetof(TString, contents) + ((l) + 1) * sizeof(char))


#define silS_newliteral(L, s)	(silS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	(strisshr(s) && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == SIL_VSHRSTR, (a) == (b))


SILI_FUNC unsigned silS_hash (const char *str, size_t l, unsigned seed);
SILI_FUNC unsigned silS_hashlongstr (TString *ts);
SILI_FUNC int silS_eqlngstr (TString *a, TString *b);
SILI_FUNC void silS_resize (sil_State *L, int newsize);
SILI_FUNC void silS_clearcache (global_State *g);
SILI_FUNC void silS_init (sil_State *L);
SILI_FUNC void silS_remove (sil_State *L, TString *ts);
SILI_FUNC Udata *silS_newudata (sil_State *L, size_t s,
                                              unsigned short nuvalue);
SILI_FUNC TString *silS_newlstr (sil_State *L, const char *str, size_t l);
SILI_FUNC TString *silS_new (sil_State *L, const char *str);
SILI_FUNC TString *silS_createlngstrobj (sil_State *L, size_t l);
SILI_FUNC TString *silS_newextlstr (sil_State *L,
		const char *s, size_t len, sil_Alloc falloc, void *ud);
SILI_FUNC size_t silS_sizelngstr (size_t len, int kind);

#endif
