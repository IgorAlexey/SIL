/*
** $Id: linit.c $
** Initialization of libraries for sil.c and other clients
** See Copyright Notice in sil.h
*/


#define linit_c
#define SIL_LIB


#include "lprefix.h"


#include <stddef.h>

#include "sil.h"

#include "sillib.h"
#include "lauxlib.h"
#include "llimits.h"


/*
** Standard Libraries. (Must be listed in the same ORDER of their
** respective constants SIL_<libname>K.)
*/
static const silL_Reg stdlibs[] = {
  {SIL_GNAME, silopen_base},
  {SIL_LOADLIBNAME, silopen_package},
  {SIL_COLIBNAME, silopen_coroutine},
  {SIL_DBLIBNAME, silopen_debug},
  {SIL_IOLIBNAME, silopen_io},
  {SIL_MATHLIBNAME, silopen_math},
  {SIL_OSLIBNAME, silopen_os},
  {SIL_STRLIBNAME, silopen_string},
  {SIL_TABLIBNAME, silopen_table},
  {SIL_UTF8LIBNAME, silopen_utf8},
  {NULL, NULL}
};


/*
** require and preload selected standard libraries
*/
SILLIB_API void silL_openselectedlibs (sil_State *L, int load, int preload) {
  int mask;
  const silL_Reg *lib;
  silL_getsubtable(L, SIL_REGISTRYINDEX, SIL_PRELOAD_TABLE);
  for (lib = stdlibs, mask = 1; lib->name != NULL; lib++, mask <<= 1) {
    if (load & mask) {  /* selected? */
      silL_requiref(L, lib->name, lib->func, 1);  /* require library */
      sil_pop(L, 1);  /* remove result from the stack */
    }
    else if (preload & mask) {  /* selected? */
      sil_pushcfunction(L, lib->func);
      sil_setfield(L, -2, lib->name);  /* add library to PRELOAD table */
    }
  }
  sil_assert((mask >> 1) == SIL_UTF8LIBK);
  sil_pop(L, 1);  /* remove PRELOAD table */
}

