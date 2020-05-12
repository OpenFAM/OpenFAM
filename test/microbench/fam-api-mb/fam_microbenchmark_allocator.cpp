/*
 * fam_microbenchmark.cpp
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
#include "rpc/fam_rpc_client.h"
#define BIG_REGION_SIZE 21474836480
#define BLOCK_SIZE 1048576
#define RESIZE_REGION_SIZE 1048576
#define NAME_BUFF_SIZE 255
#define DATA_ITEM_SIZE 1048576

using namespace std;
using namespace openfam;

int *myPE;
int NUM_MM_ITERATIONS;
Fam_Rpc_Client *rpc;
fam *my_fam;
Fam_Options fam_opts;

#if !defined(SHM) && defined(MEMSERVER_PROFILE)
#define RESET_PROFILE()                                                        \
    {                                                                          \
        rpc->reset_profile();                                                  \
        my_fam->fam_barrier_all();                                             \
    }

#define GENERATE_PROFILE()                                                     \
    {                                                                          \
        if (*myPE == 0)                                                        \
            rpc->generate_profile();                                           \
        my_fam->fam_barrier_all();                                             \
    }
#else
#define RESET_PROFILE()
#define GENERATE_PROFILE()
#endif

// Test case -  create and destroy region multiple times.
TEST(FamCreateDestroyRegion, FamCreateDestroyRegionMultiple1) {
    Fam_Region_Descriptor **descLocal =
        new Fam_Region_Descriptor *[NUM_MM_ITERATIONS];
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);

    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char regionInfo[NAME_BUFF_SIZE];
        sprintf(regionInfo, "%s_%d", testRegionLocal, i);
        EXPECT_NO_THROW(descLocal[i] = my_fam->fam_create_region(
                            regionInfo, BLOCK_SIZE, 0777, RAID1));
        EXPECT_NE((void *)NULL, descLocal[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    free((void *)testRegionLocal);
}

// Test case -  create and destroy region multiple times.
TEST(FamCreateDestroyRegion, FamCreateRegionMultiple2) {
    Fam_Region_Descriptor **descLocal =
        new Fam_Region_Descriptor *[NUM_MM_ITERATIONS];
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();

    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char regionInfo[NAME_BUFF_SIZE];
        sprintf(regionInfo, "%s_%d", testRegionLocal, i);
        EXPECT_NO_THROW(descLocal[i] = my_fam->fam_create_region(
                            regionInfo, BLOCK_SIZE, 0777, RAID1));
        EXPECT_NE((void *)NULL, descLocal[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());
    free((void *)testRegionLocal);
}

TEST(FamCreateDestroyRegion, FamDestroyRegionMultiple2) {
    Fam_Region_Descriptor **descLocal =
        new Fam_Region_Descriptor *[NUM_MM_ITERATIONS];
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);

    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char regionInfo[NAME_BUFF_SIZE];
        sprintf(regionInfo, "%s_%d", testRegionLocal, i);
        EXPECT_NO_THROW(descLocal[i] = my_fam->fam_lookup_region(regionInfo));
        EXPECT_NE((void *)NULL, descLocal[i]);
    }

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();

    for (int i = 0; i < NUM_MM_ITERATIONS; i++)
        EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal[i]));

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());
    free((void *)testRegionLocal);
}

// Test case -  change permission.
TEST(FamCreateDestroyRegion, FamCreateDestroyRegionChngPermissions) {
    Fam_Region_Descriptor **descLocal =
        new Fam_Region_Descriptor *[NUM_MM_ITERATIONS];
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    mode_t perm;
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char regionInfo[NAME_BUFF_SIZE];
        perm = (mode_t)i;
        sprintf(regionInfo, "%s_%d", testRegionLocal, i);
        EXPECT_NO_THROW(descLocal[i] = my_fam->fam_create_region(
                            regionInfo, BLOCK_SIZE, perm, RAID1));
        EXPECT_NE((void *)NULL, descLocal[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++)
        EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal[i]));
    free((void *)testRegionLocal);
}

// Test case -  fam_allocate/deallocate test.
TEST(FamAllocateDellocate, FamAllocateDellocateMultiple1) {
    Fam_Descriptor **itemLocal = new Fam_Descriptor *[NUM_MM_ITERATIONS];
    Fam_Region_Descriptor *descLocal;
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);

    EXPECT_NO_THROW(descLocal = my_fam->fam_create_region(
                        testRegionLocal, BIG_REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, descLocal);

    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItemLocal, i);
        // Allocating data items
        EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_allocate(
                            itemInfo, DATA_ITEM_SIZE, 0777, descLocal));
        EXPECT_NE((void *)NULL, itemLocal[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(itemLocal[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal));
    free((void *)testRegionLocal);
    free((void *)firstItemLocal);
}

// Test case -  fam_allocate/deallocate test.
TEST(FamAllocateDeallocate, FamAllocateMultiple2) {
    Fam_Region_Descriptor *descLocal;
    Fam_Descriptor *item1;
    Fam_Descriptor **itemLocal = new Fam_Descriptor *[NUM_MM_ITERATIONS];
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    EXPECT_NO_THROW(descLocal = my_fam->fam_create_region(
                        testRegionLocal, BIG_REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, descLocal);

    EXPECT_NO_THROW(item1 = my_fam->fam_allocate(firstItemLocal, DATA_ITEM_SIZE,
                                                 0777, descLocal));
    EXPECT_NE((void *)NULL, item1);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItemLocal, i);
        // Allocating data items
        EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_allocate(
                            itemInfo, DATA_ITEM_SIZE, 0777, descLocal));
        EXPECT_NE((void *)NULL, itemLocal[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());
    free((void *)testRegionLocal);
    free((void *)firstItemLocal);
}

TEST(FamAllocateDeallocate, FamAllocateSingleRegion) {
    Fam_Region_Descriptor *descLocal;
    Fam_Descriptor *item1;
    Fam_Descriptor **itemLocal = new Fam_Descriptor *[NUM_MM_ITERATIONS];
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    if (*myPE == 0) {
        EXPECT_NO_THROW(descLocal = my_fam->fam_create_region(
                            "test", BIG_REGION_SIZE, 0777, RAID1));
        EXPECT_NE((void *)NULL, descLocal);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    if (*myPE != 0) {
        EXPECT_NO_THROW(descLocal = my_fam->fam_lookup_region("test"));
        EXPECT_NE((void *)NULL, descLocal);
    }
    EXPECT_NO_THROW(item1 = my_fam->fam_allocate(firstItemLocal, DATA_ITEM_SIZE,
                                                 0777, descLocal));
    EXPECT_NE((void *)NULL, item1);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItemLocal, i);
        // Allocating data items
        EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_allocate(
                            itemInfo, DATA_ITEM_SIZE, 0777, descLocal));
        EXPECT_NE((void *)NULL, itemLocal[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());
    free((void *)firstItemLocal);
}

TEST(FamAllocateDeallocate, FamDeallocateMultiple2) {
    Fam_Descriptor **itemLocal = new Fam_Descriptor *[NUM_MM_ITERATIONS];
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    Fam_Descriptor *item1;

    item1 = my_fam->fam_lookup(firstItemLocal, testRegionLocal);
    EXPECT_NE((void *)NULL, item1);
    EXPECT_NO_THROW(my_fam->fam_deallocate(item1));

    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItemLocal, i);
        // Allocating data items
        EXPECT_NO_THROW(itemLocal[i] =
                            my_fam->fam_lookup(itemInfo, testRegionLocal));
        EXPECT_NE((void *)NULL, itemLocal[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(itemLocal[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());
    free((void *)testRegionLocal);
    free((void *)firstItemLocal);
}

TEST(FamAllocateDeallocate, FamDeallocateSingleRegion) {
    Fam_Descriptor **itemLocal = new Fam_Descriptor *[NUM_MM_ITERATIONS];
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);

    Fam_Descriptor *item1;
    item1 = my_fam->fam_lookup(firstItemLocal, "test");
    EXPECT_NE((void *)NULL, item1);
    EXPECT_NO_THROW(my_fam->fam_deallocate(item1));

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItemLocal, i);
        // Allocating data items
        EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_lookup(itemInfo, "test"));
        EXPECT_NE((void *)NULL, itemLocal[i]);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(itemLocal[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());
    free((void *)firstItemLocal);
}

// Test case -  DataItem Change Permission.
TEST(FamChangePermissions, DataItemChangePermission) {
    Fam_Region_Descriptor *descLocal;
    Fam_Descriptor *itemLocal;
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    EXPECT_NO_THROW(descLocal = my_fam->fam_create_region(
                        testRegionLocal, BIG_REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, descLocal);

    // Allocating data items
    EXPECT_NO_THROW(itemLocal = my_fam->fam_allocate(
                        firstItemLocal, DATA_ITEM_SIZE, 0777, descLocal));
    EXPECT_NE((void *)NULL, itemLocal);
    mode_t perm;
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        perm = (mode_t)i;
        my_fam->fam_change_permissions(itemLocal, perm);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();

    // EXPECT_NO_THROW(rpc->generate_profile());
    EXPECT_NO_THROW(my_fam->fam_deallocate(itemLocal));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal));

    free((void *)testRegionLocal);
    free((void *)firstItemLocal);
}

// Test case -  Region Change Permission.
TEST(FamChangePermissions, RegionChangePermission) {
    Fam_Region_Descriptor *descLocal;
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    EXPECT_NO_THROW(descLocal = my_fam->fam_create_region(
                        testRegionLocal, BIG_REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, descLocal);

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    mode_t perm;
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        perm = (mode_t)i;
        my_fam->fam_change_permissions(descLocal, perm);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());

    EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal));

    free((void *)testRegionLocal);
}

// Test case -  Lookup region test.
TEST(FamLookup, FamLookupRegion) {
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        Fam_Region_Descriptor *desc2;
        char *regionInfo;
        regionInfo = (char *)malloc(NAME_BUFF_SIZE);
        sprintf(regionInfo, "%s_%d", testRegionLocal, i);
        EXPECT_NO_THROW(desc2 = my_fam->fam_lookup_region(regionInfo));
        EXPECT_NE((void *)NULL, desc2);
        free(regionInfo);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());
}

// Test case -  Lookup dataite test.
TEST(FamLookup, FamLookupDataItem) {
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    Fam_Descriptor *item1;
    EXPECT_NO_THROW(item1 =
                        my_fam->fam_lookup(testRegionLocal, testRegionLocal));
    EXPECT_NE((void *)NULL, item1);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        Fam_Descriptor *item1;
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItemLocal, i);
        EXPECT_NO_THROW(item1 = my_fam->fam_lookup(itemInfo, testRegionLocal));
        EXPECT_NE((void *)NULL, item1);
        // EXPECT_NO_THROW(my_fam->fam_deallocate(itemLocal[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());

    free((void *)testRegionLocal);
    free((void *)firstItemLocal);
}

TEST(FamLookup, FamLookupDataItemSingleRegion) {
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);

    Fam_Descriptor *item1;
    EXPECT_NO_THROW(item1 = my_fam->fam_lookup(firstItemLocal, "test"));
    EXPECT_NE((void *)NULL, item1);

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        Fam_Descriptor *item1;
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItemLocal, i);
        EXPECT_NO_THROW(item1 = my_fam->fam_lookup(itemInfo, "test"));
        EXPECT_NE((void *)NULL, item1);
        // EXPECT_NO_THROW(my_fam->fam_deallocate(itemLocal[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());

    free((void *)firstItemLocal);
}

TEST(FamAllocator, FamResizeRegion) {
    Fam_Region_Descriptor *descLocal;
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    EXPECT_NO_THROW(descLocal = my_fam->fam_create_region(
                        testRegionLocal, RESIZE_REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, descLocal);

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_resize_region(
            descLocal, my_fam->fam_size(descLocal) * i));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());
    EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal));
}

// Test case -  copy and wait test
TEST(FamCopy, FamCopyAndWait) {
    Fam_Region_Descriptor *descLocal;
    Fam_Descriptor **src = new Fam_Descriptor *[NUM_MM_ITERATIONS];
    Fam_Descriptor **dest = new Fam_Descriptor *[NUM_MM_ITERATIONS];
    void **waitObj = new void *[NUM_MM_ITERATIONS];

    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    char *local = strdup("Test message");

    EXPECT_NO_THROW(descLocal = my_fam->fam_create_region(
                        testRegionLocal, BIG_REGION_SIZE, 0777, RAID1));
    EXPECT_NE((void *)NULL, descLocal);

    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItemLocal, i);
        // Allocating data items
        EXPECT_NO_THROW(src[i] = my_fam->fam_allocate(itemInfo, DATA_ITEM_SIZE,
                                                      0777, descLocal));
        EXPECT_NE((void *)NULL, src[i]);
        EXPECT_NO_THROW(my_fam->fam_put_blocking(
            local, src[i], (i * 20) % (DATA_ITEM_SIZE - 20), 20));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        // dest[i] = new Fam_Descriptor();
        EXPECT_NO_THROW(waitObj[i] =
                            my_fam->fam_copy(src[i], 0, &dest[i], 0, 13));
        EXPECT_NE((void *)NULL, waitObj[i]);
    }

    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_copy_wait(waitObj[i]));
    }

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    for (int i = NUM_MM_ITERATIONS - 1; i >= 0; i--) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(src[i]));
        EXPECT_NO_THROW(my_fam->fam_deallocate(dest[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal));
    free((void *)testRegionLocal);
    free((void *)firstItemLocal);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    for (int i = 1; i < argc; ++i) {
        printf("arg %2d = %s\n", i, (argv[i]));
    }
    if (argc == 2) {
        NUM_MM_ITERATIONS = atoi(argv[1]);
    }

// Note:this test can not be run with multiple memory server model, when memory
// server profiling is enabled.
#if !defined(SHM) && defined(MEMSERVER_PROFILE)
	const char *ip = strdup(TEST_MEMSERVER_IP);
    EXPECT_NO_THROW(rpc = new Fam_Rpc_Client(ip, atoi(TEST_GRPC_PORT)));
#endif

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));
    EXPECT_NO_THROW(myPE = (int *)my_fam->fam_get_option(strdup("PE_ID")));

    EXPECT_NE((void *)NULL, myPE);

    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    ret = RUN_ALL_TESTS();
    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    delete my_fam;
    return ret;
}
