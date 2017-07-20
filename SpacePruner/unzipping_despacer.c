//
//  unzipping_despacer.c
//  SpacePruner
//
//  Created by Derek Ledbetter on 2017-07-17.
//

#include "unzipping_despacer.h"

#ifdef __ARM_NEON
#include <arm_neon.h>

size_t neon_unzipping_despace(char *bytes, size_t howmany) {
  const size_t blockCount = 2;
  const size_t blockSize = 8 * 16;
  const uint8_t space = 32;

  uint8_t* dest = (uint8_t*)bytes;
  const uint8_t* source = (uint8_t*)bytes;
  const uint8_t* sourceEnd = source + howmany;

  while (sourceEnd - source >= blockCount * blockSize) {
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
    const uint8x16_t mask0 = vreinterpretq_u8_u16(vdupq_n_u16(0x1001));
    const uint8x16x4_t masks = {
      mask0,                 // 0x01  0x10
      vshlq_n_u8(mask0, 1),  // 0x02  0x20
      vshlq_n_u8(mask0, 2),  // 0x04  0x40
      vshlq_n_u8(mask0, 3),  // 0x08  0x80
    };

    uint8x16_t goodBits[blockCount];
    uint8x16_t goodCount[blockCount];
    for (int j = 0; j != blockCount; ++j) {
      uint8x16_t goodBitsZipped[2];
      for (int i = 0; i != 2; ++i) {
        uint8x16x4_t characters = vld4q_u8(source + 8 * 8 * (2 * j + i));

        uint8x16_t goodBits_ij = vdupq_n_u8(0);
        for (int j = 0; j != 4; ++j) {
          uint8x16_t good = vcgtq_u8(characters.val[j], vdupq_n_u8(space));
          goodBits_ij = vorrq_u8(goodBits_ij, vandq_u8(masks.val[j], good));
        }

        goodBitsZipped[i] = goodBits_ij;
      }

      const uint8x16x2_t unzipped = vuzpq_u8(goodBitsZipped[0], goodBitsZipped[1]);
      goodBits[j] = vorrq_u8(unzipped.val[0], unzipped.val[1]);
      goodCount[j] = vcntq_u8(goodBits[j]);
    }

    /*
     If we do polynomial multiplication of a sequence of bits by a sequence of all ones,
     then each bit in the result is the XOR of the corresponding bit in the original and all of the
     bits to the right. We can then pick out alternating set bits by ANDing the original with
     both the product and the complement of the product.

     Examples:
                 00011110          1111111

             00100110            00011110            11111111
           × 11111111          × 11111111          × 11111111
           __________          __________          __________
             00100110            00011110            11111111
            00100110            00011110            11111111
           00100110            00011110            11111111
          00100110            00011110            11111111
         00100110            00011110            11111111
        00100110            00011110            11111111
       00100110            00011110            11111111
      00100110            00011110            11111111
     ________________    ________________    ________________
     0001110111100010    0000101000001010    0101010101010101

     Truncate:
             11100010            00001010            01010101
     AND with original:
             00100010            00001010            01010101
     AND complement with original:
             00000100            00010100            10101010
     
     We can then do twice more to get eight bytes with the set bits in consecutive order,
     followed by zeroes once the set bits are exhausted.
     */
    const poly8x16_t allOnesPoly = vdupq_n_u8(~0);

    uint8x16_t level1[2 * blockCount];
    _Pragma("unroll") for (int i = 0; i != blockCount; ++i) {
      const uint8x16_t source = goodBits[i];
      const uint8x16_t product = vreinterpretq_u8_p8(vmulq_p8(allOnesPoly, source));
      const uint8x16_t evens = vandq_u8(source, product);
      const uint8x16_t odds = vbicq_u8(source, product);
      level1[2 * i + 0] = vzipq_u8(evens, odds).val[0];
      level1[2 * i + 1] = vzipq_u8(evens, odds).val[1];
    }

    uint8x16_t level2[4 * blockCount];
    _Pragma("unroll") for (int i = 0; i != 2 * blockCount; ++i) {
      const uint8x16_t source = level1[i];
      const uint8x16_t product = vreinterpretq_u8_p8(vmulq_p8(allOnesPoly, source));
      const uint8x16_t evens = vandq_u8(source, product);
      const uint8x16_t odds = vbicq_u8(source, product);
      level2[2*i + 0] = vreinterpretq_u8_u16(vzipq_u16(vreinterpretq_u16_u8(evens), vreinterpretq_u16_u8(odds)).val[0]);
      level2[2*i + 1] = vreinterpretq_u8_u16(vzipq_u16(vreinterpretq_u16_u8(evens), vreinterpretq_u16_u8(odds)).val[1]);
    }

    uint8x16_t indices[8 * blockCount];
    _Pragma("unroll") for (int i = 0; i != 4 * blockCount; ++i) {
      // There's at most 2 set bits left, so just read their positions directly.
      const uint8x16_t source = level2[i];
      const uint8x16_t low = vcntq_u8(vbicq_u8(vsubq_u8(source, vdupq_n_u8(1)), source));
      const uint8x16_t high = veorq_u8(vdupq_n_u8(7), vclzq_u8(source));
      indices[2*i + 0] = vreinterpretq_u8_u32(vzipq_u32(vreinterpretq_u32_u8(low), vreinterpretq_u32_u8(high)).val[0]);
      indices[2*i + 1] = vreinterpretq_u8_u32(vzipq_u32(vreinterpretq_u32_u8(low), vreinterpretq_u32_u8(high)).val[1]);
    }

    _Pragma("unroll") for (int j = 0; j != blockCount; ++j) {
      uint8x16_t goodCount_j = goodCount[j];

      _Pragma("unroll") for (int i = 0; i != 8; ++i) {
        const uint8x16_t originalCharacters = vld1q_u8(source + 16 * (8 * j + i));
        const uint8x8_t pickedCharacters1 = vtbl1_u8(vget_low_u8(originalCharacters), vget_low_u8(indices[8 * j + i]));
        const uint8x8_t pickedCharacters2 = vtbl1_u8(vget_high_u8(originalCharacters), vget_high_u8(indices[8 * j + i]));

        vst1_u8(dest, pickedCharacters1);
        dest += vgetq_lane_u8(goodCount_j, 0);
        vst1_u8(dest, pickedCharacters2);
        dest += vgetq_lane_u8(goodCount_j, 1);
        goodCount_j = vextq_u8(goodCount_j, goodCount_j, 2);
      }
    }

    source += blockCount * blockSize;
  }
  while (source < sourceEnd) {
    const char c = *source++;
    if (c > space) {
      *dest++ = c;
    }
  }
  return (char*)dest - bytes;
}

#endif // __ARM_NEON
