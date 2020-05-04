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
#define NUM_THREADS 10
#define REGION_SIZE (32 * 1024 * 1024 * NUM_THREADS)
#define REGION_PERM 0777

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t msg_size;
} ValueInfo;

typedef struct {
    Fam_Descriptor *item[4];
    uint64_t offset;
    int32_t tid;
    int32_t msg_size;
    int itemId;
} ValueInfo2;

// Test case 1 - MinMaxInt32NonBlock

void *thrd_min_max_int32(void *arg) {

    int i, sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(int32_t);
    size_t test_item_size[3] = {1024, 4096, 8192};
    int32_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffff,
                                (int32_t)0x87654321};
    int32_t operand2Value[5] = {(int32_t)0xf0000000, 0x1234, 0x7fffffff, 0x0,
                                (int32_t)0xffffffff};
    int32_t operand3Value[5] = {0x0, (int32_t)0xffff0000, 0x54321, 0x1,
                                0x7fffffff};
    int32_t testMinExpectedValue[5] = {(int32_t)0xf0000000, 0x1234, 0x54321,
                                       0x0, (int32_t)0x87654321};
    int32_t testMaxExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x1, 0x7fffffff};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(int32_t) - 1)};


    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            int32_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int32(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int32(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxInt32NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm;
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(int32_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
            if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_int32,
                                     &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }
        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    }
    delete item;
    free((void *)dataItem);
}

// Test case 2 - MinMaxUInt32NonBlock
void *thrd_min_max_uint32(void *arg) {

    int i, sm, ofs;
    size_t test_item_size[3] = {1024, 4096, 8192};
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(uint32_t);
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(uint32_t) - 1)};

    uint32_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffff, 0x87654321};
    uint32_t operand2Value[5] = {0xf0000000, 0x1234, 0x7fffffff, 0x0,
                                 0xffffffff};
    uint32_t operand3Value[5] = {0x0, 0xffff0000, 0x54321, 0x1, 0x7fffffff};
    uint32_t testMinExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x0, 0x87654321};
    uint32_t testMaxExpectedValue[5] = {0x0, 0xffff0000, 0x54321, 0x1,
                                        0x87654321};
    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            uint32_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(
                result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(
                result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }
    pthread_exit(NULL);
}
TEST(FamMinMaxAtomics, MinMaxUInt32NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm;
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(uint32_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
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
    }
    free((void *)dataItem);
}

// Test case 3 - MinMaxInt64NonBlock

void *thrd_min_max_int64(void *arg) {

    int i, sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(int64_t);
    size_t test_item_size[3] = {1024, 4096, 8192};
    // size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS, 8192
    // * NUM_THREADS};
    int64_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffffffffffff,
                                (int64_t)0xfedcba9876543210};
    int64_t operand2Value[5] = {(int64_t)0xf000000000000000, 0x1234,
                                0x7fffffffffffffff, 0x0,
                                (int64_t)0xffffffffffffffff};
    int64_t operand3Value[5] = {0x0, (int64_t)0xffffffff00000000, 0x54321, 0x1,
                                0x7fffffffffffffff};
    int64_t testMinExpectedValue[5] = {(int64_t)0xf000000000000000, 0x1234,
                                       0x54321, 0x0,
                                       (int64_t)0xfedcba9876543210};
    int64_t testMaxExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x1,
                                       0x7fffffffffffffff};

    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(int64_t) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            int64_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int64(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int64(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }

    pthread_exit(NULL);
}
TEST(FamMinMaxAtomics, MinMaxInt64NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};
    int i, sm;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(int64_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
            if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_int64,
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
    }
    free((void *)dataItem);
}

// Test case 4 - MinMaxUInt64NonBlock
void *thrd_min_max_uint64(void *arg) {

    int i, sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(uint64_t);
    // size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS, 8192
    // * NUM_THREADS};
    size_t test_item_size[3] = {1024, 4096, 8192};
    uint64_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffffffffffff,
                                 0xfedcba9876543210};
    uint64_t operand2Value[5] = {0xf000000000000000, 0x1234, 0x7fffffffffffffff,
                                 0x0, 0xffffffffffffffff};
    uint64_t operand3Value[5] = {0x0, 0xffffffff00000000, 0x54321, 0x1,
                                 0x7fffffffffffffff};
    uint64_t testMinExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x0,
                                        0xfedcba9876543210};
    uint64_t testMaxExpectedValue[5] = {0x0, 0xffffffff00000000, 0x54321, 0x1,
                                        0xfedcba9876543210};

    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(uint64_t) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            uint64_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int64(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int64(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxUInt64NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};
    int i, sm;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(uint64_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
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
    }
    delete item;
    free((void *)dataItem);
}

