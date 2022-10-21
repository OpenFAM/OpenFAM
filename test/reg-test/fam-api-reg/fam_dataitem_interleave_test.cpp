/*
 * fam_dataitem_interleave_test.cpp
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
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>

#include "common/fam_test_config.h"

#define ITERATION 10
#define FILE_MAX_LEN 255
#define TEST_REGION_SIZE 104857600
#define TEST_ALLOCATE_SIZE 16777216
#define TEST_OFFSET 1048576

using namespace std;
using namespace openfam;

fam *my_fam, *my_fam_sec;
Fam_Options fam_opts, fam_opts_sec;
time_t backup_time = 0;
uint64_t backupInterleaveSize = 0;
// Test case 1 - put get test.
TEST(DataitemInterleaving, PutGetSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;

    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 16777216, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t INTERLEAVE_SIZE = item->get_interleave_size();

    char *local = (char *)malloc(6 * INTERLEAVE_SIZE);
    memset(local, 'a', INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + INTERLEAVE_SIZE), 'b', INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 2 * INTERLEAVE_SIZE), 'c',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 3 * INTERLEAVE_SIZE), 'd',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 4 * INTERLEAVE_SIZE), 'e',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 5 * INTERLEAVE_SIZE), 'f',
           INTERLEAVE_SIZE);

    EXPECT_NO_THROW(
        my_fam->fam_put_blocking(local, item, 8, 6 * INTERLEAVE_SIZE));

    // allocate local memory to receive 20 elements
    char *local2 = (char *)malloc(6 * INTERLEAVE_SIZE);

    EXPECT_NO_THROW(
        my_fam->fam_get_blocking(local2, item, 8, 6 * INTERLEAVE_SIZE));

    EXPECT_STREQ(local, local2);
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleaving, PutGetNonblockSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;

    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 16777216, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t INTERLEAVE_SIZE = item->get_interleave_size();

    char *local = (char *)malloc(6 * INTERLEAVE_SIZE);
    memset(local, 'a', INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + INTERLEAVE_SIZE), 'b', INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 2 * INTERLEAVE_SIZE), 'c',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 3 * INTERLEAVE_SIZE), 'd',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 4 * INTERLEAVE_SIZE), 'e',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 5 * INTERLEAVE_SIZE), 'f',
           INTERLEAVE_SIZE);

    EXPECT_NO_THROW(
        my_fam->fam_put_nonblocking(local, item, 8, 6 * INTERLEAVE_SIZE));

    EXPECT_NO_THROW(my_fam->fam_quiet());
    // allocate local memory to receive 20 elements
    char *local2 = (char *)malloc(6 * INTERLEAVE_SIZE);

    EXPECT_NO_THROW(
        my_fam->fam_get_nonblocking(local2, item, 8, 6 * INTERLEAVE_SIZE));

    EXPECT_NO_THROW(my_fam->fam_quiet());

    EXPECT_STREQ(local, local2);
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleaving, ScatterGatherStrideBlockSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 10485760, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                      26, 27, 28, 29, 30, 31, 32, 33, 34, 35};

    EXPECT_NO_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 5, 2, 4096, sizeof(int)));

    int *local2 = (int *)malloc(10 * sizeof(int));

    EXPECT_NO_THROW(
        my_fam->fam_gather_blocking(local2, item, 5, 2, 4096, sizeof(int)));

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

TEST(DataitemInterleaving, ScatterGatherStrideNonblockSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 10485760, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                      26, 27, 28, 29, 30, 31, 32, 33, 34, 35};

    EXPECT_NO_THROW(my_fam->fam_scatter_nonblocking(newLocal, item, 5, 2, 4096,
                                                    sizeof(int)));

    EXPECT_NO_THROW(my_fam->fam_quiet());

    int *local2 = (int *)malloc(10 * sizeof(int));

    EXPECT_NO_THROW(
        my_fam->fam_gather_nonblocking(local2, item, 5, 2, 4096, sizeof(int)));

    EXPECT_NO_THROW(my_fam->fam_quiet());

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

TEST(DataitemInterleaving, ScatterGatherIndexBlockSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 10485760, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    uint64_t indexes[] = {2,     256,    1024,    2048,    4096,
                          32768, 524288, 1048576, 2097152, 2359296};
    EXPECT_NO_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 10, indexes, sizeof(int)));

    int *local2 = (int *)malloc(10 * sizeof(int));
    EXPECT_NO_THROW(
        my_fam->fam_gather_blocking(local2, item, 10, indexes, sizeof(int)));

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(local2[i], newLocal[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleaving, ScatterGatherIndexNonblockSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 10485760, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    uint64_t indexes[] = {2,     256,    1024,    2048,    4096,
                          32768, 524288, 1048576, 2097152, 2359296};
    EXPECT_NO_THROW(my_fam->fam_scatter_nonblocking(newLocal, item, 10, indexes,
                                                    sizeof(int)));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    int *local2 = (int *)malloc(10 * sizeof(int));
    EXPECT_NO_THROW(
        my_fam->fam_gather_nonblocking(local2, item, 10, indexes, sizeof(int)));
    EXPECT_NO_THROW(my_fam->fam_quiet());

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(local2[i], newLocal[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleaving, Copy) {
    Fam_Region_Descriptor *srcDesc, *destDesc;
    Fam_Descriptor *srcItem;
    Fam_Descriptor *destItem[ITERATION];

    const char *srcRegionName = get_uniq_str("Src_Region", my_fam);
    const char *srcItemName = get_uniq_str("Src_Itemt", my_fam);

    const char *destRegionName = get_uniq_str("Dest_Region", my_fam);
    const char *destItemName = get_uniq_str("Dest_Itemt", my_fam);

    EXPECT_NO_THROW(srcDesc = my_fam->fam_create_region(srcRegionName,
                                                        104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, srcDesc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(
        srcItem = my_fam->fam_allocate(srcItemName, 16777216, 0777, srcDesc));
    EXPECT_NE((void *)NULL, srcItem);

    uint64_t INTERLEAVE_SIZE = srcItem->get_interleave_size();

    char *local = (char *)malloc(6 * INTERLEAVE_SIZE);
    memset(local, 'a', INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + INTERLEAVE_SIZE), 'b', INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 2 * INTERLEAVE_SIZE), 'c',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 3 * INTERLEAVE_SIZE), 'd',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 4 * INTERLEAVE_SIZE), 'e',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 5 * INTERLEAVE_SIZE), 'f',
           INTERLEAVE_SIZE);

    EXPECT_NO_THROW(destDesc = my_fam->fam_create_region(
                        destRegionName, 536870912, 0777, NULL));
    EXPECT_NE((void *)NULL, destDesc);

    for (int i = 0; i < ITERATION; i++) {
        // Allocating data items in the created region
        char itemInfo[255];
        sprintf(itemInfo, "%s_%d", destItemName, i);
        EXPECT_NO_THROW(destItem[i] = my_fam->fam_allocate(itemInfo, 16777216,
                                                           0777, destDesc));
        EXPECT_NE((void *)NULL, destItem[i]);
    }
    EXPECT_NO_THROW(
        my_fam->fam_put_blocking(local, srcItem, 4096, 6 * INTERLEAVE_SIZE));

    void *waitObj[ITERATION];
    for (int i = 0; i < ITERATION; i++) {
        EXPECT_NO_THROW(waitObj[i] =
                            my_fam->fam_copy(srcItem, 4096, destItem[i], 1024,
                                             6 * INTERLEAVE_SIZE));
        EXPECT_NE((void *)NULL, waitObj[i]);
    }

    for (int i = ITERATION - 1; i >= 0; i--) {
        EXPECT_NO_THROW(my_fam->fam_copy_wait(waitObj[i]));
    }

    char *local2 = (char *)malloc(6 * INTERLEAVE_SIZE);

    for (int i = 0; i < ITERATION; i++) {
        EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, destItem[i], 1024,
                                                 6 * INTERLEAVE_SIZE));
        EXPECT_STREQ(local, local2);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(srcItem));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(srcDesc));

    for (int i = 0; i < ITERATION; i++) {
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
TEST(DataitemInterleaving, BackupSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;

    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 16777216, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    backupInterleaveSize = item->get_interleave_size();

    char *local = (char *)malloc(6 * backupInterleaveSize);
    memset(local, 'a', backupInterleaveSize);
    memset((void *)((uint64_t)local + backupInterleaveSize), 'b',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 2 * backupInterleaveSize), 'c',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 3 * backupInterleaveSize), 'd',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 4 * backupInterleaveSize), 'e',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 5 * backupInterleaveSize), 'f',
           backupInterleaveSize);

    EXPECT_NO_THROW(
        my_fam->fam_put_blocking(local, item, 0, 6 * backupInterleaveSize));

    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)malloc(sizeof(Fam_Backup_Options));

    char *backupName = (char *)malloc(FILE_MAX_LEN);
    backup_time = time(NULL);
    sprintf(backupName, "%s.%s.%ld", testRegion, firstItem, backup_time);
    void *waitobj;
    EXPECT_NO_THROW(waitobj =
                        my_fam->fam_backup(item, backupName, backupOptions));
    EXPECT_NE((void *)NULL, waitobj);
    EXPECT_NO_THROW(my_fam->fam_backup_wait(waitobj));

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    free((char *)backupName);
    free((void *)testRegion);
    free((void *)firstItem);
    free((Fam_Backup_Options *)backupOptions);
}

TEST(DataitemInterleaving, RestoreSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    const char *secondItem = get_uniq_str("second", my_fam);
    char *backupName = (char *)malloc(FILE_MAX_LEN);
    sprintf(backupName, "%s.%s.%ld", testRegion, firstItem, backup_time);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 16777216, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    void *waitobj;
    EXPECT_NO_THROW(waitobj = my_fam->fam_restore(backupName, item));
    EXPECT_NE((void *)NULL, waitobj);

    EXPECT_NO_THROW(my_fam->fam_restore_wait(waitobj));

    char *local = (char *)malloc(6 * backupInterleaveSize);
    memset(local, 'a', backupInterleaveSize);
    memset((void *)((uint64_t)local + backupInterleaveSize), 'b',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 2 * backupInterleaveSize), 'c',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 3 * backupInterleaveSize), 'd',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 4 * backupInterleaveSize), 'e',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 5 * backupInterleaveSize), 'f',
           backupInterleaveSize);
    // Restoring is complete. Now get the content of restored data item
    char *local2 = (char *)malloc(6 * backupInterleaveSize);
    EXPECT_NO_THROW(
        my_fam->fam_get_blocking(local2, item, 0, 6 * backupInterleaveSize));

    EXPECT_STREQ(local, local2);
    free(local2);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    free((void *)testRegion);

    free((void *)firstItem);
    free((void *)secondItem);
}

TEST(DataitemInterleaving, CreateDataitemRestoreSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    const char *secondItem = get_uniq_str("third", my_fam);
    char *backupName = (char *)malloc(FILE_MAX_LEN);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    char *local = (char *)malloc(6 * backupInterleaveSize);
    memset(local, 'a', backupInterleaveSize);
    memset((void *)((uint64_t)local + backupInterleaveSize), 'b',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 2 * backupInterleaveSize), 'c',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 3 * backupInterleaveSize), 'd',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 4 * backupInterleaveSize), 'e',
           backupInterleaveSize);
    memset((void *)((uint64_t)local + 5 * backupInterleaveSize), 'f',
           backupInterleaveSize);

    sprintf(backupName, "%s.%s.%ld", testRegion, firstItem, backup_time);
    void *waitobj;

    EXPECT_NO_THROW(waitobj = my_fam->fam_restore(
                        backupName, desc, (char *)secondItem, 0777, &item));

    EXPECT_NE((void *)NULL, waitobj);
    EXPECT_NO_THROW(my_fam->fam_restore_wait(waitobj));
    // Restoring is complete. Now get the content of restored data item
    char *local2 = (char *)malloc(6 * backupInterleaveSize);
    EXPECT_NO_THROW(
        my_fam->fam_get_blocking(local2, item, 0, 6 * backupInterleaveSize));
    EXPECT_STREQ(local, local2);
    free(local);
    free(local2);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    free((void *)testRegion);
    free((void *)secondItem);
    free((void *)firstItem);
}
TEST(DataitemInterleaving, AtomicCallsSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *descriptor;
    int32_t value = 0x444;
    int64_t value1 = 0x1fffffffffffffff;
    uint32_t value2 = 0xefffffff;
    uint64_t value3 = 0x7fffffffffffffff;
    float value4 = 4.3f;
    double value5 = 4.4e+38;
    int128_t value6 = 1;

    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(desc = my_fam->fam_create_region(
                        testRegion, TEST_REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);
    // Allocating data items in the created region
    EXPECT_NO_THROW(descriptor = my_fam->fam_allocate(
                        firstItem, TEST_ALLOCATE_SIZE, 0777, desc));
    EXPECT_NE((void *)NULL, descriptor);
    EXPECT_NO_THROW(value = my_fam->fam_fetch_int32(descriptor, TEST_OFFSET));
    EXPECT_NO_THROW(value1 = my_fam->fam_fetch_int64(descriptor, TEST_OFFSET));
    EXPECT_NO_THROW(value2 = my_fam->fam_fetch_uint32(descriptor, TEST_OFFSET));
    EXPECT_NO_THROW(value3 = my_fam->fam_fetch_uint64(descriptor, TEST_OFFSET));
    EXPECT_NO_THROW(value4 = my_fam->fam_fetch_float(descriptor, TEST_OFFSET));
    EXPECT_NO_THROW(value5 = my_fam->fam_fetch_double(descriptor, TEST_OFFSET));
    EXPECT_NO_THROW(value6 = my_fam->fam_fetch_int128(descriptor, TEST_OFFSET));

    EXPECT_NO_THROW((void)my_fam->fam_swap(descriptor, TEST_OFFSET, value));
    EXPECT_NO_THROW((void)my_fam->fam_swap(descriptor, TEST_OFFSET, value1));
    EXPECT_NO_THROW((void)my_fam->fam_swap(descriptor, TEST_OFFSET, value2));
    EXPECT_NO_THROW((void)my_fam->fam_swap(descriptor, TEST_OFFSET, value3));
    EXPECT_NO_THROW((void)my_fam->fam_swap(descriptor, TEST_OFFSET, value4));
    EXPECT_NO_THROW((void)my_fam->fam_swap(descriptor, TEST_OFFSET, value5));
    EXPECT_NO_THROW(
        (void)my_fam->fam_compare_swap(descriptor, TEST_OFFSET, value, value));
    EXPECT_NO_THROW((void)my_fam->fam_compare_swap(descriptor, TEST_OFFSET,
                                                   value1, value1));
    EXPECT_NO_THROW((void)my_fam->fam_compare_swap(descriptor, TEST_OFFSET,
                                                   value2, value2));
    EXPECT_NO_THROW((void)my_fam->fam_compare_swap(descriptor, TEST_OFFSET,
                                                   value3, value3));
    EXPECT_NO_THROW((void)my_fam->fam_compare_swap(descriptor, TEST_OFFSET,
                                                   value6, value6));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_add(descriptor, TEST_OFFSET, value));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_add(descriptor, TEST_OFFSET, value1));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_add(descriptor, TEST_OFFSET, value2));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_add(descriptor, TEST_OFFSET, value3));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_add(descriptor, TEST_OFFSET, value4));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_add(descriptor, TEST_OFFSET, value5));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_subtract(descriptor, TEST_OFFSET, value));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_subtract(descriptor, TEST_OFFSET, value1));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_subtract(descriptor, TEST_OFFSET, value2));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_subtract(descriptor, TEST_OFFSET, value3));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_subtract(descriptor, TEST_OFFSET, value4));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_subtract(descriptor, TEST_OFFSET, value5));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_min(descriptor, TEST_OFFSET, value));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_min(descriptor, TEST_OFFSET, value1));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_min(descriptor, TEST_OFFSET, value2));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_min(descriptor, TEST_OFFSET, value3));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_min(descriptor, TEST_OFFSET, value4));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_min(descriptor, TEST_OFFSET, value5));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_and(descriptor, TEST_OFFSET, value2));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_and(descriptor, TEST_OFFSET, value3));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_max(descriptor, TEST_OFFSET, value));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_max(descriptor, TEST_OFFSET, value1));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_max(descriptor, TEST_OFFSET, value2));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_max(descriptor, TEST_OFFSET, value3));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_max(descriptor, TEST_OFFSET, value4));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_max(descriptor, TEST_OFFSET, value5));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_or(descriptor, TEST_OFFSET, value2));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_or(descriptor, TEST_OFFSET, value3));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_xor(descriptor, TEST_OFFSET, value2));
    EXPECT_NO_THROW(
        (void)my_fam->fam_fetch_xor(descriptor, TEST_OFFSET, value3));

    EXPECT_NO_THROW(my_fam->fam_deallocate(descriptor));

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete descriptor;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
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
