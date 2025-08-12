#pragma once
#include <arm_neon.h>
#include <stdint.h>
#include <string.h>

typedef struct scan_t scan_t;
struct scan_t {
  char inuse[4];
  uint8x16_t ch[4];
  char tmpbuf[16]; // copy when (q-base) < 16 bytes

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
  if (len < 16) {
    if (len <= 0) {
      return -1;
    }
    memcpy(scan->tmpbuf, base, len);
    base = scan->tmpbuf;
  } else if (((intptr_t)base) & 0xf) {
    memcpy(scan->tmpbuf, base, 16);
    base = scan->tmpbuf;
  }

  // Load 16 bytes from base
  uint8x16_t src = vld1q_u8((const uint8_t *)base);

  // Compare each byte with ch[i] (0xFF if equal, 0x00 otherwise)
  uint8x16_t cmp = vceqq_u8(src, scan->ch[0]);
  for (int i = 1; i < 4; i++) {
    if (scan->inuse[i]) {
      cmp = vorrq_u8(cmp, vceqq_u8(src, scan->ch[i]));
    }
  }

  // convert cmp to bitmap. cmp contains 0x00 or 0xFF.
  // Extract high and low halves
  const uint8x8_t lo = vget_low_u8(cmp);
  const uint8x8_t hi = vget_high_u8(cmp);

  // Multiply by bit pattern and horizontal add
  const uint8x8_t bitmask = {1, 2, 4, 8, 16, 32, 64, 128};
  const uint16_t mask_lo = vaddlv_u8(vand_u8(lo, bitmask));
  const uint16_t mask_hi = vaddlv_u8(vand_u8(hi, bitmask));

  scan->flag = mask_lo | (mask_hi << 8);
  return 0;
}

// Initialize a scan. The interesting chars are provided in the string
// 'accept', which must not be longer than 4 chars. This is similar to
// strpbrk().
static scan_t scan_init(const char *accept) {
  scan_t scan;
  memset(&scan, 0, sizeof(scan));
  for (int i = 0; i < 4; i++) {
    if (!accept[i])
      break;
    scan.ch[i] = vdupq_n_u8(accept[i]);
    scan.inuse[i] = 1;
  }
  return scan;
}

// Get ready to scan buf[].
static inline void scan_reset(scan_t *scan, const char *buf, int64_t buflen) {
  scan->orig = buf;
  scan->base = buf;
  scan->p = buf;
  scan->q = buf + buflen;
  __scan_calcflag(scan);
}

// Move to the next SIMD pipeline
static void __scan_forward(scan_t *scan) {
  while (!scan->flag) {
    scan->base += 16; // 16 at a time
    // use new base to find the new scan->flag
    if (__scan_calcflag(scan))
      break;
  }
}

// Return a pointer to the next interesting char, or NULL if not found.
static inline const char *scan_next(scan_t *scan) {
  if (!scan->flag) {
    __scan_forward(scan);
  }
  // __builtin_ffs: returns one plus the index of the least significant 1-bit of
  // x, or if x is zero, returns zero.
  int off = __builtin_ffs(scan->flag) - 1;
  if (off >= 0) {
    scan->flag &= ~(1 << off); // clear the bit at off
    scan->p = scan->base + off;
  } else {
    scan->p = scan->q;
  }
  return (scan->p < scan->q) ? scan->p++ : 0;
}

// Return TRUE if the current char matches ch.
static inline int scan_match(scan_t *scan, int ch) { return ch == *scan->p; }
