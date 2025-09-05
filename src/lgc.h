/*
** $Id: lgc.h $
** Garbage Collector
** See Copyright Notice in sil.h
*/

#ifndef lgc_h
#define lgc_h


#include <stddef.h>


#include "lobject.h"
#include "lstate.h"

/*
** Collectable objects may have one of three colors: white, which means
** the object is not marked; gray, which means the object is marked, but
** its references may be not marked; and black, which means that the
** object and all its references are marked.  The main invariant of the
** garbage collector, while marking objects, is that a black object can
** never point to a white one. Moreover, any gray object must be in a
** "gray list" (gray, grayagain, weak, allweak, ephemeron) so that it
** can be visited again before finishing the collection cycle. (Open
** upvalues are an exception to this rule, as they are attached to
** a corresponding thread.)  These lists have no meaning when the
** invariant is not being enforced (e.g., sweep phase).
*/


/*
** Possible states of the Garbage Collector
*/
#define GCSpropagate	0
#define GCSenteratomic	1
#define GCSatomic	2
#define GCSswpallgc	3
#define GCSswpfinobj	4
#define GCSswptobefnz	5
#define GCSswpend	6
#define GCScallfin	7
#define GCSpause	8


#define issweepphase(g)  \
	(GCSswpallgc <= (g)->gcstate && (g)->gcstate <= GCSswpend)


/*
** macro to tell when main invariant (white objects cannot point to black
** ones) must be kept. During a collection, the sweep phase may break
** the invariant, as objects turned white may point to still-black
** objects. The invariant is restored when sweep ends and all objects
** are white again.
*/

#define keepinvariant(g)	((g)->gcstate <= GCSatomic)


/*
** some useful bit tricks
*/
#define resetbits(x,m)		((x) &= cast_byte(~(m)))
#define setbits(x,m)		((x) |= (m))
#define testbits(x,m)		((x) & (m))
#define bitmask(b)		(1<<(b))
#define bit2mask(b1,b2)		(bitmask(b1) | bitmask(b2))
#define l_setbit(x,b)		setbits(x, bitmask(b))
#define resetbit(x,b)		resetbits(x, bitmask(b))
#define testbit(x,b)		testbits(x, bitmask(b))


/*
** Layout for bit use in 'marked' field. First three bits are
** used for object "age" in generational mode. Last bit is used
** by tests.
*/
#define WHITE0BIT	3  /* object is white (type 0) */
#define WHITE1BIT	4  /* object is white (type 1) */
#define BLACKBIT	5  /* object is black */
#define FINALIZEDBIT	6  /* object has been marked for finalization */

#define TESTBIT		7



#define WHITEBITS	bit2mask(WHITE0BIT, WHITE1BIT)


#define iswhite(x)      testbits((x)->marked, WHITEBITS)
#define isblack(x)      testbit((x)->marked, BLACKBIT)
#define isgray(x)  /* neither white nor black */  \
	(!testbits((x)->marked, WHITEBITS | bitmask(BLACKBIT)))

#define tofinalize(x)	testbit((x)->marked, FINALIZEDBIT)

#define otherwhite(g)	((g)->currentwhite ^ WHITEBITS)
#define isdeadm(ow,m)	((m) & (ow))
#define isdead(g,v)	isdeadm(otherwhite(g), (v)->marked)

#define changewhite(x)	((x)->marked ^= WHITEBITS)
#define nw2black(x)  \
	check_exp(!iswhite(x), l_setbit((x)->marked, BLACKBIT))

#define silC_white(g)	cast_byte((g)->currentwhite & WHITEBITS)


/* object age in generational mode */
#define G_NEW		0	/* created in current cycle */
#define G_SURVIVAL	1	/* created in previous cycle */
#define G_OLD0		2	/* marked old by frw. barrier in this cycle */
#define G_OLD1		3	/* first full cycle as old */
#define G_OLD		4	/* really old object (not to be visited) */
#define G_TOUCHED1	5	/* old object touched this cycle */
#define G_TOUCHED2	6	/* old object touched in previous cycle */

#define AGEBITS		7  /* all age bits (111) */

#define getage(o)	((o)->marked & AGEBITS)
#define setage(o,a)  ((o)->marked = cast_byte(((o)->marked & (~AGEBITS)) | a))
#define isold(o)	(getage(o) > G_SURVIVAL)


