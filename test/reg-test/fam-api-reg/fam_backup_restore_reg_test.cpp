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
#define FILE_MAX_LEN 255
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
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)malloc(sizeof(Fam_Backup_Options));
    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(testRegion));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_lookup(firstItem, testRegion));
    EXPECT_NE((void *)NULL, item);
    char *backupName = (char *)malloc(FILE_MAX_LEN);
    backup_time = time(NULL);
    sprintf(backupName, "%s.%s.%ld", testRegion, firstItem, backup_time);
    void *waitobj = my_fam->fam_backup(item, backupName, backupOptions);
    my_fam->fam_backup_wait(waitobj);
    free((char *)backupName);
    free((void *)testRegion);
    free((void *)firstItem);
    free((Fam_Backup_Options *)backupOptions);
}

TEST(FamBackupRestore, BackupFailure) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)malloc(sizeof(Fam_Backup_Options));
    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(testRegion));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_lookup(firstItem, testRegion));
    EXPECT_NE((void *)NULL, item);
    char *backupName = (char *)malloc(FILE_MAX_LEN);
    try {
        sprintf(backupName, "%s.%s", testRegion, firstItem);
        void *waitobj1 = my_fam->fam_backup(item, backupName, backupOptions);
        my_fam->fam_backup_wait(waitobj1);
    } catch (Fam_Exception &e) {
        cout << "fam_backup: " << e.fam_error_msg() << endl;
    }
    free((char *)backupName);
    free((void *)testRegion);
    free((void *)firstItem);
    free((Fam_Backup_Options *)backupOptions);
}

TEST(FamBackupRestore, RestoreSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    const char *secondItem = get_uniq_str("second", my_fam);
    char *local = strdup("Test message");
    char *backupName = (char *)malloc(FILE_MAX_LEN);
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
        cout << "fam_restore Exception: " << e.fam_error_msg() << endl;
        return;
    }
    // Restoring is complete. Now get the content of restored data item
    char *local2 = (char *)malloc(DATAITEM_SIZE);
    try {
        EXPECT_NO_THROW(
            my_fam->fam_get_blocking(local2, item, 0, DATAITEM_SIZE));
    } catch (Fam_Exception &e) {
        cout << "fam_restore Exception: " << e.fam_error_msg() << endl;
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
    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    const char *secondItem = get_uniq_str("third", my_fam);
    char *backupName = (char *)malloc(FILE_MAX_LEN);
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

TEST(FamBackupRestore, RestoreFailure) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    const char *secondItem = get_uniq_str("fourth", my_fam);
    char *backupName = (char *)malloc(FILE_MAX_LEN);
    sprintf(backupName, "%s.%s.%ld", testRegion, secondItem, backup_time);

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
        cout << "fam_restore Exception: " << e.fam_error_msg() << endl;
    }

    free((void *)testRegion);
    free((void *)firstItem);
    free((void *)secondItem);
}

TEST(FamBackupRestore, ListBackupSuccess) {
    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    cout << "ListBackupSuccess" << endl;
    char *binfo;
    char *backupName = (char *)malloc(FILE_MAX_LEN);

    sprintf(backupName, "%s.%s.%ld", testRegion, firstItem, backup_time);
    try {
        binfo = my_fam->fam_list_backup(backupName);
    } catch (Fam_Exception &e) {
        cout << "fam_list_backup: " << e.fam_error_msg() << endl;
        return;
    }

    cout << "Backup list of " << backupName << " : " << endl;
    cout << binfo << endl;
}

TEST(FamBackupRestore, ListBackupAllSuccess) {
    char *binfo;
    try {
        binfo = my_fam->fam_list_backup(strdup("*"));
    } catch (Fam_Exception &e) {
        cout << "fam_list_backup: " << e.fam_error_msg() << endl;
        return;
    }

    cout << "Backup list: " << endl;
    cout << binfo << endl;
}

TEST(FamBackupRestore, DeleteBackupSuccess) {

    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)malloc(sizeof(Fam_Backup_Options));

    char *backupName = (char *)malloc(FILE_MAX_LEN);
    try {
        sprintf(backupName, "%s.%s.%lu_delete", testRegion, firstItem,
                backup_time);
        void *waitobj = my_fam->fam_backup(item, backupName, backupOptions);

        EXPECT_NO_THROW(my_fam->fam_backup_wait(waitobj));
    } catch (Fam_Exception &e) {
        cout << "fam_backup: " << e.fam_error_msg() << endl;
    }
    char *before_delete;
    try {
        before_delete = my_fam->fam_list_backup(backupName);
    } catch (Fam_Exception &e) {
        cout << "fam_list_backup: " << e.fam_error_msg() << endl;
    }

    cout << "Listing Backup before delete operation: " << endl;
    cout << before_delete << endl;
    try {
        void *delwaitobj = my_fam->fam_delete_backup(backupName);
        my_fam->fam_delete_backup_wait(delwaitobj);
    } catch (Fam_Exception &e) {
        cout << "fam_delete_backup: " << e.fam_error_msg() << endl;
        return;
    }
    char *after_delete;
    after_delete = my_fam->fam_list_backup(backupName);

    cout << "Backup list after delete operation : " << endl;
    cout << after_delete << endl;

    free(backupName);
    free((Fam_Backup_Options *)backupOptions);
}

TEST(FamBackupRestore, DeleteBackupFailure) {

    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("second", my_fam);
    char *backupName = (char *)malloc(FILE_MAX_LEN);
    sprintf(backupName, "%s.%s.%lu", testRegion, firstItem, backup_time);
    try {
        void *delwaitobj = my_fam->fam_delete_backup(backupName);
        my_fam->fam_delete_backup_wait(delwaitobj);
    } catch (Fam_Exception &e) {
        cout << "fam_delete_backup: " << e.fam_error_msg() << endl;
    }
    free(backupName);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);
    my_fam = new fam();
    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, REGION_SIZE, 0777, NULL));
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
