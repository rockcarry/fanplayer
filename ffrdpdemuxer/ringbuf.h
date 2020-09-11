#ifndef __RINGBUF_H__
#define __RINGBUF_H__

#include <stdint.h>

int ringbuf_write(uint8_t *rbuf, int maxsize, int tail, uint8_t *src, int len);
int ringbuf_read (uint8_t *rbuf, int maxsize, int head, uint8_t *dst, int len);

#endif
