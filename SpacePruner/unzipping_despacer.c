//
//  unzipping_despacer.c
//  SpacePruner
//
//  Created by Derek Ledbetter on 2017-07-17.
//

#include "unzipping_despacer.h"

#ifdef __aarch64__
#include <arm_neon.h>

size_t neon_unzipping_despace(char *bytes, size_t howmany) {
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

    const uint8x16x2_t unzipped = vuzpq_u8(goodBitsZipped[0], goodBitsZipped[1]);
    const uint8x16_t goodBits = vorrq_u8(unzipped.val[0], unzipped.val[1]);

    uint8x16_t goodCount = vcntq_u8(goodBits);

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

    uint8x16_t level1[2];
    {
      const uint8x16_t source = goodBits;
      const uint8x16_t product = vreinterpretq_u8_p8(vmulq_p8(allOnesPoly, source));
      const uint8x16_t evens = vandq_u8(source, product);
      const uint8x16_t odds = vbicq_u8(source, product);
      level1[0] = vzip1q_u8(evens, odds);
      level1[1] = vzip2q_u8(evens, odds);
    }

    uint8x16_t level2[4];
    _Pragma("unroll") for (int i = 0; i != 2; ++i) {
      const uint8x16_t source = level1[i];
      const uint8x16_t product = vreinterpretq_u8_p8(vmulq_p8(allOnesPoly, source));
      const uint8x16_t evens = vandq_u8(source, product);
      const uint8x16_t odds = vbicq_u8(source, product);
      level2[2*i + 0] = vreinterpretq_u8_u16(vzip1q_u16(vreinterpretq_u16_u8(evens), vreinterpretq_u16_u8(odds)));
      level2[2*i + 1] = vreinterpretq_u8_u16(vzip2q_u16(vreinterpretq_u16_u8(evens), vreinterpretq_u16_u8(odds)));
    }

    uint8x16_t indices[8];
    _Pragma("unroll") for (int i = 0; i != 4; ++i) {
      // There's at most 2 set bits left, so just read their positions directly.
      const uint8x16_t source = level2[i];
      const uint8x16_t low = vcntq_u8(vbicq_u8(vsubq_u8(source, vdupq_n_u8(1)), source));
      const uint8x16_t high = veorq_u8(vdupq_n_u8(7), vclzq_u8(source));
      indices[2*i + 0] = vreinterpretq_u8_u32(vzip1q_u32(vreinterpretq_u32_u8(low), vreinterpretq_u32_u8(high)));
      indices[2*i + 1] = vreinterpretq_u8_u32(vzip2q_u32(vreinterpretq_u32_u8(low), vreinterpretq_u32_u8(high)));
    }

    _Pragma("unroll") for (int i = 0; i != 8; ++i) {
      const uint8x16_t originalCharacters = vld1q_u8(source + 16 * i);

      const uint8x16_t offset = vextq_u8(vdupq_n_u8(0), vdupq_n_u8(8), 8);
      const uint8x16_t pickedCharacters = vqtbl1q_u8(originalCharacters, vaddq_u8(indices[i], offset));
      const uint8x8_t pickedCharacters1 = vget_low_u8(pickedCharacters);
      const uint8x8_t pickedCharacters2 = vget_high_u8(pickedCharacters);

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

#endif // __aarch64__
