/*
 * fam_resize_reg_test.cpp
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

#define NAME_BUFF_SIZE 255

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;

// Test case 1 - resize region test.
TEST(FamResize, ResizeSuccess) {
    Fam_Region_Descriptor *desc;
    const char *testRegion = get_uniq_str("test1", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    uint64_t regionSize;
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    EXPECT_NO_THROW(my_fam->fam_stat(desc, info));
    regionSize = info->size;

    EXPECT_EQ(regionSize, 8192);

    // Resizing the region
    EXPECT_NO_THROW(my_fam->fam_resize_region(desc, 16384));

    Fam_Region_Descriptor *descCopy;
    EXPECT_NO_THROW(descCopy = my_fam->fam_lookup_region(testRegion));
    EXPECT_NE((void *)NULL, descCopy);

    EXPECT_NO_THROW(my_fam->fam_stat(descCopy, info));
    regionSize = info->size;

    EXPECT_EQ(regionSize, 16384);

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete desc;
    free(info);

    free((void *)testRegion);
}

// Test case 2 - Let allocation exaust memory in the region, then resize the
// region and check allocation succeeds
TEST(FamResize, MultiAllocationMultiResizeSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item[128];
    const char *testRegion = get_uniq_str("test2", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 1048576, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    uint64_t regionSize;
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    EXPECT_NO_THROW(my_fam->fam_stat(desc, info));
    regionSize = info->size;
    EXPECT_EQ(regionSize, 1048576);

    uint32_t iCount = 0;
    uint32_t resizeCount = 2;
    while (resizeCount < 127) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItem, iCount);
        try {
            item[iCount] = my_fam->fam_allocate(itemInfo, 524288, 0777, desc);
            iCount++;
        } catch (Fam_Exception &e) {
            try {
                EXPECT_NO_THROW(my_fam->fam_stat(desc, info));
                regionSize = info->size;
                my_fam->fam_resize_region(desc, info->size * (resizeCount + 2));
            } catch (Fam_Exception &e) {
                break;
            }
            resizeCount++;
            continue;
        }
    }

    for (uint32_t i = 0; i < iCount; i++) {
        if (!item[i]) {
            EXPECT_NO_THROW(my_fam->fam_deallocate(item[i]));
        }
    }

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    free(info);
    delete desc;

    free((void *)testRegion);
}

#if 0
// Test case 3 - The max. no. of resize is 127, it fails beyond that
TEST(FamResize, MultiAllocationMultiResizeFail) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item[128];
    const char *testRegion = get_uniq_str("test2", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 1048576, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    uint64_t regionSize;
    EXPECT_NO_THROW(my_fam->fam_stat(desc,info));
    regionSize  = info->size; 

    EXPECT_EQ(regionSize, 1048576);

    uint32_t iCount = 0;
    uint32_t resizeCount = 2;
    while (resizeCount <= 128) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItem, iCount);
        try {
            item[iCount] = my_fam->fam_allocate(itemInfo, 524288, 0777, desc);
            iCount++;
        } catch (Fam_Exception &e) {
            if(resizeCount < 128) { 
                EXPECT_NO_THROW(my_fam->fam_stat(desc,info));
                regionSize  = info->size; 
                EXPECT_NO_THROW(my_fam->fam_resize_region(
                desc, regionSize * (resizeCount + 2)));
            } else {
                EXPECT_NO_THROW(my_fam->fam_stat(desc,info));
                regionSize  = info->size; 
                EXPECT_THROW(my_fam->fam_resize_region(
                desc, regionSize * (resizeCount + 2)), Fam_Allocator_Exception);
            }
            resizeCount++;
            continue;
        }
    }

    for (uint32_t i = 0; i < iCount; i++) {
        if (!item[i]) {
            EXPECT_NO_THROW(my_fam->fam_deallocate(item[i]));
        }
    }

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete desc;
    free(info);

    free((void *)testRegion);
}
#endif

// Test case 4: Certain PEs allocate data item and rest are responsible
// for resizing the region
TEST(FamResize, MultiPeAllocationResizeSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item1, *item2;
    char testRegion[50];
    const char *firstItem = get_uniq_str("first", my_fam);
    const char *secondItem = get_uniq_str("second", my_fam);

    int *numPEs;
    EXPECT_NO_THROW(numPEs = (int *)my_fam->fam_get_option(strdup("PE_COUNT")));
    EXPECT_NE((void *)NULL, numPEs);

    char *runtime;
    EXPECT_NO_THROW(runtime =
                        (char *)my_fam->fam_get_option(strdup("RUNTIME")));

    EXPECT_NE((void *)NULL, runtime);

    if ((strcmp(runtime, "PMI2") == 0) || (*numPEs < 2)) {
        GTEST_SKIP();
    }

    int *myPE;
    EXPECT_NO_THROW(myPE = (int *)my_fam->fam_get_option(strdup("PE_ID")));

    EXPECT_NE((void *)NULL, myPE);

    int lastOddPE;
    bool isLastOddPE = false;
    if ((*numPEs % 2) != 0) {
        lastOddPE = *numPEs - 1;
        if (*myPE == lastOddPE)
            isLastOddPE = true;
    }

    if (((*myPE % 2) == 0) && !isLastOddPE) {
        sprintf(testRegion, "Test_%d", *myPE);
        EXPECT_NO_THROW(
            desc = my_fam->fam_create_region(testRegion, 1048576, 0777, RAID1));
        EXPECT_NE((void *)NULL, desc);

        EXPECT_NO_THROW(
            item1 = my_fam->fam_allocate(firstItem, 524288, 0777, desc));
        EXPECT_NE((void *)NULL, item1);

        EXPECT_THROW(item2 =
                         my_fam->fam_allocate(secondItem, 524288, 0777, desc),
                     Fam_Exception);
    }

    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    int id;
    if (((*myPE % 2) == 1) && !isLastOddPE) {
        id = *myPE - 1;
        sprintf(testRegion, "Test_%d", id);
        EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(testRegion));
        EXPECT_NE((void *)NULL, desc);

        // Resizing the region
        EXPECT_NO_THROW(my_fam->fam_resize_region(desc, 2097152));
    }

    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    if (((*myPE % 2) == 0) && !isLastOddPE) {
        EXPECT_NO_THROW(
            item2 = my_fam->fam_allocate(secondItem, 524288, 0777, desc));
        EXPECT_NE((void *)NULL, item2);

        EXPECT_NO_THROW(my_fam->fam_deallocate(item1));
        EXPECT_NO_THROW(my_fam->fam_deallocate(item2));

        EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    }

    if (!desc)
        delete desc;

    free((void *)firstItem);
    free((void *)secondItem);
}

// Test case 5 - trying to resize a region which has only read permission
// and expect resize to fail.
TEST(FamResize, ResizeNoPerm) {
    Fam_Region_Descriptor *desc;
    const char *testRegion = get_uniq_str("test1", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0444, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Resizing the region
    EXPECT_THROW(my_fam->fam_resize_region(desc, 16384), Fam_Exception);

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete desc;

    free((void *)testRegion);
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
