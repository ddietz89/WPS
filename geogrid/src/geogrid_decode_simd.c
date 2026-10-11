/* File: geogrid_decode_simd.c

   Portable SIMD decode helper for read_geogrid wordsize 1 and 2.

   This translation unit is intended to be compiled separately from the
   original read_geogrid.c reader object.  The original reader retains its
   original O2 flags; this helper may be compiled by root with strict flags
   and/or -march=znver2.

   Exact semantics:
     - wordsize 1 unsigned: dst[i] = (float)(int)c[i]
     - wordsize 1 signed:   if ival > 128 then ival -= 256; then float convert
     - wordsize 2 unsigned: dst[i] = (float)(int)((hi << 8) | lo)
     - wordsize 2 signed:   if ival > 32768 then ival -= 65536; then float convert
     - if scale != 1.0f, multiply converted float by scale, preserving
       original float multiplication, no FMA, no reassociation, no fast-math.

   Portable C only.  No explicit AVX intrinsics, architecture attributes,
   GPU, threading, allocation, I/O, or OpenMP team creation.
*/

#include <stddef.h>
#include <stdint.h>

/*
   WPS_GEOGRID_READER_SIMD must be defined externally by the build system if
   OpenMP SIMD pragmas are desired, e.g. -fopenmp-simd.  This file does not
   assume _OPENMP and does not require an OpenMP runtime.  If the macro is
   absent, the helper remains ordinary portable C with identical outputs.
*/
#ifdef WPS_GEOGRID_READER_SIMD
#define WPS_GEOGRID_PRAGMA_SIMD _Pragma("omp simd")
#else
#define WPS_GEOGRID_PRAGMA_SIMD
#endif

static void wps_geogrid_decode_word1(
      const unsigned char * restrict src,
      float * restrict dst,
      size_t n,
      int isigned,
      int endian,
      float scale)
{
   size_t i;
   (void)endian;

   if (!isigned) {
      if (scale == 1.0f) {
         WPS_GEOGRID_PRAGMA_SIMD
         for (i = 0; i < n; i++) {
            int ival = (int)src[i];
            dst[i] = (float)ival;
         }
      }
      else {
         WPS_GEOGRID_PRAGMA_SIMD
         for (i = 0; i < n; i++) {
            int ival = (int)src[i];
            dst[i] = (float)ival * scale;
         }
      }
   }
   else {
      if (scale == 1.0f) {
         WPS_GEOGRID_PRAGMA_SIMD
         for (i = 0; i < n; i++) {
            int ival = (int)src[i];
            if (ival > 128) {
               ival -= 256;
            }
            dst[i] = (float)ival;
         }
      }
      else {
         WPS_GEOGRID_PRAGMA_SIMD
         for (i = 0; i < n; i++) {
            int ival = (int)src[i];
            if (ival > 128) {
               ival -= 256;
            }
            dst[i] = (float)ival * scale;
         }
      }
   }
}

static void wps_geogrid_decode_word2(
      const unsigned char * restrict src,
      float * restrict dst,
      size_t n,
      int isigned,
      int endian,
      float scale)
{
   size_t i;
   size_t A2;
   size_t B2;

   if (endian == 0) {
      A2 = 0;
      B2 = 1;
   }
   else {
      A2 = 1;
      B2 = 0;
   }

   if (!isigned) {
      if (scale == 1.0f) {
         WPS_GEOGRID_PRAGMA_SIMD
         for (i = 0; i < n; i++) {
            int ival = ((int)src[2 * i + A2] << 8) | (int)src[2 * i + B2];
            dst[i] = (float)ival;
         }
      }
      else {
         WPS_GEOGRID_PRAGMA_SIMD
         for (i = 0; i < n; i++) {
            int ival = ((int)src[2 * i + A2] << 8) | (int)src[2 * i + B2];
            dst[i] = (float)ival * scale;
         }
      }
   }
   else {
      if (scale == 1.0f) {
         WPS_GEOGRID_PRAGMA_SIMD
         for (i = 0; i < n; i++) {
            int ival = ((int)src[2 * i + A2] << 8) | (int)src[2 * i + B2];
            if (ival > 32768) {
               ival -= 65536;
            }
            dst[i] = (float)ival;
         }
      }
      else {
         WPS_GEOGRID_PRAGMA_SIMD
         for (i = 0; i < n; i++) {
            int ival = ((int)src[2 * i + A2] << 8) | (int)src[2 * i + B2];
            if (ival > 32768) {
               ival -= 65536;
            }
            dst[i] = (float)ival * scale;
         }
      }
   }
}

void wps_geogrid_decode12(
      const unsigned char * restrict src,
      float * restrict dst,
      size_t n,
      int wordsize,
      int signedness,
      int endian,
      float scale)
{
   if (n == 0) {
      return;
   }

   if (wordsize == 1) {
      wps_geogrid_decode_word1(src, dst, n, signedness, endian, scale);
   }
   else if (wordsize == 2) {
      wps_geogrid_decode_word2(src, dst, n, signedness, endian, scale);
   }
}
