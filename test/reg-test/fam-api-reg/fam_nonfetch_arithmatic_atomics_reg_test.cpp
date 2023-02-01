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
TEST(FamNonfetchArithAtomicInt32, NonfetchArithAtomicInt32Success) {
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

    int32_t valueInt32 = 0xAAAAAAAA;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueInt32));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueInt32 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_add(item, 0, valueInt32));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueInt32 = my_fam->fam_fetch_int32(item, 0));
    EXPECT_EQ(valueInt32, (int32_t)0xAAAAAAAB);
    valueInt32 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, 0, valueInt32));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueInt32 = my_fam->fam_fetch_int32(item, 0));
    EXPECT_EQ(valueInt32, (int32_t)0xAAAAAAAA);

	EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamNonfetchArithAtomicInt64, NonfetchArithAtomicInt64Success) {
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
	
	// Atomic tests for int64
    int64_t valueInt64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueInt64));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueInt64 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_add(item, 0, valueInt64));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueInt64 = my_fam->fam_fetch_int64(item, 0));
    EXPECT_EQ(valueInt64, (int64_t)0xBBBBBBBBBBBBBBBC);
    valueInt64 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, 0, valueInt64));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueInt64 = my_fam->fam_fetch_int64(item, 0));
    EXPECT_EQ(valueInt64, (int64_t)0xBBBBBBBBBBBBBBBB);

	EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamNonfetchArithAtomicUint32, NonfetchArithAtomicUint32Success) {
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

	// Atomic tests for uint32
    uint32_t valueUint32 = 0xBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueUint32 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_add(item, 0, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, 0));
    EXPECT_EQ(valueUint32, (uint32_t)0xBBBBBBBC);
    valueUint32 = 0;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueUint32 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, 0, valueUint32));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueUint32 = my_fam->fam_fetch_uint32(item, 0));
    EXPECT_EQ(valueUint32, (uint32_t)0xFFFFFFFF);

	EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamNonfetchArithAtomicUint64, NonfetchArithAtomicUint64Success) {
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

    // Atomic tests for uint64
    uint64_t valueUint64 = 0xBBBBBBBBBBBBBBBB;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueUint64 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_add(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, 0));
    EXPECT_EQ(valueUint64, (uint64_t)0xBBBBBBBBBBBBBBBC);
    valueUint64 = 0;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueUint64 = 0x1;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, 0, valueUint64));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueUint64 = my_fam->fam_fetch_uint64(item, 0));
    EXPECT_EQ(valueUint64, (uint64_t)0xFFFFFFFFFFFFFFFF);

	EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamNonfetchArithAtomicFloat, NonfetchArithAtomicFloatSuccess) {
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

    // Atomic tests for float
    float valueFloat = 4.3f;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueFloat));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueFloat = 1.2f;
    EXPECT_NO_THROW(my_fam->fam_add(item, 0, valueFloat));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueFloat = my_fam->fam_fetch_float(item, 0));
    EXPECT_EQ(valueFloat, 5.5f);
    valueFloat = 1.2f;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, 0, valueFloat));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueFloat = my_fam->fam_fetch_float(item, 0));
    EXPECT_EQ(valueFloat, 4.3f);

	EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamNonfetchArithAtomicDouble, NonfetchArithAtomicDoubleSuccess) {
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

    // Atomic tests for double
    double valueDouble = 4.4e+38;
    EXPECT_NO_THROW(my_fam->fam_set(item, 0, valueDouble));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    valueDouble = 1.2e+38;
    EXPECT_NO_THROW(my_fam->fam_add(item, 0, valueDouble));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueDouble = my_fam->fam_fetch_double(item, 0));
    EXPECT_EQ(valueDouble, 5.6e+38);
    valueDouble = 1.2e+38;
    EXPECT_NO_THROW(my_fam->fam_subtract(item, 0, valueDouble));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(valueDouble = my_fam->fam_fetch_double(item, 0));
    EXPECT_EQ(valueDouble, 4.4e+38);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

// Negative test case with invalid descriptor
TEST(FamNonfetchArithAtomicDouble, ArithAtomicInvalidDescriptor) {
    Fam_Descriptor *item = NULL;

    EXPECT_THROW(my_fam->fam_set(item, 0, (int32_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, 0, (int64_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, 0, (int128_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, 0, (uint32_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, 0, (uint64_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, 0, (float)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, 0, (double)0), Fam_Exception);

    EXPECT_THROW(my_fam->fam_add(item, 0, (int32_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_subtract(item, 0, (int32_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_add(item, 0, (int64_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_subtract(item, 0, (int64_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_add(item, 0, (uint32_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_subtract(item, 0, (uint32_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_add(item, 0, (uint64_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_subtract(item, 0, (uint64_t)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_add(item, 0, (float)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_subtract(item, 0, (float)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_add(item, 0, (double)0), Fam_Exception);
    EXPECT_THROW(my_fam->fam_subtract(item, 0, (double)0), Fam_Exception);

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
