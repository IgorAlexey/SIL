/*
** $Id: lundump.h $
** load precompiled SIL chunks
** See Copyright Notice in sil.h
*/

#ifndef lundump_h
#define lundump_h

#include <limits.h>

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define SILC_DATA	"\x19\x93\r\n\x1a\n"

#define SILC_INT	-0x5678
#define SILC_INST	0x12345678
#define SILC_NUM	cast_num(-370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define SILC_VERSION	(SIL_VERSION_MAJOR_N*16+SIL_VERSION_MINOR_N)

#define SILC_FORMAT	0	/* this is the official format */


/* load one chunk; from lundump.c */
SILI_FUNC LClosure* silU_undump (sil_State* L, ZIO* Z, const char* name,
                                               int fixed);

/* dump one chunk; from ldump.c */
SILI_FUNC int silU_dump (sil_State* L, const Proto* f, sil_Writer w,
                         void* data, int strip);

#endif
