/* fam_context_reg_test.cpp
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
 * operations for single threaded model.
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
#define NUM_ITERATIONS 100
#define NUM_IO_ITERATIONS 5
#define MESSAGE_SIZE 12

// To increase the number of contexts, we need to increase
// the open files limit for the process
#define NUM_CONTEXTS 50

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t deltaValue;
} ValueInfo;

// Test case 1 - FamContextOpenCloseStressTest
TEST(FamContext, FamContextOpenCloseStressTest) {
    fam_context *ctx = NULL;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(ctx = my_fam->fam_context_open());
        EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
    }
}
// Test case 2 - FamContextOpenCloseWithIOOperation
TEST(FamContext, FamContextOpenCloseWithIOOperation) {
    Fam_Region_Descriptor *rd = NULL;
    Fam_Descriptor *descriptor = NULL;
    fam_context *ctx = NULL;
    EXPECT_NO_THROW(ctx = my_fam->fam_context_open());

    // create a 100 MB region with 0777 permissions
    EXPECT_NO_THROW(rd = my_fam->fam_create_region(
                        "myRegion", (uint64_t)10000000, 0777, NULL));
    // use the created region...
    EXPECT_NE((void *)NULL, rd);
    EXPECT_NO_THROW(descriptor = my_fam->fam_allocate(
                        "myItem", (uint64_t)(50 * sizeof(int)), 0600, rd));

    EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)1));
    EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)2));
    EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)3));
    EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)4));
    EXPECT_NO_THROW(my_fam->fam_or(descriptor, 0, (uint32_t)4));
    EXPECT_NO_THROW(ctx->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_quiet());

    // We are done with the operations. Destroy the region and everything in it
    EXPECT_NO_THROW(my_fam->fam_destroy_region(rd));
    EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
}

// Test case 3 - FamContextOpenCloseWithIOOperationStressTest
TEST(FamContext, FamContextOpenCloseWithIOOperationStressTest) {
    Fam_Region_Descriptor *rd = NULL;
    Fam_Descriptor *descriptor = NULL;
    fam_context *ctx = NULL;

    // create a 100 MB region with 0777 permissions
    EXPECT_NO_THROW(rd = my_fam->fam_create_region(
                        "myRegion", (uint64_t)10000000, 0777, NULL));
    // use the created region...

    EXPECT_NE((void *)NULL, rd);
    EXPECT_NO_THROW(descriptor = my_fam->fam_allocate(
                        "myItem", (uint64_t)(50 * sizeof(int)), 0600, rd));

    for (int i = 0; i < NUM_IO_ITERATIONS; i++) {
        EXPECT_NO_THROW(ctx = my_fam->fam_context_open());

        EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)1));
        EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)2));
        EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)3));
        EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)4));
        EXPECT_NO_THROW(my_fam->fam_or(descriptor, 0, (uint32_t)4));
        EXPECT_NO_THROW(ctx->fam_quiet());
        EXPECT_NO_THROW(my_fam->fam_quiet());
        EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
    }
    // We are done with the operations. Destroy the region and everything in
    // it
    EXPECT_NO_THROW(my_fam->fam_destroy_region(rd));
}
// Test case 4 - FamContextSimultaneousOpenCloseStressTest
TEST(FamContext, FamContextSimultaneousOpenCloseStressTest) {

    fam_context *ctx[NUM_CONTEXTS];
    for (int i = 0; i < NUM_CONTEXTS; i++) {
        EXPECT_NO_THROW(ctx[i] = my_fam->fam_context_open());
    }
    for (int i = 1; i < NUM_CONTEXTS; i++) {
        EXPECT_NO_THROW(my_fam->fam_context_close(ctx[i]));
    }
}

// Test case 5- FamContextNegativeTest
TEST(FamContextModel, FamContextNegativeTest) {
    fam_context *ctx = NULL;
    Fam_Descriptor *item = NULL;
    EXPECT_NO_THROW(ctx = my_fam->fam_context_open());
    EXPECT_THROW(ctx->fam_initialize("myApplication", &fam_opts),
                 Fam_Exception);
    EXPECT_THROW(ctx->fam_finalize("myApplication"), Fam_Exception);
    EXPECT_THROW(ctx->fam_abort(-1), Fam_Exception);
    EXPECT_THROW(ctx->fam_barrier_all(), Fam_Exception);
    EXPECT_THROW(ctx->fam_list_options(), Fam_Exception);
    EXPECT_THROW(ctx->fam_get_option(strdup("PE_ID")), Fam_Exception);
    EXPECT_THROW(ctx->fam_lookup_region("myRegion"), Fam_Exception);
    EXPECT_THROW(ctx->fam_lookup("myItem", ("myRegion")), Fam_Exception);
    EXPECT_THROW(ctx->fam_create_region("myRegion", (uint64_t)1000, 0777, NULL),
                 Fam_Exception);
    EXPECT_THROW(ctx->fam_destroy_region(testRegionDesc), Fam_Exception);
    EXPECT_THROW(ctx->fam_resize_region(testRegionDesc, 1024), Fam_Exception);
    EXPECT_THROW(ctx->fam_allocate(1024, 0444, testRegionDesc), Fam_Exception);
    EXPECT_THROW(ctx->fam_allocate("myItem", 1024, 0444, testRegionDesc),
                 Fam_Exception);
    EXPECT_THROW(ctx->fam_deallocate(item), Fam_Exception);
    EXPECT_THROW(ctx->fam_change_permissions(item, 0777), Fam_Exception);
    EXPECT_THROW(ctx->fam_context_open(), Fam_Exception);
    EXPECT_THROW(ctx->fam_context_close(ctx), Fam_Exception);
    EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
}
// Test case 6- FamContextAllIODataPathOpsTest

TEST(FamContextModel, FamContextAllDataPathOpsIOTest) {

    fam_context *ctx = NULL;

    Fam_Descriptor *item = NULL;

    EXPECT_NO_THROW(ctx = my_fam->fam_context_open());

    Fam_Region_Descriptor *desc;

    const char *testRegion = get_uniq_str("test", ctx);

    const char *firstItem = get_uniq_str("first", ctx);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, NULL));

    EXPECT_NE((void *)NULL, desc);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));

    // FAM scatter gather tests with fam_context

    EXPECT_NE((void *)NULL, item);

    // allocate an integer array and initialize it
    int newlocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    char *local = strdup("Test message");

    uint64_t indexes[] = {0, 7, 3, 5, 8};
    EXPECT_NO_THROW(
        ctx->fam_scatter_blocking(newlocal, item, 5, indexes, sizeof(int)));
    int *local2 = (int *)malloc(10 * sizeof(int));

    EXPECT_NO_THROW(
        ctx->fam_gather_blocking(local2, item, 5, indexes, sizeof(int)));

    for (int i = 0; i < 5; i++) {

        EXPECT_EQ(local2[i], newlocal[i]);
    }

    // fam get put with fam_context

    EXPECT_NO_THROW(ctx->fam_put_blocking(local, item, 0, 13));

    // allocate local memory to receive 20 elements
    char *local3 = (char *)malloc(20);

    EXPECT_NO_THROW(ctx->fam_get_blocking(local3, item, 0, 13));

    EXPECT_STREQ(local, local3);

    // fam copy

    Fam_Region_Descriptor *srcDesc, *destDesc;
    Fam_Descriptor *srcItem;
    Fam_Descriptor *destItem[MESSAGE_SIZE];

    const char *srcRegionName = get_uniq_str("Src_Region", my_fam);
    const char *srcItemName = get_uniq_str("Src_Itemt", my_fam);

    const char *destRegionName = get_uniq_str("Dest_Region", my_fam);
    const char *destItemName = get_uniq_str("Dest_Itemt", my_fam);

    EXPECT_NO_THROW(
        srcDesc = my_fam->fam_create_region(srcRegionName, 8192, 0777, NULL));
    EXPECT_NE((void *)NULL, srcDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(srcItem =
                        my_fam->fam_allocate(srcItemName, 128, 0777, srcDesc));
    EXPECT_NE((void *)NULL, srcItem);

    EXPECT_NO_THROW(
        destDesc = my_fam->fam_create_region(destRegionName, 8192, 0777, NULL));
    EXPECT_NE((void *)NULL, destDesc);

    for (int i = 0; i < MESSAGE_SIZE; i++) {
        // Allocating data items in the created region
        char itemInfo[255];
        sprintf(itemInfo, "%s_%d", destItemName, i);
        EXPECT_NO_THROW(
            destItem[i] = my_fam->fam_allocate(itemInfo, 128, 0777, destDesc));
        EXPECT_NE((void *)NULL, destItem[i]);
    }
    EXPECT_NO_THROW(ctx->fam_put_blocking(local, srcItem, 0, 13));

    void *waitObj[MESSAGE_SIZE];
    for (int i = 0; i < MESSAGE_SIZE; i++) {
        EXPECT_NO_THROW(waitObj[i] =
                            ctx->fam_copy(srcItem, 0, destItem[i], 0, i + 1));
        EXPECT_NE((void *)NULL, waitObj[i]);
    }

    for (int i = MESSAGE_SIZE - 1; i >= 0; i--) {
        EXPECT_NO_THROW(ctx->fam_copy_wait(waitObj[i]));
    }

    // allocate local memory to receive 20 elements
    char *tmpLocal = (char *)malloc(20);
    char *local4 = (char *)malloc(20);

    for (int i = 0; i < MESSAGE_SIZE; i++) {
        strncpy(tmpLocal, local, i + 1);
        tmpLocal[i + 1] = '\0';
        EXPECT_NO_THROW(ctx->fam_get_blocking(local4, destItem[i], 0, 13));
        EXPECT_STREQ(tmpLocal, local4);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(srcItem));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(srcDesc));

    for (int i = 0; i < MESSAGE_SIZE; i++) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(destItem[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_destroy_region(destDesc));

    delete srcItem;
    delete srcDesc;
    delete destDesc;

    free((void *)srcRegionName);
    free((void *)srcItemName);
    free((void *)destRegionName);
    free((void *)destItemName);
    // fam copy end
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
    delete item;

    delete desc;

    free((void *)testRegion);

    free((void *)firstItem);
}

// Test case 7- FamContextAllIOAtomicsTest

TEST(FamContextModel, FamContextAllAtomicsIOTest) {

    fam_context *ctx = NULL;

    EXPECT_NO_THROW(ctx = my_fam->fam_context_open());

    Fam_Region_Descriptor *testRegionDesc;

    const char *testRegion = get_uniq_str("test", ctx);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(testRegion, 8192,
                                                               0777, NULL));

    EXPECT_NE((void *)NULL, testRegionDesc);

    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    int64_t baseValue[5] = {0x0, 0x1234, 0x1111222233334321, 0x7ffffffffffffffe,
                            0x7fffffffffffffff};
    int64_t testAddValue[5] = {0x1, 0x1234, 0x1111222233334321, 0x1, 0x1};
    int64_t testExpectedValue[5] = {0x1, 0x2468, 0x2222444466668642,
                                    0x7fffffffffffffff, LONG_MIN};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - 2 * sizeof(int64_t))};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                int64_t result;
                EXPECT_NO_THROW(
                    ctx->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(result = ctx->fam_fetch_add(
                                    item, testOffset[ofs], testAddValue[i]));

                EXPECT_EQ(baseValue[i], result);
                EXPECT_NO_THROW(result = ctx->fam_fetch_subtract(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_EQ(testExpectedValue[i], result);
            }
        }

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                int64_t result;
                EXPECT_NO_THROW(
                    ctx->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());
                EXPECT_NO_THROW(
                    ctx->fam_add(item, testOffset[ofs], testAddValue[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());
                EXPECT_NO_THROW(
                    result = ctx->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(testExpectedValue[i], result);
                EXPECT_NO_THROW(
                    ctx->fam_subtract(item, testOffset[ofs], testAddValue[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());
                EXPECT_NO_THROW(
                    result = ctx->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(baseValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    // fam_min and fam_max
    const char *secondItem = get_uniq_str("second", my_fam);
    // Allocating data items in the created region
    EXPECT_NO_THROW(
        item = my_fam->fam_allocate(secondItem, 1024, 0777, testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    // Atomic min and max operations for uint32
    uint32_t valueUint32 = 0xBBBBBBBB;
    EXPECT_NO_THROW(ctx->fam_set(item, 0, valueUint32));
    EXPECT_NO_THROW(ctx->fam_quiet());
    valueUint32 = 0x11111111;
    EXPECT_NO_THROW(ctx->fam_min(item, 0, valueUint32));
    EXPECT_NO_THROW(ctx->fam_quiet());
    EXPECT_NO_THROW(valueUint32 = ctx->fam_fetch_uint32(item, 0));
    EXPECT_EQ(valueUint32, (uint32_t)0x11111111);
    valueUint32 = 0xCCCCCCCC;
    EXPECT_NO_THROW(ctx->fam_max(item, 0, valueUint32));
    EXPECT_NO_THROW(ctx->fam_quiet());
    EXPECT_NO_THROW(valueUint32 = ctx->fam_fetch_uint32(item, 0));
    EXPECT_EQ(valueUint32, (uint32_t)0xCCCCCCCC);

    // fam_fetch_min and fam_fetch_max

    // Atomic min and max operations for int64
    int64_t valueInt64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(ctx->fam_set(item, 0, valueInt64));
    EXPECT_NO_THROW(ctx->fam_quiet());
    valueInt64 = 0xAAAAAAAAAAAAAAAA;
    EXPECT_NO_THROW(valueInt64 = ctx->fam_fetch_min(item, 0, valueInt64));
    EXPECT_EQ(valueInt64, (int64_t)0xBBBBBBBBBBBBBBBB);
    EXPECT_NO_THROW(valueInt64 = ctx->fam_fetch_uint64(item, 0));
    EXPECT_EQ(valueInt64, (int64_t)0xAAAAAAAAAAAAAAAA);
    valueInt64 = 0xCCCCCCCCCCCCCCCC;
    EXPECT_NO_THROW(valueInt64 = ctx->fam_fetch_max(item, 0, valueInt64));
    EXPECT_EQ(valueInt64, (int64_t)0xAAAAAAAAAAAAAAAA);
    EXPECT_NO_THROW(valueInt64 = ctx->fam_fetch_uint64(item, 0));
    EXPECT_EQ(valueInt64, (int64_t)0xCCCCCCCCCCCCCCCC);

    // fam_add and fam_or

    const char *thirdItem = get_uniq_str("third", my_fam);

    uint64_t operand1Value[5] = {0x0, 0x1234, 0xffff222233334321,
                                 0x7ffffffffffffffe, 0x7fffffffffffffff};
    uint64_t operand2Value[5] = {0x0, 0x1234, 0x1111eeee33334321, 0x0, 0x100};
    uint64_t testAndExpectedValue[5] = {0x0, 0x1234, 0x1111222233334321, 0x0,
                                        0x100};
    uint64_t testOrExpectedValue[5] = {0x0, 0x1234, 0xFFFFEEEE33334321,
                                       0x7ffffffffffffffe, 0x7FFFFFFFFFFFFFFF};
    uint64_t testXorExpectedValue[5] = {0x0, 0x0, 0xEEEECCCC00000000,
                                        0x7FFFFFFFFFFFFFFE, 0x7FFFFFFFFFFFFEFF};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(thirdItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - 2 * sizeof(uint64_t))};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                uint64_t result;
                EXPECT_NO_THROW(
                    ctx->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());

                EXPECT_NO_THROW(
                    ctx->fam_and(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());

                EXPECT_NO_THROW(
                    result = ctx->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testAndExpectedValue[i], result);

                EXPECT_NO_THROW(
                    ctx->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());

                EXPECT_NO_THROW(
                    ctx->fam_or(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());

                EXPECT_NO_THROW(
                    result = ctx->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testOrExpectedValue[i], result);

                EXPECT_NO_THROW(
                    ctx->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());

                EXPECT_NO_THROW(
                    ctx->fam_xor(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());

                EXPECT_NO_THROW(
                    result = ctx->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testXorExpectedValue[i], result);
            }
        }

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                uint64_t result;
                EXPECT_NO_THROW(
                    ctx->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());

                EXPECT_NO_THROW(result = ctx->fam_fetch_and(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);

                EXPECT_NO_THROW(
                    result = ctx->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testAndExpectedValue[i], result);

                EXPECT_NO_THROW(
                    ctx->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());

                EXPECT_NO_THROW(result = ctx->fam_fetch_or(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);

                EXPECT_NO_THROW(
                    result = ctx->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testOrExpectedValue[i], result);

                EXPECT_NO_THROW(
                    ctx->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());

                EXPECT_NO_THROW(result = ctx->fam_fetch_xor(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);

                EXPECT_NO_THROW(
                    result = ctx->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testXorExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }

    // fam_swap

    uint64_t oldValue[5] = {0x0, 0x1234, 0x1111222233334444, 0x7ffffffffffffffe,
                            0x7fffffffffffffff};
    uint64_t newValue[5] = {0x1, 0x4321, 0x2111222233334321, 0x8000000000000000,
                            0xefffffffffffffff};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - 2 * sizeof(uint64_t))};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {

                uint64_t result;
                EXPECT_NO_THROW(
                    ctx->fam_set(item, testOffset[ofs], oldValue[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());
                EXPECT_NO_THROW(
                    result = ctx->fam_swap(item, testOffset[ofs], newValue[i]));
                EXPECT_EQ(oldValue[i], result);
                EXPECT_NO_THROW(
                    result = ctx->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(newValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }

    // comapare and swap
    const char *fourthItem = get_uniq_str("fourth", my_fam);
    int64_t oldValue_cas[5] = {0x0, 0x1234, 0x1111222233334444,
                               0x7ffffffffffffffe, 0x7fffffffffffffff};
    int64_t cmpValue[5] = {0x0, 0x1234, 0x11112222ffff4321, 0x7ffffffffffffffe,
                           0x0000000000000000};
    int64_t newValue_cas[5] = {0x1, 0x4321, 0x2111222233334321,
                               0x0000000000000000, 0x1111111111111111};
    int64_t expValue[5] = {0x1, 0x4321, 0x1111222233334444, 0x0000000000000000,
                           0x7fffffffffffffff};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(fourthItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - 2 * sizeof(int64_t))};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {

                int64_t result;
                EXPECT_NO_THROW(
                    ctx->fam_set(item, testOffset[ofs], oldValue_cas[i]));
                EXPECT_NO_THROW(ctx->fam_quiet());
                EXPECT_NO_THROW(ctx->fam_compare_swap(
                    item, testOffset[ofs], cmpValue[i], newValue_cas[i]));
                EXPECT_NO_THROW(
                    result = ctx->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(expValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }

    free((void *)fourthItem);

    free((void *)thirdItem);
    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));

    free((void *)dataItem);
    EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
}
int main(int argc, char **argv) {
    int ret = 0;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
