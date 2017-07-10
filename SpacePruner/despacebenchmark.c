// gcc -std=c99 -O3 -o despacebenchmark despacebenchmark.c
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "despacer.h"
#include "interleaved_despacer.h"

static inline uint64_t time_in_ns() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000000000 + (uint32_t)tv.tv_usec * 1000;
}

#define BEST_TIME(test)                                                        \
  do {                                                                         \
    uint64_t min_diff = (uint64_t)-1;                                          \
    for (int i = 0; i < repeat; i++) {                                         \
      fillwithtext(buffer, N);                                                 \
                                                                               \
      __asm volatile("" ::: /* pretend to clobber */ "memory");                \
      const uint64_t cycles_start = time_in_ns();                              \
      test(buffer, N);                                                         \
      const uint64_t cycles_final = time_in_ns();                              \
                                                                               \
      const uint64_t cycles_diff = (cycles_final - cycles_start);              \
      if (cycles_diff < min_diff)                                              \
        min_diff = cycles_diff;                                                \
    }                                                                          \
    printf("%-40s: ", #test);                                                  \
    float cycle_per_op = (float)min_diff / (float)N;                           \
    printf(" %.2f ns per operation\n", cycle_per_op);                          \
    fflush(NULL);                                                              \
  } while (0)


// let us estimate that we have a 1% proba of hitting a white space
size_t fillwithtext(char *buffer, size_t size) {
  size_t howmany = 0;
  for (size_t i = 0; i < size; ++i) {
    double r = ((double)rand() / (RAND_MAX));
    if (r < 0.01) {
      buffer[i] = ' ';
      howmany++;
    } else if (r < 0.02) {
      buffer[i] = '\n';
      howmany++;
    } else if (r < 0.03) {
      buffer[i] = '\r';
      howmany++;
    } else {
      do {
        buffer[i] = (char)rand();
      } while (buffer[i] <= 32);
    }
  }
  return howmany;
}

typedef size_t (*despace_function_ptr)(char *bytes, size_t howmany);

#define FUNCTION_AND_NAME(func) { &func, #func }

struct FunctionAndName {
  despace_function_ptr ptr;
  const char* name;
};

const struct FunctionAndName functionsToTest[] = {
  FUNCTION_AND_NAME(despace),
#if __ARM_NEON
  FUNCTION_AND_NAME(neon_despace),
  FUNCTION_AND_NAME(neon_interleaved_despace),
#endif
};
const size_t functionsToTestCount = sizeof(functionsToTest) / sizeof(functionsToTest[0]);

void despace_benchmark(void) {
  const int N = 1024 * 32;
  const int repeat = 100;
  const int alignoffset = 0;

  // Add one in case we want to null-terminate.
  char *origbuffer = malloc(N + alignoffset + 1);
  char *origtmpbuffer = malloc(N + alignoffset + 1);
  char *buffer = origbuffer + alignoffset;
  char *tmpbuffer = origtmpbuffer + alignoffset;
  char *correctbuffer = malloc(N + 1);
  printf("pointer alignment = %d bytes \n", 1 << __builtin_ctzll((uintptr_t)(const void *)(buffer)));

  static const size_t testSizes[] = { 0, 1, 2, 3, 4, 7, 8, 9, 13, 16, 17, 61, 64, 67,
      100, 123, 1000, 10000, N };
  const size_t testSizesCount = sizeof(testSizes) / sizeof(testSizes[0]);
  bool failedTests[functionsToTestCount] = {};

  for (size_t i = 0; i != testSizesCount; ++i) {
    const size_t sourceCount = testSizes[i];
    assert(sourceCount <= N);

    const size_t howmanywhite = fillwithtext(buffer, sourceCount);
    const size_t correctResultSize = sourceCount - howmanywhite;

    int j = 0;
    for (int i = 0; i < sourceCount; ++i) {
      char c = buffer[i];
      if (c > 32) {
        correctbuffer[j++] = c;
      }
    }
    assert(j == correctResultSize);

    for (size_t t = 0; t != functionsToTestCount; ++t) {
      if (failedTests[t]) {
        continue;
      }

      memcpy(tmpbuffer, buffer, sourceCount);
      size_t resultSize = (*functionsToTest[t].ptr)(tmpbuffer, sourceCount);

      if (resultSize != correctResultSize
          || memcmp(tmpbuffer, correctbuffer, resultSize) != 0) {
        failedTests[t] = true;
      }
    }
  }

  for (size_t t = 0; t != functionsToTestCount; ++t) {
    printf("%-40s: %s\n", functionsToTest[t].name, failedTests[t] ? "FAILURE" : "OK");
  }
  fflush(NULL);

  fillwithtext(buffer, N);

  printf("\n");
  BEST_TIME(despace);
#if __ARM_NEON
  BEST_TIME(neon_despace);
  BEST_TIME(neon_interleaved_despace);
#endif // __ARM_NEON
  printf("\n");

  free(correctbuffer);
  free(origbuffer);
  free(origtmpbuffer);
}

