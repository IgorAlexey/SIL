/*
** $Id: lstring.c $
** String table (keeps all strings handled by SIL)
** See Copyright Notice in sil.h
*/

#define lstring_c
#define SIL_CORE

#include "lprefix.h"


#include <string.h>

#include "sil.h"

#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"


/*
** Maximum size for string table.
*/
#define MAXSTRTB	cast_int(silM_limitN(INT_MAX, TString*))

/*
** Initial size for the string table (must be power of 2).
** The SIL core alone registers ~50 strings (reserved words +
** metaevent keys + a few others). Libraries would typically add
** a few dozens more.
*/
#if !defined(MINSTRTABSIZE)
#define MINSTRTABSIZE   128
#endif


/*
** equality for long strings
*/
int silS_eqlngstr (TString *a, TString *b) {
  size_t len = a->u.lnglen;
  sil_assert(a->tt == SIL_VLNGSTR && b->tt == SIL_VLNGSTR);
  return (a == b) ||  /* same instance or... */
    ((len == b->u.lnglen) &&  /* equal length and ... */
     (memcmp(getlngstr(a), getlngstr(b), len) == 0));  /* equal contents */
}


unsigned silS_hash (const char *str, size_t l, unsigned seed) {
  unsigned int h = seed ^ cast_uint(l);
  for (; l > 0; l--)
    h ^= ((h<<5) + (h>>2) + cast_byte(str[l - 1]));
  return h;
}


unsigned silS_hashlongstr (TString *ts) {
  sil_assert(ts->tt == SIL_VLNGSTR);
  if (ts->extra == 0) {  /* no hash? */
    size_t len = ts->u.lnglen;
    ts->hash = silS_hash(getlngstr(ts), len, ts->hash);
    ts->extra = 1;  /* now it has its hash */
  }
  return ts->hash;
}


static void tablerehash (TString **vect, int osize, int nsize) {
  int i;
  for (i = osize; i < nsize; i++)  /* clear new elements */
    vect[i] = NULL;
  for (i = 0; i < osize; i++) {  /* rehash old part of the array */
    TString *p = vect[i];
    vect[i] = NULL;
    while (p) {  /* for each string in the list */
      TString *hnext = p->u.hnext;  /* save next */
      unsigned int h = lmod(p->hash, nsize);  /* new position */
      p->u.hnext = vect[h];  /* chain it into array */
      vect[h] = p;
      p = hnext;
    }
  }
}


/*
** Resize the string table. If allocation fails, keep the current size.
** (This can degrade performance, but any non-zero size should work
** correctly.)
*/
void silS_resize (sil_State *L, int nsize) {
  stringtable *tb = &G(L)->strt;
  int osize = tb->size;
  TString **newvect;
  if (nsize < osize)  /* shrinking table? */
    tablerehash(tb->hash, osize, nsize);  /* depopulate shrinking part */
  newvect = silM_reallocvector(L, tb->hash, osize, nsize, TString*);
  if (l_unlikely(newvect == NULL)) {  /* reallocation failed? */
    if (nsize < osize)  /* was it shrinking table? */
      tablerehash(tb->hash, nsize, osize);  /* restore to original size */
    /* leave table as it was */
  }
  else {  /* allocation succeeded */
    tb->hash = newvect;
    tb->size = nsize;
    if (nsize > osize)
      tablerehash(newvect, osize, nsize);  /* rehash for new size */
  }
}


/*
** Clear API string cache. (Entries cannot be empty, so fill them with
** a non-collectable string.)
*/
void silS_clearcache (global_State *g) {
  int i, j;
  for (i = 0; i < STRCACHE_N; i++)
    for (j = 0; j < STRCACHE_M; j++) {
      if (iswhite(g->strcache[i][j]))  /* will entry be collected? */
        g->strcache[i][j] = g->memerrmsg;  /* replace it with something fixed */
    }
}


