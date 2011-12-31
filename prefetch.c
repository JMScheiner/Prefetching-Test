/**
 * @file prefetch.c
 *
 * @brief Experiment with prefetching in a case where the typical prefetching
 * behavior might be inadequate - i.e. the programmer knows a lot more about
 * the access pattern then the cache.  Here the access pattern is fibonacci
 * numbers - mod the size of the working set.
 *
 * Might want to pretty this up for more tests.
 *
 * @author Justin Scheiner
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// 1G (Significantly greater than cache size).
#define LEN 0x8000000

// Arbitrary high number.
#define ITERS 50000000

// Experimentally works well.
#define NAHEAD 32

typedef void(*op_t)(long *, uint64_t);

/**
 * Looks n fibonacci numbers ahead, and calls op_highest with the value,
 * then op with the current fibonacci number.
 */
void fibo_lookahead_foreach(long *data, op_t op_highest, op_t op_lowest) {
  uint64_t *buf = calloc(NAHEAD, sizeof(uint64_t));
  int i_buf = 0, i;

  // Fill the first n elements of the fibo buffer.
  // To avoid expensive %'s buf[i] < n for all i.
  uint64_t index, follow, temp;
  for (i_buf = 0, index = 1, follow = 0; i_buf < NAHEAD; i_buf++) {
    buf[i_buf] = index;
    temp = index;
    index += follow;
    follow = temp;
    while (index >= LEN) {
      index -= LEN;
    }
  }

  for (i = 0, i_buf = 0; i < ITERS; i++) {
    // i_buf always points at the lowest number in the buffer.
    op_lowest(data, buf[i_buf]);

    int prev = dec_ring(i_buf);
    buf[i_buf] = buf[prev] + buf[dec_ring(prev)];
    while (buf[i_buf] >= LEN) {
      buf[i_buf] -= LEN;
    }
    op_highest(data, buf[i_buf]);
    i_buf = inc_ring(i_buf);
  }
  free(buf);
}

int inc_ring(int i) {
  int next = i + 1;
  return next >= NAHEAD ? 0 : next;
}

int dec_ring(int i) {
  int next = i + NAHEAD - 1;
  return next >= NAHEAD ? next - NAHEAD : next;
}

void prefetch(long *data, uint64_t index) {
  __builtin_prefetch(&data[index]);
}

void increment(long *data, uint64_t index) {
  data[index]++;
}

void nop(long *data, uint64_t index) {
  return;
}

int main(int argc, const char *argv[]) {
  // Use totally separate chunks so the cache is cold at the start of both.
  long *data0 = calloc(LEN, sizeof(long));
  long *data1 = calloc(LEN, sizeof(long));
  clock_t start, end;
  double avg;

  start = clock();
  fibo_lookahead_foreach(data0, nop, increment);
  end = clock();

  printf("CPU time w/o prefetching: %f\n",
      ((double)(end - start)) / CLOCKS_PER_SEC);

  // Try with prefetching.
  start = clock();
  fibo_lookahead_foreach(data1, prefetch, increment);
  end = clock();

  printf("CPU time prefetching %d ahead: %f\n",
      NAHEAD, ((double)(end - start)) / CLOCKS_PER_SEC);

  // Results:
  // CPU time w/o prefetching: 6.919791
  // CPU time prefetching 32 ahead: 3.671016

  free(data0);
  free(data1);
  return EXIT_SUCCESS;
}
