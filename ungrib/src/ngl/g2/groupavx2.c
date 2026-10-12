#include <stdint.h>
#include <string.h>

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#include <immintrin.h>

#define START(W,B,L) (((B)+(L)*(W))/8)
#define LANE(W,B,L) START(W,B,L)+3,START(W,B,L)+2,START(W,B,L)+1,START(W,B,L)
#define ROW(W,B) {LANE(W,B,0),LANE(W,B,1),LANE(W,B,2),LANE(W,B,3),LANE(W,B,4),LANE(W,B,5),LANE(W,B,6),LANE(W,B,7)}
#define WIDTHROWS(W) {ROW(W,0),ROW(W,1),ROW(W,2),ROW(W,3),ROW(W,4),ROW(W,5),ROW(W,6),ROW(W,7)}

static const uint8_t gpa_shuf[8][8][32] = {
  WIDTHROWS(2), WIDTHROWS(3), WIDTHROWS(4), WIDTHROWS(5),
  WIDTHROWS(6), WIDTHROWS(7), WIDTHROWS(8), WIDTHROWS(9)
};

#define SFT(W,B,L) (32-(W)-(((B)+(L)*(W))&7))
#define SROW(W,B) {SFT(W,B,0),SFT(W,B,1),SFT(W,B,2),SFT(W,B,3),SFT(W,B,4),SFT(W,B,5),SFT(W,B,6),SFT(W,B,7)}
#define SWIDTH(W) {SROW(W,0),SROW(W,1),SROW(W,2),SROW(W,3),SROW(W,4),SROW(W,5),SROW(W,6),SROW(W,7)}

static const int32_t gpa_shift[8][8][8] = {
  SWIDTH(2), SWIDTH(3), SWIDTH(4), SWIDTH(5),
  SWIDTH(6), SWIDTH(7), SWIDTH(8), SWIDTH(9)
};

__attribute__((target("avx2")))
static int gpa_unpack_group_avx2(const unsigned char *p, int64_t plen,
                                 int64_t boff, int width, int count,
                                 int32_t *out)
{
  int wi = width - 2;
  int intra = (int)(boff & 7);
  int done = 0;
  int64_t curbyte = boff >> 3;
  __m256i shuf = _mm256_loadu_si256((const __m256i *)gpa_shuf[wi][intra]);
  __m256i shiftv = _mm256_loadu_si256((const __m256i *)gpa_shift[wi][intra]);
  __m256i vmask = _mm256_set1_epi32((int)((1u << (unsigned)width) - 1u));
  uint32_t mask32 = (width == 32) ? 0xffffffffu : ((1u << (unsigned)width) - 1u);

  while (done + 8 <= count) {
    if (plen - curbyte >= 16) {
      __m128i d128 = _mm_loadu_si128((const __m128i *)(const void *)(p + curbyte));
      __m256i data = _mm256_broadcastsi128_si256(d128);
      __m256i perm = _mm256_shuffle_epi8(data, shuf);
      __m256i val = _mm256_srlv_epi32(perm, shiftv);
      val = _mm256_and_si256(val, vmask);
      _mm256_storeu_si256((__m256i *)(void *)(out + done), val);
      curbyte += width;
      done += 8;
    } else {
      unsigned char tmp[32];
      int64_t rem = plen - curbyte;
      if (rem > 15) rem = 15;
      memset(tmp, 0, sizeof(tmp));
      memcpy(tmp, p + curbyte, (size_t)rem);
      __m256i data = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i *)(const void *)tmp));
      __m256i perm = _mm256_shuffle_epi8(data, shuf);
      __m256i val = _mm256_srlv_epi32(perm, shiftv);
      val = _mm256_and_si256(val, vmask);
      _mm256_storeu_si256((__m256i *)(void *)(out + done), val);
      curbyte += width;
      done += 8;
    }
  }

  while (done < count) {
    int64_t bitpos = boff + (int64_t)done * (int64_t)width;
    int64_t by = bitpos >> 3;
    int sh = (int)(bitpos & 7);
    int nbytes = (sh + width + 7) >> 3;
    uint32_t acc = 0;
    for (int i = 0; i < nbytes; ++i) {
      acc = (acc << 8) | (uint32_t)p[by + i];
    }
    int totalbits = nbytes * 8;
    uint32_t val = (acc >> (totalbits - sh - width)) & mask32;
    out[done] = (int32_t)val;
    done += 1;
  }

  return 1;
}
#endif

int gpa_unpack_group(const unsigned char *p, int64_t plen, int64_t boff,
                     int width, int count, int32_t *out)
{
#if !defined(__GNUC__) || (!defined(__x86_64__) && !defined(__i386__))
  (void)p; (void)plen; (void)boff; (void)width; (void)count; (void)out;
  return 0;
#else
  if (p == 0 || out == 0) return 0;
  if (width < 2 || width > 9) return 0;
  if (count < 8) return 0;
  if (plen < 0 || boff < 0) return 0;
  if (!__builtin_cpu_supports("avx2")) return 0;

  int64_t cap = (plen >= 268435456) ? (int64_t)0x7fffffff : plen * 8;
  if (boff > cap) return 0;
  if (count > (cap - boff) / width) return 0;

  return gpa_unpack_group_avx2(p, plen, boff, width, count, out);
#endif
}
