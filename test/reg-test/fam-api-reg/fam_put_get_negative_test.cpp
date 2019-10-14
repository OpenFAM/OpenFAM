/*
 * fam_put_get_negative_test.cpp
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

// Test case 1 - put get negative test.
TEST(FamPutGetT, PutGetFail) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    char *local = strdup("Test message");
    const char *testRegion = get_uniq_str("negtest", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0444, desc));
    EXPECT_NE((void *)NULL, item);

    // allocate local memory to receive 20 elements
    char *local2 = (char *)malloc(20);

    // No write perm
    EXPECT_THROW(my_fam->fam_put_blocking(local, item, 0, 13),
                 Fam_Datapath_Exception);

    EXPECT_NO_THROW(my_fam->fam_put_nonblocking(local, item, 0, 13));
    EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

    // No read permission
    EXPECT_NO_THROW(my_fam->fam_change_permissions(item, 0333));

    EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, item, 0, 13));

    EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local2, item, 0, 13));

    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(my_fam->fam_change_permissions(item, 0777));

    // write to invalid offset bigger than alloc size
    EXPECT_THROW(my_fam->fam_put_blocking(local, item, 2048, 13),
                 Fam_Datapath_Exception);

    EXPECT_NO_THROW(my_fam->fam_put_nonblocking(local, item, 2048, 13));
    EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

    // Read from offset bigger than alloc size
    EXPECT_THROW(my_fam->fam_get_blocking(local2, item, 2048, 13),
                 Fam_Datapath_Exception);

    EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local2, item, 2048, 13));
    EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

    char *local3 = (char *)malloc(8192);

    // Read size bigger than alloc size
    EXPECT_THROW(my_fam->fam_get_blocking(local3, item, 0, 8192),
                 Fam_Datapath_Exception);

    EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local3, item, 0, 8192));
    EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

    // Pass invalid option
    EXPECT_THROW(my_fam->fam_put_blocking(NULL, item, 0, 13),
                 Fam_InvalidOption_Exception);
    EXPECT_THROW(my_fam->fam_put_blocking(local, NULL, 0, 13),
                 Fam_InvalidOption_Exception);
    EXPECT_THROW(my_fam->fam_get_blocking(NULL, item, 0, 13),
                 Fam_InvalidOption_Exception);
    EXPECT_THROW(my_fam->fam_get_blocking(local2, NULL, 0, 13),
                 Fam_InvalidOption_Exception);

    EXPECT_THROW(my_fam->fam_put_nonblocking(NULL, item, 0, 13),
                 Fam_InvalidOption_Exception);
    EXPECT_THROW(my_fam->fam_put_nonblocking(local, NULL, 0, 13),
                 Fam_InvalidOption_Exception);
    EXPECT_THROW(my_fam->fam_get_nonblocking(NULL, item, 0, 13),
                 Fam_InvalidOption_Exception);
    EXPECT_THROW(my_fam->fam_get_nonblocking(local2, NULL, 0, 13),
                 Fam_InvalidOption_Exception);

    // Pass 0 as nbytes
    EXPECT_THROW(my_fam->fam_put_blocking(local, item, 0, 0),
                 Fam_InvalidOption_Exception);
    EXPECT_THROW(my_fam->fam_get_blocking(local, item, 0, 0),
                 Fam_InvalidOption_Exception);
    EXPECT_THROW(my_fam->fam_put_nonblocking(local, item, 0, 0),
                 Fam_InvalidOption_Exception);
    EXPECT_THROW(my_fam->fam_get_nonblocking(local, item, 0, 0),
                 Fam_InvalidOption_Exception);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case 2 - scatter gather index negative test.
TEST(FamPutGetT, ScatterGatherIndexFail) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("sgstridetest", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0444, desc));
    EXPECT_NE((void *)NULL, item);

    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                      26, 27, 28, 29, 30, 31, 32, 33, 34, 35};

    uint64_t indexes[] = {0, 7, 3, 5, 8};

    int *local2 = (int *)malloc(100 * sizeof(int));

    // No write perm
    EXPECT_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 5, indexes, sizeof(int)),
        Fam_Datapath_Exception);

    EXPECT_NO_THROW(my_fam->fam_scatter_nonblocking(newLocal, item, 5, indexes,
                                                    sizeof(int)));
    EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

    // No read permission
    EXPECT_NO_THROW(my_fam->fam_change_permissions(item, 0333));

    EXPECT_NO_THROW(
        my_fam->fam_gather_blocking(local2, item, 5, indexes, sizeof(int)));

    EXPECT_NO_THROW(
        my_fam->fam_gather_nonblocking(local2, item, 5, indexes, sizeof(int)));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(my_fam->fam_change_permissions(item, 0777));

    // Pass invalid option
    EXPECT_THROW(
        my_fam->fam_gather_blocking(NULL, item, 5, indexes, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_gather_nonblocking(NULL, item, 5, indexes, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_scatter_blocking(NULL, item, 5, indexes, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_scatter_nonblocking(NULL, item, 5, indexes, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_gather_blocking(local2, NULL, 5, indexes, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_gather_nonblocking(local2, NULL, 5, indexes, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_scatter_blocking(newLocal, NULL, 5, indexes, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(my_fam->fam_scatter_nonblocking(newLocal, NULL, 5, indexes,
                                                 sizeof(int)),
                 Fam_InvalidOption_Exception);

    // Pass 0 as nelements
    EXPECT_THROW(
        my_fam->fam_gather_blocking(local2, item, 0, indexes, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_gather_nonblocking(local2, item, 0, indexes, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 0, indexes, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(my_fam->fam_scatter_nonblocking(newLocal, item, 0, indexes,
                                                 sizeof(int)),
                 Fam_InvalidOption_Exception);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case 3 - scatter gather stride negative test.
TEST(FamPutGetT, ScatterGatherStrideFail) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("sgstridetest", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0444, desc));
    EXPECT_NE((void *)NULL, item);

    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                      26, 27, 28, 29, 30, 31, 32, 33, 34, 35};

    int *local2 = (int *)malloc(100 * sizeof(int));

    // No write perm
    EXPECT_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 5, 2, 3, sizeof(int)),
        Fam_Datapath_Exception);

    EXPECT_NO_THROW(
        my_fam->fam_scatter_nonblocking(newLocal, item, 5, 2, 3, sizeof(int)));
    EXPECT_THROW(my_fam->fam_quiet(), Fam_Datapath_Exception);

    // No read permission
    EXPECT_NO_THROW(my_fam->fam_change_permissions(item, 0333));

    EXPECT_NO_THROW(
        my_fam->fam_gather_blocking(local2, item, 5, 2, 3, sizeof(int)));

    EXPECT_NO_THROW(
        my_fam->fam_gather_nonblocking(local2, item, 5, 2, 3, sizeof(int)));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_NO_THROW(my_fam->fam_change_permissions(item, 0777));

    // Pass invalid option
    EXPECT_THROW(my_fam->fam_gather_blocking(NULL, item, 5, 2, 3, sizeof(int)),
                 Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_gather_nonblocking(NULL, item, 5, 2, 3, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(my_fam->fam_scatter_blocking(NULL, item, 5, 2, 3, sizeof(int)),
                 Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_scatter_nonblocking(NULL, item, 5, 2, 3, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_gather_blocking(local2, NULL, 5, 2, 3, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_gather_nonblocking(local2, NULL, 5, 2, 3, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_scatter_blocking(newLocal, NULL, 5, 2, 3, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_scatter_nonblocking(newLocal, NULL, 5, 2, 3, sizeof(int)),
        Fam_InvalidOption_Exception);

    // Pass 0 as nelements
    EXPECT_THROW(
        my_fam->fam_gather_blocking(local2, item, 0, 2, 3, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_gather_nonblocking(local2, item, 0, 2, 3, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 0, 2, 3, sizeof(int)),
        Fam_InvalidOption_Exception);

    EXPECT_THROW(
        my_fam->fam_scatter_nonblocking(newLocal, item, 0, 2, 3, sizeof(int)),
        Fam_InvalidOption_Exception);

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
