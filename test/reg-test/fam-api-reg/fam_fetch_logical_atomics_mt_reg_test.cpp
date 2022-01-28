/*
 * fam_fetch_logical_atomics_reg_test.cpp
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
/* Test Case Description: Tests fetching logical operations for multithreaded
 * model.
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
const char *testRegionStr;
int rc;
#define NUM_THREADS 10
#define REGION_SIZE (1024 * 1024 * NUM_THREADS)
//#define REGION_PERM 0777

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t deltaValue;
} ValueInfo;

// Test case 1 - test for Logical AND,OR and XOR with uint32_t datatype

void *thrd_logical_uint32(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint32_t valueUint32 = 0xAAAAAAAA;
    uint64_t offset = addInfo->tid * sizeof(valueUint32);

    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint32));

    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueUint32 = 0x12345678;

    EXPECT_NO_THROW(valueUint32 =
                        my_fam->fam_fetch_and(item, offset, valueUint32));

    EXPECT_EQ(valueUint32, 0xAAAAAAAA);

    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, offset));

    EXPECT_EQ(valueUint32, 0x02200228);

    valueUint32 = 0xAAAAAAAA;

    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint32));

    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueUint32 = 0x12345678;

    EXPECT_NO_THROW(valueUint32 =
                        my_fam->fam_fetch_or(item, offset, valueUint32));

    EXPECT_EQ(valueUint32, 0xAAAAAAAA);

    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, offset));

    EXPECT_EQ(valueUint32, 0xBABEFEFA);

    valueUint32 = 0xAAAAAAAA;

    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint32));

    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueUint32 = 0x12345678;

    EXPECT_NO_THROW(valueUint32 =
                        my_fam->fam_fetch_xor(item, offset, valueUint32));

    EXPECT_EQ(valueUint32, 0xAAAAAAAA);

    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, offset));

    EXPECT_EQ(valueUint32, 0xB89EFCD2);

    pthread_exit(NULL);
}
TEST(FamLogicalAtomicUint32, LogicalAtomicUint32Success) {
    Fam_Descriptor *item;
    testRegionStr = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(uint32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_logical_uint32,
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

    free((void *)testRegionStr);
    free((void *)firstItem);
}

// Test case 2 - test for Logical AND,OR,XOR with uint64_t datatype

void *thrd_logical_uint64(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    // Logical atomic operations for uint64
    uint64_t valueUint64 = 0xAAAAAAAAAAAAAAAA;
    uint64_t offset = addInfo->tid * sizeof(valueUint64);

    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint64));

    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueUint64 = 0x1234567890ABCDEF;

    EXPECT_NO_THROW(valueUint64 =
                        my_fam->fam_fetch_and(item, offset, valueUint64));

    EXPECT_EQ(valueUint64, 0xAAAAAAAAAAAAAAAA);

    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, offset));

    EXPECT_EQ(valueUint64, 0x0220022880AA88AA);

    valueUint64 = 0xAAAAAAAAAAAAAAAA;

    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint64));

    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueUint64 = 0x1234567890ABCDEF;

    EXPECT_NO_THROW(valueUint64 =
                        my_fam->fam_fetch_or(item, offset, valueUint64));

    EXPECT_EQ(valueUint64, 0xAAAAAAAAAAAAAAAA);

    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, offset));

    EXPECT_EQ(valueUint64, 0xBABEFEFABAABEFEF);

    valueUint64 = 0xAAAAAAAAAAAAAAAA;

    EXPECT_NO_THROW(my_fam->fam_set(item, offset, valueUint64));

    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueUint64 = 0x1234567890ABCDEF;

    EXPECT_NO_THROW(valueUint64 =
                        my_fam->fam_fetch_xor(item, offset, valueUint64));

    EXPECT_EQ(valueUint64, 0xAAAAAAAAAAAAAAAA);

    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, offset));

    EXPECT_EQ(valueUint64, 0xB89EFCD23A016745);
    pthread_exit(NULL);
}
TEST(FamLogicalAtomicUint64, LogicalAtomicUint64Success) {
    Fam_Descriptor *item;
    testRegionStr = get_uniq_str("test", my_fam);
    pthread_t thr[NUM_THREADS];
    const char *firstItem = get_uniq_str("first", my_fam);
    ValueInfo *info;
    int i;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(
                        firstItem, 1024 * NUM_THREADS * sizeof(uint32_t), 0777,
                        testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        info[i] = {item, 0, i, 0};
        if ((rc = pthread_create(&thr[i], NULL, thrd_logical_uint32,
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

    free((void *)testRegionStr);
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

    return ret;
}
