/*
 * fam_nonfetch_arithmatic_atomics_reg_test.cpp
 * Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All rights
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
/* Test Case Description: Tests non-fetching arithematic operations for
 * multithreaded model.
 * (test case has been commented as there are some issues with negative test
 * scenarios with non-blocking calls.)
 *
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
int rc;
#define NUM_THREADS 10
#define REGION_SIZE (1024 * 1024 * NUM_THREADS)
typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t msg_size;
} ValueInfo;

pthread_barrier_t barrier;

// Test case 1 - test for fam_add and fam_subtract APIs for int32_t.
void *thrd_add_subtract_int32(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(int32_t);

    int32_t valueInt32 = 0xAAAAAAAA;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueInt32));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    valueInt32 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, valueInt32));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueInt32 = my_fam->fam_fetch_int32(item, offset));
    EXPECT_EQ(valueInt32, (int32_t)0xAAAAAAAB);
    valueInt32 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, valueInt32));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueInt32 = my_fam->fam_fetch_int32(item, offset));
    EXPECT_EQ(valueInt32, (int32_t)0xAAAAAAAA);
    pthread_exit(NULL);
}

TEST(FamNonfetchArithAtomicInt32, NonfetchArithAtomicInt32Success) {
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegion, REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 1024 * sizeof(int32_t),
                                             0777, testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_add_subtract_int32,
                                 &info[i]))) {
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

    free(info);
    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case 2 - test for fam_add and fam_subtract APIs for int64_t.
void *thrd_add_subtract_int64(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(int64_t);
    // Atomic tests for int64
    int64_t valueInt64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueInt64));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    valueInt64 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, valueInt64));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueInt64 = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(valueInt64, (int64_t)0xBBBBBBBBBBBBBBBC);
    valueInt64 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, valueInt64));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueInt64 = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(valueInt64, (int64_t)0xBBBBBBBBBBBBBBBB);

    pthread_exit(NULL);
}
TEST(FamNonfetchArithAtomicInt64, NonfetchArithAtomicInt64Success) {
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegion, REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 1024 * sizeof(int64_t),
                                             0777, testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_add_subtract_int64,
                                 &info[i]))) {
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

    free(info);
    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case 3 - test for fam_add and fam_subtract APIs for uint32_t.
void *thrd_add_subtract_uint32(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(uint32_t);
    // Atomic tests for uint32
    uint32_t valueUint32 = 0xBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint32));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    valueUint32 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, valueUint32));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(valueUint32, (uint32_t)0xBBBBBBBC);
    valueUint32 = 0;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint32));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    valueUint32 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, valueUint32));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(valueUint32, (uint32_t)0xFFFFFFFF);

    pthread_exit(NULL);
}
TEST(FamNonfetchArithAtomicUint32, NonfetchArithAtomicUint32Success) {
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegion, REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 1024 * sizeof(uint32_t),
                                             0777, testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_add_subtract_uint32,
                                 &info[i]))) {
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

    free(info);
    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case 4 - test for fam_add and fam_subtract APIs for uint64_t.
void *thrd_add_subtract_uint64(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(uint64_t);
    // Atomic tests for uint64
    uint64_t valueUint64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint64));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    valueUint64 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, valueUint64));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, offset));
    EXPECT_EQ(valueUint64, (uint64_t)0xBBBBBBBBBBBBBBBC);
    valueUint64 = 0;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint64));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    valueUint64 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, valueUint64));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, offset));
    EXPECT_EQ(valueUint64, (uint64_t)0xFFFFFFFFFFFFFFFF);

    pthread_exit(NULL);
}
TEST(FamNonfetchArithAtomicUint64, NonfetchArithAtomicUint64Success) {
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegion, REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 1024 * sizeof(uint64_t),
                                             0777, testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_add_subtract_uint64,
                                 &info[i]))) {
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

    free(info);
    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case 5 - test for fam_add and fam_subtract APIs for float.
void *thrd_add_subtract_float(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(float);
    // Atomic tests for float
    float valueFloat = 4.3f;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueFloat));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    valueFloat = 1.2f;
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, valueFloat));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueFloat = my_fam->fam_fetch_float(item, offset));
    EXPECT_EQ(valueFloat, 5.5f);
    valueFloat = 1.2f;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, valueFloat));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueFloat = my_fam->fam_fetch_float(item, offset));
    EXPECT_EQ(valueFloat, 4.3f);

    pthread_exit(NULL);
}

TEST(FamNonfetchArithAtomicFloat, NonfetchArithAtomicFloatSuccess) {
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegion, REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024 * sizeof(float),
                                                0777, testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_add_subtract_float,
                                 &info[i]))) {
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

    free(info);
    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case 6 - test for fam_add and fam_subtract APIs for double
void *thrd_add_subtract_double(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(double);
    // Atomic tests for double
    double valueDouble = 4.4e+38;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueDouble));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    valueDouble = 1.2e+38;
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, valueDouble));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueDouble = my_fam->fam_fetch_double(item, offset));
    EXPECT_EQ(valueDouble, 5.6e+38);
    valueDouble = 1.2e+38;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, valueDouble));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);

    EXPECT_NO_THROW(valueDouble = my_fam->fam_fetch_double(item, offset));
    EXPECT_EQ(valueDouble, 4.4e+38);

    pthread_exit(NULL);
}

TEST(FamNonfetchArithAtomicDouble, NonfetchArithAtomicDoubleSuccess) {
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegion, REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 1024 * sizeof(double),
                                             0777, testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_add_subtract_double,
                                 &info[i]))) {
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

    free(info);
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

    pthread_barrier_init(&barrier, NULL, NUM_THREADS);
    ret = RUN_ALL_TESTS();
    pthread_barrier_destroy(&barrier);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
