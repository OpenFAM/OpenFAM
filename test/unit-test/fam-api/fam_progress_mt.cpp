/* fam_progress_mt.cpp
 * Copyright (c) 2022 Hewlett Packard Enterprise Development, LP. All rights
 * reserved. Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * See https://spdx.org/licenses/BSD-3-Clause
 *
 */
/* Test Case Description: Tests fam_context_open, fam_context_close and
 * fam_progress operations for multithreaded model.
 */

#include <fam/fam.h>
#include <fam/fam_exception.h>

#include <iostream>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;
fam *my_fam;
Fam_Options fam_opts;

Fam_Region_Descriptor *testRegionDesc;
const char *testRegionStr;
int rc;

#define NUM_THREADS 32
#define TOTAL_NUM_THREADS 33
#define REGION_SIZE (2000 * 1024 * NUM_THREADS)
#define NUM_CONTEXTS 8
#define REGION_PERM 0777
#define NUM_IO_ITERATIONS 50
fam_context *ctx[NUM_CONTEXTS];
int activethreads = NUM_THREADS;
pthread_barrier_t barrier;
typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t deltaValue;
} ValueInfo;

void *thr_check_fam_progress(void *arg) {
    uint64_t progress = 0;
    pthread_barrier_wait(&barrier);
    while (activethreads != 0) {
        for (int i = 0; i < NUM_CONTEXTS; i++) {
            try {
                progress = ctx[i]->fam_progress();
                cout << "I/Os in Progress for context" << i << " is "
                     << progress << endl;
            } catch (Fam_Exception &e) {
                cout << "Exception caught while invoking fam_progress()"
                     << endl;
                cout << "Error msg: " << e.fam_error_msg() << endl;
                cout << "Error: " << e.fam_error() << endl;
            }
        }
    }

    pthread_exit(NULL);
}

void *thrd_fam_context_mt_with_fam_progress(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int i = 0;
    i = addInfo->tid % NUM_CONTEXTS;

    pthread_barrier_wait(&barrier);
    try {
        for (int j = 0; j < NUM_IO_ITERATIONS; j++) {
            int *local1 = (int *)malloc(1000000);
            int *local2 = (int *)malloc(1000000);
            ctx[i]->fam_get_nonblocking(local1, item, 500, (i + 1) * 100000);
            ctx[i]->fam_get_nonblocking(local2, item, 10, (i + 1) * 100000);
        }
        ctx[i]->fam_quiet();

    } catch (Fam_Exception &e) {
        printf("fam API failed: %d: %s\n", e.fam_error(), e.fam_error_msg());
    }
    --activethreads;

    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    int ret = 0;
    my_fam = new fam();
    init_fam_options(&fam_opts);
    fam_opts.famThreadModel = strdup("FAM_THREAD_MULTIPLE");

    Fam_Descriptor *item;
    pthread_t thr[TOTAL_NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * TOTAL_NUM_THREADS);
    const char *dataItem = "first";

    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        printf("FAM Initialization failed: %s\n", e.fam_error_msg());
        my_fam->fam_abort(-1); // abort the program
        // note that fam_abort currently returns after signaling
        // so we must terminate with the same value
        return -1;
    }
    testRegionStr = get_uniq_str("test", my_fam);
    try {
        testRegionDesc = my_fam->fam_create_region(testRegionStr, REGION_SIZE,
                                                   REGION_PERM, NULL);

        if (testRegionDesc == NULL) {
            cout << "fam create region failed" << endl;
            exit(1);
        }
    } catch (Fam_Exception &e) {
        printf("Create region failed: %d: %s\n", e.fam_error(),
               e.fam_error_msg());
        return -1;
    }

    // Allocating data items in the created region
    try {
        item =
            my_fam->fam_allocate(dataItem, 1024 * 2000, 0777, testRegionDesc);
    } catch (Fam_Exception &e) {
        printf("Create region/Allocate Data item failed: %d: %s\n",
               e.fam_error(), e.fam_error_msg());
        return -1;
    }
    if (item != NULL)
        printf("Successully completed fam_allocate\n");
    int local[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    my_fam->fam_put_nonblocking(local, item, 6 * sizeof(int), 10 * sizeof(int));
    try {
        for (i = 0; i < NUM_CONTEXTS; i++) {
            ctx[i] = my_fam->fam_context_open();
        }
    } catch (Fam_Exception &e) {
        printf("Create region failed: %d: %s\n", e.fam_error(),
               e.fam_error_msg());
        return -1;
    }
    if (pthread_barrier_init(&barrier, NULL, TOTAL_NUM_THREADS) != 0) {

        cout << "Error in barrier initialization" << endl;
        return -1;
    }
    // Create a thread for checking fam_progress - The 0 th thread
    if ((rc = pthread_create(&thr[0], NULL, thr_check_fam_progress, NULL))) {
        fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        return -1;
    }

    for (i = 1; i < TOTAL_NUM_THREADS; i++) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL,
                                 thrd_fam_context_mt_with_fam_progress,
                                 &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    pthread_barrier_destroy(&barrier);

    for (int j = 0; j < 33; j++) {
        pthread_join(thr[j], NULL);
    }
    try {

        for (i = 0; i < NUM_CONTEXTS; i++) {
            my_fam->fam_context_close(ctx[i]);
        }
    } catch (Fam_Exception &e) {
        printf("Create region failed: %d: %s\n", e.fam_error(),
               e.fam_error_msg());
        return -1;
    }

    try {
        my_fam->fam_deallocate(item);
    } catch (Fam_Exception &e) {
        printf("fam API failed: %d: %s\n", e.fam_error(), e.fam_error_msg());
        ret = -1;
    }

    try {
        my_fam->fam_destroy_region(testRegionDesc);
    } catch (Fam_Exception &e) {
        printf("Destroy region failed: %d: %s\n", e.fam_error(),
               e.fam_error_msg());
        ret = -1;
    }
    delete testRegionDesc;
    free((void *)testRegionStr);
    try {
        my_fam->fam_finalize("default");
    } catch (Fam_Exception &e) {
        printf("FAM Finalization failed: %s\n", e.fam_error_msg());
        my_fam->fam_abort(-1); // abort the program
        // note that fam_abort currently returns after signaling
        // so we must terminate with the same value
        return -1;
    }
    return ret;
}
