/*
 * fam_compare_swap_atomics_reg_test.cpp
 * Copyright (c) 2019, 2023 Hewlett Packard Enterprise Development, LP. All rights
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
/* Test Case Description: Tests fam_compare_swap and certain atomic operations
 * for multithreaded model.
 * (test case has been commented as there are some issues with negative test
 * scenarios with non-blocking calls.)
 */

#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;

Fam_Region_Descriptor *testRegionDesc;
const char *testRegionStr;
int rc;
#define NUM_THREADS 10
#define REGION_SIZE (16 * 1024 * 1024 * NUM_THREADS)
#define REGION_PERM 0777

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t deltaValue;
} ValueInfo;

pthread_barrier_t barrier;

void *thrd_cas_int32(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    int32_t valueInt32 = 0xAAAAAAAA;
    uint64_t offset = tid * sizeof(int32_t);
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueInt32));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    int32_t oldValueInt32 = 0xAAAAAAAA;
    int32_t newValueInt32 = 0xBBBBBBBB;
    EXPECT_NO_THROW(
        my_fam->fam_compare_swap(item, offset, oldValueInt32, newValueInt32));
    EXPECT_NO_THROW(valueInt32 = my_fam->fam_fetch_int32(item, offset));
    EXPECT_EQ(valueInt32, (int32_t)0xBBBBBBBB);

    pthread_exit(NULL);
}

// Test case 1 - Compare & Swap int32 test.
TEST(FamCompareSwapInt32, CompareSwapInt32Success) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    const char *firstItem = get_uniq_str("first", my_fam);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(int32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_cas_int32, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    delete item;
    free(info);
}

void *thrd_cas_int64(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(int64_t);
    // Compare atomicas operation for int64
    int64_t valueInt64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueInt64));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    int64_t oldValueInt64 = 0xBBBBBBBBBBBBBBBB;
    int64_t newValueInt64 = 0xCCCCCCCCCCCCCCCC;
    EXPECT_NO_THROW(
        my_fam->fam_compare_swap(item, offset, oldValueInt64, newValueInt64));
    EXPECT_NO_THROW(valueInt64 = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(valueInt64, (int64_t)0xCCCCCCCCCCCCCCCC);

    pthread_exit(NULL);
}
// Test case 2 - Compare & Swap int64 test.
TEST(FamCompareSwapInt64, CompareSwapInt64Success) {

    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    const char *firstItem = get_uniq_str("first", my_fam);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(int64_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_cas_int64, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    delete item;
    free(info);
}
void *thrd_cas_uint32(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint32_t valueUint32 = 0xBBBBBBBB;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(uint32_t);
    // Compare atomic operations for uint32
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint32));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    uint32_t oldValueUint32 = 0xBBBBBBBB;
    uint32_t newValueUint32 = 0xCCCCCCCC;
    EXPECT_NO_THROW(
        my_fam->fam_compare_swap(item, offset, oldValueUint32, newValueUint32));
    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(valueUint32, (uint32_t)0xCCCCCCCC);

    pthread_exit(NULL);
}
TEST(FamCompareSwapUint32, CompareSwapUint32Success) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    const char *firstItem = get_uniq_str("first", my_fam);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(uint32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_cas_uint32, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    delete item;
    free(info);
}
void *thrd_cas_uint64(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(uint64_t);
    // Compare atomic operation for uint64
    uint64_t valueUint64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint64));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    uint64_t oldValueUint64 = 0xBBBBBBBBBBBBBBBB;
    uint64_t newValueUint64 = 0xCCCCCCCCCCCCCCCC;
    EXPECT_NO_THROW(
        my_fam->fam_compare_swap(item, offset, oldValueUint64, newValueUint64));
    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, offset));
    EXPECT_EQ(valueUint64, (int64_t)0xCCCCCCCCCCCCCCCC);

    pthread_exit(NULL);
}
// Test case 4 - Compare & Swap int64 test.