/*
** Initialize the string table and the string cache
*/
void silS_init (sil_State *L) {
  global_State *g = G(L);
  int i, j;
  stringtable *tb = &G(L)->strt;
  tb->hash = silM_newvector(L, MINSTRTABSIZE, TString*);
  tablerehash(tb->hash, 0, MINSTRTABSIZE);  /* clear array */
  tb->size = MINSTRTABSIZE;
  /* pre-create memory-error message */
  g->memerrmsg = silS_newliteral(L, MEMERRMSG);
  silC_fix(L, obj2gco(g->memerrmsg));  /* it should never be collected */
  for (i = 0; i < STRCACHE_N; i++)  /* fill cache with valid strings */
    for (j = 0; j < STRCACHE_M; j++)
      g->strcache[i][j] = g->memerrmsg;
}


size_t silS_sizelngstr (size_t len, int kind) {
  switch (kind) {
    case LSTRREG:  /* regular long string */
      /* don't need 'falloc'/'ud', but need space for content */
      return offsetof(TString, falloc) + (len + 1) * sizeof(char);
    case LSTRFIX:  /* fixed external long string */
      /* don't need 'falloc'/'ud' */
      return offsetof(TString, falloc);
    default:  /* external long string with deallocation */
      sil_assert(kind == LSTRMEM);
      return sizeof(TString);
  }
}


/*
** creates a new string object
*/
static TString *createstrobj (sil_State *L, size_t totalsize, lu_byte tag,
                              unsigned h) {
  TString *ts;
  GCObject *o;
  o = silC_newobj(L, tag, totalsize);
  ts = gco2ts(o);
  ts->hash = h;
  ts->extra = 0;
  return ts;
}


TString *silS_createlngstrobj (sil_State *L, size_t l) {
  size_t totalsize = silS_sizelngstr(l, LSTRREG);
  TString *ts = createstrobj(L, totalsize, SIL_VLNGSTR, G(L)->seed);
  ts->u.lnglen = l;
  ts->shrlen = LSTRREG;  /* signals that it is a regular long string */
  ts->contents = cast_charp(ts) + offsetof(TString, falloc);
  ts->contents[l] = '\0';  /* ending 0 */
  return ts;
}


void silS_remove (sil_State *L, TString *ts) {
  stringtable *tb = &G(L)->strt;
  TString **p = &tb->hash[lmod(ts->hash, tb->size)];
  while (*p != ts)  /* find previous element */
    p = &(*p)->u.hnext;
  *p = (*p)->u.hnext;  /* remove element from its list */
  tb->nuse--;
}


static void growstrtab (sil_State *L, stringtable *tb) {
  if (l_unlikely(tb->nuse == INT_MAX)) {  /* too many strings? */
    silC_fullgc(L, 1);  /* try to free some... */
    if (tb->nuse == INT_MAX)  /* still too many? */
      silM_error(L);  /* cannot even create a message... */
  }
  if (tb->size <= MAXSTRTB / 2)  /* can grow string table? */
    silS_resize(L, tb->size * 2);
}


/*
** Checks whether short string exists and reuses it or creates a new one.
*/
static TString *internshrstr (sil_State *L, const char *str, size_t l) {
  TString *ts;
  global_State *g = G(L);
  stringtable *tb = &g->strt;
  unsigned int h = silS_hash(str, l, g->seed);
  TString **list = &tb->hash[lmod(h, tb->size)];
  sil_assert(str != NULL);  /* otherwise 'memcmp'/'memcpy' are undefined */
  for (ts = *list; ts != NULL; ts = ts->u.hnext) {
    if (l == cast_uint(ts->shrlen) &&
        (memcmp(str, getshrstr(ts), l * sizeof(char)) == 0)) {
      /* found! */
      if (isdead(g, ts))  /* dead (but not collected yet)? */
        changewhite(ts);  /* resurrect it */
      return ts;
    }
  }
  /* else must create a new string */
  if (tb->nuse >= tb->size) {  /* need to grow string table? */
    growstrtab(L, tb);
    list = &tb->hash[lmod(h, tb->size)];  /* rehash with new size */
  }
  ts = createstrobj(L, sizestrshr(l), SIL_VSHRSTR, h);
  ts->shrlen = cast(ls_byte, l);
  getshrstr(ts)[l] = '\0';  /* ending 0 */
  memcpy(getshrstr(ts), str, l * sizeof(char));
  ts->u.hnext = *list;
  *list = ts;
  tb->nuse++;
  return ts;
}


