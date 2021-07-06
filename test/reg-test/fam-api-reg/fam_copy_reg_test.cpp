/*
 * fam_copy_reg_test.cpp
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

#define MESSAGE_SIZE 12

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;

// Test case 1 - fam_copy and fam_copy_wait test (success).
TEST(FamCopy, CopySuccess) {
    Fam_Region_Descriptor *srcDesc, *destDesc;
    Fam_Descriptor *srcItem;
    Fam_Descriptor *destItem[MESSAGE_SIZE];

    char *local = strdup("Test message");
    const char *srcRegionName = get_uniq_str("Src_Region", my_fam);
    const char *srcItemName = get_uniq_str("Src_Itemt", my_fam);

    const char *destRegionName = get_uniq_str("Dest_Region", my_fam);
    const char *destItemName = get_uniq_str("Dest_Itemt", my_fam);

    EXPECT_NO_THROW(
        srcDesc = my_fam->fam_create_region(srcRegionName, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, srcDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(srcItem =
                        my_fam->fam_allocate(srcItemName, 128, 0777, srcDesc));
    EXPECT_NE((void *)NULL, srcItem);

    EXPECT_NO_THROW(destDesc = my_fam->fam_create_region(destRegionName, 8192,
                                                         0777, RAID1));
    EXPECT_NE((void *)NULL, destDesc);

    for (int i = 0; i < MESSAGE_SIZE; i++) {
        // Allocating data items in the created region
        char itemInfo[255];
        sprintf(itemInfo, "%s_%d", destItemName, i);
        EXPECT_NO_THROW(
            destItem[i] = my_fam->fam_allocate(itemInfo, 128, 0777, destDesc));
        EXPECT_NE((void *)NULL, destItem[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_put_blocking(local, srcItem, 0, 13));

    void *waitObj[MESSAGE_SIZE];
    /*
        for (int i = 0; i < MESSAGE_SIZE; i++) {
            dest[i] = new Fam_Descriptor();
        }
    */
    for (int i = 0; i < MESSAGE_SIZE; i++) {
        EXPECT_NO_THROW(
            waitObj[i] = my_fam->fam_copy(srcItem, 0, destItem[i], 0, i + 1));
        EXPECT_NE((void *)NULL, waitObj[i]);
    }

    for (int i = MESSAGE_SIZE - 1; i >= 0; i--) {
        EXPECT_NO_THROW(my_fam->fam_copy_wait(waitObj[i]));
    }

    // allocate local memory to receive 20 elements
    char *tmpLocal = (char *)malloc(20);
    char *local2 = (char *)malloc(20);

    for (int i = 0; i < MESSAGE_SIZE; i++) {
        strncpy(tmpLocal, local, i + 1);
        tmpLocal[i + 1] = '\0';
        EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, destItem[i], 0, 13));
        EXPECT_STREQ(tmpLocal, local2);
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
}

