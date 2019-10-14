/*
 * fam_compare_swap_atomics_reg_test.cpp
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

// Test case 1 - put get test.
TEST(FamCompareSwapInt32, CompareSwapInt32Success) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    int32_t valueInt32 = 0xAAAAAAAA;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueInt32));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    int32_t oldValueInt32 = 0xAAAAAAAA;
    int32_t newValueInt32 = 0xBBBBBBBB;
    EXPECT_NO_THROW(
        my_fam->fam_compare_swap(item, 0, oldValueInt32, newValueInt32));
    EXPECT_NO_THROW(valueInt32 = my_fam->fam_fetch_int32(item, 0));
    EXPECT_EQ(valueInt32, (int32_t)0xBBBBBBBB);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamCompareSwapInt64, CompareSwapInt64Success) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // Compare atomicas operation for int64
    int64_t valueInt64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueInt64));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    int64_t oldValueInt64 = 0xBBBBBBBBBBBBBBBB;
    int64_t newValueInt64 = 0xCCCCCCCCCCCCCCCC;
    EXPECT_NO_THROW(
        my_fam->fam_compare_swap(item, 0, oldValueInt64, newValueInt64));
    EXPECT_NO_THROW(valueInt64 = my_fam->fam_fetch_int64(item, 0));
    EXPECT_EQ(valueInt64, (int64_t)0xCCCCCCCCCCCCCCCC);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamCompareSwapUint32, CompareSwapUint32Success) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // Compare atomic operations for uint32
    uint32_t valueUint32 = 0xBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    uint32_t oldValueUint32 = 0xBBBBBBBB;
    uint32_t newValueUint32 = 0xCCCCCCCC;
    EXPECT_NO_THROW(
        my_fam->fam_compare_swap(item, 0, oldValueUint32, newValueUint32));
    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, 0));
    EXPECT_EQ(valueUint32, (uint32_t)0xCCCCCCCC);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamCompareSwapUint64, CompareSwapUint64Success) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // Compare atomic operation for uint64
    uint64_t valueUint64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    uint64_t oldValueUint64 = 0xBBBBBBBBBBBBBBBB;
    uint64_t newValueUint64 = 0xCCCCCCCCCCCCCCCC;
    EXPECT_NO_THROW(
        my_fam->fam_compare_swap(item, 0, oldValueUint64, newValueUint64));
    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, 0));
    EXPECT_EQ(valueUint64, (uint64_t)0xCCCCCCCCCCCCCCCC);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamCompareSwapInt128, CompareSwapInt128Success) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

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

    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueInt128.i64[0]));
    EXPECT_NO_THROW(my_fam->fam_set(item, sizeof(int64_t), valueInt128.i64[1]));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueInt128.i64[0] = 0;
    valueInt128.i64[1] = 0;

    oldValueInt128.i64[0] = 0xAAAAAAAABBBBBBBB;
    oldValueInt128.i64[1] = 0xCCCCCCCCDDDDDDDD;

    newValueInt128.i64[0] = 0x2222222233333333;
    newValueInt128.i64[1] = 0x4444444455555555;

    EXPECT_NO_THROW(my_fam->fam_compare_swap(item, 0, oldValueInt128.i128,
                                             newValueInt128.i128));
    EXPECT_NO_THROW(valueInt128.i64[0] = my_fam->fam_fetch_int64(item, 0));
    EXPECT_NO_THROW(valueInt128.i64[1] =
                        my_fam->fam_fetch_int64(item, sizeof(int64_t)));
    EXPECT_EQ(newValueInt128.i64[0], valueInt128.i64[0]);
    EXPECT_EQ(newValueInt128.i64[1], valueInt128.i64[1]);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamCompareSwapNegativeCase, CompareSwapNegativeCaseSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // Failure test case
    uint64_t valueUint64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    uint64_t oldValueUint64 = 0xAAAAAAAAAAAAAAAA;
    uint64_t newValueUint64 = 0xCCCCCCCCCCCCCCCC;
    EXPECT_NO_THROW(
        my_fam->fam_compare_swap(item, 0, oldValueUint64, newValueUint64));
    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, 0));
    EXPECT_EQ(valueUint64, (uint64_t)0xBBBBBBBBBBBBBBBB);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamCompareSwapNegativeCaseInt128, CompareSwapNegativeCaseInt128Success) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

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

    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueInt128.i64[0]));
    EXPECT_NO_THROW(my_fam->fam_set(item, sizeof(int64_t), valueInt128.i64[1]));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueInt128.i64[0] = 0;
    valueInt128.i64[1] = 0;

    oldValueInt128.i64[0] = 0xAAAAAAAABBBBBBBB;
    oldValueInt128.i64[1] = 0xCCCCCCCCDDDDDDDD;

    newValueInt128.i64[0] = 0x2222222233333333;
    newValueInt128.i64[1] = 0x4444444455555555;

    EXPECT_NO_THROW(my_fam->fam_compare_swap(item, 0, oldValueInt128.i128,
                                             newValueInt128.i128));
    EXPECT_NO_THROW(valueInt128.i64[0] = my_fam->fam_fetch_int64(item, 0));
    EXPECT_NO_THROW(valueInt128.i64[1] =
                        my_fam->fam_fetch_int64(item, sizeof(int64_t)));
    EXPECT_NE(newValueInt128.i64[0], valueInt128.i64[0]);
    EXPECT_NE(newValueInt128.i64[1], valueInt128.i64[1]);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
