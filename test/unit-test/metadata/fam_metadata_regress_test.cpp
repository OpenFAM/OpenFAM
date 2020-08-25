/*
 * fam_metadata_regress_test.cpp
 * Copyright (c) 2019-2020 Hewlett Packard Enterprise Development, LP. All
 * rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * See https://spdx.org/licenses/BSD-3-Clause
 *
 */

#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <common/memserver_exception.h>
#include <fam/fam.h>
#include <fam/fam_exception.h>

#include "metadata_service/fam_metadata_service.h"
#include "metadata_service/fam_metadata_service_direct.h"

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;
using namespace metadata;

#define REGION_SIZE 128 * 1024 * 1024 // 128MB
#define MIN_OBJ_SIZE 128

fam *my_fam;
Fam_Options fam_opts;
Fam_Metadata_Service *manager;

// Test case#1 Success cases for region.
TEST(FamMetadata, RegionSuccess) {

    Fam_Region_Metadata node;
    Fam_Region_Metadata *regnode = new Fam_Region_Metadata();
    Fam_Region_Descriptor *desc;
    const char *testRegion = get_uniq_str("test", my_fam);
    uint64_t regionId;

    memset(regnode, 0, sizeof(Fam_Region_Metadata));

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    EXPECT_EQ(true, manager->metadata_find_region(testRegion, node));

    regionId = node.regionId;

    EXPECT_EQ(true, manager->metadata_find_region(regionId, node));

    memcpy(regnode, &node, sizeof(Fam_Region_Metadata));
    regnode->uid = getuid();
    regnode->gid = getgid();

    EXPECT_NO_THROW(manager->metadata_modify_region(testRegion, regnode));

    EXPECT_NO_THROW(manager->metadata_modify_region(regionId, regnode));

    EXPECT_THROW(manager->metadata_insert_region(regionId, testRegion, regnode),
                 Metadata_Service_Exception);

    EXPECT_NO_THROW(manager->metadata_delete_region(regionId));

    EXPECT_THROW(manager->metadata_delete_region(regionId),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_delete_region(testRegion),
                 Metadata_Service_Exception);

    EXPECT_EQ(false, manager->metadata_find_region(testRegion, node));

    EXPECT_EQ(false, manager->metadata_find_region(regionId, node));

    EXPECT_THROW(manager->metadata_modify_region(regionId, regnode),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_modify_region(testRegion, regnode),
                 Metadata_Service_Exception);

    EXPECT_NO_THROW(
        manager->metadata_insert_region(regionId, testRegion, regnode));

    EXPECT_NO_THROW(manager->metadata_find_region(testRegion, node));

    EXPECT_NO_THROW(manager->metadata_delete_region(testRegion));

    EXPECT_EQ(false, manager->metadata_find_region(testRegion, node));

    EXPECT_EQ(false, manager->metadata_find_region(regionId, node));

    EXPECT_NO_THROW(
        manager->metadata_insert_region(regionId, testRegion, regnode));

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete regnode;

    free((void *)testRegion);
}

// Test case#2 Region negative test cases.
TEST(FamMetadata, RegionFail) {

    Fam_Region_Metadata node;
    Fam_Region_Metadata *regnode = new Fam_Region_Metadata();
    Fam_Region_Descriptor *desc;
    const char *testRegion = get_uniq_str("test", my_fam);
    uint64_t regionId;

    memset(regnode, 0, sizeof(Fam_Region_Metadata));

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    EXPECT_NO_THROW(manager->metadata_find_region(testRegion, node));

    EXPECT_THROW(
        my_fam->fam_create_region(testRegion, REGION_SIZE, 0777, RAID1),
        Fam_Exception);

    regionId = node.regionId;

    memcpy(regnode, &node, sizeof(Fam_Region_Metadata));
    regnode->uid = getuid();
    regnode->gid = getgid();

    EXPECT_THROW(manager->metadata_insert_region(regionId, testRegion, regnode),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_insert_region(regionId, "test1234", regnode),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_insert_region(5000, testRegion, regnode),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_modify_region(5000, regnode),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_modify_region("test1234", regnode),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_delete_region("test1234"),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_delete_region(5000),
                 Metadata_Service_Exception);

    EXPECT_NO_THROW(manager->metadata_delete_region(testRegion));

    EXPECT_THROW(manager->metadata_delete_region(testRegion),
                 Metadata_Service_Exception);

    EXPECT_THROW(my_fam->fam_destroy_region(desc), Fam_Exception);

    EXPECT_NO_THROW(
        manager->metadata_insert_region(regionId, testRegion, regnode));

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete regnode;

    free((void *)testRegion);
}

