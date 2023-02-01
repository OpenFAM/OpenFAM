/*
 * fam_scatter_gather_index_blocking_reg_test.cpp
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

// Test case 1
TEST(FamScatterGatherIndexBlock, ScatterGatherIndexBlockSuccess) {
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

    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    uint64_t indexes[] = {0, 7, 3, 5, 8};

    EXPECT_NO_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 5, indexes, sizeof(int)));

    int *local2 = (int *)malloc(10 * sizeof(int));

    EXPECT_NO_THROW(
        my_fam->fam_gather_blocking(local2, item, 5, indexes, sizeof(int)));

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(local2[i], newLocal[i]);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case 2
TEST(FamScatterGatherIndexBlock, PutGatherIndexBlockSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8388608, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 4194304, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    uint64_t indexes[] = {0, 7, 3, 5, 8};

    EXPECT_NO_THROW(
        my_fam->fam_put_blocking(newLocal, item, 0, 10 * sizeof(int)));

    int *local2 = (int *)malloc(10 * sizeof(int));

    EXPECT_NO_THROW(
        my_fam->fam_gather_blocking(local2, item, 5, indexes, sizeof(int)));

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(local2[i], newLocal[indexes[i]]);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case 3
TEST(FamScatterGatherIndexBlock, ScatterGetIndexBlockSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8388608, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 4194304, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    uint64_t indexes[] = {0, 7, 3, 5, 8};

    EXPECT_NO_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 5, indexes, sizeof(int)));

    int *local2 = (int *)malloc(10 * sizeof(int));

    EXPECT_NO_THROW(
        my_fam->fam_get_blocking(local2, item, 0, 10 * sizeof(int)));

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(local2[indexes[i]], newLocal[i]);
    }

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
