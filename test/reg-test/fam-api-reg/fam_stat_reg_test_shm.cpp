/*
 * fam_stat_reg_test.cpp
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

TEST(FamStat, FamStatSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_NO_THROW(my_fam->fam_stat(desc, info));
    cout << "Region Info  - Name : " << info->name << " Size: " << std::dec
         << info->size << " Item perm: " << std::oct << info->perm << endl;
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);
    EXPECT_NO_THROW(my_fam->fam_stat(item, info));
    cout << "Item Info  - Name : " << info->name << " Size: " << std::dec
         << info->size << " Item perm: " << std::oct << info->perm << endl;

    free(info);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamStat, FamStatSuccessUnnamedDataItem) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_NO_THROW(my_fam->fam_stat(desc, info));
    cout << "Region Info  - Name : " << info->name << " Size: " << std::dec
         << info->size << " Item perm: " << std::oct << info->perm << endl;
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate("", 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);
    EXPECT_NO_THROW(my_fam->fam_stat(item, info));
    cout << "Item Info  - Name : " << info->name << " Size: " << std::dec
         << info->size << " Item perm: " << std::oct << info->perm << endl;

    free(info);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamStat, FamStatSuccessCompareSize) {
    Fam_Region_Descriptor *desc, *descCopy;
    Fam_Descriptor *item, *itemCopy;
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    uint64_t size, sizeCopy;

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    EXPECT_NO_THROW(descCopy = my_fam->fam_lookup_region(testRegion));
    EXPECT_NE((void *)NULL, descCopy);

    EXPECT_NO_THROW(my_fam->fam_stat(desc, info));
    size = info->size;

    EXPECT_NO_THROW(my_fam->fam_stat(descCopy, info));
    sizeCopy = info->size;

    EXPECT_EQ(size, sizeCopy);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    EXPECT_NO_THROW(itemCopy = my_fam->fam_lookup(firstItem, testRegion));
    EXPECT_NE((void *)NULL, itemCopy);

    EXPECT_NO_THROW(my_fam->fam_stat(item, info));
    size = info->size;

    EXPECT_NO_THROW(my_fam->fam_stat(itemCopy, info));
    sizeCopy = info->size;

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;
    free(info);
    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamStat, FamStatLookup) {
    Fam_Region_Descriptor *desc;
    Fam_Region_Descriptor *desc2 = NULL;
    Fam_Descriptor *item;
    Fam_Descriptor *item2 = NULL;
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    EXPECT_NO_THROW(desc2 = my_fam->fam_lookup_region(testRegion));
    EXPECT_NO_THROW(item2 = my_fam->fam_lookup(firstItem, testRegion));

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Region_Descriptor *)desc2, info));
    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Descriptor *)item2, info));

    free(info);

    delete item2;
    delete desc2;
    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}
TEST(FamStat, FamStatGlobalDescriptor) {
    Fam_Region_Descriptor *desc;
    Fam_Region_Descriptor *desc1 = NULL;
    Fam_Descriptor *item;
    Fam_Descriptor *item1 = NULL;
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    Fam_Global_Descriptor globalDesc = item->get_global_descriptor();

    EXPECT_NO_THROW(item1 = new Fam_Descriptor(globalDesc));
    EXPECT_NO_THROW(desc1 = new Fam_Region_Descriptor(globalDesc));

    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Region_Descriptor *)desc1, info));
    cout << "Region Info - Name : " << info->name << " Size: " << std::dec
         << info->size << " Region perm: " << std::oct << info->perm << endl;
    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Descriptor *)item1, info));
    cout << "Item Info  - Name : " << info->name << " Size: " << std::dec
         << info->size << " Item perm: " << std::oct << info->perm << endl;

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    free(info);

    delete item1;
    delete desc1;
    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamStat, FamStatFailure) {
    Fam_Region_Descriptor *desc;
    Fam_Region_Descriptor *desc1 = NULL;
    Fam_Region_Descriptor *desc2 = NULL;
    Fam_Descriptor *item;
    Fam_Descriptor *item1 = NULL;
    Fam_Descriptor *item2 = NULL;
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    EXPECT_NO_THROW(desc2 = my_fam->fam_lookup_region(testRegion));
    EXPECT_NO_THROW(item2 = my_fam->fam_lookup(firstItem, testRegion));
    Fam_Global_Descriptor globalDesc = item2->get_global_descriptor();

    EXPECT_NO_THROW(item1 = new Fam_Descriptor(globalDesc));
    EXPECT_NO_THROW(desc1 = new Fam_Region_Descriptor(globalDesc));

    cout << "All destroy region, deallocation done from now" << endl;
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    EXPECT_THROW(my_fam->fam_deallocate(item1), Fam_Exception);
    EXPECT_THROW(my_fam->fam_destroy_region(desc1), Fam_Exception);

    EXPECT_THROW(my_fam->fam_deallocate(item2), Fam_Exception);
    EXPECT_THROW(my_fam->fam_destroy_region(desc2), Fam_Exception);

    EXPECT_THROW(my_fam->fam_stat((Fam_Region_Descriptor *)desc, info),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_stat((Fam_Descriptor *)item, info), Fam_Exception);

    EXPECT_THROW(my_fam->fam_stat((Fam_Region_Descriptor *)desc1, info),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_stat((Fam_Descriptor *)item1, info),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_stat((Fam_Region_Descriptor *)desc2, info),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_stat((Fam_Descriptor *)item2, info),
                 Fam_Exception);

    free(info);

    delete item2;
    delete desc2;
    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamStat, FamStatAll) {
    Fam_Region_Descriptor *desc;
    Fam_Region_Descriptor *desc1 = NULL;
    Fam_Region_Descriptor *desc2 = NULL;
    Fam_Descriptor *item;
    Fam_Descriptor *item1 = NULL;
    Fam_Descriptor *item2 = NULL;
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    EXPECT_NO_THROW(desc2 = my_fam->fam_lookup_region(testRegion));
    EXPECT_NO_THROW(item2 = my_fam->fam_lookup(firstItem, testRegion));

    Fam_Global_Descriptor globalDesc = item2->get_global_descriptor();

    EXPECT_NO_THROW(item1 = new Fam_Descriptor(globalDesc));
    EXPECT_NO_THROW(desc1 = new Fam_Region_Descriptor(globalDesc));

    cout << "From lookup" << endl;
    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Region_Descriptor *)desc2, info));
    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Descriptor *)item2, info));

    cout << "From lookup and then global descriptor" << endl;
    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Region_Descriptor *)desc1, info));
    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Descriptor *)item1, info));

    cout << "From lookup" << endl;
    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Region_Descriptor *)desc2, info));
    cout << "Region Info - Name : " << info->name << " Size: " << std::dec
         << info->size << " Region perm: " << std::oct << info->perm << endl;
    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Descriptor *)item2, info));
    cout << "Item Info  - Name : " << info->name << " Size: " << std::dec
         << info->size << " Item perm: " << std::oct << info->perm << endl;

    cout << "From lookup and then global descriptor" << endl;
    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Region_Descriptor *)desc1, info));
    EXPECT_NO_THROW(my_fam->fam_stat((Fam_Descriptor *)item1, info));

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    free(info);

    delete item2;
    delete desc2;
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
    fam_opts.allocator = strdup("NVMM");
    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
