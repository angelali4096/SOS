/*
 *  This test program is derived from an example program in the
 *  OpenSHMEM specification.
 */

#include <shmemx.h>
#include <shmem.h>
#include <stdio.h>
#include <stdlib.h>

#define NELEMS 32

int main(void) {
  shmem_init();
  int mype = shmem_my_pe();
  int npes = shmem_n_pes();

  int *values = shmem_malloc(NELEMS * sizeof(int));

  unsigned char *value_is_maximal     = shmem_malloc(NELEMS * sizeof(unsigned char));
  unsigned char *value_is_maximal_all = shmem_malloc(NELEMS * sizeof(unsigned char));

  static int maximal_values_count = 0;
  static int maximal_values_total;

  srand((unsigned)mype);

  for (int i = 0; i < NELEMS; i++) {
    values[i] = rand() % npes;

    /* Track and count instances of maximal values (i.e., values equal to (npes-1)) */
    value_is_maximal[i] = (values[i] == (npes - 1)) ? 1 : 0;
    maximal_values_count += value_is_maximal[i];
  }

  /* Wait for all PEs to initialize reductions arrays */
  shmemx_sync(SHMEMX_TEAM_WORLD);

  shmemx_or_reduce(SHMEMX_TEAM_WORLD, value_is_maximal_all, value_is_maximal, NELEMS);
  shmemx_sum_reduce(SHMEMX_TEAM_WORLD, &maximal_values_total, &maximal_values_count, 1);

  if (mype == 0) {
    printf("Found %d maximal random numbers across all PEs.\n", maximal_values_total);
    printf("A maximal number occured (at least once) at the following indices:\n");
    for (int i = 0; i < NELEMS; i++) {
      if (value_is_maximal_all[i] == 1) {
        printf("%d ", i);
      }
    }
    printf("\n");
  }

  shmem_finalize();
  return 0;
}
