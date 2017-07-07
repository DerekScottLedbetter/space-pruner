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
    fflush(NULL);                                                              \
    uint64_t min_diff = (uint64_t)-1;                                          \
    bool wrong_answer = false;                                                 \
    for (int i = 0; i < repeat; i++) {                                         \
      memcpy(tmpbuffer, buffer, N);                                            \
                                                                               \
      __asm volatile("" ::: /* pretend to clobber */ "memory");                \
      const uint64_t cycles_start = time_in_ns();                              \
      size_t result_length = test(tmpbuffer, N);                               \
      const uint64_t cycles_final = time_in_ns();                              \
                                                                               \
      if (false && i == 0 && result_length <= N) {                             \
        tmpbuffer[result_length] = 0;                                          \
        printf("\"%s\"\n", tmpbuffer);                                         \
      }                                                                        \
      if (result_length != N - howmanywhite                                    \
            || memcmp(tmpbuffer, correctbuffer, result_length) != 0)           \
        wrong_answer = true;                                                   \
                                                                               \
      const uint64_t cycles_diff = (cycles_final - cycles_start);              \
      if (cycles_diff < min_diff)                                              \
        min_diff = cycles_diff;                                                \
    }                                                                          \
    printf("%-40s: ", #test);                                                  \
    float cycle_per_op = (float)min_diff / (float)N;                           \
    printf(" %.2f ns per operation", cycle_per_op);                            \
    if (wrong_answer)                                                          \
      printf(" [ERROR]");                                                      \
    printf("\n");                                                              \
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
  printf("pointer alignment = %d bytes \n", 1 << __builtin_ctzll((uintptr_t)(const void *)(tmpbuffer)));

  size_t howmanywhite = fillwithtext(buffer, N);

  int j = 0;
  for (int i = 0; i < N; ++i) {
    char c = buffer[i];
    if (c > 32) {
      correctbuffer[j++] = c;
    }
  }
  assert(j == N - howmanywhite);

  printf("\n");
  BEST_TIME(despace);
#if __ARM_NEON
  BEST_TIME(neon_despace);
  BEST_TIME(neon_interleaved_despace);
#endif // __ARM_NEON

  free(correctbuffer);
  free(origbuffer);
  free(origtmpbuffer);
}