// Test case 5 - MinMaxFloatNonBlock
void *thrd_min_max_float(void *arg) {

    int i, sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(float);
    size_t test_item_size[3] = {1024, 4096, 8192};
    // size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS, 8192
    // * NUM_THREADS};
    float operand1Value[5] = {0.0f, 1234.12f, 54321.87f, 8888.33f, 99999.99f};
    float operand2Value[5] = {0.1f, 1234.12f, 0.12f, 9999.22f, 0.01f};
    float operand3Value[5] = {0.5f, 1234.23f, 0.01f, 5432.10f, 0.05f};
    float testMinExpectedValue[5] = {0.0f, 1234.12f, 0.12f, 8888.33f, 0.01f};
    float testMaxExpectedValue[5] = {0.5f, 1234.23f, 0.12f, 8888.33f, 0.05f};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(float) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            float result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_float(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_float(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxFloatNonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};
    int i, sm;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};

    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);
    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(float),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
            if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_float,
                                     &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    }
    delete item;
    free((void *)dataItem);
}

// Test case 6 - MinMaxDoubleNonBlock
void *thrd_min_max_double(void *arg) {

    int i, sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(double);
    // size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS, 8192
    // * NUM_THREADS};
    size_t test_item_size[3] = {1024, 4096, 8192};
    double operand1Value[5] = {0.0, 1234.123, 987654321.8765,
                               2222555577778888.3333, (DBL_MAX - 1.0)};
    double operand2Value[5] = {0.1, 1234.123, 0.1234, 1111.2222, 1.0};
    double operand3Value[5] = {1.5, 5432.123, 0.0123, 2222555577778888.3333,
                               DBL_MAX - 1};
    double testMinExpectedValue[5] = {0.0, 1234.123, 0.1234, 1111.2222, 1.0};
    double testMaxExpectedValue[5] = {1.5, 5432.123, 0.1234,
                                      2222555577778888.3333, DBL_MAX - 1};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(double) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            double result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(
                result = my_fam->fam_fetch_double(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(
                result = my_fam->fam_fetch_double(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxDoubleNonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};
    int i, sm;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(double),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
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
    }
    delete item;
    free((void *)dataItem);
}
// Test case 7 - MinMaxInt32Block
void *thrd_min_max_int32_block(void *arg) {

    int i, sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(int32_t);

    int32_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffff,
                                (int32_t)0x87654321};
    int32_t operand2Value[5] = {(int32_t)0xf0000000, 0x1234, 0x7fffffff, 0x0,
                                (int32_t)0xffffffff};
    int32_t operand3Value[5] = {0x0, (int32_t)0xffff0000, 0x54321, 0x1,
                                0x7fffffff};
    int32_t testMinExpectedValue[5] = {(int32_t)0xf0000000, 0x1234, 0x54321,
                                       0x0, (int32_t)0x87654321};
    int32_t testMaxExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x1, 0x7fffffff};

    //    size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS,
    //    8192 * NUM_THREADS};
    size_t test_item_size[3] = {1024, 4096, 8192};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(int32_t) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            int32_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                item, testOffset[ofs], operand2Value[i]));
            EXPECT_EQ(operand1Value[i], result);
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int32(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                item, testOffset[ofs], operand3Value[i]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int32(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxInt32Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};
    int i, sm;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(int32_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
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
    }
    delete item;
    free((void *)dataItem);
}

// Test case 8 - MinMaxUInt32Block
void *thrd_min_max_uint32_block(void *arg) {

    int i, sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(uint32_t);
    // size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS, 8192
    // * NUM_THREADS};
    size_t test_item_size[3] = {1024, 4096, 8192};
    uint32_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffff, 0x87654321};
    uint32_t operand2Value[5] = {0xf0000000, 0x1234, 0x7fffffff, 0x0,
                                 0xffffffff};
    uint32_t operand3Value[5] = {0x0, 0xffff0000, 0x54321, 0x1, 0x7fffffff};
    uint32_t testMinExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x0, 0x87654321};
    uint32_t testMaxExpectedValue[5] = {0x0, 0xffff0000, 0x54321, 0x1,
                                        0x87654321};

    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(uint32_t) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            uint32_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                item, testOffset[ofs], operand2Value[i]));
            EXPECT_EQ(operand1Value[i], result);
            EXPECT_NO_THROW(
                result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                item, testOffset[ofs], operand3Value[i]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(
                result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxUInt32Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};
    int i, sm;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(uint32_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
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
    }
    delete item;
    free((void *)dataItem);
}

