/*
 * fam_fetch_min_max_atomics_reg_test.cpp
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
#define REGION_SIZE (1024 * 1024)
//#define REGION_PERM 0777

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t deltaValue;
} ValueInfo;

// Test case 1 - test that fetches the value in given offset in FAM, compares
// with given value, place max/min value (based on API) in FAM at that offset
// and returns old value

void *thrd_max_min_int32(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(int32_t);
    // Atomic min and max operations for int32
    int32_t valueInt32 = 0xBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueInt32));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueInt32 = 0xAAAAAAAA;
    EXPECT_NO_THROW(valueInt32 =
                        my_fam->fam_fetch_min(item, offset, valueInt32));
    EXPECT_EQ(valueInt32, (int32_t)0xBBBBBBBB);
    EXPECT_NO_THROW(valueInt32 = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(valueInt32, (int32_t)0xAAAAAAAA);
    valueInt32 = 0xCCCCCCCC;
    EXPECT_NO_THROW(valueInt32 =
                        my_fam->fam_fetch_max(item, offset, valueInt32));
    EXPECT_EQ(valueInt32, (int32_t)0xAAAAAAAA);
    EXPECT_NO_THROW(valueInt32 = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(valueInt32, (int32_t)0xCCCCCCCC);

    pthread_exit(NULL);
}

TEST(FamMinMaxAtomicInt32, MinMaxAtomicInt32Success) {
    Fam_Descriptor *item;
    testRegionStr = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    ValueInfo *info;
    pthread_t thr[NUM_THREADS];
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(int32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc =
                 pthread_create(&thr[i], NULL, thrd_max_min_int32, &info[i]))) {
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

    free((void *)firstItem);
    free((void *)info);
}

// Test case 2 - test that fetches the value in given offset in FAM, compares
// with given value, place max/min value (based on API) in FAM at that offset
// and returns old value

void *thrd_max_min_int64(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(int64_t);
    // Atomic min and max operations for int64
    int64_t valueInt64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueInt64));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    valueInt64 = 0xAAAAAAAAAAAAAAAA;
    EXPECT_NO_THROW(valueInt64 =
                        my_fam->fam_fetch_min(item, offset, valueInt64));
    EXPECT_EQ(valueInt64, (int64_t)0xBBBBBBBBBBBBBBBB);
    EXPECT_NO_THROW(valueInt64 = my_fam->fam_fetch_uint64(item, offset));
    EXPECT_EQ(valueInt64, (int64_t)0xAAAAAAAAAAAAAAAA);
    valueInt64 = 0xCCCCCCCCCCCCCCCC;
    EXPECT_NO_THROW(valueInt64 =
                        my_fam->fam_fetch_max(item, offset, valueInt64));
    EXPECT_EQ(valueInt64, (int64_t)0xAAAAAAAAAAAAAAAA);
    EXPECT_NO_THROW(valueInt64 = my_fam->fam_fetch_uint64(item, offset));
    EXPECT_EQ(valueInt64, (int64_t)0xCCCCCCCCCCCCCCCC);

    pthread_exit(NULL);
}

TEST(FamMinMaxAtomicInt64, MinMaxAtomicInt64Success) {
    Fam_Descriptor *item;
    testRegionStr = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    ValueInfo *info;
    pthread_t thr[NUM_THREADS];
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(int32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc =
                 pthread_create(&thr[i], NULL, thrd_max_min_int64, &info[i]))) {
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

    free((void *)firstItem);
    free((void *)info);
}

// Test case 3 - test that fetches the value in given offset in FAM, compares
// with given value, place max/min value (based on API) in FAM at that offset
// and returns old value

void *thrd_max_min_uint32(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(uint32_t);
    // Atomic min and max operations for uint32
    uint32_t valueUint32 = 0xBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    valueUint32 = 0x11111111;
    EXPECT_NO_THROW(valueUint32 =
                        my_fam->fam_fetch_min(item, offset, valueUint32));
    EXPECT_EQ(valueUint32, 0xBBBBBBBB);
    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(valueUint32, (uint32_t)0x11111111);
    valueUint32 = 0xCCCCCCCC;
    EXPECT_NO_THROW(valueUint32 =
                        my_fam->fam_fetch_max(item, offset, valueUint32));
    EXPECT_EQ(valueUint32, (uint32_t)0x11111111);
    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(valueUint32, (uint32_t)0xCCCCCCCC);

    pthread_exit(NULL);
}

TEST(FamMinMaxAtomicUint32, MinMaxAtomicUint32Success) {
    Fam_Descriptor *item;
    testRegionStr = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    ValueInfo *info;
    pthread_t thr[NUM_THREADS];
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(int32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_max_min_uint32,
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

    free((void *)firstItem);
    free((void *)info);
}

// Test case 4 - test that fetches the value in given offset in FAM, compares
// with given value, place max/min value (based on API) in FAM at that offset
// and returns old value

void *thrd_max_min_uint64(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(uint64_t);
    // Atomic min and max operations for uint64
    uint64_t valueUint64 = 0xAAAAAAAAAAAAAAAA;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    valueUint64 = 0x1111111111111111;
    EXPECT_NO_THROW(valueUint64 =
                        my_fam->fam_fetch_min(item, offset, valueUint64));
    EXPECT_EQ(valueUint64, (uint64_t)0xAAAAAAAAAAAAAAAA);
    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, offset));
    EXPECT_EQ(valueUint64, (uint64_t)0x1111111111111111);
    valueUint64 = 0xCCCCCCCCCCCCCCCC;
    EXPECT_NO_THROW(valueUint64 =
                        my_fam->fam_fetch_max(item, offset, valueUint64));
    EXPECT_EQ(valueUint64, (uint64_t)0x1111111111111111);
    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, offset));
    EXPECT_EQ(valueUint64, (uint64_t)0xCCCCCCCCCCCCCCCC);

    pthread_exit(NULL);
}
TEST(FamMinMaxAtomicUint64, MinMaxAtomicUint64Success) {
    Fam_Descriptor *item;
    testRegionStr = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    ValueInfo *info;
    int i;
    pthread_t thr[NUM_THREADS];
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(int32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_max_min_uint64,
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

    free((void *)firstItem);
    free((void *)info);
}

// Test case 5 - test that fetches the value in given offset in FAM, compares
// with given value, place max/min value (based on API) in FAM at that offset
// and returns old value

void *thrd_max_min_float(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(float);
    // Atomic min and max operations for float
    float valueFloat = 3.3f;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueFloat));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    valueFloat = 1.2f;
    EXPECT_NO_THROW(valueFloat =
                        my_fam->fam_fetch_min(item, offset, valueFloat));
    EXPECT_EQ(valueFloat, 3.3f);
    EXPECT_NO_THROW(valueFloat = my_fam->fam_fetch_float(item, offset));
    EXPECT_EQ(valueFloat, 1.2f);
    valueFloat = 5.6f;
    EXPECT_NO_THROW(valueFloat =
                        my_fam->fam_fetch_max(item, offset, valueFloat));
    EXPECT_EQ(valueFloat, 1.2f);
    EXPECT_NO_THROW(valueFloat = my_fam->fam_fetch_float(item, offset));
    EXPECT_EQ(valueFloat, 5.6f);

    pthread_exit(NULL);
}
TEST(FamMinMaxAtomicFloat, MinMaxAtomicFloatSuccess) {
    Fam_Descriptor *item;
    testRegionStr = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(int32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc =
                 pthread_create(&thr[i], NULL, thrd_max_min_float, &info[i]))) {
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

    free((void *)firstItem);
    free((void *)info);
}
// Test case 6 - test that fetches the value in given offset in FAM, compares
// with given value, place max/min value (based on API) in FAM at that offset
// and returns old value

void *thrd_max_min_double(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(double);
    // Atomic min and max operations for double
    double valueDouble = 3.3e+38;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueDouble));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    valueDouble = 1.2e+38;
    EXPECT_NO_THROW(valueDouble =
                        my_fam->fam_fetch_min(item, offset, valueDouble));
    EXPECT_EQ(valueDouble, 3.3e+38);
    EXPECT_NO_THROW(valueDouble = my_fam->fam_fetch_double(item, offset));
    EXPECT_EQ(valueDouble, 1.2e+38);
    valueDouble = 5.6e+38;
    EXPECT_NO_THROW(valueDouble =
                        my_fam->fam_fetch_max(item, offset, valueDouble));
    EXPECT_EQ(valueDouble, 1.2e+38);
    EXPECT_NO_THROW(valueDouble = my_fam->fam_fetch_double(item, offset));
    EXPECT_EQ(valueDouble, 5.6e+38);

    pthread_exit(NULL);
}

TEST(FamMinMaxAtomicDouble, MinMaxAtomicDoubleSuccess) {
    Fam_Descriptor *item;
    testRegionStr = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(int32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_max_min_double,
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

    free((void *)firstItem);
    free((void *)info);
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
    free((void *)testRegionStr);

    return ret;
}
