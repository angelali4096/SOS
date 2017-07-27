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

static int rank = -1;
static int size = 0;
static char *kvs_name, *kvs_key, *kvs_value;
static int max_name_len, max_key_len, max_val_len;

#define SINGLETON_KEY_LEN 128
#define SINGLETON_VAL_LEN 256

typedef struct {
    char key[SINGLETON_KEY_LEN];
    char val[SINGLETON_VAL_LEN];
    UT_hash_handle hh;
} singleton_kvs_t;

singleton_kvs_t *singleton_kvs = NULL;

static int
encode(const void *inval, int invallen, char *outval, int outvallen)
{
    static unsigned char encodings[] = {
        '0','1','2','3','4','5','6','7', \
        '8','9','a','b','c','d','e','f' };
    int i;

    if (invallen * 2 + 1 > outvallen) {
        return 1;
    }

    for (i = 0; i < invallen; i++) {
        outval[2 * i] = encodings[((unsigned char *)inval)[i] & 0xf];
        outval[2 * i + 1] = encodings[((unsigned char *)inval)[i] >> 4];
    }

    outval[invallen * 2] = '\0';

    return 0;
}


static int
decode(const char *inval, void *outval, int outvallen)
{
    int i;
    char *ret = (char*) outval;

    if (outvallen != strlen(inval) / 2) {
        return 1;
    }

    for (i = 0 ; i < outvallen ; ++i) {
        if (*inval >= '0' && *inval <= '9') {
            ret[i] = *inval - '0';
        } else {
            ret[i] = *inval - 'a' + 10;
        }
        inval++;
        if (*inval >= '0' && *inval <= '9') {
            ret[i] |= ((*inval - '0') << 4);
        } else {
            ret[i] |= ((*inval - 'a' + 10) << 4);
        }
        inval++;
    }

    return 0;
}


int
shmem_runtime_init(void)
{
    printf("HELLO FROM PMIX\n");

    pmix_status_t rc;
    pmix_proc_t proc = myproc;
    proc.rank = PMIX_RANK_WILDCARD;
    pmix_value_t *val;
    pmix_info_t info[1];

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

    max_key_len = PMIX_MAX_KEYLEN;
    max_name_len = PMIX_MAX_NSLEN;
    max_val_len = PMIX_MAX_NSLEN;

    kvs_name = (char*) malloc(max_name_len);
    kvs_key = (char*) malloc(max_key_len);
    kvs_value = (char*) malloc(max_val_len);
    
    if (NULL == kvs_name) {
        fprintf(stderr, "kvs_name was not successfully initialized\n");
        return 4;
    }
    if (NULL == kvs_key) {
        fprintf(stderr, "kvs_key was not successfully initialized\n");
        return 6;
    }
    if (NULL == kvs_value) {
        fprintf(stderr, "kvs_value was not successfully initialized\n");
        return 8;
    }

    rank = myproc.rank;

    if (PMIX_SUCCESS == PMIx_Get(&proc, PMIX_UNIV_SIZE, NULL, 0, &val)) {
        size = val->data.size;
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
    return rank;
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
    if (PMIX_SUCCESS != (rc = PMIx_Fence(NULL, 0, NULL, 0))) {
        fprintf(stderr, "PMIx_Fence failed");

        return rc;
    }
    // else{
    //     pmix_output_verbose(stderr, pmix_globals.debug_output,
    //         "PMIx_Fence executed successfully");
    // }

    return PMIX_SUCCESS;
}


int
shmem_runtime_put(char *key, void *value, size_t valuelen)
{
    snprintf(kvs_key, max_key_len, "shmem-%lu-%s", (long unsigned) rank, key);
    if (0 != encode(value, valuelen, kvs_value, max_val_len)) {
        return 1;
    }

    return PMIX_SUCCESS;
}

int
shmem_runtime_get(int pe, char *key, void *value, size_t valuelen)
{
    snprintf(kvs_key, max_key_len, "shmem-%lu-%s", (long unsigned) pe, key);

    pmix_status_t rc;
    pmix_value_t **val;
    if (PMIX_SUCCESS != (rc = PMIx_Get(&myproc, key, NULL, 0, &val))) {
        fprintf(stderr, "PMIx_Get failed");

        return rc;
    }
    // else{
    //     pmix_output_verbose(stderr, pmix_globals.debug_output,
    //         "PMIx_Get executed successfully");
    // }

    if (0 != decode(kvs_value, value, valuelen)) {
        return 2;
    }

    return 0;
}


void
shmem_runtime_barrier(void)
{
    PMIx_Fence(NULL, 0, NULL, 0);
}
