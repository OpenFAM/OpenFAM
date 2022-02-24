/*
 * fam_min_max_atomics_mt_reg_test.cpp
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

/* Test Case Description: Tests for non-fetching operations like fam_set,
 * fam_min, fam_max for multithreaded model.
 * (test case has been commented as there are some issues with negative test
 * scenarios with non-blocking calls.
 *
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

char *openFamModel;

Fam_Region_Descriptor *testRegionDesc;
const char *testRegionStr;

int rc;
#define NUM_THREADS 10
#define REGION_SIZE (32 * 1024 * NUM_THREADS)
#define REGION_PERM 0777

//#define SHM_CHECK (strcmp(openFamModel, "shared_memory") == 0)

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t msg_size;
} ValueInfo;

// Test case 1 - MinMaxInt32NonBlock

void *thrd_min_max_int32(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(int32_t);
    int32_t operand1Value = (int32_t)0x87654321;
    int32_t operand2Value = (int32_t)0xffffffff;
    int32_t operand3Value = 0x7fffffff;
    int32_t testMinExpectedValue = (int32_t)0x87654321;
    int32_t testMaxExpectedValue = 0x7fffffff;

    int32_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_min(item, offset, operand2Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_max(item, offset, operand3Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxInt32NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    pthread_t thr[NUM_THREADS];
    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc =
                 pthread_create(&thr[i], NULL, thrd_min_max_int32, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    delete item;
    free((void *)dataItem);
}

// Test case 2 - MinMaxUInt32NonBlock
void *thrd_min_max_uint32(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(uint32_t);

    uint32_t operand1Value = 0x7fffffff;
    uint32_t operand2Value = 0x0;
    uint32_t operand3Value = 0x1;
    uint32_t testMinExpectedValue = 0x0;
    uint32_t testMaxExpectedValue = 0x1;
    uint32_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_min(item, offset, operand2Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_max(item, offset, operand3Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);
    pthread_exit(NULL);
}
TEST(FamMinMaxAtomics, MinMaxUInt32NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;
    pthread_t thr[NUM_THREADS];

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(uint32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_uint32,
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

    free((void *)dataItem);
}

// Test case 3 - MinMaxInt64NonBlock

void *thrd_min_max_int64(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(int64_t);
    int64_t operand1Value = (int64_t)0xfedcba9876543210;
    int64_t operand2Value = (int64_t)0xffffffffffffffff;
    int64_t operand3Value = 0x7fffffffffffffff;
    int64_t testMinExpectedValue = (int64_t)0xfedcba9876543210;
    int64_t testMaxExpectedValue = 0x7fffffffffffffff;

    int64_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_min(item, offset, operand2Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_max(item, offset, operand3Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxInt64NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int64_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc =
                 pthread_create(&thr[i], NULL, thrd_min_max_int64, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    delete item;

    free((void *)dataItem);
}

// Test case 4 - MinMaxUInt64NonBlock
void *thrd_min_max_uint64(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(uint64_t);
    uint64_t operand1Value = 0x54321;
    uint64_t operand2Value = 0x7fffffffffffffff;
    uint64_t operand3Value = 0x54321;
    uint64_t testMinExpectedValue = 0x54321;
    uint64_t testMaxExpectedValue = 0x54321;

    uint64_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_min(item, offset, operand2Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_max(item, offset, operand3Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxUInt64NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(uint64_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_uint64,
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
    free((void *)dataItem);
}

// Test case 5 - MinMaxFloatNonBlock
void *thrd_min_max_float(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(float);
    float operand1Value = 8888.33f;
    float operand2Value = 9999.22f;
    float operand3Value = 5432.10f;
    float testMinExpectedValue = 8888.33f;
    float testMaxExpectedValue = 8888.33f;
    float result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_min(item, offset, operand2Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_float(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_max(item, offset, operand3Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_float(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxFloatNonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(float), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc =
                 pthread_create(&thr[i], NULL, thrd_min_max_float, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    delete item;
    free((void *)dataItem);
}

// Test case 6 - MinMaxDoubleNonBlock
void *thrd_min_max_double(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(double);
    double operand1Value = (DBL_MAX - 1.0);
    double operand2Value = 1.0;
    double operand3Value = DBL_MAX - 1;
    double testMinExpectedValue = 1.0;
    double testMaxExpectedValue = DBL_MAX - 1;
    double result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_min(item, offset, operand2Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_double(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(my_fam->fam_max(item, offset, operand3Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_double(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxDoubleNonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(double), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_double,
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
    free((void *)dataItem);
}
// Test case 7 - MinMaxInt32Block
void *thrd_min_max_int32_block(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(int32_t);

    int32_t operand1Value = (int32_t)0x87654321;
    int32_t operand2Value = (int32_t)0xffffffff;
    int32_t operand3Value = 0x7fffffff;
    int32_t testMinExpectedValue = (int32_t)0x87654321;
    int32_t testMaxExpectedValue = 0x7fffffff;

    int32_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_min(item, offset, operand2Value));
    EXPECT_EQ(operand1Value, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_max(item, offset, operand3Value));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxInt32Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;
    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_int32_block,
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
    free((void *)dataItem);
}

// Test case 8 - MinMaxUInt32Block
void *thrd_min_max_uint32_block(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(uint32_t);
    uint32_t operand1Value = 0x1234;
    uint32_t operand2Value = 0x1234;
    uint32_t operand3Value = 0xffff0000;
    uint32_t testMinExpectedValue = 0x1234;
    uint32_t testMaxExpectedValue = 0xffff0000;

    uint32_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_min(item, offset, operand2Value));
    EXPECT_EQ(operand1Value, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_max(item, offset, operand3Value));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_uint32(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxUInt32Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(uint32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_uint32_block,
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
    free((void *)dataItem);
}

// Test case 9 - MinMaxInt64Block
void *thrd_min_max_int64_block(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(int64_t);
    int64_t operand1Value = (int64_t)0xfedcba9876543210;
    int64_t operand2Value = (int64_t)0xffffffffffffffff;
    int64_t operand3Value = 0x7fffffffffffffff;
    int64_t testMinExpectedValue = (int64_t)0xfedcba9876543210;
    int64_t testMaxExpectedValue = 0x7fffffffffffffff;
    int64_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_min(item, offset, operand2Value));
    EXPECT_EQ(operand1Value, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_max(item, offset, operand3Value));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxInt64Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(int64_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_int64_block,
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
    free((void *)dataItem);
}

// Test case 10 - MinMaxUInt64Block
void *thrd_min_max_uint64_block(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(uint64_t);
    uint64_t operand1Value = 0xfedcba9876543210;
    uint64_t operand2Value = 0xffffffffffffffff;
    uint64_t operand3Value = 0x7fffffffffffffff;
    uint64_t testMinExpectedValue = 0xfedcba9876543210;
    uint64_t testMaxExpectedValue = 0xfedcba9876543210;
    uint64_t result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_min(item, offset, operand2Value));
    EXPECT_EQ(operand1Value, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_max(item, offset, operand3Value));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int64(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);

    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxUInt64Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(uint64_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_uint64_block,
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
    free((void *)dataItem);
}

// Test case 11 - MinMaxFloatBlock
void *thrd_min_max_float_block(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(float);

    float operand1Value = 99999.99f;
    float operand2Value = 0.01f;
    float operand3Value = 0.05f;
    float testMinExpectedValue = 0.01f;
    float testMaxExpectedValue = 0.05f;

    float result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_min(item, offset, operand2Value));
    EXPECT_EQ(operand1Value, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_float(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_max(item, offset, operand3Value));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_float(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxFloatBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(float), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_float_block,
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
    free((void *)dataItem);
}

// Test case 12 - MinMaxDoubleBlock
void *thrd_min_max_double_block(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(double);
    double operand1Value = 2222555577778888.3333;
    double operand2Value = 1111.2222;
    double operand3Value = 2222555577778888.3333;
    double testMinExpectedValue = 1111.2222;
    double testMaxExpectedValue = 2222555577778888.3333;
    double result;
    EXPECT_NO_THROW(my_fam->fam_set(item, offset, operand1Value));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_min(item, offset, operand2Value));
    EXPECT_EQ(operand1Value, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_double(item, offset));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result =
                        my_fam->fam_fetch_max(item, offset, operand3Value));
    EXPECT_EQ(testMinExpectedValue, result);
    EXPECT_NO_THROW(result = my_fam->fam_fetch_double(item, offset));
    EXPECT_EQ(testMaxExpectedValue, result);

    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxDoubleBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        dataItem, 1024 * NUM_THREADS * sizeof(double), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item = item;
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_double_block,
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
    free((void *)dataItem);
}
#if 0
// Test case 13 - Min Max Negative test case with invalid permissions
void *thrd_min_max_inv_perms(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(double);
    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};

if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandInt32[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandInt32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

}
           EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandInt32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandInt32[1]),
                Fam_Exception);

if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandUint32[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandUint32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandUint32[1]),
                Fam_Exception);
if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandInt64[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandInt64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandInt64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandInt64[1]),
                Fam_Exception);

if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandUint64[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandUint64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandUint64[1]),
                Fam_Exception);

if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandFloat[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandFloat[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandFloat[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandFloat[1]),
                Fam_Exception);

if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandDouble[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandDouble[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandDouble[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandDouble[1]),
                Fam_Exception);

    pthread_exit(NULL);
}	    

TEST(FamMinMaxAtomics, MinMaxNegativeBlockPerm) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];

    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, 1024 * NUM_THREADS * sizeof(double),
                                        0444, testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
                info[i].item = item;
                info[i].offset = (uint64_t)i;
                info[i].tid = i;
                if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_inv_perms, &info[i]))) {
                    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                    exit(1);
                }
            }

            for (i = 0; i < NUM_THREADS; ++i) {
                pthread_join(thr[i], NULL);
            }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    
        delete item;
    free((void *)dataItem);
}


// Test case 14 - Min Max Negative test case with invalid permissions
void *thrd_min_max_inv_perms2(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(int32_t);
    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};
if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandInt32[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandInt32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandInt32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandUint32[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandUint32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandUint32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandInt64[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandInt64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandInt64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandUint64[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandUint64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandUint64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandFloat[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandFloat[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandFloat[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandDouble[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandDouble[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandDouble[1]),
                Fam_Exception);

} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandInt32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandInt32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandInt32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandInt64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandInt64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandInt64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandFloat[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandFloat[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandFloat[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandDouble[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandDouble[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandDouble[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}

    pthread_exit(NULL);
}	    

TEST(FamMinMaxAtomics, MinMaxNegativeNonblockPerm) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);


        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, 1024 * NUM_THREADS * sizeof(double),
                                        0666, testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
                info[i].item = item;
                info[i].offset = (uint64_t)i;
                info[i].tid = i;
                if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_inv_perms2, &info[i]))) {
                    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                    exit(1);
                }
            }

            for (i = 0; i < NUM_THREADS; ++i) {
                pthread_join(thr[i], NULL);
            }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    
        delete item;
    free((void *)dataItem);
}
// Test case 15 - Min Max Negative test case with invalid offset
void *thrd_min_max_inv_offset(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = 2 * addInfo->tid * sizeof(double);
    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};
if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandInt32[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandInt32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandInt32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandUint32[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandUint32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandUint32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandInt64[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandInt64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandInt64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandUint64[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandUint64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandUint64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandFloat[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandFloat[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandFloat[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandDouble[0]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, offset, operandDouble[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, offset, operandDouble[1]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandInt32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandInt32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandInt32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandInt64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandInt64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandInt64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandFloat[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandFloat[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandFloat[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandDouble[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, offset, operandDouble[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, offset, operandDouble[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}

    pthread_exit(NULL);
}	    

TEST(FamMinMaxAtomics, MinMaxNegativeNonblockInvalidOffset) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);


        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, NUM_THREADS * sizeof(double),
                                        0777, testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
                info[i].item = item;
                info[i].offset = (uint64_t)i;
                info[i].tid = i;
                if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_inv_offset, &info[i]))) {
                    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                    exit(1);
                }
            }

            for (i = 0; i < NUM_THREADS; ++i) {
                pthread_join(thr[i], NULL);
            }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    
        delete item;
    free((void *)dataItem);
}


// Test case 16 - Min Max Negative test case with invalid offset
void *thrd_min_max_inv_offset2(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = 2 * addInfo->tid * sizeof(double);
    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};
if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandInt32[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandInt32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandInt32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandInt32[1]),
                Fam_Exception);

if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandUint32[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandUint32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandUint32[1]),
                Fam_Exception);

if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandInt64[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandInt64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandInt64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandInt64[1]),
                Fam_Exception);

if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandUint64[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandUint64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandUint64[1]),
                Fam_Exception);

if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandFloat[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandFloat[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandFloat[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandFloat[1]),
                Fam_Exception);

if (SHM_CHECK) {
            EXPECT_THROW(
                my_fam->fam_set(item, offset, operandDouble[0]),
                Fam_Exception);
} else {
            EXPECT_NO_THROW(
                my_fam->fam_set(item, offset, operandDouble[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
}
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, offset, operandDouble[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, offset, operandDouble[1]),
                Fam_Exception);
    pthread_exit(NULL);
}	    

TEST(FamMinMaxAtomics, MinMaxNegativeBlockInvalidOffset) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    const char *dataItem = get_uniq_str("first", my_fam);
    int  i;

    ValueInfo *info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, NUM_THREADS *  sizeof(double),
                                        0777  , testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
                info[i].item = item;
                info[i].offset = (uint64_t)i;
                info[i].tid = i;
                if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_inv_offset, &info[i]))) {
                    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                    exit(1);
                }
            }

            for (i = 0; i < NUM_THREADS; ++i) {
                pthread_join(thr[i], NULL);
            }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    
        delete item;
    free((void *)dataItem);
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

    openFamModel = (char *)my_fam->fam_get_option(strdup("OPENFAM_MODEL"));

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
