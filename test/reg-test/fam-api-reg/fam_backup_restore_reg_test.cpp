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

char *regionName = NULL;
char *firstItemName = NULL;

char *backup_restore_file;
fam *my_fam;
Fam_Options fam_opts;
time_t backup_time = 0;

// Test case 1 - put get test.
TEST(FamBackupRestore, BackupSuccess) {
    Fam_Descriptor *item;
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)calloc(1, sizeof(Fam_Backup_Options));

    // Lookup data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_lookup(firstItemName, regionName));
    EXPECT_NE((void *)NULL, item);

    char *backupName = (char *)calloc(1, FILE_MAX_LEN);
    backup_time = time(NULL);
    sprintf(backupName, "%s.%s.%ld", regionName, firstItemName, backup_time);

    void *waitobj = NULL;
    EXPECT_NO_THROW(waitobj =
                        my_fam->fam_backup(item, backupName, backupOptions));
    EXPECT_NO_THROW(my_fam->fam_backup_wait(waitobj));

    free((char *)backupName);
    free((Fam_Backup_Options *)backupOptions);
}

TEST(FamBackupRestore, BackupFailureInvDataItem) {
    const char *backupName = get_uniq_str("test_backup_fail_1", my_fam);
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)calloc(1, sizeof(Fam_Backup_Options));

    EXPECT_THROW(my_fam->fam_backup(NULL, backupName, backupOptions),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_backup_wait(NULL), Fam_Exception);

    free((char *)backupName);
    free((Fam_Backup_Options *)backupOptions);
}

TEST(FamBackupRestore, BackupFailureInvBackupName) {
    Fam_Descriptor *item;
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)calloc(1, sizeof(Fam_Backup_Options));

    // Lookup for existing data item in the created region
    EXPECT_NO_THROW(item = my_fam->fam_lookup(firstItemName, regionName));
    EXPECT_NE((void *)NULL, item);

    EXPECT_THROW(my_fam->fam_backup(item, NULL, backupOptions), Fam_Exception);
    EXPECT_THROW(my_fam->fam_backup_wait(NULL), Fam_Exception);

    free((Fam_Backup_Options *)backupOptions);
}

TEST(FamBackupRestore, RestoreSuccess) {
    const char *backupName = get_uniq_str("test_backup_RES_S", my_fam);
    const char *secondItem = get_uniq_str("second_RES_S", my_fam);
    char *local = strdup("Test message");

    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    Fam_Descriptor *item2;
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)calloc(1, sizeof(Fam_Backup_Options));

    EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(regionName));
    EXPECT_NE((void *)NULL, desc);

    // Lookup data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_lookup(firstItemName, regionName));
    EXPECT_NE((void *)NULL, item);

    // Take a backup
    void *bckwaitobj = NULL;
    EXPECT_NO_THROW(bckwaitobj =
                        my_fam->fam_backup(item, backupName, backupOptions));
    EXPECT_NO_THROW(my_fam->fam_backup_wait(bckwaitobj));

    // Allocating data item2 in the created region
    EXPECT_NO_THROW(item2 = my_fam->fam_allocate(secondItem, 2 * DATAITEM_SIZE,
                                                 0777, desc));
    EXPECT_NE((void *)NULL, item2);

    // Start Restore
    void *waitobj = NULL;
    EXPECT_NO_THROW(waitobj = my_fam->fam_restore(backupName, item2));
    EXPECT_NO_THROW(my_fam->fam_restore_wait(waitobj));

    // Restoring is complete. Now get the content of restored data item
    char *local2 = (char *)calloc(1, DATAITEM_SIZE);
    EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, item2, 0, DATAITEM_SIZE));

    EXPECT_STREQ(local, local2);

    free((char *)backupName);
    free((Fam_Backup_Options *)backupOptions);
    free(local2);
    free((void *)secondItem);
}

