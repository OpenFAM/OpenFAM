/*
 * fam_arithmetic_atomics_mt_reg_test.cpp
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
/* Test Case Description: Tests fam_add/fam_sub and certain fam_atomic
 * operations for multithreaded model.
 * (test case has been commented as there are some issues with negative test
 * scenarios with non-blocking calls.
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

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t msg_size;
} ValueInfo;

pthread_barrier_t barrier;

#define NUM_THREADS 10
#define REGION_SIZE (8 * 1024 * 1024 * NUM_THREADS)
#define REGION_PERM 0777

void *thr_add_sub_int32_nonblocking(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(int32_t);

    int32_t baseValue = 0x1234;
    int32_t testAddValue = 0x1234;
    int32_t testExpectedValue = 0x2468;
    EXPECT_NE((void *)NULL, item);
    int32_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, offset));
    EXPECT_EQ(testExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, offset));
    EXPECT_EQ(baseValue, result);
    pthread_exit(NULL);
}

// Test case 1 - AddSubInt32NonBlock
TEST(FamArithmaticAtomics, AddSubInt32NonBlock) {
    int rc;
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];

    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int32_t), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;

        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_int32_nonblocking,
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
}

void *thr_add_sub_uint32_nonblocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(uint32_t);

    int32_t baseValue = 0x7ffffffe;
    int32_t testAddValue = 0x1;
    int32_t testExpectedValue = 0x7fffffff;
    EXPECT_NE((void *)NULL, item);
    int32_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(testExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(baseValue, result);
    pthread_exit(NULL);
}

// Test case 2 - AddSubUInt32NonBlock
TEST(FamArithmaticAtomics, AddSubUInt32NonBlock) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item;

    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(uint32_t), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_uint32_nonblocking,
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
}

void *thr_add_sub_int64_nonblocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(int64_t);

    int64_t baseValue = 0x1111222233334321;
    int64_t testAddValue = 0x1111222233334321;
    int64_t testExpectedValue = 0x2222444466668642;
    EXPECT_NE((void *)NULL, item);
    int64_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(testExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(baseValue, result);
    pthread_exit(NULL);
}
// Test case 3 - AddSubInt64NonBlock
TEST(FamArithmaticAtomics, AddSubInt64NonBlock) {
    Fam_Descriptor *item;
    int rc;
    pthread_t thr[NUM_THREADS];

    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(uint32_t), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_int64_nonblocking,
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
}

void *thr_add_sub_uint64_nonblocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(uint64_t);

    uint64_t baseValue = 0x7fffffffffffffff;
    uint64_t testAddValue = 0x1;
    uint64_t testExpectedValue = 0x8000000000000000;
    EXPECT_NE((void *)NULL, item);
    uint64_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_uint64(item, offset));
    EXPECT_EQ(testExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_uint64(item, offset));
    EXPECT_EQ(baseValue, result);
    pthread_exit(NULL);
}

// Test case 4 - AddSubUInt64NonBlock
TEST(FamArithmaticAtomics, AddSubUInt64NonBlock) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(uint64_t), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_uint64_nonblocking,
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
}

void *thr_add_sub_float_nonblocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(float);

    float baseValue = 1234.12f;
    float testAddValue = 1234.12f;
    float testExpectedValue = 2468.24f;
    EXPECT_NE((void *)NULL, item);
    float result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_float(item, offset));
    EXPECT_FLOAT_EQ(testExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_float(item, offset));
    EXPECT_FLOAT_EQ(baseValue, result);
    pthread_exit(NULL);
}
// Test case 5 - AddSubFloatNonBlock
TEST(FamArithmaticAtomics, AddSubFloatNonBlock) {
    int rc;
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];

    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(float), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_float_nonblocking,
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
}

void *thr_add_sub_double_nonblocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(double);

    double baseValue = 2222555577778888.3333;
    double testAddValue = 1111.2222;
    double testExpectedValue = 2222555577779999.5555;
    EXPECT_NE((void *)NULL, item);
    double result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(my_fam->fam_add(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_double(item, offset));
    EXPECT_DOUBLE_EQ(testExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_subtract(item, offset, testAddValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_double(item, offset));
    EXPECT_DOUBLE_EQ(baseValue, result);
    pthread_exit(NULL);
}
// Test case 6 - AddSubDoubleNonBlock
TEST(FamArithmaticAtomics, AddSubDoubleNonBlock) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item;

    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info =
        (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS * sizeof(double));

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(double), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_double_nonblocking,
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
}
// Test case 7 - AddSubInt32Blocking
void *thr_add_sub_int32_blocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(int32_t);

    int32_t baseValue = 0x1234;
    int32_t testAddValue = 0x1234;
    int32_t testExpectedValue = 0x2468;
    EXPECT_NE((void *)NULL, item);
    int32_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_add(item, offset, testAddValue));
    EXPECT_EQ(baseValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_subtract(item, offset, testAddValue));
    EXPECT_EQ(testExpectedValue, result);

    pthread_exit(NULL);
}

TEST(FamArithmaticAtomics, AddSubInt32Blocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int32_t), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_int32_blocking,
                                 &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
            //        return -1;
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    free(info);
}

void *thr_add_sub_uint32_blocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(uint32_t);

    uint32_t baseValue = 0x54321;
    uint32_t testAddValue = 0x54321;
    uint32_t testExpectedValue = 0xA8642;
    EXPECT_NE((void *)NULL, item);
    uint32_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_add(item, offset, testAddValue));
    EXPECT_EQ(baseValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_subtract(item, offset, testAddValue));
    EXPECT_EQ(testExpectedValue, result);

    pthread_exit(NULL);
}

// Test case 8 - AddSubUInt32Blocking
TEST(FamArithmaticAtomics, AddSubUInt32Blocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(uint32_t), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_uint32_blocking,
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
}
void *thr_add_sub_int64_blocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(int64_t);
    int64_t baseValue = 0x1111222233334321;
    int64_t testAddValue = 0x1111222233334321;
    int64_t testExpectedValue = 0x2222444466668642;
    int64_t result;
    EXPECT_NE((void *)NULL, item);
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_add(item, offset, testAddValue));
    EXPECT_EQ(baseValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_subtract(item, offset, testAddValue));
    EXPECT_EQ(testExpectedValue, result);
    pthread_exit(NULL);
}
// Test case 9 - AddSubInt64Blocking
TEST(FamArithmaticAtomics, AddSubInt64Blocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int64_t), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_int64_blocking,
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
}

void *thr_add_sub_uint64_blocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(uint64_t);

    uint64_t baseValue = 0x7fffffffffffffff;
    uint64_t testAddValue = 0x1;
    uint64_t testExpectedValue = 0x8000000000000000;
    EXPECT_NE((void *)NULL, item);

    uint64_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_add(item, offset, testAddValue));
    EXPECT_EQ(baseValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_subtract(item, offset, testAddValue));
    EXPECT_EQ(testExpectedValue, result);

    pthread_exit(NULL);
}
// Test case 10 - AddSubUInt64Blocking
TEST(FamArithmaticAtomics, AddSubUInt64Blocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(uint64_t), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_uint64_blocking,
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
}

void *thr_add_sub_float_blocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(float);

    float baseValue = 99999.99f;
    float testAddValue = 0.01f;
    float testExpectedValue = 100000.00f;
    float result = 0.0f;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_add(item, offset, testAddValue));
    EXPECT_FLOAT_EQ(baseValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_subtract(item, offset, testAddValue));
    EXPECT_FLOAT_EQ(testExpectedValue, result);

    pthread_exit(NULL);
}
// Test case 11 - AddSubFloatBlocking
TEST(FamArithmaticAtomics, AddSubFloatBlocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(float), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_float_blocking,
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
}

void *thr_add_sub_double_blocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    int tid = addInfo->tid;
    uint64_t offset = tid * sizeof(double);

    double baseValue = 2222555577778888.3333;
    double testAddValue = 1111.2222;
    double testExpectedValue = 2222555577779999.5555;
    EXPECT_NE((void *)NULL, item);
    double result = 0.0;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, baseValue));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_add(item, offset, testAddValue));
    EXPECT_DOUBLE_EQ(baseValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_subtract(item, offset, testAddValue));
    EXPECT_DOUBLE_EQ(testExpectedValue, result);

    pthread_exit(NULL);
}
// Test case 12 - AddSubDoubleBlocking
TEST(FamArithmaticAtomics, AddSubDoubleBlocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(double), 0777,
                        testRegionDesc));

    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_double_blocking,
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
}

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
    delete testRegionDesc;
    free((void *)testRegionStr);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
