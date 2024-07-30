/*
 * fam_atomics_reg_test.cpp
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

#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <limits.h>
#include <pthread.h>
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

#define REGION_SIZE (1024 * 1024)
#define REGION_PERM 0777

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t totValue;
    int32_t deltaValue;
} ValueInfo;

void *famAdd(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    int32_t val = addInfo->totValue;
    int32_t del = addInfo->deltaValue;

    while (val != 0) {
        del = (val >= del ? del : val);
        val -= del;
        cout << "item :" << hex << addInfo->item
             << " offset : " << addInfo->offset << " del : " << del << endl;
        EXPECT_NO_THROW(my_fam->fam_add(addInfo->item, addInfo->offset, del));
    }
    return NULL;
}

void *famSubtract(void *arg) {
    ValueInfo *subInfo = (ValueInfo *)arg;
    int32_t val = subInfo->totValue;
    int32_t del = subInfo->deltaValue;

    while (val != 0) {
        del = (val >= del ? del : val);
        val -= del;
        cout << "item :" << hex << subInfo->item
             << " offset : " << subInfo->offset << " del : " << del << endl;
        EXPECT_NO_THROW(
            my_fam->fam_subtract(subInfo->item, subInfo->offset, del));
    }
    return NULL;
}

// Test case 1 - AddSubInt32NonBlock
TEST(FamGeneriAtomics, AddSubInt32NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;

    mode_t test_perm_mode = 0777;
    size_t test_item_size = 1024;

    int32_t baseValue = 100;
    int32_t resValue = 80;
    uint64_t testOffset = (test_item_size - 2 * sizeof(int32_t));
#define NT 2
    pthread_t nThread[NT];
    int32_t result;

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(dataItem, test_item_size,
                                             test_perm_mode, testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    // Set the BaseValue
    // First set value need to be done - TWICE - Bug in fam atomics
    EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, baseValue));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, testOffset));
    EXPECT_EQ(baseValue, result);

    // Spawn two threads
    // Thread 1 - Calls atomic subtract for total value = 30, with delta = 5
    ValueInfo subValue = {item, testOffset, 30, 5};
    pthread_create(&nThread[0], NULL, famSubtract, &subValue);

    // Thread 2 - Calls atomic add for total value = 10, with delta = 5
    ValueInfo addValue = {item, testOffset, 10, 5};
    pthread_create(&nThread[1], NULL, famAdd, &addValue);

    // Resultant - Test the value in fam  100 - 30 + 10 = 80
    for (i = 0; i < NT; i++)
        pthread_join(nThread[i], NULL);

    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, testOffset));
    EXPECT_EQ(resValue, result);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    delete item;
    free((void *)dataItem);
#undef NT
}

// Test case 2 - AddInt32NonBlock
TEST(FamGeneriAtomics, AddInt32NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;

    mode_t test_perm_mode = 0777;
    size_t test_item_size = 1024;

    int32_t baseValue = 100;
    int32_t resValue = 1100;
    uint64_t testOffset = (test_item_size - 2 * sizeof(int32_t));
#define NT 10
    pthread_t nThread[NT];
    int32_t result;

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(dataItem, test_item_size,
                                             test_perm_mode, testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    // Set the BaseValue
    // First set value need to be done - TWICE - Bug in fam atomics
    EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, baseValue));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, testOffset));
    EXPECT_EQ(baseValue, result);

    // Spawn 10 threads
    // Each Thread - Calls atomic add for total value = 10, with delta = 5

    ValueInfo addValue = {item, testOffset, 100, 10};
    for (i = 0; i < NT; i++)
        pthread_create(&nThread[i], NULL, famAdd, &addValue);

    // Resultant - Test the value in fam  100 + (10 * 100) = 1100
    for (i = 0; i < NT; i++)
        pthread_join(nThread[i], NULL);

    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, testOffset));
    EXPECT_EQ(resValue, result);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    delete item;
    free((void *)dataItem);
#undef NT
}

// Test case 3 - SubInt32NonBlock
TEST(FamGeneriAtomics, SubInt32NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i;

    mode_t test_perm_mode = 0777;
    size_t test_item_size = 1024;

    int32_t baseValue = 1100;
    int32_t resValue = 100;
    uint64_t testOffset = (test_item_size - 2 * sizeof(int32_t));
#define NT 10
    pthread_t nThread[NT];
    int32_t result;

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(dataItem, test_item_size,
                                             test_perm_mode, testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    // Set the BaseValue
    // First set value need to be done - TWICE - Bug in fam atomics
    EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, baseValue));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, testOffset));
    EXPECT_EQ(baseValue, result);

    // Spawn 10 threads
    // Each Thread - Calls atomic sub for total value = 100, with delta = 10

    ValueInfo subValue = {item, testOffset, 100, 10};
    for (i = 0; i < NT; i++)
        pthread_create(&nThread[i], NULL, famSubtract, &subValue);

    // Resultant - Test the value in fam  1100 - (10 * 100) = 100
    for (i = 0; i < NT; i++)
        pthread_join(nThread[i], NULL);

    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(item, testOffset));
    EXPECT_EQ(resValue, result);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    delete item;
    free((void *)dataItem);
#undef NT
}

// Test case 3 - SubInt32NonBlock
TEST(FamFetchInt, FamFetchIntInvalidDesc) {
    Fam_Descriptor *item = NULL;
    EXPECT_THROW(my_fam->fam_fetch_int32(item, 0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_int64(item, 0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_int128(item, 0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_uint32(item, 0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_uint64(item, 0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_float(item, 0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_double(item, 0), Fam_Exception);
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

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));
    delete testRegionDesc;
    free((void *)testRegionStr);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