TEST(FamBackupRestore, CreateDataitemRestoreSuccess) {
    const char *backupName = get_uniq_str("test_backup_RES_S2", my_fam);
    const char *secondItem = get_uniq_str("second_RES_S2", my_fam);
    char *local = strdup("Test message");

    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    Fam_Descriptor *item2;
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)calloc(1, sizeof(Fam_Backup_Options));

    EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(regionName));
    EXPECT_NE((void *)NULL, desc);

    // Lookup data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_lookup(firstItemName, regionName));
    EXPECT_NE((void *)NULL, item);

    // Take a backup
    void *bckwaitobj = NULL;
    EXPECT_NO_THROW(bckwaitobj =
                        my_fam->fam_backup(item, backupName, backupOptions));
    EXPECT_NO_THROW(my_fam->fam_backup_wait(bckwaitobj));

    void *waitobj = NULL;
    EXPECT_NO_THROW(waitobj = my_fam->fam_restore(
                        backupName, desc, (char *)secondItem, 0777, &item2));

    EXPECT_NE((void *)NULL, waitobj);
    EXPECT_NO_THROW(my_fam->fam_restore_wait(waitobj));
    char *local2 = (char *)calloc(1, DATAITEM_SIZE);

    EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, item2, 0, DATAITEM_SIZE));
    EXPECT_STREQ(local, local2);

    free((char *)backupName);
    free((Fam_Backup_Options *)backupOptions);
    free(local);
    free(local2);
    free((void *)secondItem);
}

TEST(FamBackupRestore, RestoreFailureNonExistentBackup) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *backupName = get_uniq_str("test_backup_Res_Fail1", my_fam);
    const char *secondItem = get_uniq_str("secondItem_Res_Fail1", my_fam);

    EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(regionName));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(
        item = my_fam->fam_allocate(secondItem, 2 * DATAITEM_SIZE, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    // Restore Invalid Backup Name.
    EXPECT_THROW(my_fam->fam_restore(NULL, item), Fam_Exception);

    // Start Restore
    void *waitObj;
    bool exception_raised = 0;
    try {
        waitObj = my_fam->fam_restore(backupName, item);
        my_fam->fam_restore_wait(waitObj);
    } catch (Fam_Exception &e) {
        exception_raised = 1;
    }
    EXPECT_NE(0, exception_raised);
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    free((void *)backupName);
    free((void *)secondItem);
}
TEST(FamBackupRestore, RestoreFailureInsufficientDataSize) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *src;
    Fam_Descriptor *item;
    const char *backupName = get_uniq_str("test_backup_Res_Fail2", my_fam);
    const char *firstItem = get_uniq_str("firstItem_Res_Fail2", my_fam);
    const char *secondItem = get_uniq_str("secondItem_Res_Fail2", my_fam);
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)calloc(1, sizeof(Fam_Backup_Options));

    char *local = (char *)calloc(1, 2 * DATAITEM_SIZE);
    memset(local, 0, 2 * DATAITEM_SIZE);

    EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(regionName));
    EXPECT_NE((void *)NULL, desc);

    // Allocate a data item of two times DATAITEM_SIZE
    EXPECT_NO_THROW(
        src = my_fam->fam_allocate(firstItem, 2 * DATAITEM_SIZE, 0777, desc));

    EXPECT_NE((void *)NULL, src);
    my_fam->fam_put_blocking(local, src, 0, 2 * DATAITEM_SIZE);

    // Create backup of given data item.
    void *waitobj1 = NULL;
    EXPECT_NO_THROW(waitobj1 =
                        my_fam->fam_backup(src, backupName, backupOptions));
    EXPECT_NE((void *)NULL, waitobj1);
    EXPECT_NO_THROW(my_fam->fam_backup_wait(waitobj1));

    // Allocating data item in the created region of size DATAITEM_SIZE/2
    EXPECT_NO_THROW(
        item = my_fam->fam_allocate(secondItem, DATAITEM_SIZE / 2, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    void *waitobj = NULL;
    bool exception_raised = 0;
    try {

        waitobj = my_fam->fam_restore(backupName, item);
        my_fam->fam_restore_wait(waitobj);
    } catch (Fam_Exception &e) {
        exception_raised = 1;
    }
    EXPECT_NE(0, exception_raised);
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_deallocate(src));

    free(local);
    free((void *)firstItem);
    free((void *)secondItem);
    free((Fam_Backup_Options *)backupOptions);
    free((void *)backupName);
}

