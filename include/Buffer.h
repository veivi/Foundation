#ifndef BUFFER_H
#define BUFFER_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t VPBufferIndex_t;
typedef int16_t VPBufferSize_t;

#define VPBUFFER_INDEX(b, p, i) (((p)+(i)) & (b).mask)
#define VPBUFFER_GAUGE(b)       (((b).inPtr - (b).outPtr) & (b).mask)
#define VPBUFFER_SPACE(b)       ((b).mask - VPBUFFER_GAUGE(b))


typedef struct VPBuffer {
  VPBufferIndex_t inPtr, outPtr, watermark, mask;
  char *storage;
  bool overrun;
} VPBuffer_t;

#define VPBUFFER_CONS_WM(store, wm) { 0, 0, wm, sizeof(store) - 1, store }
#define VPBUFFER_CONS(store) { 0, 0, 0, sizeof(store) - 1, store }
#define VPBUFFER_CONS_NULL { 0, 0, 0, 0, NULL }

void vpbuffer_init(VPBuffer_t*, VPBufferSize_t size, char *storage);
VPBufferSize_t vpbuffer_insert(VPBuffer_t*, const char *b, VPBufferSize_t s, bool overwrite);
VPBufferSize_t vpbuffer_extract(VPBuffer_t*, char *, VPBufferSize_t);
void vpbuffer_flush(VPBuffer_t*);
bool vpbuffer_hasOverrun(VPBuffer_t*);
void vpbuffer_insertChar(VPBuffer_t*, char c);
char vpbuffer_extractChar(VPBuffer_t*);

#define vpbuffer_space(i) VPBUFFER_SPACE(*i)
#define vpbuffer_gauge(i) VPBUFFER_GAUGE(*i)

#endif