// Test case 9 - MinMaxInt64Block
void *thrd_min_max_int64_block(void *arg) {

    int i, sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(int64_t);
    //    size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS,
    //    8192 * NUM_THREADS};
    size_t test_item_size[3] = {1024, 4096, 8192};
    int64_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffffffffffff,
                                (int64_t)0xfedcba9876543210};
    int64_t operand2Value[5] = {(int64_t)0xf000000000000000, 0x1234,
                                0x7fffffffffffffff, 0x0,
                                (int64_t)0xffffffffffffffff};
    int64_t operand3Value[5] = {0x0, (int64_t)0xffffffff00000000, 0x54321, 0x1,
                                0x7fffffffffffffff};
    int64_t testMinExpectedValue[5] = {(int64_t)0xf000000000000000, 0x1234,
                                       0x54321, 0x0,
                                       (int64_t)0xfedcba9876543210};
    int64_t testMaxExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x1,
                                       0x7fffffffffffffff};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(int64_t) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            int64_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                item, testOffset[ofs], operand2Value[i]));
            EXPECT_EQ(operand1Value[i], result);
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int64(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                item, testOffset[ofs], operand3Value[i]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int64(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }

    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxInt64Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};
    int i, sm;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(int64_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
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
    }
    delete item;
    free((void *)dataItem);
}

// Test case 10 - MinMaxUInt64Block
void *thrd_min_max_uint64_block(void *arg) {

    int i, sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(uint64_t);
    // size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS, 8192
    // * NUM_THREADS};
    size_t test_item_size[3] = {1024, 4096, 8192};
    uint64_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffffffffffff,
                                 0xfedcba9876543210};
    uint64_t operand2Value[5] = {0xf000000000000000, 0x1234, 0x7fffffffffffffff,
                                 0x0, 0xffffffffffffffff};
    uint64_t operand3Value[5] = {0x0, 0xffffffff00000000, 0x54321, 0x1,
                                 0x7fffffffffffffff};
    uint64_t testMinExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x0,
                                        0xfedcba9876543210};
    uint64_t testMaxExpectedValue[5] = {0x0, 0xffffffff00000000, 0x54321, 0x1,
                                        0xfedcba9876543210};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(int64_t) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            uint64_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                item, testOffset[ofs], operand2Value[i]));
            EXPECT_EQ(operand1Value[i], result);
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int64(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                item, testOffset[ofs], operand3Value[i]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int64(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }

    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxUInt64Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};
    int i, sm;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(uint64_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
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
    }
    delete item;
    free((void *)dataItem);
}

// Test case 11 - MinMaxFloatBlock
void *thrd_min_max_float_block(void *arg) {

    int i, sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(float);
    // size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS, 8192
    // * NUM_THREADS};
    size_t test_item_size[3] = {1024, 4096, 8192};

    float operand1Value[5] = {0.0f, 1234.12f, 54321.87f, 8888.33f, 99999.99f};
    float operand2Value[5] = {0.1f, 1234.12f, 0.12f, 9999.22f, 0.01f};
    float operand3Value[5] = {0.5f, 1234.23f, 0.01f, 5432.10f, 0.05f};
    float testMinExpectedValue[5] = {0.0f, 1234.12f, 0.12f, 8888.33f, 0.01f};
    float testMaxExpectedValue[5] = {0.5f, 1234.23f, 0.12f, 8888.33f, 0.05f};

    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(float) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            float result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                item, testOffset[ofs], operand2Value[i]));
            EXPECT_EQ(operand1Value[i], result);
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_float(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                item, testOffset[ofs], operand3Value[i]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_float(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }
    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxFloatBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};
    int i, sm;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);
    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(float),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
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
    }
    delete item;
    free((void *)dataItem);
}