/*
** In generational mode, objects are created 'new'. After surviving one
** cycle, they become 'survival'. Both 'new' and 'survival' can point
** to any other object, as they are traversed at the end of the cycle.
** We call them both 'young' objects.
** If a survival object survives another cycle, it becomes 'old1'.
** 'old1' objects can still point to survival objects (but not to
** new objects), so they still must be traversed. After another cycle
** (that, being old, 'old1' objects will "survive" no matter what)
** finally the 'old1' object becomes really 'old', and then they
** are no more traversed.
**
** To keep its invariants, the generational mode uses the same barriers
** also used by the incremental mode. If a young object is caught in a
** forward barrier, it cannot become old immediately, because it can
** still point to other young objects. Instead, it becomes 'old0',
** which in the next cycle becomes 'old1'. So, 'old0' objects is
** old but can point to new and survival objects; 'old1' is old
** but cannot point to new objects; and 'old' cannot point to any
** young object.
**
** If any old object ('old0', 'old1', 'old') is caught in a back
** barrier, it becomes 'touched1' and goes into a gray list, to be
** visited at the end of the cycle.  There it evolves to 'touched2',
** which can point to survivals but not to new objects. In yet another
** cycle then it becomes 'old' again.
**
** The generational mode must also control the colors of objects,
** because of the barriers.  While the mutator is running, young objects
** are kept white. 'old', 'old1', and 'touched2' objects are kept black,
** as they cannot point to new objects; exceptions are threads and open
** upvalues, which age to 'old1' and 'old' but are kept gray. 'old0'
** objects may be gray or black, as in the incremental mode. 'touched1'
** objects are kept gray, as they must be visited again at the end of
** the cycle.
*/


/*
** {======================================================
** Default Values for GC parameters
** =======================================================
*/

/*
** Minor collections will shift to major ones after SILI_MINORMAJOR%
** bytes become old.
*/
#define SILI_MINORMAJOR         70

/*
** Major collections will shift to minor ones after a collection
** collects at least SILI_MAJORMINOR% of the new bytes.
*/
#define SILI_MAJORMINOR         50

/*
** A young (minor) collection will run after creating SILI_GENMINORMUL%
** new bytes.
*/
#define SILI_GENMINORMUL         20


/* incremental */

/* Number of bytes must be SILI_GCPAUSE% before starting new cycle */
#define SILI_GCPAUSE    250

/*
** Step multiplier: The collector handles SILI_GCMUL% work units for
** each new allocated word. (Each "work unit" corresponds roughly to
** sweeping one object or traversing one slot.)
*/
#define SILI_GCMUL      200

/* How many bytes to allocate before next GC step */
#define SILI_GCSTEPSIZE	(200 * sizeof(Table))


#define setgcparam(g,p,v)  (g->gcparams[SIL_GCP##p] = silO_codeparam(v))
#define applygcparam(g,p,x)  silO_applyparam(g->gcparams[SIL_GCP##p], x)

/* }====================================================== */


/*
** Control when GC is running:
*/
#define GCSTPUSR	1  /* bit true when GC stopped by user */
#define GCSTPGC		2  /* bit true when GC stopped by itself */
#define GCSTPCLS	4  /* bit true when closing SIL state */
#define gcrunning(g)	((g)->gcstp == 0)


/*
** Does one step of collection when debt becomes zero. 'pre'/'pos'
** allows some adjustments to be done only when needed. macro
** 'condchangemem' is used only for heavy tests (forcing a full
** GC cycle on every opportunity)
*/

#if !defined(HARDMEMTESTS)
#define condchangemem(L,pre,pos,emg)	((void)0)
#else
#define condchangemem(L,pre,pos,emg)  \
	{ if (gcrunning(G(L))) { pre; silC_fullgc(L, emg); pos; } }
#endif

#define silC_condGC(L,pre,pos) \
	{ if (G(L)->GCdebt <= 0) { pre; silC_step(L); pos;}; \
	  condchangemem(L,pre,pos,0); }

/* more often than not, 'pre'/'pos' are empty */
#define silC_checkGC(L)		silC_condGC(L,(void)0,(void)0)


#define silC_objbarrier(L,p,o) (  \
	(isblack(p) && iswhite(o)) ? \
	silC_barrier_(L,obj2gco(p),obj2gco(o)) : cast_void(0))

#define silC_barrier(L,p,v) (  \
	iscollectable(v) ? silC_objbarrier(L,p,gcvalue(v)) : cast_void(0))

#define silC_objbarrierback(L,p,o) (  \
	(isblack(p) && iswhite(o)) ? silC_barrierback_(L,p) : cast_void(0))

#define silC_barrierback(L,p,v) (  \
	iscollectable(v) ? silC_objbarrierback(L, p, gcvalue(v)) : cast_void(0))

SILI_FUNC void silC_fix (sil_State *L, GCObject *o);
SILI_FUNC void silC_freeallobjects (sil_State *L);
SILI_FUNC void silC_step (sil_State *L);
SILI_FUNC void silC_runtilstate (sil_State *L, int state, int fast);
SILI_FUNC void silC_fullgc (sil_State *L, int isemergency);
SILI_FUNC GCObject *silC_newobj (sil_State *L, lu_byte tt, size_t sz);
SILI_FUNC GCObject *silC_newobjdt (sil_State *L, lu_byte tt, size_t sz,
                                                 size_t offset);
SILI_FUNC void silC_barrier_ (sil_State *L, GCObject *o, GCObject *v);
SILI_FUNC void silC_barrierback_ (sil_State *L, GCObject *o);
SILI_FUNC void silC_checkfinalizer (sil_State *L, GCObject *o, Table *mt);
SILI_FUNC void silC_changemode (sil_State *L, int newmode);


#endif
