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

    for (int i = 0; i != 8; ++i) {
      characters[i] = vld1_u8(source + 8 * i);
    }
    transpose_8x8(characters);

    uint8x8_t goodBits = vdup_n_u8(0);
    for (int i = 0; i != 8; ++i) {
      uint8x8_t good = vcgt_u8(characters[i], vdup_n_u8(space));
      goodBits = vbsl_u8(vdup_n_u8(1), good, vshl_n_u8(goodBits, 1));
    }
    const uint8x8_t goodCount = vcnt_u8(goodBits);

    uint8x8_t indices[8];
    for (int i = 0; i != 8; ++i) {
      uint8x8_t indicesOfSetBits = vclz_u8(goodBits);
      indices[i] = indicesOfSetBits;
      goodBits = vbic_u8(goodBits, vshl_u8(vdup_n_u8(0x80), vneg_s8(vreinterpret_s8_u8(indicesOfSetBits))));
    }
    transpose_8x8(indices);

    #define EXTRACT_AND_STORE(i) \
    { \
      uint8x8_t originalCharacters = vld1_u8(source + 8 * i); \
      uint8x8_t pickedCharacters = vtbl1_u8(originalCharacters, indices[i]); \
      vst1_u8(dest, pickedCharacters); \
      dest += vget_lane_u8(goodCount, i); \
    }
    EXTRACT_AND_STORE(0);
    EXTRACT_AND_STORE(1);
    EXTRACT_AND_STORE(2);
    EXTRACT_AND_STORE(3);
    EXTRACT_AND_STORE(4);
    EXTRACT_AND_STORE(5);
    EXTRACT_AND_STORE(6);
    EXTRACT_AND_STORE(7);
    #undef EXTRACT_AND_STORE

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
