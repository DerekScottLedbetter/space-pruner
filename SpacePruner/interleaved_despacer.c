//
//  interleaved_despacer.c
//  SpacePruner
//
//  Created by Derek Ledbetter on 2017-07-07.
//  Copyright © 2017 Derek Ledbetter. All rights reserved.
//

#include "interleaved_despacer.h"

#include <ConditionalMacros.h>
#include <stddef.h>
#include <stdint.h>

#if __ARM_NEON
#include <arm_neon.h>

static inline void transpose_in_place_8(uint8x8_t* restrict v0, uint8x8_t* restrict v1) {
  uint8x8x2_t result = vtrn_u8(*v0, *v1);
  *v0 = result.val[0];
  *v1 = result.val[1];
}

static inline void transpose_in_place_16(uint8x8_t* restrict v0, uint8x8_t* restrict v1) {
  uint16x4x2_t result = vtrn_u16(vreinterpret_u16_u8(*v0), vreinterpret_u16_u8(*v1));
  *v0 = vreinterpret_u8_u16(result.val[0]);
  *v1 = vreinterpret_u8_u16(result.val[1]);
}

static inline void transpose_in_place_32(uint8x8_t* restrict v0, uint8x8_t* restrict v1) {
  uint32x2x2_t result = vtrn_u32(vreinterpret_u32_u8(*v0), vreinterpret_u32_u8(*v1));
  *v0 = vreinterpret_u8_u32(result.val[0]);
  *v1 = vreinterpret_u8_u32(result.val[1]);
}

static inline void transpose_8x8(uint8x8_t vectors[8]) {
  transpose_in_place_8(&vectors[0], &vectors[1]);
  transpose_in_place_8(&vectors[2], &vectors[3]);
  transpose_in_place_8(&vectors[4], &vectors[5]);
  transpose_in_place_8(&vectors[6], &vectors[7]);
  transpose_in_place_16(&vectors[0], &vectors[2]);
  transpose_in_place_16(&vectors[1], &vectors[3]);
  transpose_in_place_16(&vectors[4], &vectors[6]);
  transpose_in_place_16(&vectors[5], &vectors[7]);
  transpose_in_place_32(&vectors[0], &vectors[4]);
  transpose_in_place_32(&vectors[1], &vectors[5]);
  transpose_in_place_32(&vectors[2], &vectors[6]);
  transpose_in_place_32(&vectors[3], &vectors[7]);
}

size_t neon_interleaved_despace(char *bytes, size_t howmany) {
  const size_t blockSize = 8 * 8;
  const uint8_t space = 32;

  uint8_t* dest = (uint8_t*)bytes;
  const uint8_t* source = (uint8_t*)bytes;
  const uint8_t* sourceEnd = source + howmany;

  while (sourceEnd - source >= blockSize) {
    uint8x8_t characters[8];

    /*
     vld4_u8 0–3, 4–7:
     [ 00, 04, 08, 0C, 10, 14, 18, 1C ]
     [ 01, 05, 09, 0D, 11, 15, 19, 1D ]
     [ 02, 06, 0A, 0E, 12, 16, 1A, 1E ]
     [ 03, 07, 0B, 0F, 13, 17, 1B, 1F ]
     [ 20, 24, 28, 2C, 30, 34, 38, 3C ]
     [ 21, 25, 29, 2D, 31, 35, 39, 3D ]
     [ 22, 26, 2A, 2E, 32, 36, 3A, 3E ]
     [ 23, 27, 2B, 2F, 33, 37, 3B, 3F ]

     vuzp_u8 0–4, 1–5, 2–6, 3–7:
     0 [ 00, 08, 10, 18, 20, 28, 30, 38 ]
     1 [ 01, 09, 11, 19, 21, 29, 31, 39 ]
     2 [ 02, 0A, 12, 1A, 22, 2A, 32, 3A ]
     3 [ 03, 0B, 13, 1B, 23, 2B, 33, 3B ]
     4 [ 04, 0C, 14, 1C, 24, 2C, 34, 3C ]
     5 [ 05, 0D, 15, 1D, 25, 2D, 35, 3D ]
     6 [ 06, 0E, 16, 1E, 26, 2E, 36, 3E ]
     7 [ 07, 0F, 17, 1F, 27, 2F, 37, 3F ]
     */

    uint8x8x4_t characters0 = vld4_u8(source);
    uint8x8x4_t characters1 = vld4_u8(source + 8 * 4);
    for (int i = 0; i != 4; ++i) {
      uint8x8x2_t unzipped = vuzp_u8(characters0.val[i], characters1.val[i]);
      characters[i + 0] = unzipped.val[0];
      characters[i + 4] = unzipped.val[1];
    }

    uint8x8_t goodBits = vdup_n_u8(0);
    for (int i = 0; i != 8; ++i) {
      uint8x8_t good = vcgt_u8(characters[i], vdup_n_u8(space));
      goodBits = vbsl_u8(vdup_n_u8(0x80U >> i), good, goodBits);
    }
    uint8x8_t goodCount = vcnt_u8(goodBits);

    uint8x8_t indices[8];
    for (int i = 0; i != 8; ++i) {
      uint8x8_t indicesOfSetBits = vclz_u8(goodBits);
      indices[i] = indicesOfSetBits;
      goodBits = vbic_u8(goodBits, vshl_u8(vdup_n_u8(0x80), vneg_s8(vreinterpret_s8_u8(indicesOfSetBits))));
    }
    transpose_8x8(indices);

    for (int i = 0; i != 8; ++i) {
      uint8x8_t originalCharacters = vld1_u8(source + 8 * i);
      uint8x8_t pickedCharacters = vtbl1_u8(originalCharacters, indices[i]);
      vst1_u8(dest, pickedCharacters);
      dest += vget_lane_u8(goodCount, 0);
      goodCount = vext_u8(goodCount, goodCount, 1);
    }

    source += blockSize;
  }
  while (source < sourceEnd) {
    const char c = *source++;
    if (c > space) {
      *dest++ = c;
    }
  }
  return (char*)dest - bytes;
}

#endif
