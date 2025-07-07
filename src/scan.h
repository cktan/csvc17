#pragma once
#include <stdint.h>
#include <string.h>
#include <x86intrin.h>

/**
 *  This is a scanner that uses SIMD to locate the next interesting
 *  char in an array of bytes. Supports up to 4 interesting chars.
 */
typedef struct scan_t scan_t;
struct scan_t {
  // for alignment
  char inuse[4]; // flag which of ch[x] are in use
  __m256i ch[4];
  char tmpbuf[32]; // copy when (q-base) < 32b

  // orig <= base <= p <= q.
  const char *orig; // scan started here
  const char *base; // the current 32-byte in orig[]
  const char *p;    // ptr to current token
  const char *q;    // scan ends here

  uint32_t flag; // bmap marks interesting bits offset from base
};

static int __scan_calcflag(scan_t *scan) {
  const char *base = scan->base;
  int64_t len = scan->q - base;
  if (len < 32) {
    if (len <= 0) {
      return -1;
    }
    // memset(scan->tmpbuf, 0, sizeof(scan->tmpbuf));
    memcpy(scan->tmpbuf, base, len);
    base = scan->tmpbuf;
  }

  __m256i src = _mm256_loadu_si256((__m256i *)base);
  scan->flag = _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, scan->ch[0]));
  for (int i = 1; i < 4; i++) {
    if (scan->inuse[i]) {
      scan->flag |= _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, scan->ch[i]));
    }
  }
  return 0;
}

static scan_t scan_init(const char *accept) {
  scan_t scan;
  memset(&scan, 0, sizeof(scan));
  for (int i = 0; i < 4; i++) {
    if (!accept[i])
      break;
    scan.ch[i] = _mm256_set1_epi8(accept[i]);
    scan.inuse[i] = 1;
  }
  return scan;
}

static void scan_reset(scan_t *scan, const char *buf, int64_t buflen) {
  scan->orig = buf;
  scan->base = buf;
  scan->p = buf;
  scan->q = buf + buflen;
  __scan_calcflag(scan);
}

static void __scan_forward(scan_t *scan) {
  while (!scan->flag) {
    scan->base += 32;
    if (__scan_calcflag(scan))
      break;
  }
}

static inline const char *scan_next(scan_t *scan) {
  if (!scan->flag) {
    __scan_forward(scan);
  }
  // __builtin_ffs: returns one plus the index of the least significant 1-bit of
  // x, or if x is zero, returns zero.
  int off = __builtin_ffs(scan->flag) - 1;
  if (off >= 0) {
    scan->flag &= ~(1 << off);
    scan->p = scan->base + off;
  } else {
    scan->p = scan->q;
  }
  return (scan->p < scan->q) ? scan->p++ : 0;
}

static inline int scan_match(scan_t *scan, int ch) { return ch == *scan->p; }
