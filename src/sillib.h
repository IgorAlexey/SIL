/*
** $Id: sillib.h $
** SIL standard libraries
** See Copyright Notice in sil.h
*/


#ifndef sillib_h
#define sillib_h

#include "sil.h"


/* version suffix for environment variable names */
#define SIL_VERSUFFIX          "_" SIL_VERSION_MAJOR "_" SIL_VERSION_MINOR

#define SIL_GLIBK		1
SILMOD_API int (silopen_base) (sil_State *L);

#define SIL_LOADLIBNAME	"package"
#define SIL_LOADLIBK	(SIL_GLIBK << 1)
SILMOD_API int (silopen_package) (sil_State *L);


#define SIL_COLIBNAME	"coroutine"
#define SIL_COLIBK	(SIL_LOADLIBK << 1)
SILMOD_API int (silopen_coroutine) (sil_State *L);

#define SIL_DBLIBNAME	"debug"
#define SIL_DBLIBK	(SIL_COLIBK << 1)
SILMOD_API int (silopen_debug) (sil_State *L);

#define SIL_IOLIBNAME	"io"
#define SIL_IOLIBK	(SIL_DBLIBK << 1)
SILMOD_API int (silopen_io) (sil_State *L);

#define SIL_MATHLIBNAME	"math"
#define SIL_MATHLIBK	(SIL_IOLIBK << 1)
SILMOD_API int (silopen_math) (sil_State *L);

#define SIL_OSLIBNAME	"os"
#define SIL_OSLIBK	(SIL_MATHLIBK << 1)
SILMOD_API int (silopen_os) (sil_State *L);

#define SIL_STRLIBNAME	"string"
#define SIL_STRLIBK	(SIL_OSLIBK << 1)
SILMOD_API int (silopen_string) (sil_State *L);

#define SIL_TABLIBNAME	"table"
#define SIL_TABLIBK	(SIL_STRLIBK << 1)
SILMOD_API int (silopen_table) (sil_State *L);

#define SIL_UTF8LIBNAME	"utf8"
#define SIL_UTF8LIBK	(SIL_TABLIBK << 1)
SILMOD_API int (silopen_utf8) (sil_State *L);


/* open selected libraries */
SILLIB_API void (silL_openselectedlibs) (sil_State *L, int load, int preload);

/* open all libraries */
#define silL_openlibs(L)	silL_openselectedlibs(L, ~0, 0)


#endif
