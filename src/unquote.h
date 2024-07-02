#pragma once
#include <stdint.h>
#include <string.h>
#include <x86intrin.h>

/**
 *  NOTE: unquote_t is basically scan_t without scanning for delim and newline.
 */

typedef struct unquote_t unquote_t;
struct unquote_t {
  // for alignment
  __m256i qte, esc;
  char tmpbuf[32]; // copy when (q-base) < 32b

  // orig <= base <= p <= q.
  const char *orig; // scan started here
  const char *base; // the current 32-byte in orig[]
  const char *p;    // ptr to current token
  const char *q;    // scan ends here

  uint32_t flag;  // bmap marks interesting bits offset from base
  int esc_is_qte; // true if esc == qte
};

static int __unquote_calcflag(unquote_t *unq) {
  const char *base = unq->base;
  int len = unq->q - base;
  if (len < 32) {
    if (len <= 0) {
      return -1;
    }
    memset(unq->tmpbuf, 0, sizeof(unq->tmpbuf));
    memcpy(unq->tmpbuf, base, len);
    base = unq->tmpbuf;
  }

  __m256i src = _mm256_loadu_si256((__m256i *)base);
  if (unq->esc_is_qte) {
    unq->flag = _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, unq->qte));
  } else {
    unq->flag = _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, unq->qte)) |
                _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, unq->esc));
  }
  return 0;
}

static inline void unquote_init(unquote_t *unq, char qte, char esc) {
  memset(unq, 0, sizeof(*unq));
  unq->qte = _mm256_set1_epi8(qte);
  unq->esc = _mm256_set1_epi8(esc);
  unq->esc_is_qte = (esc == qte);
}

static inline void unquote_reset(unquote_t *unq, const char *buf, int buflen) {
  unq->orig = buf;
  unq->base = buf;
  unq->p = buf;
  unq->q = buf + buflen;
  __unquote_calcflag(unq);
}

static void __unquote_forward(unquote_t *unq) {
  while (!unq->flag) {
    unq->base += 32;
    if (__unquote_calcflag(unq))
      break;
  }
}

static inline const char *unquote_peek(unquote_t *unq) {
  if (!unq->flag) {
    __unquote_forward(unq);
  }
  // __builtin_ffs: returns one plus the index of the least significant 1-bit of
  // x, or if x is zero, returns zero.
  int off = __builtin_ffs(unq->flag) - 1;
  return off >= 0 ? unq->base + off : 0;
}

static inline const char *unquote_pop(unquote_t *unq) {
  if (!unq->flag) {
    __unquote_forward(unq);
  }
  // __builtin_ffs: returns one plus the index of the least significant 1-bit of
  // x, or if x is zero, returns zero.
  int off = __builtin_ffs(unq->flag) - 1;
  if (off >= 0) {
    unq->flag &= ~(1 << off);
    unq->p = unq->base + off;
  } else {
    unq->p = unq->q;
  }
  return (unq->p < unq->q) ? unq->p : 0;
}
