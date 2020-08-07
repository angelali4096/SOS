/*
 *  This test program is derived from an example program in the
 *  OpenSHMEM specification.
 */

#include <shmem.h>
#include <shmemx.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    int                  rank, npes;
    int                  t_pe;
    int                  t_global;
    shmemx_team_t        new_team;
    shmemx_team_config_t *config;

    shmem_init();
    config = NULL;
    rank   = shmem_my_pe();
    npes   = shmem_n_pes();

    if (npes < 2) {
        fprintf(stderr, "ERR - Requires > 1 PEs\n");
        shmem_finalize();
        return 0;
    }

    shmemx_team_split_strided(SHMEMX_TEAM_WORLD, 0, 2, npes / 2, config, 0,
                             &new_team);

    t_pe     = shmemx_team_my_pe(new_team);
    t_global = shmemx_team_translate_pe(new_team, t_pe, SHMEMX_TEAM_WORLD);

    if (new_team != SHMEMX_TEAM_INVALID) {
        if (t_global != rank) {
            shmem_global_exit(2);
        }
    } else {
        if (t_global != -1) {
            shmem_global_exit(3);
        }
    }

    shmem_finalize();
    return 0;
}
