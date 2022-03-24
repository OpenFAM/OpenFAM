/* fam_context_mt_reg_test.cpp
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
/* Test Case Description: Tests fam_context_open and fam_context_close
 * operations for multithreaded model.
 */

#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
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
#define NUM_THREADS 7
#define REGION_SIZE (32 * 1024 * NUM_THREADS)
#define REGION_PERM 0777
#define NUM_ITERATIONS 100
#define NUM_IO_ITERATIONS 5

// to increase the number of contexts, we need to increase the number of open
// files
#define NUM_CONTEXTS 7

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t deltaValue;
} ValueInfo;

void *thrd_fam_context_mt(void *arg) {
    fam_context *ctx;
    EXPECT_NO_THROW(ctx = my_fam->fam_context_open());
    EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
    pthread_exit(NULL);
}

void *thrd_fam_context_mt_stress(void *arg) {
    fam_context *ctx;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(ctx = my_fam->fam_context_open());
        EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
    }
    pthread_exit(NULL);
}

void *thrd_fam_context_mt_with_io(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    fam_context *ctx;
    EXPECT_NO_THROW(ctx = my_fam->fam_context_open());
    EXPECT_NO_THROW(ctx->fam_or(item, 0, (uint32_t)1));
    EXPECT_NO_THROW(ctx->fam_or(item, 0, (uint32_t)2));
    EXPECT_NO_THROW(ctx->fam_or(item, 0, (uint32_t)3));
    EXPECT_NO_THROW(ctx->fam_or(item, 0, (uint32_t)4));
    EXPECT_NO_THROW(my_fam->fam_or(item, 0, (uint32_t)4));
    EXPECT_NO_THROW(ctx->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
    pthread_exit(NULL);
}

void *thrd_fam_context_mt_with_io_stress(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    fam_context *ctx;
    for (int i = 0; i < NUM_IO_ITERATIONS; i++) {
        EXPECT_NO_THROW(ctx = my_fam->fam_context_open());
        EXPECT_NO_THROW(ctx->fam_or(item, 0, (uint32_t)1));
        EXPECT_NO_THROW(ctx->fam_or(item, 0, (uint32_t)2));
        EXPECT_NO_THROW(ctx->fam_or(item, 0, (uint32_t)3));
        EXPECT_NO_THROW(ctx->fam_or(item, 0, (uint32_t)4));
        EXPECT_NO_THROW(my_fam->fam_or(item, 0, (uint32_t)4));
        EXPECT_NO_THROW(ctx->fam_quiet());
        EXPECT_NO_THROW(my_fam->fam_quiet());
        EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
    }

    pthread_exit(NULL);
}

void *thrd_fam_context_simul_mt(void *arg) {

    fam_context *ctx[NUM_CONTEXTS];
    for (int i = 0; i < NUM_CONTEXTS; i++) {
        EXPECT_NO_THROW(ctx[i] = my_fam->fam_context_open());
    }
    for (int i = 1; i < NUM_CONTEXTS; i++) {
        EXPECT_NO_THROW(my_fam->fam_context_close(ctx[i]));
    }

    pthread_exit(NULL);
}

// Test case 1 - FamContextOpenCloseMultithreaded

TEST(FamContextModel, FamContextOpenCloseMultithreaded) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int128_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;

        if ((rc = pthread_create(&thr[i], NULL, thrd_fam_context_mt,
                                 &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    free(info);

    free((void *)dataItem);
}
// Test case 2 - FamContextOpenCloseMultithreadedStressTest

TEST(FamContextModel, FamContextOpenCloseMultithreadedStressTest) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    const char *dataItem = get_uniq_str("first", my_fam);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int128_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;

        if ((rc = pthread_create(&thr[i], NULL, thrd_fam_context_mt_stress,
                                 &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    free(info);

    free((void *)dataItem);
}

// Test case 3 - FamContextOpenCloseMultithreadedWithIOOperation

TEST(FamContextModel, FamContextWithIOMt) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    const char *dataItem = get_uniq_str("first", my_fam);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int128_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;

        if ((rc = pthread_create(&thr[i], NULL, thrd_fam_context_mt_with_io,
                                 &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    free(info);

    free((void *)dataItem);
}
// Test case 4 - FamContextOpenCloseMultithreadedWithIOOperationStressTest

TEST(FamContextModel, FamContextWithIOMtStressTest) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    const char *dataItem = get_uniq_str("first", my_fam);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int128_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;

        if ((rc = pthread_create(&thr[i], NULL,
                                 thrd_fam_context_mt_with_io_stress,
                                 &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    free(info);

    free((void *)dataItem);
}

// Test case 5 - FamContextSimultaneousOpenCloseMtStressTest
TEST(FamContextModel, FamContextSimulMt) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    const char *dataItem = get_uniq_str("first", my_fam);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int128_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;

        if ((rc = pthread_create(&thr[i], NULL, thrd_fam_context_simul_mt,
                                 &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    free(info);

    free((void *)dataItem);
}

int main(int argc, char **argv) {
    int ret = 0;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();
    init_fam_options(&fam_opts);
    fam_opts.famThreadModel = strdup("FAM_THREAD_MULTIPLE");
    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));
    testRegionStr = get_uniq_str("test", my_fam);
    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, REGION_PERM, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));
    delete testRegionDesc;
    free((void *)testRegionStr);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
