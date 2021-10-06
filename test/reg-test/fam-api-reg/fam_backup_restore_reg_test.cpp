/*
 * fam_backup_restore_reg_test.cpp
 * Copyright (c) 2021 Hewlett Packard Enterprise Development, LP. All
 * rights reserved. Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following conditions
 * are met:
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
#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common/fam_test_config.h"

#define REGION_SIZE 2147483648
#define DATAITEM_SIZE 1048576
using namespace std;
using namespace openfam;
char *backup_restore_file;
fam *my_fam;
Fam_Options fam_opts;
Fam_Region_Descriptor *desc;
Fam_Descriptor *item;
time_t backup_time = 0;

// Test case 1 - put get test.
TEST(FamBackupRestore, BackupSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(testRegion));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_lookup(firstItem, testRegion));
    EXPECT_NE((void *)NULL, item);
    char *backupName = (char *)malloc(100);
    try {
        backup_time = time(NULL);
        sprintf(backupName, "%s.%s.%ld", testRegion, firstItem, backup_time);
        void *waitobj = my_fam->fam_backup(item, backupName);
        EXPECT_NO_THROW(my_fam->fam_backup_wait(waitobj));
    } catch (Fam_Exception &e) {
        cout << "fam_backup: " << e.fam_error_msg() << endl;
    }
    free((char *)backupName);
    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(FamBackupRestore, RestoreSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    const char *secondItem = get_uniq_str("second", my_fam);
    char *local = strdup("Test message");
    char *backupName = (char *)malloc(100);
    sprintf(backupName, "%s.%s.%ld", testRegion, firstItem, backup_time);

    EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(testRegion));
    EXPECT_NE((void *)NULL, desc);

    try {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item = my_fam->fam_allocate(
                            secondItem, 2 * DATAITEM_SIZE, 0777, desc));
        EXPECT_NE((void *)NULL, item);

        // Start Restore
        void *waitobj = my_fam->fam_restore(backupName, item);
        my_fam->fam_restore_wait(waitobj);

    } catch (Fam_Exception &e) {
        cout << "fam_restore: " << e.fam_error_msg() << endl;
        return;
    }
    // Restoring is complete. Now get the content of restored data item
    char *local2 = (char *)malloc(DATAITEM_SIZE);
    try {
        EXPECT_NO_THROW(
            my_fam->fam_get_blocking(local2, item, 0, DATAITEM_SIZE));
    } catch (Fam_Exception &e) {
        cout << "fam_restore: " << e.fam_error_msg() << endl;
    }

    EXPECT_STREQ(local, local2);
    free(local2);

    free((void *)testRegion);
    free((void *)firstItem);
    free((void *)secondItem);
}

TEST(FamBackupRestore, CreateDataitemRestoreSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    const char *secondItem = get_uniq_str("third", my_fam);
    char *backupName = (char *)malloc(100);
    char *local = strdup("Test message");

    EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(testRegion));
    EXPECT_NE((void *)NULL, desc);

    sprintf(backupName, "%s.%s.%ld", testRegion, firstItem, backup_time);
    try {
        void *waitobj = my_fam->fam_restore(backupName, desc,
                                            (char *)secondItem, 0777, &item);

        my_fam->fam_restore_wait(waitobj);
    } catch (Fam_Exception &e) {
        cout << "fam_restore: " << e.fam_error_msg() << endl;
        return;
    }
    char *local2 = (char *)malloc(DATAITEM_SIZE);
    try {
        my_fam->fam_get_blocking(local2, item, 0, DATAITEM_SIZE);
    } catch (Fam_Exception &e) {
        cout << "fam_get_blocking: " << e.fam_error_msg() << endl;
    }
    EXPECT_STREQ(local, local2);
    free(local);
    free(local2);
    free((void *)testRegion);
    free((void *)secondItem);
    free((void *)firstItem);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);
    my_fam = new fam();
    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(
        item = my_fam->fam_allocate(firstItem, DATAITEM_SIZE, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    char *local = strdup("Test message");

    my_fam->fam_put_blocking(local, item, 0, strlen(local));
    Fam_Stat famInfo;
    my_fam->fam_stat(item, &famInfo);
    ret = RUN_ALL_TESTS();
    free(local);
    // allocate local memory to receive 20 elements
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;
    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}