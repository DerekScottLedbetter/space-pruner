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
    
    uint8x16_t unzippedIndices[8];
    _Pragma("unroll") for (int i = 0; i != 8; ++i) {
      /*
       Suppose that goodBits[i] ends in a 1 followed by k 0's.
       Then subtracting one will change those to a 0 followed by k 1's, leaving the top bits the same.
       ANDing will clear the lowest set bit.
       ANDing minus one with the complemented original will give a number that ends in k 1's,
       and zero elsewhere.
       */
      uint8x16_t minusOne = vsubq_u8(goodBits, vdupq_n_u8(1));
      unzippedIndices[i] = vcntq_u8(vbicq_u8(minusOne, goodBits));
      goodBits = vandq_u8(goodBits, minusOne);
    }

    #define ZIP_BY_1(v0, v1) \
      vzipq_u8(v0, v1).val[0], \
      vzipq_u8(v0, v1).val[1],
    #define ZIP_BY_2(v0, v1) \
      vreinterpretq_u8_u16(vzipq_u16(vreinterpretq_u16_u8(v0), vreinterpretq_u16_u8(v1)).val[0]), \
      vreinterpretq_u8_u16(vzipq_u16(vreinterpretq_u16_u8(v0), vreinterpretq_u16_u8(v1)).val[1]),
    #define ZIP_BY_4(v0, v1) \
      vreinterpretq_u8_u32(vzipq_u32(vreinterpretq_u32_u8(v0), vreinterpretq_u32_u8(v1)).val[0]), \
      vreinterpretq_u8_u32(vzipq_u32(vreinterpretq_u32_u8(v0), vreinterpretq_u32_u8(v1)).val[1]),

    const uint8x16_t indices_01[2] = {
      ZIP_BY_1(unzippedIndices[0], unzippedIndices[1])
    };
    const uint8x16_t indices_23[2] = {
      ZIP_BY_1(unzippedIndices[2], unzippedIndices[3])
    };
    const uint8x16_t indices_45[2] = {
      ZIP_BY_1(unzippedIndices[4], unzippedIndices[5])
    };
    const uint8x16_t indices_67[2] = {
      ZIP_BY_1(unzippedIndices[6], unzippedIndices[7])
    };
    const uint8x16_t indices_0123[4] = {
      ZIP_BY_2(indices_01[0], indices_23[0])
      ZIP_BY_2(indices_01[1], indices_23[1])
    };
    const uint8x16_t indices_4567[4] = {
      ZIP_BY_2(indices_45[0], indices_67[0])
      ZIP_BY_2(indices_45[1], indices_67[1])
    };
    const uint8x16_t indices[8] = {
      ZIP_BY_4(indices_0123[0], indices_4567[0])
      ZIP_BY_4(indices_0123[1], indices_4567[1])
      ZIP_BY_4(indices_0123[2], indices_4567[2])
      ZIP_BY_4(indices_0123[3], indices_4567[3])
    };

    _Pragma("unroll") for (int i = 0; i != 8; ++i) {
      const uint8x16_t originalCharacters = vld1q_u8(source + 16 * i);

#if defined(__aarch64__)
      const uint8x16_t offset = vextq_u8(vdupq_n_u8(0), vdupq_n_u8(8), 8);
      const uint8x16_t pickedCharacters = vqtbl1q_u8(originalCharacters, vaddq_u8(indices[i], offset));
      const uint8x8_t pickedCharacters1 = vget_low_u8(pickedCharacters);
      const uint8x8_t pickedCharacters2 = vget_high_u8(pickedCharacters);
#else
      const uint8x8_t pickedCharacters1 = vtbl1_u8(vget_low_u8(originalCharacters), vget_low_u8(indices[i]));
      const uint8x8_t pickedCharacters2 = vtbl1_u8(vget_high_u8(originalCharacters), vget_high_u8(indices[i]));
#endif

      vst1_u8(dest, pickedCharacters1);
      dest += vgetq_lane_u8(goodCount, 0);
      vst1_u8(dest, pickedCharacters2);
      dest += vgetq_lane_u8(goodCount, 1);
      goodCount = vextq_u8(goodCount, goodCount, 2);
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
