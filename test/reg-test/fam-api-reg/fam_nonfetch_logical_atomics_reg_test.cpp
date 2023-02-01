/*
 * fam_nonfetch_logical_atomics_reg_test.cpp
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
TEST(FamNonfetchLogicalAtomicUint32, NonfetchLogicalAtomicUint32Success) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint32_t valueUint32 = 0xAAAAAAAA;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueUint32 = 0x12345678;
    EXPECT_NO_THROW(my_fam->fam_and(item, 0, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, 0));
    EXPECT_EQ(valueUint32, 0x02200228);
    valueUint32 = 0xAAAAAAAA;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    valueUint32 = 0x12345678;
    EXPECT_NO_THROW(my_fam->fam_or(item, 0, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, 0));
    EXPECT_EQ(valueUint32, 0xBABEFEFA);
    valueUint32 = 0xAAAAAAAA;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    valueUint32 = 0x12345678;
    EXPECT_NO_THROW(my_fam->fam_xor(item, 0, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, 0));
    EXPECT_EQ(valueUint32, 0xB89EFCD2);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamNonfetchLogicalAtomicUint64, NonfetchLogicalAtomicUint64Success) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // Logical atomic operations for uint64
    uint64_t valueUint64 = 0xAAAAAAAAAAAAAAAA;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    valueUint64 = 0x1234567890ABCDEF;
    EXPECT_NO_THROW(my_fam->fam_and(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, 0));
    EXPECT_EQ(valueUint64, 0x0220022880AA88AA);
    valueUint64 = 0xAAAAAAAAAAAAAAAA;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    valueUint64 = 0x1234567890ABCDEF;
    EXPECT_NO_THROW(my_fam->fam_or(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, 0));
    EXPECT_EQ(valueUint64, 0xBABEFEFABAABEFEF);
    valueUint64 = 0xAAAAAAAAAAAAAAAA;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    valueUint64 = 0x1234567890ABCDEF;
    EXPECT_NO_THROW(my_fam->fam_xor(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, 0));
    EXPECT_EQ(valueUint64, 0xB89EFCD23A016745);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamNonfetchLogicalAtomicU, NonfetchLogicalAtomicInvalidDescriptor) {
    Fam_Descriptor *item = NULL;

    EXPECT_THROW(my_fam->fam_and(item, 0, (uint32_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_or(item, 0, (uint32_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_xor(item, 0, (uint32_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_and(item, 0, (uint64_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_or(item, 0, (uint64_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_xor(item, 0, (uint64_t)0), Fam_Exception);

    delete item;
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
