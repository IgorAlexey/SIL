/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in sil.h
*/


#ifndef lzio_h
#define lzio_h

#include "sil.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : silZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define silZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define silZ_buffer(buff)	((buff)->buffer)
#define silZ_sizebuffer(buff)	((buff)->buffsize)
#define silZ_bufflen(buff)	((buff)->n)

#define silZ_buffremove(buff,i)	((buff)->n -= cast_sizet(i))
#define silZ_resetbuffer(buff) ((buff)->n = 0)


#define silZ_resizebuffer(L, buff, size) \
	((buff)->buffer = silM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define silZ_freebuffer(L, buff)	silZ_resizebuffer(L, buff, 0)


SILI_FUNC void silZ_init (sil_State *L, ZIO *z, sil_Reader reader,
                                        void *data);
SILI_FUNC size_t silZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */

SILI_FUNC const void *silZ_getaddr (ZIO* z, size_t n);


/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  sil_Reader reader;		/* reader function */
  void *data;			/* additional data */
  sil_State *L;			/* SIL state (for reader) */
};


SILI_FUNC int silZ_fill (ZIO *z);

#endif
