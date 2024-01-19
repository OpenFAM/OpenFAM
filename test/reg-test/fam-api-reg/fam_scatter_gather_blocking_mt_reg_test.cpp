/*
 * fam_scatter_gather_index_blocking_mt_reg_test.cpp
 * Copyright (c) 2019,2023 Hewlett Packard Enterprise Development, LP. All
 * rights reserved. Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following conditions
 * are met:
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
/* Test Case Description: Tests blocking scatter/gather operations for
 * multithreaded model.
 */

#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>

#include "common/fam_test_config.h"
#define NUM_THREADS 20

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;
Fam_Region_Descriptor *testRegionDesc;

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t deltaValue;
} ValueInfo;

void *thr_func_index(void *arg) {
    ValueInfo *valInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = valInfo->item;

    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    uint64_t indexes[] = {0, 7, 3, 5, 8, 1, 2, 4, 6, 9};

    EXPECT_NO_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 10, indexes, sizeof(int)));

    int *local2 = (int *)malloc(10 * sizeof(int));

    EXPECT_NO_THROW(
        my_fam->fam_gather_blocking(local2, item, 10, indexes, sizeof(int)));

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(local2[i], newLocal[i]);
    }
    free((void *)local2);
    pthread_exit(NULL);
}
void *thr_func_stride(void *arg) {
    ValueInfo *valInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = valInfo->item;

    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                      26, 27, 28, 29, 30, 31, 32, 33, 34, 35};

    EXPECT_NO_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 10, 2, 3, sizeof(int)));

    int *local2 = (int *)malloc(10 * sizeof(int));

    EXPECT_NO_THROW(
        my_fam->fam_gather_blocking(local2, item, 10, 2, 3, sizeof(int)));

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(local2[i], newLocal[i]);
    }
    free((void *)local2);
    pthread_exit(NULL);
}

void *thr_func_index_expect_failure(void *arg) {
    ValueInfo *valInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = valInfo->item;

    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    uint64_t indexes[] = {0, 7, 3, 5, 8, 1, 2, 4, 6, 9};
    EXPECT_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 10, indexes, sizeof(int)),
        Fam_Exception);

    int *local2 = (int *)malloc(10 * sizeof(int));

    EXPECT_NO_THROW(
        my_fam->fam_gather_blocking(local2, item, 10, indexes, sizeof(int)));

    free((void *)local2);
    pthread_exit(NULL);
}
void *thr_func_stride_expect_failure(void *arg) {
    ValueInfo *valInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = valInfo->item;

    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                      26, 27, 28, 29, 30, 31, 32, 33, 34, 35};

    EXPECT_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 5, 2, 3, sizeof(int)),
        Fam_Exception);

    int *local2 = (int *)malloc(10 * sizeof(int));

    EXPECT_NO_THROW(
        my_fam->fam_gather_blocking(local2, item, 5, 2, 3, sizeof(int)));

    free((void *)local2);
    pthread_exit(NULL);
}

// Test case 1 - scatter gather index test.
TEST(FamScatterGatherIndexBlockMT, ScatterGatherIndexBlockSuccess) {

    Fam_Descriptor *item;
    int i, rc;
    pthread_t thr[NUM_THREADS];
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegion, (8192 * NUM_THREADS), 0777, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, (1024 * NUM_THREADS),
                                                0777, testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    ValueInfo info = {item, 0, 0, 0};

    for (i = 0; i < NUM_THREADS; ++i) {
        if ((rc = pthread_create(&thr[i], NULL, thr_func_index, &info))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));

    delete item;
    delete testRegionDesc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamScatterGatherStrideBlockMT, ScatterGatherStrideBlockSuccess) {
    Fam_Region_Descriptor *testRegionDesc;
    Fam_Descriptor *item;
    int i, rc;
    pthread_t thr[NUM_THREADS];
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegion, (8192 * NUM_THREADS), 0777, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, (1024 * NUM_THREADS),
                                                0777, testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    ValueInfo info = {item, 0, 0, 0};

    for (i = 0; i < NUM_THREADS; ++i) {
        if ((rc = pthread_create(&thr[i], NULL, thr_func_stride, &info))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));

    delete item;
    delete testRegionDesc;

    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case 3 - scatter gather index test.(Negative TestCase)
TEST(FamScatterGatherIndexBlockMT, ScatterGatherIndexBlockFailure) {
    Fam_Region_Descriptor *testRegionDesc;
    Fam_Descriptor *item;
    int i, rc;
    pthread_t thr[NUM_THREADS];
    const char *testRegion = get_uniq_str("test_item_perm", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    Fam_Region_Attributes *regionAttributes = new Fam_Region_Attributes();
    regionAttributes->permissionLevel = DATAITEM;

    EXPECT_NO_THROW(
        testRegionDesc = my_fam->fam_create_region(
            testRegion, (8192 * NUM_THREADS), 0777, regionAttributes));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, (1024 * NUM_THREADS),
                                                0444, testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    ValueInfo info = {item, 0, 0, 0};

    for (i = 0; i < NUM_THREADS; ++i) {
        if ((rc = pthread_create(&thr[i], NULL, thr_func_index_expect_failure,
                                 &info))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));

    delete item;
    delete testRegionDesc;

    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case 4 - scatter gather stride test.(Negative TestCase)
TEST(FamScatterGatherStrideBlockMT, ScatterGatherStrideBlockFailure) {
    Fam_Region_Descriptor *testRegionDesc;
    Fam_Descriptor *item;
    int i, rc;
    pthread_t thr[NUM_THREADS];
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    Fam_Region_Attributes *regionAttributes = new Fam_Region_Attributes();
    regionAttributes->permissionLevel = DATAITEM;

    EXPECT_NO_THROW(
        testRegionDesc = my_fam->fam_create_region(
            testRegion, (8192 * NUM_THREADS), 0777, regionAttributes));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, (1024 * NUM_THREADS),
                                                0444, testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    ValueInfo info = {item, 0, 0, 0};

    for (i = 0; i < NUM_THREADS; ++i) {
        if ((rc = pthread_create(&thr[i], NULL, thr_func_stride_expect_failure,
                                 &info))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));

    delete item;
    delete testRegionDesc;

    free((void *)testRegion);
    free((void *)firstItem);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);
    fam_opts.famThreadModel = strdup("FAM_THREAD_MULTIPLE");

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    delete my_fam;

    return ret;
}