TEST(FamBackupRestore, ListBackupSuccess) {
    char *binfo;
    const char *backupName = get_uniq_str("test_backup_List_S", my_fam);
    Fam_Descriptor *item;
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)calloc(1, sizeof(Fam_Backup_Options));

    // Lookup data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_lookup(firstItemName, regionName));
    EXPECT_NE((void *)NULL, item);

    // Take a backup
    void *waitobj = NULL;
    EXPECT_NO_THROW(waitobj =
                        my_fam->fam_backup(item, backupName, backupOptions));
    EXPECT_NO_THROW(my_fam->fam_backup_wait(waitobj));

    EXPECT_NO_THROW(binfo = my_fam->fam_list_backup(backupName));
    cout << "Backup list of " << backupName << " : " << endl;
    cout << binfo << endl;

    free((void *)backupName);
    free(backupOptions);
}

TEST(FamBackupRestore, ListBackupAllSuccess) {
    char *binfo;
    const char *backupName = get_uniq_str("test_backup_List_S_all", my_fam);
    Fam_Descriptor *item;
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)calloc(1, sizeof(Fam_Backup_Options));

    // Lookup data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_lookup(firstItemName, regionName));
    EXPECT_NE((void *)NULL, item);

    // Take a backup
    void *waitobj = NULL;
    EXPECT_NO_THROW(waitobj =
                        my_fam->fam_backup(item, backupName, backupOptions));
    EXPECT_NO_THROW(my_fam->fam_backup_wait(waitobj));

    EXPECT_NO_THROW(binfo = my_fam->fam_list_backup(strdup("*")));
    cout << "Backup list: " << endl;
    cout << binfo << endl;

    free((void *)backupName);
    free(backupOptions);
}

TEST(FamBackupRestore, ListBackupFailure) {

    EXPECT_THROW(my_fam->fam_list_backup(NULL), Fam_Exception);
}

TEST(FamBackupRestore, DeleteBackupSuccess) {
    Fam_Descriptor *item;
    const char *backupName = get_uniq_str("test_backup_Del_S", my_fam);
    Fam_Backup_Options *backupOptions =
        (Fam_Backup_Options *)calloc(1, sizeof(Fam_Backup_Options));

    // Lookup data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_lookup(firstItemName, regionName));
    EXPECT_NE((void *)NULL, item);

    // Take a backup
    void *waitobj = NULL;
    EXPECT_NO_THROW(waitobj =
                        my_fam->fam_backup(item, backupName, backupOptions));
    EXPECT_NO_THROW(my_fam->fam_backup_wait(waitobj));

    void *delwaitobj = NULL;
    EXPECT_NO_THROW(delwaitobj = my_fam->fam_delete_backup(backupName));
    EXPECT_NO_THROW(my_fam->fam_delete_backup_wait(delwaitobj));

    free((void *)backupName);
    free((Fam_Backup_Options *)backupOptions);
}

TEST(FamBackupRestore, DeleteBackupFailure) {

    EXPECT_THROW(my_fam->fam_delete_backup(NULL), Fam_Exception);
    EXPECT_THROW(my_fam->fam_delete_backup_wait(NULL), Fam_Exception);
}

int main(int argc, char **argv) {
    int ret;
    Fam_Region_Descriptor *desc = NULL;
    Fam_Descriptor *item = NULL;
    ::testing::InitGoogleTest(&argc, argv);
    my_fam = new fam();
    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    const char *testRegion = get_uniq_str("test_backup", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    regionName = (char *)testRegion;
    firstItemName = (char *)firstItem;

    try {
        desc = my_fam->fam_create_region(testRegion, REGION_SIZE, 0777, NULL);
        // EXPECT_NE((void *)NULL, desc);
        item = my_fam->fam_allocate(firstItem, DATAITEM_SIZE, 0777, desc);
        // EXPECT_NE((void *)NULL, item);
    } catch (Fam_Exception &e) {

        cout << "Create_region Exception/Allocate exception:"
             << e.fam_error_msg() << endl;
    }
    // Allocating data items in the created region

    char *local = strdup("Test message");
    my_fam->fam_put_blocking(local, item, 0, strlen(local));
    Fam_Stat famInfo;
    my_fam->fam_stat(item, &famInfo);

    ret = RUN_ALL_TESTS();

    // allocate local memory to receive 20 elements
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    free(local);
    delete item;
    delete desc;
    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