// Test case#1 Success cases for dataitem.
TEST(FamMetadata, DataitemSuccess) {

    Fam_Region_Metadata node;
    Fam_Region_Metadata *regnode = new Fam_Region_Metadata();
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;

    Fam_DataItem_Metadata dinode;
    Fam_DataItem_Metadata *datanode = new Fam_DataItem_Metadata();

    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    uint64_t regionId;

    memset(regnode, 0, sizeof(Fam_Region_Metadata));
    memset(datanode, 0, sizeof(Fam_DataItem_Metadata));

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    EXPECT_EQ(true, manager->metadata_find_region(testRegion, node));

    regionId = node.regionId;

    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    EXPECT_EQ(true,
              manager->metadata_find_dataitem(firstItem, regionId, dinode));

    uint64_t dataitemId = dinode.offset / MIN_OBJ_SIZE;

    memcpy(datanode, &dinode, sizeof(Fam_DataItem_Metadata));

    EXPECT_THROW(
        manager->metadata_insert_dataitem(dataitemId, regionId, datanode),
        Metadata_Service_Exception);
    EXPECT_THROW(
        manager->metadata_insert_dataitem(dataitemId, testRegion, datanode),
        Metadata_Service_Exception);

    EXPECT_EQ(true,
              manager->metadata_find_dataitem(firstItem, regionId, dinode));
    EXPECT_EQ(true,
              manager->metadata_find_dataitem(firstItem, testRegion, dinode));
    EXPECT_EQ(true,
              manager->metadata_find_dataitem(dataitemId, regionId, dinode));
    EXPECT_EQ(true,
              manager->metadata_find_dataitem(dataitemId, testRegion, dinode));

    EXPECT_NO_THROW(
        manager->metadata_modify_dataitem(firstItem, regionId, datanode));
    EXPECT_NO_THROW(
        manager->metadata_modify_dataitem(firstItem, testRegion, datanode));
    EXPECT_NO_THROW(
        manager->metadata_modify_dataitem(dataitemId, regionId, datanode));
    EXPECT_NO_THROW(
        manager->metadata_modify_dataitem(dataitemId, testRegion, datanode));

    EXPECT_NO_THROW(manager->metadata_delete_dataitem(dataitemId, testRegion));

    EXPECT_NO_THROW(manager->metadata_insert_dataitem(dataitemId, regionId,
                                                      datanode, firstItem));
    EXPECT_NO_THROW(manager->metadata_delete_dataitem(dataitemId, regionId));

    EXPECT_NO_THROW(manager->metadata_insert_dataitem(dataitemId, testRegion,
                                                      datanode, firstItem));
    EXPECT_NO_THROW(manager->metadata_delete_dataitem(dataitemId, testRegion));

    EXPECT_NO_THROW(manager->metadata_insert_dataitem(dataitemId, regionId,
                                                      datanode, firstItem));
    EXPECT_NO_THROW(manager->metadata_delete_dataitem(firstItem, regionId));

    EXPECT_NO_THROW(manager->metadata_insert_dataitem(dataitemId, testRegion,
                                                      datanode, firstItem));
    EXPECT_NO_THROW(manager->metadata_delete_dataitem(firstItem, testRegion));

    EXPECT_NO_THROW(manager->metadata_insert_dataitem(dataitemId, testRegion,
                                                      datanode, firstItem));

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete regnode;
    delete datanode;

    free((void *)testRegion);
    free((void *)firstItem);
}

