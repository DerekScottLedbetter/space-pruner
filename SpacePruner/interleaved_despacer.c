//
//  interleaved_despacer.c
//  SpacePruner
//
//  Created by Derek Ledbetter on 2017-07-07.
//

#include "interleaved_despacer.h"

#include <ConditionalMacros.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if __ARM_NEON
#include <arm_neon.h>

static inline void transpose_in_place_q_8(uint8x16_t* restrict v0, uint8x16_t* restrict v1) {
  uint8x16x2_t result = vtrnq_u8(*v0, *v1);
  *v0 = result.val[0];
  *v1 = result.val[1];
}

static inline void transpose_in_place_q_16(uint8x16_t* restrict v0, uint8x16_t* restrict v1) {
  uint16x8x2_t result = vtrnq_u16(vreinterpretq_u16_u8(*v0), vreinterpretq_u16_u8(*v1));
  *v0 = vreinterpretq_u8_u16(result.val[0]);
  *v1 = vreinterpretq_u8_u16(result.val[1]);
}

static inline void transpose_in_place_q_32(uint8x16_t* restrict v0, uint8x16_t* restrict v1) {
  uint32x4x2_t result = vtrnq_u32(vreinterpretq_u32_u8(*v0), vreinterpretq_u32_u8(*v1));
  *v0 = vreinterpretq_u8_u32(result.val[0]);
  *v1 = vreinterpretq_u8_u32(result.val[1]);
}

static inline void transpose_q_8x8(uint8x16_t vectors[8]) {
  transpose_in_place_q_8(&vectors[0], &vectors[1]);
  transpose_in_place_q_8(&vectors[2], &vectors[3]);
  transpose_in_place_q_8(&vectors[4], &vectors[5]);
  transpose_in_place_q_8(&vectors[6], &vectors[7]);
  transpose_in_place_q_16(&vectors[0], &vectors[2]);
  transpose_in_place_q_16(&vectors[1], &vectors[3]);
  transpose_in_place_q_16(&vectors[4], &vectors[6]);
  transpose_in_place_q_16(&vectors[5], &vectors[7]);
  transpose_in_place_q_32(&vectors[0], &vectors[4]);
  transpose_in_place_q_32(&vectors[1], &vectors[5]);
  transpose_in_place_q_32(&vectors[2], &vectors[6]);
  transpose_in_place_q_32(&vectors[3], &vectors[7]);
}

#define PRINT_8x8(var) ((void)printf("%s = %02X %02X %02X %02X  %02X %02X %02X %02X\n", #var, var[0], var[1], var[2], var[3], var[4], var[5], var[6], var[7]))

size_t neon_interleaved_despace(char *bytes, size_t howmany) {
  const size_t blockSize = 8 * 16;
  const uint8_t space = 32;

  uint8_t* dest = (uint8_t*)bytes;
  const uint8_t* source = (uint8_t*)bytes;
  const uint8_t* sourceEnd = source + howmany;

  while (sourceEnd - source >= blockSize) {
    /*
     Represent indices in octal.

     vld4q_u8 gives 4 uint8x16_t
     [ 00 04 10 14 … ]
     [ 01 05 11 15 … ]
     [ 02 06 12 16 … ]
     [ 03 07 13 17 … ]

     Test > 32, then AND with these masks, repeated eight times:
     [ b0000_0001 b0001_0000 … ]
     [ b0000_0010 b0010_0000 … ]
     [ b0000_0100 b0100_0000 … ]
     [ b0000_1000 b1000_0000 … ]

     Then OR these together to get a single uint8x16_t:
     [ 0000 03–00, 07–04 0000, 0000 13–10, 17–14 0000, … ]

     Unzip and OR together:
     [ 00–07, 10–17, 20–27, 30–37, 40–47, 50–57, 60–67, 70–77 ]
     */
    
    uint8x16_t goodBitsZipped[2];
    for (int i = 0; i != 2; ++i) {
      const uint8x16_t mask0 = vreinterpretq_u8_u16(vdupq_n_u16(0x1001));
      const uint8x16x4_t masks = {
        mask0,                 // 0x01  0x10
        vshlq_n_u8(mask0, 1),  // 0x02  0x20
        vshlq_n_u8(mask0, 2),  // 0x04  0x40
        vshlq_n_u8(mask0, 3),  // 0x08  0x80
      };

      uint8x16x4_t characters = vld4q_u8(source + 8 * 8 * i);

      uint8x16_t goodBits_i = vdupq_n_u8(0);
      for (int j = 0; j != 4; ++j) {
        uint8x16_t good = vcgtq_u8(characters.val[j], vdupq_n_u8(space));
        goodBits_i = vorrq_u8(goodBits_i, vandq_u8(masks.val[j], good));
      }

      goodBitsZipped[i] = goodBits_i;
    }

    uint8x16x2_t unzipped = vuzpq_u8(goodBitsZipped[0], goodBitsZipped[1]);
    uint8x16_t goodBits = vorrq_u8(unzipped.val[0], unzipped.val[1]);

    uint8x16_t goodCount = vcntq_u8(goodBits);

    uint8x16_t indices[8];
    for (int i = 0; i != 8; ++i) {
      /*
      Suppose that goodBits[i] ends in a 1 followed by k 0's.
      Then subtracting one will change those to a 1 followed by k 1's, leaving the top bits the same.
      ANDing will clear the lowest set bit.
      ANDing minus one with the complemented original will give a number that ends in k 1's, 
      and zero elsewhere.
      */
      uint8x16_t minusOne = vsubq_u8(goodBits, vdupq_n_u8(1));
      indices[i] = vcntq_u8(vbicq_u8(minusOne, goodBits));
      goodBits = vandq_u8(goodBits, minusOne);
    }
    transpose_q_8x8(indices);

    _Pragma("unroll") for (int i = 0; i != 2; ++i) {
      uint8x8_t goodCount_i = (i == 0) ? vget_low_u8(goodCount) : vget_high_u8(goodCount);
      for (int j = 0; j != 8; ++j) {
        uint8x8_t originalCharacters = vld1_u8(source + 8 * (8 * i + j));
        uint8x8_t indices_ij = (i == 0) ? vget_low_u8(indices[j]) : vget_high_u8(indices[j]);
        uint8x8_t pickedCharacters = vtbl1_u8(originalCharacters, indices_ij);
        vst1_u8(dest, pickedCharacters);
        dest += vget_lane_u8(goodCount_i, 0);
        goodCount_i = vext_u8(goodCount_i, goodCount_i, 1);
      }
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