// Test case 2 - (Negative test case) source offset is out of range
TEST(FamCopy, CopyFailSrcOutOfRange) {
    Fam_Region_Descriptor *srcDesc, *destDesc;
    Fam_Descriptor *srcItem;
    Fam_Descriptor *destItem;

    char *local = strdup("Test message");
    const char *srcRegionName = get_uniq_str("Src_Region", my_fam);
    const char *srcItemName = get_uniq_str("Src_Itemt", my_fam);

    const char *destRegionName = get_uniq_str("Dest_Region", my_fam);
    const char *destItemName = get_uniq_str("Dest_Itemt", my_fam);

    EXPECT_NO_THROW(
        srcDesc = my_fam->fam_create_region(srcRegionName, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, srcDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(srcItem =
                        my_fam->fam_allocate(srcItemName, 128, 0777, srcDesc));
    EXPECT_NE((void *)NULL, srcItem);

    EXPECT_NO_THROW(destDesc = my_fam->fam_create_region(destRegionName, 8192,
                                                         0777, RAID1));
    EXPECT_NE((void *)NULL, destDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(
        destItem = my_fam->fam_allocate(destItemName, 128, 0777, destDesc));
    EXPECT_NE((void *)NULL, destItem);

    EXPECT_NO_THROW(my_fam->fam_put_blocking(local, srcItem, 0, 13));

    EXPECT_THROW(my_fam->fam_copy(srcItem, 130, destItem, 0, 13),
                 Fam_Exception);

    EXPECT_NO_THROW(my_fam->fam_deallocate(srcItem));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(srcDesc));
    EXPECT_NO_THROW(my_fam->fam_deallocate(destItem));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(destDesc));

    delete srcItem;
    delete srcDesc;
    delete destItem;
    delete destDesc;

    free((void *)srcRegionName);
    free((void *)srcItemName);
    free((void *)destRegionName);
    free((void *)destItemName);
}

// Test case 3 - (Negative test case) destination offset is out of range
TEST(FamCopy, CopyFailDstOutOfRange) {
    Fam_Region_Descriptor *srcDesc, *destDesc;
    Fam_Descriptor *srcItem;
    Fam_Descriptor *destItem;

    char *local = strdup("Test message");
    const char *srcRegionName = get_uniq_str("Src_Region", my_fam);
    const char *srcItemName = get_uniq_str("Src_Itemt", my_fam);

    const char *destRegionName = get_uniq_str("Dest_Region", my_fam);
    const char *destItemName = get_uniq_str("Dest_Itemt", my_fam);

    EXPECT_NO_THROW(
        srcDesc = my_fam->fam_create_region(srcRegionName, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, srcDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(srcItem =
                        my_fam->fam_allocate(srcItemName, 128, 0777, srcDesc));
    EXPECT_NE((void *)NULL, srcItem);

    EXPECT_NO_THROW(destDesc = my_fam->fam_create_region(destRegionName, 8192,
                                                         0777, RAID1));
    EXPECT_NE((void *)NULL, destDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(
        destItem = my_fam->fam_allocate(destItemName, 128, 0777, destDesc));
    EXPECT_NE((void *)NULL, destItem);

    EXPECT_NO_THROW(my_fam->fam_put_blocking(local, srcItem, 0, 13));

    EXPECT_THROW(my_fam->fam_copy(srcItem, 0, destItem, 130, 13),
                 Fam_Exception);

    EXPECT_NO_THROW(my_fam->fam_deallocate(srcItem));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(srcDesc));
    EXPECT_NO_THROW(my_fam->fam_deallocate(destItem));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(destDesc));

    delete srcItem;
    delete srcDesc;
    delete destItem;
    delete destDesc;

    free((void *)srcRegionName);
    free((void *)srcItemName);
    free((void *)destRegionName);
    free((void *)destItemName);
}

// Test case 4 - (Negative test case) length is out of range
TEST(FamCopy, CopyFailLenOutOfRange) {
    Fam_Region_Descriptor *srcDesc, *destDesc;
    Fam_Descriptor *srcItem;
    Fam_Descriptor *destItem;

    char *local = strdup("Test message");
    const char *srcRegionName = get_uniq_str("Src_Region", my_fam);
    const char *srcItemName = get_uniq_str("Src_Itemt", my_fam);

    const char *destRegionName = get_uniq_str("Dest_Region", my_fam);
    const char *destItemName = get_uniq_str("Dest_Itemt", my_fam);

    EXPECT_NO_THROW(
        srcDesc = my_fam->fam_create_region(srcRegionName, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, srcDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(srcItem =
                        my_fam->fam_allocate(srcItemName, 128, 0777, srcDesc));
    EXPECT_NE((void *)NULL, srcItem);

    EXPECT_NO_THROW(destDesc = my_fam->fam_create_region(destRegionName, 8192,
                                                         0777, RAID1));
    EXPECT_NE((void *)NULL, destDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(
        destItem = my_fam->fam_allocate(destItemName, 128, 0777, destDesc));
    EXPECT_NE((void *)NULL, destItem);

    EXPECT_NO_THROW(my_fam->fam_put_blocking(local, srcItem, 0, 13));

    EXPECT_THROW(my_fam->fam_copy(srcItem, 0, destItem, 126, 13),
                 Fam_Exception);

    EXPECT_NO_THROW(my_fam->fam_deallocate(srcItem));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(srcDesc));
    EXPECT_NO_THROW(my_fam->fam_deallocate(destItem));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(destDesc));

    delete srcItem;
    delete srcDesc;
    delete destItem;
    delete destDesc;

    free((void *)srcRegionName);
    free((void *)srcItemName);
    free((void *)destRegionName);
    free((void *)destItemName);
}

// Test case 5 - fam_copy and fam_copy_wait test (success)
// fam_copy dataitems from the same region.
// For multi memory node, these data items may be placed
// across different memory servers.
TEST(FamCopy, CopyWithinSameRegionSuccess) {
    Fam_Region_Descriptor *srcDesc;
    Fam_Descriptor *srcItem;
    Fam_Descriptor *destItem[MESSAGE_SIZE];

    char *local = strdup("Test message");
    const char *srcRegionName = get_uniq_str("Src_Region", my_fam);
    const char *srcItemName = get_uniq_str("Src_Itemt", my_fam);
    const char *destItemName = get_uniq_str("Dest_Item", my_fam);

    EXPECT_NO_THROW(srcDesc = my_fam->fam_create_region(srcRegionName, 819200,
                                                        0777, RAID1));
    EXPECT_NE((void *)NULL, srcDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(srcItem =
                        my_fam->fam_allocate(srcItemName, 128, 0777, srcDesc));
    EXPECT_NE((void *)NULL, srcItem);

    for (int i = 0; i < MESSAGE_SIZE; i++) {
        // Allocating data items in the created region
        char itemInfo[255];
        sprintf(itemInfo, "%s_%d", destItemName, i);
        EXPECT_NO_THROW(destItem[i] =
                            my_fam->fam_allocate(itemInfo, 128, 0777, srcDesc));
        EXPECT_NE((void *)NULL, destItem[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_put_blocking(local, srcItem, 0, 13));

    void *waitObj[MESSAGE_SIZE];
    /*
        for (int i = 0; i < MESSAGE_SIZE; i++) {
            dest[i] = new Fam_Descriptor();
        }
    */
    for (int i = 0; i < MESSAGE_SIZE; i++) {
        EXPECT_NO_THROW(
            waitObj[i] = my_fam->fam_copy(srcItem, 0, destItem[i], 0, i + 1));
        EXPECT_NE((void *)NULL, waitObj[i]);
    }

    for (int i = MESSAGE_SIZE - 1; i >= 0; i--) {
        EXPECT_NO_THROW(my_fam->fam_copy_wait(waitObj[i]));
    }

    // allocate local memory to receive 20 elements
    char *tmpLocal = (char *)malloc(20);
    char *local2 = (char *)malloc(20);

    for (int i = 0; i < MESSAGE_SIZE; i++) {
        strncpy(tmpLocal, local, i + 1);
        tmpLocal[i + 1] = '\0';
        EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, destItem[i], 0, 13));
        EXPECT_STREQ(tmpLocal, local2);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(srcItem));

    for (int i = 0; i < MESSAGE_SIZE; i++) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(destItem[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_destroy_region(srcDesc));

    delete srcItem;
    delete srcDesc;

    free((void *)srcRegionName);
    free((void *)srcItemName);
    free((void *)destItemName);
}

// Test case 6 - (Negative test case) Invalid descriptor
TEST(FamCopy, CopyInvalidDescriptor) {

    EXPECT_THROW(my_fam->fam_copy(NULL, 0, NULL, 0, 10), Fam_Exception);

    EXPECT_THROW(my_fam->fam_copy_wait(NULL), Fam_Exception);
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