TEST(FamCompareSwapUint64, CompareSwapUint64Success) {

    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    const char *firstItem = get_uniq_str("first", my_fam);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(int64_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_cas_uint64, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    delete item;
    free(info);
}

void *thrd_cas_int128(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(int64_t) * 2;
    //    offset = 0;
    // Compare atomics operation for int128
    union int128store {
        struct {
            uint64_t low;
            uint64_t high;
        };
        int64_t i64[2];
        int128_t i128;
    };
    int128store oldValueInt128;
    int128store newValueInt128;
    int128store valueInt128;

    oldValueInt128.i128 = 0;
    newValueInt128.i128 = 0;
    valueInt128.i64[0] = 0xAAAAAAAABBBBBBBB;
    valueInt128.i64[1] = 0xCCCCCCCCDDDDDDDD;

    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueInt128.i64[0]));
    EXPECT_NO_THROW(
        my_fam->fam_set(item, offset + sizeof(int64_t), valueInt128.i64[1]));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    valueInt128.i64[0] = 0;
    valueInt128.i64[1] = 0;

    oldValueInt128.i64[0] = 0xAAAAAAAABBBBBBBB;
    oldValueInt128.i64[1] = 0xCCCCCCCCDDDDDDDD;

    newValueInt128.i64[0] = 0x2222222233333333;
    newValueInt128.i64[1] = 0x4444444455555555;

    EXPECT_NO_THROW(my_fam->fam_compare_swap(item, offset, oldValueInt128.i128,
                                             newValueInt128.i128));
    EXPECT_NO_THROW(valueInt128.i64[0] = my_fam->fam_fetch_int64(item, offset));
    EXPECT_NO_THROW(valueInt128.i64[1] = my_fam->fam_fetch_int64(
                        item, offset + sizeof(int64_t)));
    EXPECT_EQ(newValueInt128.i64[0], valueInt128.i64[0]);
    EXPECT_EQ(newValueInt128.i64[1], valueInt128.i64[1]);

    pthread_exit(NULL);
}

#ifdef ENABLE_KNOWN_ISSUES
TEST(FamCompareSwapInt128, CompareSwapInt128Success) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    const char *firstItem = get_uniq_str("first", my_fam);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(int128_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_cas_int128, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    delete item;
    free(info);
}
#endif

void *thrd_cas_uint64_neg(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(uint64_t);
    // Failure test case
    uint64_t valueUint64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint64));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    uint64_t oldValueUint64 = 0xAAAAAAAAAAAAAAAA;
    uint64_t newValueUint64 = 0xCCCCCCCCCCCCCCCC;
    EXPECT_NO_THROW(
        my_fam->fam_compare_swap(item, offset, oldValueUint64, newValueUint64));
    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, offset));
    EXPECT_EQ(valueUint64, (uint64_t)0xBBBBBBBBBBBBBBBB);
    pthread_exit(NULL);
}

TEST(FamCompareSwapNegativeCase, CompareSwapNegativeCaseSuccess) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    const char *firstItem = get_uniq_str("first", my_fam);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(uint64_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_cas_uint64_neg,
                                 &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    delete item;
    free(info);
}

void *thrd_cas_int128_neg(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(int64_t) * 2;
    // Compare atomics operation for int128
    union int128store {
        struct {
            uint64_t low;
            uint64_t high;
        };
        int64_t i64[2];
        int128_t i128;
    };
    int128store oldValueInt128;
    int128store newValueInt128;
    int128store valueInt128;

    oldValueInt128.i128 = 0;
    newValueInt128.i128 = 0;
    valueInt128.i64[0] = 0xAAAAAAAABBBBBBBB;
    valueInt128.i64[1] = 0xAAAAAAAABBBBBBBB;

    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueInt128.i64[0]));
    EXPECT_NO_THROW(
        my_fam->fam_set(item, offset + sizeof(int64_t), valueInt128.i64[1]));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    valueInt128.i64[0] = 0;
    valueInt128.i64[1] = 0;

    oldValueInt128.i64[0] = 0xAAAAAAAABBBBBBBB;
    oldValueInt128.i64[1] = 0xCCCCCCCCDDDDDDDD;

    newValueInt128.i64[0] = 0x2222222233333333;
    newValueInt128.i64[1] = 0x4444444455555555;

    EXPECT_NO_THROW(my_fam->fam_compare_swap(item, offset, oldValueInt128.i128,
                                             newValueInt128.i128));
    EXPECT_NO_THROW(valueInt128.i64[0] = my_fam->fam_fetch_int64(item, offset));
    EXPECT_NO_THROW(valueInt128.i64[1] = my_fam->fam_fetch_int64(
                        item, offset + sizeof(int64_t)));
    EXPECT_NE(newValueInt128.i64[0], valueInt128.i64[0]);
    EXPECT_NE(newValueInt128.i64[1], valueInt128.i64[1]);

    pthread_exit(NULL);
}
#if ENABLE_KNOWN_ISSUES
TEST(FamCompareSwapNegativeCaseInt128, CompareSwapNegativeCaseInt128Success) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    const char *firstItem = get_uniq_str("first", my_fam);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(int128_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_cas_int128_neg,
                                 &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    delete item;
    free(info);
}
#endif
int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);
    fam_opts.famThreadModel = strdup("FAM_THREAD_MULTIPLE");

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    testRegionStr = get_uniq_str("test", my_fam);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, REGION_PERM, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);
    ret = RUN_ALL_TESTS();
    pthread_barrier_destroy(&barrier);
    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
