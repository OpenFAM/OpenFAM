/*
 * fam_mm_reg_test.cpp
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

#include "fam_utils.h"

#include <fam/fam.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;

TEST(FamMMTest, FamCreateDestroySuccess) {
    Fam_Region_Descriptor *desc = NULL;
    const char *testRegion = get_uniq_str("mmtest", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 4096, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    free((void *)testRegion);
}

TEST(FamMMTest, FamAllocateDeallocateSuccess) {
    Fam_Region_Descriptor *desc = NULL;
    Fam_Descriptor *item = NULL;
    const char *testRegion = get_uniq_str("mm_test", my_fam);
    const char *dataItem = get_uniq_str("mm_data", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 4096, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_NO_THROW(item = my_fam->fam_allocate(dataItem, 1024, 0444, desc));
    EXPECT_NE((void *)NULL, item);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    delete item;
    free((void *)testRegion);
    free((void *)dataItem);
}

TEST(FamMMTest, FamAllocateDeallocateBignameSuccess) {
    Fam_Region_Descriptor *desc = NULL;
    Fam_Descriptor *item = NULL;
    const char *testRegion = get_uniq_str("mm_test", my_fam);
    const char *dataItem =
        get_uniq_str("testdatatestdatatestdatatestdatate", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 4096, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_NO_THROW(item = my_fam->fam_allocate(dataItem, 1024, 0444, desc));
    EXPECT_NE((void *)NULL, item);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    delete item;
    free((void *)testRegion);
}

TEST(FamMMTest, FamAllocateSameSuccess) {
    Fam_Region_Descriptor *desc = NULL;
    Fam_Descriptor *item = NULL;

    const char *testRegion = get_uniq_str("mm_test", my_fam);
    const char *dataItem = get_uniq_str("mm_data", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 4096, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_NO_THROW(item = my_fam->fam_allocate(dataItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);
    EXPECT_THROW(item = my_fam->fam_allocate(dataItem, 512, 0777, desc),
                 Fam_Allocator_Exception);
    EXPECT_THROW(item = my_fam->fam_allocate(dataItem, 1024, 0444, desc),
                 Fam_Allocator_Exception);
    EXPECT_THROW(item = my_fam->fam_allocate(dataItem, 1024, 0777, desc),
                 Fam_Allocator_Exception);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    delete item;
    free((void *)testRegion);
    free((void *)dataItem);
}

TEST(FamMMTest, FamCheckPermissionSuccess) {
    Fam_Region_Descriptor *desc = NULL;
    Fam_Descriptor *item = NULL;

    const char *testRegion = get_uniq_str("mm_test", my_fam);
    const char *dataItem = get_uniq_str("mm_data", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 4096, 0444, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_NO_THROW(my_fam->fam_change_permissions(desc, 0777));

    EXPECT_NO_THROW(item = my_fam->fam_allocate(dataItem, 1024, 0444, desc));
    EXPECT_NE((void *)NULL, item);
    EXPECT_NO_THROW(my_fam->fam_change_permissions(item, 0333));
    EXPECT_NO_THROW(my_fam->fam_change_permissions(item, 0777));

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    delete item;
    free((void *)testRegion);
    free((void *)dataItem);
}

TEST(FamMMTest, FamCreateSameRegionDestroysSuccess) {
    Fam_Region_Descriptor *desc = NULL;
    const char *testRegion = get_uniq_str("mm_test", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 4096, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    EXPECT_THROW(desc =
                     my_fam->fam_create_region(testRegion, 4096, 0777, RAID1),
                 Fam_Allocator_Exception);
    EXPECT_THROW(desc =
                     my_fam->fam_create_region(testRegion, 1024, 0777, RAID1),
                 Fam_Allocator_Exception);
    EXPECT_THROW(desc =
                     my_fam->fam_create_region(testRegion, 4096, 0444, RAID1),
                 Fam_Allocator_Exception);

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    free((void *)testRegion);
}

TEST(FamMMTest, FamAllocateMultipleItemsSameHeapSuccess) {
    Fam_Region_Descriptor *desc = NULL;
    Fam_Descriptor *ditem[500];
    int num_allocs = 50;

    const char *testRegion = get_uniq_str("mm_test", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 2147483648, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    for (int i = 0; i < num_allocs; i++) {
        string name = "test" + to_string(rand());
        EXPECT_NO_THROW(
            ditem[i] = my_fam->fam_allocate(name.c_str(), 1024, 0444, desc));
        EXPECT_NE((void *)NULL, ditem[i]);
    }
    for (int i = 0; i < num_allocs; i++) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(ditem[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    free((void *)testRegion);
}

TEST(FamMMTest, FamAllocateUnnamedItemSuccess) {
    Fam_Region_Descriptor *desc = NULL;
    Fam_Descriptor *item = NULL;
    const char *testRegion = get_uniq_str("mm_test", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 4096, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_NO_THROW(item = my_fam->fam_allocate(1024, 0444, desc));
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    delete item;
    free((void *)testRegion);
}

TEST(FamMMNegativeTest, FamCreateGreaterBignameRegionFailure) {
    const char *testRegion =
        get_uniq_str("testtesttesttesttesttesttesttesttesttest", my_fam);

    EXPECT_THROW(my_fam->fam_create_region(testRegion, 4096, 0777, RAID1),
                 Fam_Allocator_Exception);
}

TEST(FamMMTest, FamCreateBignameRegionDestroySuccess) {
    Fam_Region_Descriptor *desc = NULL;
    const char *testRegion = get_uniq_str("memory_manager_test_region", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 4096, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
}

TEST(FamMMTest, FamCreateDestroyMultipleSuccess) {
    Fam_Region_Descriptor *desc = NULL;
    string name;
    bool pass = true;
    name = to_string(rand());

    lookup_region(pass, my_fam, desc, name.c_str());
    if (pass == false && desc == NULL)
        create_region(pass, my_fam, desc, name.c_str(), 4096, 0777, RAID1);
    sleep(2);
    if (desc == NULL) {
        lookup_region(pass, my_fam, desc, name.c_str());
        EXPECT_NE((void *)NULL, desc);
    }
    sleep(2);
    if (desc != NULL)
        destroy_region(pass, my_fam, desc);
}

TEST(FamMMTest, FamCreateSameRegionDestroySuccess) {
    Fam_Region_Descriptor *desc = NULL;
    string name;
    bool pass = true;
    srand(100);
    name = to_string(rand());

    lookup_region(pass, my_fam, desc, name.c_str());
    if (pass == false && desc == NULL) {
        create_region(pass, my_fam, desc, name.c_str(), 4096, 0777, RAID1);
    }
    sleep(2);
    if (desc == NULL) {
        lookup_region(pass, my_fam, desc, name.c_str());
        EXPECT_NE((void *)NULL, desc);
    }
    sleep(2);
    if (desc != NULL)
        destroy_region(pass, my_fam, desc);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    delete my_fam;

    return ret;
}
