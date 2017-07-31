/* -*- C -*-
 *
 * Copyright 2011 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S.  Government
 * retains certain rights in this software.
 *
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * This software is available to you under the BSD license.
 *
 * This file is part of the Sandia OpenSHMEM software package. For license
 * information, see the LICENSE file in the top level directory of the
 * distribution.
 *
 */

/*
 * Wrappers to interface with PMI runtime
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if defined(PMI_PORTALS4)
#include <portals4/pmi.h>
#else
#include <pmix.h>
#endif

#include "runtime.h"
#include "shmem_internal.h"
#include "uthash.h"

static pmix_proc_t myproc;
static size_t size;

int
shmem_runtime_init(void)
{
    printf("HELLO FROM PMIX\n");

    pmix_status_t rc;
    pmix_proc_t proc;
    proc.rank = PMIX_RANK_WILDCARD;
    pmix_value_t *val;

    int initialized = PMIx_Initialized();

    if (!initialized) {
        if (PMIX_SUCCESS != (rc = PMIx_Init(&myproc, NULL, 0))) {

            fprintf(stderr, "PMIx_Init failed\n");
            return rc;
        }
        // else {
        //     pmix_output_verbose(stderr, pmix_globals.debug_output,
        //     "PMIx_Init executed successfully");
        // }
    }

    (void)strncpy(proc.nspace, myproc.nspace, PMIX_MAX_NSLEN);
    proc.rank = PMIX_RANK_WILDCARD;

    if (PMIX_SUCCESS == (rc = PMIx_Get(&proc, PMIX_JOB_SIZE, NULL, 0, &val))) {
        size = val->data.uint32;
        PMIX_VALUE_RELEASE(val);
    }
    else {
        fprintf(stderr, "Size is not properly initiated\n");

        return rc;
    }

    return PMIX_SUCCESS;
}


int
shmem_runtime_fini(void)
{
    pmix_status_t rc;

    if (PMIX_SUCCESS != (rc = PMIx_Finalize(NULL, 0))) {
        fprintf(stderr, "PMIx_Finalize failed\n");

        return rc;
    }
    // else{
    //     pmix_output_verbose(stderr, pmix_globals.debug_output,
    //         "PMIx_Finalize executed successfully");
    // }

    return PMIX_SUCCESS;
}


void
shmem_runtime_abort(int exit_code, const char msg[])
{

#ifdef HAVE___BUILTIN_TRAP
    if (shmem_internal_trap_on_abort)
        __builtin_trap();
#endif

    pmix_status_t rc;

    if (PMIX_SUCCESS != (rc = PMIx_Abort(exit_code, msg, NULL, 0))) {
        fprintf(stderr, "PMIx_Abort failed");
    }
    // else{
    //     pmix_output_verbose(stderr, pmix_globals.debug_output,
    //         "PMIx_Abort executed successfully");
    // }

    /* PMI_Abort should not return */
    abort();

}



int
shmem_runtime_get_rank(void)
{
    return myproc.rank;
}


int
shmem_runtime_get_size(void)
{
    return size;
}


int
shmem_runtime_exchange(void)
{
    pmix_status_t rc;
    pmix_info_t info;
    bool wantit=true;

    /* commit any values we "put" */
    if (PMIX_SUCCESS != (rc = PMIx_Commit())) {
        fprintf(stderr, "PMIx_Commit failed\n");
        return rc;
    }

    /* execute a fence, directing that all info be exchanged */
    PMIX_INFO_CONSTRUCT(&info);
    PMIX_INFO_LOAD(&info, PMIX_COLLECT_DATA, &wantit, PMIX_BOOL);
    if (PMIX_SUCCESS != (rc = PMIx_Fence(NULL, 0, &info, 1))) {
        fprintf(stderr, "PMIx_Fence failed");
    }
    PMIX_INFO_DESTRUCT(&info);

    return rc;
}


int
shmem_runtime_put(char *key, void *value, size_t valuelen)
{
    pmix_value_t val;
    pmix_status_t rc;

    PMIX_VALUE_CONSTRUCT(&val);
    val.type = PMIX_BYTE_OBJECT;
    val.data.bo.bytes = value;
    val.data.bo.size = valuelen;

    rc = PMIx_Put(PMIX_GLOBAL, key, &val);
    val.data.bo.bytes = NULL;  // protect the data
    val.data.bo.size = 0;
    PMIX_VALUE_DESTRUCT(&val);

    return rc;
}

/* I'm assuming you malloc'd a region and are giving me its length */
int
shmem_runtime_get(int pe, char *key, void *value, size_t valuelen)
{
    pmix_proc_t proc;
    pmix_value_t *val;
    pmix_status_t rc;

    /* ensure the region is zero'd out */
    memset(value, 0, valuelen);

    /* setup the ID of the proc whose info we are getting */
    (void)strncpy(proc.nspace, myproc.nspace, PMIX_MAX_NSLEN);
    proc.rank = pe;

    rc = PMIx_Get(&proc, key, NULL, 0, &val);

    if (PMIX_SUCCESS == rc) {
        if (NULL != val) {
            /* see if the data fits into the given region */
            if (valuelen < val->data.bo.size) {
                PMIX_VALUE_RELEASE(val);
                return PMIX_ERROR;
            }
            /* copy the results across */
            memcpy(value, val->data.bo.bytes, val->data.bo.size);
            PMIX_VALUE_RELEASE(val);
        }
    }

    return rc;
}


void
shmem_runtime_barrier(void)
{
    PMIx_Fence(NULL, 0, NULL, 0);
}
