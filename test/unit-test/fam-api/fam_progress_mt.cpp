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

#include <atomic>
#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <iostream>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

//#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;
fam *my_fam;
Fam_Options fam_opts;

Fam_Region_Descriptor *testRegionDesc;
const char *testRegionStr;
int rc;

#define NUM_THREADS 16
#define TOTAL_NUM_THREADS 17
#define REGION_SIZE (2000 * 1024 * NUM_THREADS)
#define NUM_CONTEXTS 8
#define REGION_PERM 0777
#define NUM_IO_ITERATIONS 50
fam_context *ctx[NUM_CONTEXTS];
std::atomic<int> activethreads{NUM_THREADS};
pthread_barrier_t barrier;
typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t deltaValue;
} ValueInfo;

void *thr_check_fam_progress(void *arg) {
    // uint64_t progress = 0; //Commenting out printing of progress in normal
    // execution of test
    /* Commenting out cpu affinity code for the time being.  It can be used
    later if needed. cpu_set_t cpuset;
    // the CPU we whant to use
    int cpu;
    cpu = 0;

    CPU_ZERO(&cpuset);     // clears the cpuset
    CPU_SET(cpu, &cpuset); // set CPU 2 on cpuset
    */
    /*
     * cpu affinity for the calling thread
     * first parameter is the pid, 0 = calling thread
     * second parameter is the size of your cpuset
     * third param is the cpuset in which your thread will be
     * placed. Each bit represents a CPU
     */
    // sched_setaffinity(0, sizeof(cpuset), &cpuset);

    pthread_barrier_wait(&barrier);
    while (activethreads != 0) {
        for (int i = 0; i < NUM_CONTEXTS; i++) {
            try {
                uint64_t progress = ctx[i]->fam_progress();
                //#TODO: Compare fam_progress value with expected values
                (void)progress;
                pthread_yield();
                /* if (progress != 0) {
                  cout << "I/Os in Progress for context" << i << " is "
                         << progress << endl;
                } */
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
    /* Commenting out cpu affinity code for the time being.  It can be used
    later if needed.

    cpu_set_t cpuset;
    int node0[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
                   14, 15, 16, 17, 18, 19, 40, 41, 42, 43, 44, 45, 46, 47,
                   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59};
    int node1[] = {20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
                   34, 35, 36, 37, 38, 39, 60, 61, 62, 63, 64, 65, 66, 67,
                   68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79};
    // the CPU we whant to use
    int cpu;
    if (addInfo->tid % 2 == 0)
        cpu = node0[addInfo->tid];
    else
        cpu = node1[addInfo->tid];

    CPU_ZERO(&cpuset);     // clears the cpuset
    CPU_SET(cpu, &cpuset); // set CPU 2 on cpuset */

    /*
     * cpu affinity for the calling thread
     * first parameter is the pid, 0 = calling thread
     * second parameter is the size of your cpuset
     * third param is the cpuset in which your thread will be
     * placed. Each bit represents a CPU
     */
    // sched_setaffinity(0, sizeof(cpuset), &cpuset);
    i = addInfo->tid % NUM_CONTEXTS;

    pthread_barrier_wait(&barrier);
    try {
        for (int j = 0; j < NUM_IO_ITERATIONS; j++) {
            int *local1 = (int *)malloc((i + 1) * 1000000);
            int *local2 = (int *)malloc((i + 1) * 1000000);
            ctx[i]->fam_get_nonblocking(local1, item, 500, (i + 1) * 100000);
            ctx[i]->fam_get_nonblocking(local2, item, 10, (i + 1) * 100000);
        }
        if (((addInfo->tid - 1) / NUM_CONTEXTS) == 0) {
            ctx[i]->fam_quiet();
        }

    } catch (Fam_Exception &e) {
        printf("fam API failed: %d: %s\n", e.fam_error(), e.fam_error_msg());
    }

    activethreads.fetch_add(-1);

    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    int ret = 0;
    my_fam = new fam();
    fam_opts.famThreadModel = strdup("FAM_THREAD_MULTIPLE");

    Fam_Options *fam_opts = (Fam_Options *)malloc(sizeof(Fam_Options));
    memset((void *)fam_opts, 0, sizeof(Fam_Options));
    // assume that no specific options are needed by the implementation
    fam_opts->runtime = strdup("NONE");

    Fam_Descriptor *item;
    pthread_t thr[TOTAL_NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * TOTAL_NUM_THREADS);
    const char *dataItem = "first";

    try {
        my_fam->fam_initialize("default", fam_opts);
        printf("FAM initialized\n");
    } catch (Fam_Exception &e) {
        printf("FAM Initialization failed: %s\n", e.fam_error_msg());
        my_fam->fam_abort(-1); // abort the program
        // note that fam_abort currently returns after signaling
        // so we must terminate with the same value
        return -1;
    }
    testRegionStr = "testfp";
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
    if (item != NULL) {
        int local[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        my_fam->fam_put_nonblocking(local, item, 6 * sizeof(int),
                                    10 * sizeof(int));
    }
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

    for (int j = 0; j < TOTAL_NUM_THREADS; j++) {
        pthread_join(thr[j], NULL);
    }

    pthread_barrier_destroy(&barrier);

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
    try {
        my_fam->fam_finalize("default");
        printf("FAM finalized\n");
    } catch (Fam_Exception &e) {
        printf("FAM Finalization failed: %s\n", e.fam_error_msg());
        my_fam->fam_abort(-1); // abort the program
        // note that fam_abort currently returns after signaling
        // so we must terminate with the same value
        return -1;
    }
    return ret;
}
