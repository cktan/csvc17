#pragma once
#include <stdint.h>
#include <string.h>
#include <x86intrin.h>

typedef struct scan_t scan_t;
struct scan_t {
  uint32_t flag;
  // orig <= base <= p <= q.
  char *orig;   // scan started here
  char *base;   // the current 32-byte
  char *prev_p; // ptr to previous token
  char *p;      // ptr to current token
  char *q;      // scan ends here
  __m256i qte, esc, delim, endl;
  char tmpbuf[32]; // copy when (q-base) < 32b
  int esc_is_qte;
};

static inline void scan_init(scan_t *scan, char qte, char esc, char delim) {
  memset(scan, 0, sizeof(*scan));
  scan->qte = _mm256_set1_epi8(qte);
  scan->esc = _mm256_set1_epi8(esc);
  scan->delim = _mm256_set1_epi8(delim);
  scan->endl = _mm256_set1_epi8('\n');
  scan->esc_is_qte = (esc == qte);
}

static inline void scan_reset(scan_t *scan, char *p, int len) {
  scan->orig = p;
  scan->base = p;
  scan->p = p;
  scan->q = p + len;
  scan->flag = 0;
}

static void __scan_forward(scan_t *scan) {
  while (!scan->flag) {
    char *base = scan->base;
    int len = scan->q - base;
    if (len < 32) {
      if (len <= 0) {
        return;
      }
      memset(scan->tmpbuf, 0, sizeof(scan->tmpbuf));
      memcpy(scan->tmpbuf, scan->p, len);
      base = scan->tmpbuf;
    }

    __m256i src = _mm256_loadu_si256((__m256i *)base);
    if (scan->esc_is_qte) {
      scan->flag = _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, scan->qte)) |
                   _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, scan->delim)) |
                   _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, scan->endl));
    } else {
      scan->flag = _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, scan->qte)) |
                   _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, scan->esc)) |
                   _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, scan->delim)) |
                   _mm256_movemask_epi8(_mm256_cmpeq_epi8(src, scan->endl));
    }

    if (!scan->flag) {
      scan->base += 32;
    }
  }
}

static inline char *scan_peek(scan_t *scan) {
  if (!scan->flag) {
    __scan_forward(scan);
  }
  // __builtin_ffs: returns one plus the index of the least significant 1-bit of
  // x, or if x is zero, returns zero.
  int off = __builtin_ffs(scan->flag) - 1;
  return off >= 0 ? scan->base + off : 0;
}

static inline char *scan_pop(scan_t *scan) {
  if (!scan->flag) {
    __scan_forward(scan);
  }
  // __builtin_ffs: returns one plus the index of the least significant 1-bit of
  // x, or if x is zero, returns zero.
  scan->prev_p = scan->p;
  int off = __builtin_ffs(scan->flag) - 1;
  if (off >= 0) {
    scan->flag &= ~(1 << off);
    scan->p = scan->base + off;
  } else {
    scan->p = scan->q;
  }
  return (scan->p < scan->q) ? scan->p : 0;
}