// Test case 12 - MinMaxDoubleBlock
void *thrd_min_max_double_block(void *arg) {

    int i, sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(double);
    // size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS, 8192
    // * NUM_THREADS};
    size_t test_item_size[3] = {1024, 4096, 8192};
    double operand1Value[5] = {0.0, 1234.123, 987654321.8765,
                               2222555577778888.3333, (DBL_MAX - 1.0)};
    double operand2Value[5] = {0.1, 1234.123, 0.1234, 1111.2222, 1.0};
    double operand3Value[5] = {1.5, 5432.123, 0.0123, 2222555577778888.3333,
                               DBL_MAX - 1};
    double testMinExpectedValue[5] = {0.0, 1234.123, 0.1234, 1111.2222, 1.0};
    double testMaxExpectedValue[5] = {1.5, 5432.123, 0.1234,
                                      2222555577778888.3333, DBL_MAX - 1};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(double) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            double result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                item, testOffset[ofs], operand2Value[i]));
            EXPECT_EQ(operand1Value[i], result);
            EXPECT_NO_THROW(
                result = my_fam->fam_fetch_double(item, testOffset[ofs]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                item, testOffset[ofs], operand3Value[i]));
            EXPECT_EQ(testMinExpectedValue[i], result);
            EXPECT_NO_THROW(
                result = my_fam->fam_fetch_double(item, testOffset[ofs]));
            EXPECT_EQ(testMaxExpectedValue[i], result);
        }
    }

    pthread_exit(NULL);
}

TEST(FamMinMaxAtomics, MinMaxDoubleBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[3] = {1024, 4096, 8192};
    int i, sm;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            dataItem, test_item_size[sm] * sizeof(double),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
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
    }
    delete item;
    free((void *)dataItem);
}
#if 0
// Test case 13 - Min Max Negative test case with invalid permissions
void *thrd_min_max_inv_perms(void *arg) {

    int  sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(double);
    //size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    size_t test_item_size[4] = {1024 , 4096 , 8192,16384};
    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};

        uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(double) - 1)};
        for (ofs = 0; ofs < 1; ofs++) {
#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt32[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

#endif
           EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandInt32[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandInt32[1]),
                Fam_Datapath_Exception);

#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandUint32[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandUint32[1]),
                Fam_Datapath_Exception);
#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt64[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandInt64[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandInt64[1]),
                Fam_Datapath_Exception);

#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandUint64[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandUint64[1]),
                Fam_Datapath_Exception);

#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandFloat[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandFloat[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandFloat[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandFloat[1]),
                Fam_Datapath_Exception);

#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandDouble[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandDouble[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandDouble[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandDouble[1]),
                Fam_Datapath_Exception);
	    
        }
    pthread_exit(NULL);
}	    

TEST(FamMinMaxAtomics, MinMaxNegativeBlockPerm) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];

    //size_t test_item_size[4] = {1024 * sizeof(double) , 4096 * sizeof(double), 8192 * sizeof(double),16384 * sizeof(double)};
    size_t test_item_size[4] = {1024 , 4096 , 8192,16384};
    int sm, i;

    mode_t test_perm_mode[4] = {0400, 0444, 0455, 0411};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    //for (sm = 0; sm < 4; sm++) 
    for (sm = 0; sm < 1; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(double),
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
                info[i].item[sm] = item;
                info[i].offset = (uint64_t)i;
                info[i].tid = i;
                info[i].itemId = sm;
                if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_inv_perms, &info[i]))) {
                    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                    exit(1);
                }
            }

            for (i = 0; i < NUM_THREADS; ++i) {
                pthread_join(thr[i], NULL);
            }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    }
        delete item;
    free((void *)dataItem);
}