/*
** new string (with explicit length)
*/
TString *silS_newlstr (sil_State *L, const char *str, size_t l) {
  if (l <= SILI_MAXSHORTLEN)  /* short string? */
    return internshrstr(L, str, l);
  else {
    TString *ts;
    if (l_unlikely(l * sizeof(char) >= (MAX_SIZE - sizeof(TString))))
      silM_toobig(L);
    ts = silS_createlngstrobj(L, l);
    memcpy(getlngstr(ts), str, l * sizeof(char));
    return ts;
  }
}


/*
** Create or reuse a zero-terminated string, first checking in the
** cache (using the string address as a key). The cache can contain
** only zero-terminated strings, so it is safe to use 'strcmp' to
** check hits.
*/
TString *silS_new (sil_State *L, const char *str) {
  unsigned int i = point2uint(str) % STRCACHE_N;  /* hash */
  int j;
  TString **p = G(L)->strcache[i];
  for (j = 0; j < STRCACHE_M; j++) {
    if (strcmp(str, getstr(p[j])) == 0)  /* hit? */
      return p[j];  /* that is it */
  }
  /* normal route */
  for (j = STRCACHE_M - 1; j > 0; j--)
    p[j] = p[j - 1];  /* move out last element */
  /* new element is first in the list */
  p[0] = silS_newlstr(L, str, strlen(str));
  return p[0];
}


Udata *silS_newudata (sil_State *L, size_t s, unsigned short nuvalue) {
  Udata *u;
  int i;
  GCObject *o;
  if (l_unlikely(s > MAX_SIZE - udatamemoffset(nuvalue)))
    silM_toobig(L);
  o = silC_newobj(L, SIL_VUSERDATA, sizeudata(nuvalue, s));
  u = gco2u(o);
  u->len = s;
  u->nuvalue = nuvalue;
  u->metatable = NULL;
  for (i = 0; i < nuvalue; i++)
    setnilvalue(&u->uv[i].uv);
  return u;
}


struct NewExt {
  ls_byte kind;
  const char *s;
   size_t len;
  TString *ts;  /* output */
};


static void f_newext (sil_State *L, void *ud) {
  struct NewExt *ne = cast(struct NewExt *, ud);
  size_t size = silS_sizelngstr(0, ne->kind);
  ne->ts = createstrobj(L, size, SIL_VLNGSTR, G(L)->seed);
}


static void f_pintern (sil_State *L, void *ud) {
  struct NewExt *ne = cast(struct NewExt *, ud);
  ne->ts = internshrstr(L, ne->s, ne->len);
}


TString *silS_newextlstr (sil_State *L,
	          const char *s, size_t len, sil_Alloc falloc, void *ud) {
  struct NewExt ne;
  if (len <= SILI_MAXSHORTLEN) {  /* short string? */
    ne.s = s; ne.len = len;
    if (!falloc)
      f_pintern(L, &ne);  /* just internalize string */
    else {
      TStatus status = silD_rawrunprotected(L, f_pintern, &ne);
      (*falloc)(ud, cast_voidp(s), len + 1, 0);  /* free external string */
      if (status != SIL_OK)  /* memory error? */
        silM_error(L);  /* re-raise memory error */
    }
    return ne.ts;
  }
  /* "normal" case: long strings */
  if (!falloc) {
    ne.kind = LSTRFIX;
    f_newext(L, &ne);  /* just create header */
  }
  else {
    ne.kind = LSTRMEM;
    if (silD_rawrunprotected(L, f_newext, &ne) != SIL_OK) {  /* mem. error? */
      (*falloc)(ud, cast_voidp(s), len + 1, 0);  /* free external string */
      silM_error(L);  /* re-raise memory error */
    }
    ne.ts->falloc = falloc;
    ne.ts->ud = ud;
  }
  ne.ts->shrlen = ne.kind;
  ne.ts->u.lnglen = len;
  ne.ts->contents = cast_charp(s);
  return ne.ts;
}