// Test case#1 failure cases for dataitem.
TEST(FamMetadata, DataitemFail) {

    Fam_Region_Metadata node;
    Fam_Region_Metadata *regnode = new Fam_Region_Metadata();
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;

    Fam_DataItem_Metadata dinode;
    Fam_DataItem_Metadata *datanode = new Fam_DataItem_Metadata();

    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    uint64_t regionId;

    memset(regnode, 0, sizeof(Fam_Region_Metadata));
    memset(datanode, 0, sizeof(Fam_DataItem_Metadata));

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    EXPECT_EQ(true, manager->metadata_find_region(testRegion, node));

    regionId = node.regionId;
    memcpy(regnode, &node, sizeof(Fam_Region_Metadata));

    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    EXPECT_THROW(my_fam->fam_allocate(firstItem, 1024, 0777, desc),
                 Fam_Exception);

    EXPECT_EQ(true,
              manager->metadata_find_dataitem(firstItem, regionId, dinode));

    uint64_t dataitemId = dinode.offset / MIN_OBJ_SIZE;

    memcpy(datanode, &dinode, sizeof(Fam_DataItem_Metadata));

    EXPECT_THROW(
        manager->metadata_insert_dataitem(dataitemId, regionId, datanode),
        Metadata_Service_Exception);
    EXPECT_THROW(
        manager->metadata_insert_dataitem(dataitemId, testRegion, datanode),
        Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_insert_dataitem(dataitemId, 5000, datanode),
                 Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_insert_dataitem(
                     dataitemId, "testregionname1234", datanode),
                 Metadata_Service_Exception);

    EXPECT_NO_THROW(manager->metadata_delete_dataitem(dataitemId, testRegion));

    EXPECT_NO_THROW(manager->metadata_insert_dataitem(dataitemId, regionId,
                                                      datanode, firstItem));

    EXPECT_THROW(
        manager->metadata_insert_dataitem(5000, regionId, datanode, firstItem),
        Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_insert_dataitem(5000, testRegion, datanode,
                                                   firstItem),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_insert_dataitem(dataitemId, regionId,
                                                   datanode, "ABCD"),
                 Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_insert_dataitem(dataitemId, testRegion,
                                                   datanode, "ABCD"),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_find_dataitem(firstItem, 5000, dinode),
                 Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_find_dataitem(firstItem, "Test1234", dinode),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_find_dataitem(dataitemId, 5000, dinode),
                 Metadata_Service_Exception);
    EXPECT_THROW(
        manager->metadata_find_dataitem(dataitemId, "Test1234", dinode),
        Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_find_dataitem(5000, 5000, dinode),
                 Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_find_dataitem(5000, "Test1234", dinode),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_find_dataitem("abcd", regionId, dinode),
                 Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_find_dataitem("abcd", testRegion, dinode),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_modify_dataitem("ABCD", regionId, datanode),
                 Metadata_Service_Exception);
    EXPECT_THROW(
        manager->metadata_modify_dataitem("ABCD", testRegion, datanode),
        Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_modify_dataitem(5000, regionId, datanode),
                 Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_modify_dataitem(5000, testRegion, datanode),
                 Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_modify_dataitem(firstItem, 1000, datanode),
                 Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_modify_dataitem(firstItem, "ABCD", datanode),
                 Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_modify_dataitem(dataitemId, 1000, datanode),
                 Metadata_Service_Exception);
    EXPECT_THROW(
        manager->metadata_modify_dataitem(dataitemId, "ABCD", datanode),
        Metadata_Service_Exception);

    EXPECT_THROW(manager->metadata_delete_dataitem("ABCD", regionId),
                 Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_delete_dataitem("ABCD", testRegion),
                 Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_delete_dataitem(5000, regionId),
                 Metadata_Service_Exception);
    EXPECT_THROW(manager->metadata_delete_dataitem(5000, testRegion),
                 Metadata_Service_Exception);

    EXPECT_NO_THROW(manager->metadata_delete_dataitem(dataitemId, testRegion));

    EXPECT_THROW(my_fam->fam_deallocate(item), Fam_Exception);

    EXPECT_NO_THROW(manager->metadata_insert_dataitem(dataitemId, regionId,
                                                      datanode, firstItem));

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    EXPECT_NO_THROW(manager->metadata_delete_region(testRegion));

    EXPECT_THROW(my_fam->fam_destroy_region(desc), Fam_Exception);

    EXPECT_NO_THROW(
        manager->metadata_insert_region(regionId, testRegion, regnode));

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete regnode;
    delete datanode;

    free((void *)testRegion);
    free((void *)firstItem);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    my_fam = new fam();
    manager = new Fam_Metadata_Service_Direct();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    int ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    delete my_fam;
    return ret;
}