// Test case 14 - Min Max Negative test case with invalid permissions
void *thrd_min_max_inv_perms2(void *arg) {

    int  sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(int32_t);
    //size_t test_item_size[3] = {1024 * NUM_THREADS , 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    size_t test_item_size[4] = {1024 , 4096 , 8192,16384};
    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};
        uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(double) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt32[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandInt32[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandInt32[1]),
                Fam_Datapath_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandUint32[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandUint32[1]),
                Fam_Datapath_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt64[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandInt64[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandInt64[1]),
                Fam_Datapath_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandUint64[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandUint64[1]),
                Fam_Datapath_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandFloat[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandFloat[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandFloat[1]),
                Fam_Datapath_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandDouble[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandDouble[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandDouble[1]),
                Fam_Datapath_Exception);

#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandInt32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandInt32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandInt64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandInt64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandFloat[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandFloat[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandFloat[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandDouble[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandDouble[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandDouble[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
        }

    pthread_exit(NULL);
}	    

TEST(FamMinMaxAtomics, MinMaxNegativeNonblockPerm) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[4] = {1024 , 4096 , 8192,16384};
    int sm, i;

    mode_t test_perm_mode[4] = {0400, 0444, 0455, 0411};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);


    for (sm = 0; sm < 4; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(double),
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
                info[i].item[sm] = item;
                info[i].offset = (uint64_t)i;
                info[i].tid = i;
                info[i].itemId = sm;
                if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_inv_perms2, &info[i]))) {
                    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                    exit(1);
                }
            }

            for (i = 0; i < NUM_THREADS; ++i) {
                pthread_join(thr[i], NULL);
            }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    }
        delete item;
    free((void *)dataItem);
}
// Test case 15 - Min Max Negative test case with invalid offset
void *thrd_min_max_inv_offset(void *arg) {

    int  sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(double);
    size_t test_item_size[4] = {1024 , 4096 , 8192,16384};
    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};
    uint64_t testOffset[3] = {offset + test_item_size[sm] , offset + (2 * test_item_size[sm] ),
                                  offset + (test_item_size[sm] + 1)};
        for (ofs = 0; ofs < 3; ofs++) {
#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt32[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandInt32[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandInt32[1]),
                Fam_Datapath_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandUint32[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandUint32[1]),
                Fam_Datapath_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt64[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandInt64[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandInt64[1]),
                Fam_Datapath_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandUint64[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandUint64[1]),
                Fam_Datapath_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandFloat[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandFloat[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandFloat[1]),
                Fam_Datapath_Exception);

            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandDouble[0]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandDouble[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandDouble[1]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandInt32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandInt32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandInt64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandInt64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandFloat[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandFloat[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandFloat[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandDouble[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_min(item, testOffset[ofs], operandDouble[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
            EXPECT_NO_THROW(
                my_fam->fam_max(item, testOffset[ofs], operandDouble[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif

        }

    pthread_exit(NULL);
}	    

TEST(FamMinMaxAtomics, MinMaxNegativeNonblockInvalidOffset) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[4] = {1024 , 4096 , 8192,16384};
    int sm, i;

    mode_t test_perm_mode[4] = {0600, 0644, 0755, 0711};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);


    for (sm = 0; sm < 4; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(double),
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
                info[i].item[sm] = item;
                info[i].offset = (uint64_t)i;
                info[i].tid = i;
                info[i].itemId = sm;
                if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_inv_offset, &info[i]))) {
                    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                    exit(1);
                }
            }

            for (i = 0; i < NUM_THREADS; ++i) {
                pthread_join(thr[i], NULL);
            }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    }
        delete item;
    free((void *)dataItem);
}


// Test case 16 - Min Max Negative test case with invalid offset
void *thrd_min_max_inv_offset2(void *arg) {

    int  sm, ofs;
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    sm = addInfo->itemId;
    Fam_Descriptor *item = addInfo->item[sm];
    uint64_t offset = addInfo->tid * sizeof(double);
    size_t test_item_size[4] = {1024 , 4096 , 8192,16384};
    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};
        uint64_t testOffset[3] = {offset + test_item_size[sm], offset + (2 * test_item_size[sm]),
                                  offset + (test_item_size[sm] + 1)};

        for (ofs = 0; ofs < 3; ofs++) {
#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt32[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandInt32[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandInt32[1]),
                Fam_Datapath_Exception);

#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandUint32[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandUint32[1]),
                Fam_Datapath_Exception);

#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt64[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandInt64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandInt64[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandInt64[1]),
                Fam_Datapath_Exception);

#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandUint64[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandUint64[1]),
                Fam_Datapath_Exception);

#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandFloat[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandFloat[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandFloat[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandFloat[1]),
                Fam_Datapath_Exception);

#ifdef SHM
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandDouble[0]),
                Fam_Datapath_Exception);
#else
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandDouble[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandDouble[1]),
                Fam_Datapath_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandDouble[1]),
                Fam_Datapath_Exception);
        }


    pthread_exit(NULL);
}	    

TEST(FamMinMaxAtomics, MinMaxNegativeBlockInvalidOffset) {
    Fam_Descriptor *item;
    pthread_t thr[NUM_THREADS];
    size_t test_item_size[4] = {1024 , 4096 , 8192,16384};
    const char *dataItem = get_uniq_str("first", my_fam);
    int sm, i;

    mode_t test_perm_mode[4] = {0600, 0644, 0755, 0711};
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 4; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm] *  sizeof(double),
                                        test_perm_mode[sm]  , testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
                info[i].item[sm] = item;
                info[i].offset = (uint64_t)i;
                info[i].tid = i;
                info[i].itemId = sm;
                if ((rc = pthread_create(&thr[i], NULL, thrd_min_max_inv_offset, &info[i]))) {
                    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                    exit(1);
                }
            }

            for (i = 0; i < NUM_THREADS; ++i) {
                pthread_join(thr[i], NULL);
            }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    }
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

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, REGION_PERM, RAID1));
    EXPECT_NE((void *)NULL, testRegionDesc);
    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));
    delete testRegionDesc;
    free((void *)testRegionStr);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
