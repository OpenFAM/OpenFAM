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

#include "cis/fam_cis_client.h"
#include "cis/fam_cis_direct.h"
#include "common/fam_test_config.h"
#define NUM_ITERATIONS 100
#define BIG_REGION_SIZE 21474836480
#define SMALL_REGION_SIZE 1073741824
#define BLOCK_SIZE 1048576
#define RESIZE_REGION_SIZE 1048576
#define ALL_PERM 0777
#define NAME_BUFF_SIZE 255
using namespace std;
using namespace openfam;

int *myPE;
int NUM_MM_ITERATIONS;
int DATA_ITEM_SIZE;
Fam_CIS *cis;
fam *my_fam;
Fam_Options fam_opts;
Fam_Descriptor *item;
Fam_Region_Descriptor *desc;
mode_t test_perm_mode;
size_t test_item_size;

#ifdef MEMSERVER_PROFILE
#define RESET_PROFILE()                                                        \
    {                                                                          \
        cis->reset_profile();                                                  \
        my_fam->fam_barrier_all();                                             \
    }

#define GENERATE_PROFILE()                                                     \
    {                                                                          \
        if (*myPE == 0)                                                        \
            cis->dump_profile();                                               \
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
                            regionInfo, BLOCK_SIZE, 0777, NULL));
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
                            regionInfo, BLOCK_SIZE, 0777, NULL));
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
                            regionInfo, BLOCK_SIZE, perm, NULL));
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
                        testRegionLocal, BIG_REGION_SIZE, 0777, NULL));
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
                        testRegionLocal, BIG_REGION_SIZE, 0777, NULL));
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
                            "test", BIG_REGION_SIZE, 0777, NULL));
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
                        testRegionLocal, BIG_REGION_SIZE, 0777, NULL));
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
                        testRegionLocal, BIG_REGION_SIZE, 0777, NULL));
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

// Test case -  Blocking put get test.
TEST(FamPutGet, BlockingFamPutGet) {
    char *local = strdup("Test message");

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        unsigned int offset = i * BLOCK_SIZE;
        EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item, offset, 20));
    }
    char *local2 = (char *)malloc(20);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // allocate local memory to receive 20 elements
        unsigned int offset = i * BLOCK_SIZE;
        EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, item, offset, 20));
    }
    free(local);
    free(local2);
}

// Test case -  Non-Blocking put get test.
TEST(FamPutGet, NonBlockingFamPutGet) {
    char *local = strdup("Test message");

    unsigned int offset = 0;
    // Put all data first
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        offset = i * BLOCK_SIZE;
        EXPECT_NO_THROW(my_fam->fam_put_nonblocking(local, item, offset, 20));
    }
    // Wait for I/os to complete
    EXPECT_NO_THROW(my_fam->fam_quiet());
    // Get and compare the data obtained from FAM
    offset = 0;
    char *local2 = (char *)malloc(20);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // allocate local memory to receive 20 elements
        offset = i * BLOCK_SIZE;
        EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local2, item, offset, 20));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local);
    free(local2);
}

// Test case -  Blocking scatter and gather (Index) test.
TEST(FamScatter, BlockingScatterGatherIndex) {

    int *local2 = (int *)malloc(5 * sizeof(int));

    for (int j = 0; j < NUM_ITERATIONS; j++) {

        int newLocal[NUM_ITERATIONS];
        uint64_t indexes[] = { 0, 7, 3, 5, 8 };
        for (int e = 0; e < NUM_ITERATIONS; e++) {
            newLocal[e] = NUM_ITERATIONS - e;
        }
        my_fam->fam_scatter_blocking(newLocal, item, 5, indexes, sizeof(int));

        my_fam->fam_gather_blocking(local2, item, 5, indexes, sizeof(int));
    }
}

// Test case - Blocking scatter and gather (Stride) test.
TEST(FamScatter, BlockingScatterGatherStride) {

    int *local2 = (int *)malloc(5 * sizeof(int));

    for (int j = 0; j < NUM_ITERATIONS; j++) {

        int newLocal[NUM_ITERATIONS];
        for (int e = 0; e < NUM_ITERATIONS; e++) {
            newLocal[e] = NUM_ITERATIONS - e;
        }
        my_fam->fam_scatter_blocking(newLocal, item, 5, 2, 3, sizeof(int));

        my_fam->fam_gather_blocking(local2, item, 5, 2, 3, sizeof(int));
    }
}

// Test case -  Non-Blocking scatter and gather (Index) test.
TEST(FamScatter, NonBlockingScatterGatherIndex) {

    uint64_t indexes[] = { 0, 7, 3, 5, 8 };

    for (int j = 0; j < NUM_ITERATIONS; j++) {

        int newLocal[NUM_ITERATIONS];
        for (int e = 0; e < NUM_ITERATIONS; e++) {
            newLocal[e] = NUM_ITERATIONS - e;
        }
        my_fam->fam_scatter_nonblocking(newLocal, item, 5, indexes,
                                        sizeof(int));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    int *local2 = (int *)malloc(5 * sizeof(int));

    for (int j = 0; j < NUM_ITERATIONS; j++) {

        my_fam->fam_gather_nonblocking(local2, item, 5, indexes, sizeof(int));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local2);
}

// Test case -  Non-Blocking scatter and gather (stride) test.
TEST(FamScatter, NonBlockingScatterGatherStride) {

    // Put all datta at once
    for (int j = 0; j < NUM_ITERATIONS; j++) {

        // allocate an integer array and initialize it
        int newLocal[NUM_ITERATIONS];
        for (int e = 0; e < NUM_ITERATIONS; e++) {
            newLocal[e] = NUM_ITERATIONS - e;
        }
        my_fam->fam_scatter_nonblocking(newLocal, item, 5, 2, 3, sizeof(int));
    }
    // Wait for writes to complete
    EXPECT_NO_THROW(my_fam->fam_quiet());

    // Get data from FAM and compare
    int *local2 = (int *)malloc(5 * sizeof(int));
    for (int j = 0; j < NUM_ITERATIONS; j++) {
        my_fam->fam_gather_nonblocking(local2, item, 5, 2, 3, sizeof(int));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local2);
}

TEST(FamAllocator, FamCreateRegion) {
    Fam_Region_Descriptor **descLocal =
        new Fam_Region_Descriptor *[NUM_MM_ITERATIONS];
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);

    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char regionInfo[NAME_BUFF_SIZE];
        sprintf(regionInfo, "%s_%d", testRegionLocal, i);
        EXPECT_NO_THROW(descLocal[i] = my_fam->fam_create_region(
                            regionInfo, BLOCK_SIZE, 0777, NULL));
        EXPECT_NE((void *)NULL, descLocal[i]);
    }
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

TEST(FamAllocator, FamAllocateItem) {
    Fam_Region_Descriptor *descLocal;
    Fam_Descriptor **itemLocal = new Fam_Descriptor *[NUM_MM_ITERATIONS];
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    EXPECT_NO_THROW(descLocal = my_fam->fam_create_region(
                        testRegionLocal, BIG_REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, descLocal);

    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItemLocal, i);
        // Allocating data items
        EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_allocate(
                            itemInfo, DATA_ITEM_SIZE, 0777, descLocal));
        EXPECT_NE((void *)NULL, itemLocal[i]);
    }
    free((void *)testRegionLocal);
    free((void *)firstItemLocal);
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
                        testRegionLocal, RESIZE_REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, descLocal);

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_stat(descLocal, info));
        EXPECT_NO_THROW(my_fam->fam_resize_region(descLocal, info->size * i));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();
    // EXPECT_NO_THROW(rpc->generate_profile());
    EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal));
    free(info);
}

// Test case -  copy and wait test
TEST(FamCopy, FamCopyAndWait) {
    Fam_Region_Descriptor *srcRegion, *destRegion;
    Fam_Descriptor **src = new Fam_Descriptor *[NUM_MM_ITERATIONS];
    Fam_Descriptor **dest = new Fam_Descriptor *[NUM_MM_ITERATIONS];
    void **waitObj = new void *[NUM_MM_ITERATIONS];

    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    const char *firstRegionLocal = get_uniq_str("firstRegionLocal", my_fam);
    char *local = strdup("Test message");

    EXPECT_NO_THROW(srcRegion = my_fam->fam_create_region(
                        firstRegionLocal, BIG_REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, srcRegion);

    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItemLocal, i);
        // Allocating data items
        EXPECT_NO_THROW(src[i] = my_fam->fam_allocate(itemInfo, DATA_ITEM_SIZE,
                                                      0777, srcRegion));
        EXPECT_NE((void *)NULL, src[i]);
        EXPECT_NO_THROW(my_fam->fam_put_blocking(
            local, src[i], (i * 20) % (DATA_ITEM_SIZE - 20), 20));
    }

    const char *secondItemLocal = get_uniq_str("firstLocal", my_fam);
    const char *secondRegionLocal = get_uniq_str("secondRegionLocal", my_fam);

    EXPECT_NO_THROW(destRegion = my_fam->fam_create_region(
                        secondRegionLocal, BIG_REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, destRegion);

    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", secondItemLocal, i);
        // Allocating data items
        EXPECT_NO_THROW(dest[i] = my_fam->fam_allocate(itemInfo, DATA_ITEM_SIZE,
                                                       0777, destRegion));
        EXPECT_NE((void *)NULL, dest[i]);
    }

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        // dest[i] = new Fam_Descriptor();
        EXPECT_NO_THROW(waitObj[i] =
                            my_fam->fam_copy(src[i], 0, dest[i], 0, 13));
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
    EXPECT_NO_THROW(my_fam->fam_destroy_region(srcRegion));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(destRegion));
    free((void *)firstRegionLocal);
    free((void *)firstItemLocal);
    free((void *)secondRegionLocal);
    free((void *)secondItemLocal);
}

// Test case -  Fetch add for Int32
TEST(FamArithmaticAtomicmicrobench, FetchAddInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int32_t operand1Value = rand() % numeric_limits<int32_t>::max();
            int32_t operand2Value = rand() % numeric_limits<int32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_add(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
        }
    }
}

// Test case -  Fetch subtract for Int32
TEST(FamArithmaticAtomicmicrobench, FetchSubInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int32_t operand1Value = rand() % numeric_limits<int32_t>::max();
            int32_t operand2Value = rand() % numeric_limits<int32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_subtract(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
        }
    }
}

// Test case -  Fetch add for Int64
TEST(FamArithmaticAtomicmicrobench, FetchAddInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int64_t operand1Value = rand() % numeric_limits<int64_t>::max();
            int64_t operand2Value = rand() % numeric_limits<int64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_add(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int64(item, testOffset));
        }
    }
}

// Test case -  Fetch subtract for Int64
TEST(FamArithmaticAtomicmicrobench, FetchSubInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int64_t operand1Value = rand() % numeric_limits<int64_t>::max();
            int64_t operand2Value = rand() % numeric_limits<int64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_subtract(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int64(item, testOffset));
        }
    }
}

// Test case -  Fetch add for UInt32
TEST(FamArithmaticAtomicmicrobench, FetchAddUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % numeric_limits<uint32_t>::max();
            uint32_t operand2Value = rand() % numeric_limits<uint32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_add(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  Fetch sub for UInt32
TEST(FamArithmaticAtomicmicrobench, FetchsubUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % numeric_limits<uint32_t>::max();
            uint32_t operand2Value = rand() % numeric_limits<uint32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_subtract(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  Fetch add for UInt64
TEST(FamArithmaticAtomicmicrobench, FetchAddUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % numeric_limits<uint64_t>::max();
            uint64_t operand2Value = rand() % numeric_limits<uint64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_add(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  Fetch subtract for UInt64
TEST(FamArithmaticAtomicmicrobench, FetchSubUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % numeric_limits<uint64_t>::max();
            uint64_t operand2Value = rand() % numeric_limits<uint64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_subtract(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  Fetch add for Float
TEST(FamArithmaticAtomicmicrobench, FetchAddFloat) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(float));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            float operand1Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));
            float operand2Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_add(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_float(item, testOffset));
        }
    }
}

// Test case -  Fetch subtract for Float
TEST(FamArithmaticAtomicmicrobench, FetchSubFloat) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(float));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            float operand1Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));
            float operand2Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_subtract(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_float(item, testOffset));
        }
    }
}

// Test case -  Fetch add for Double
TEST(FamArithmaticAtomicmicrobench, FetchAddDouble) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(double));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            double operand1Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));
            double operand2Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_add(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_double(item, testOffset));
        }
    }
}

// Test case -  Fetch subtract for Double
TEST(FamArithmaticAtomicmicrobench, FetchSubDouble) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(double));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            // double operand1Value = rand()%numeric_limits<double>::max();
            // double operand2Value = rand()%numeric_limits<double>::max();
            double operand1Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));
            double operand2Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_subtract(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_double(item, testOffset));
        }
    }
}

// Test case -  NonFetch add for Int32
TEST(FamArithmaticAtomicmicrobench, NonFetchAddInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int32_t operand1Value = rand() % numeric_limits<int32_t>::max();
            int32_t operand2Value = rand() % numeric_limits<int32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_add(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
        }
    }
}

// Test case -  NonFetch subtract for Int32
TEST(FamArithmaticAtomicmicrobench, NonFetchSubInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int32_t operand1Value = rand() % numeric_limits<int32_t>::max();
            int32_t operand2Value = rand() % numeric_limits<int32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_subtract(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
        }
    }
}

// Test case -  NonFetch add for Int64
TEST(FamArithmaticAtomicmicrobench, NonFetchAddInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int64_t operand1Value = rand() % numeric_limits<int64_t>::max();
            int64_t operand2Value = rand() % numeric_limits<int64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_add(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_int64(item, testOffset));
        }
    }
}

// Test case -  NonFetch subtract for Int64
TEST(FamArithmaticAtomicmicrobench, NonFetchSubInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int64_t operand1Value = rand() % numeric_limits<int64_t>::max();
            int64_t operand2Value = rand() % numeric_limits<int64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_subtract(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_int64(item, testOffset));
        }
    }
}

// Test case -  NonFetch add for UInt32
TEST(FamArithmaticAtomicmicrobench, NonFetchAddUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % numeric_limits<uint32_t>::max();
            uint32_t operand2Value = rand() % numeric_limits<uint32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_add(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  NonFetch sub for UInt32
TEST(FamArithmaticAtomicmicrobench, NonFetchsubUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % numeric_limits<uint32_t>::max();
            uint32_t operand2Value = rand() % numeric_limits<uint32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_subtract(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  NonFetch add for UInt64
TEST(FamArithmaticAtomicmicrobench, NonFetchAddUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % numeric_limits<uint64_t>::max();
            uint64_t operand2Value = rand() % numeric_limits<uint64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_add(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  NonFetch subtract for UInt64
TEST(FamArithmaticAtomicmicrobench, NonFetchSubUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % numeric_limits<uint64_t>::max();
            uint64_t operand2Value = rand() % numeric_limits<uint64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_subtract(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  NonFetch add for Float
TEST(FamArithmaticAtomicmicrobench, NonFetchAddFloat) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(float));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            float operand1Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));
            float operand2Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_add(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_float(item, testOffset));
        }
    }
}

// Test case -  NonFetch subtract for Float
TEST(FamArithmaticAtomicmicrobench, NonFetchSubFloat) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(float));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            float operand1Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));
            float operand2Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_subtract(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_float(item, testOffset));
        }
    }
}

// Test case -  NonFetch add for Double
TEST(FamArithmaticAtomicmicrobench, NonFetchAddDouble) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(double));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            double operand1Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));
            double operand2Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_add(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_double(item, testOffset));
        }
    }
}

// Test case -  NonFetch subtract for Double
TEST(FamArithmaticAtomicmicrobench, NonFetchSubDouble) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(double));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            double operand1Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));
            double operand2Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_subtract(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_double(item, testOffset));
        }
    }
}

// Test case -  Fetch and for UInt32
TEST(FamLogicalAtomics, FetchAndUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % 0xffffffff;
            uint32_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_and(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  Fetch or for UInt32
TEST(FamLogicalAtomics, FetchOrUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % 0xffffffff;
            uint32_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_or(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  Fetch xor for UInt32
TEST(FamLogicalAtomics, FetchXorUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % 0xffffffff;
            uint32_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_xor(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  Fetch and for UInt64
TEST(FamLogicalAtomics, FetchAndUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % 0xffffffff;
            uint64_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_and(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  Fetch or for UInt64
TEST(FamLogicalAtomics, FetchOrUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % 0xffffffff;
            uint64_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_or(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  Fetch xor for UInt64
TEST(FamLogicalAtomics, FetchXorUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % 0xffffffff;
            uint64_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_xor(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case - Fetch and for UInt32
TEST(FamLogicalAtomics, NonFetchAndUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % 0xffffffff;
            uint32_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_and(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case - Fetch or for UInt32
TEST(FamLogicalAtomics, NonFetchOrUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % 0xffffffff;
            uint32_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_or(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case - Fetch xor for UInt32
TEST(FamLogicalAtomics, NonFetchXorUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % 0xffffffff;
            uint32_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_xor(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case - Fetch and for UInt64
TEST(FamLogicalAtomics, NonFetchAndUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % 0xffffffff;
            uint64_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_and(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case - Fetch or for UInt64
TEST(FamLogicalAtomics, NonFetchOrUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % 0xffffffff;
            uint64_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_or(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case - Fetch xor for UInt64
TEST(FamLogicalAtomics, NonFetchXorUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % 0xffffffff;
            uint64_t operand2Value = rand() % 0xffffffff;

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_xor(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  Fetch min for Int32
TEST(FamMinMaxAtomicMicrobench, FetchMinInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int32_t operand1Value = rand() % numeric_limits<int32_t>::max();
            int32_t operand2Value = rand() % numeric_limits<int32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_min(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
        }
    }
}

// Test case -  Fetch max for Int32
TEST(FamMinMaxAtomicMicrobench, FetchMaxInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int32_t operand1Value = rand() % numeric_limits<int32_t>::max();
            int32_t operand2Value = rand() % numeric_limits<int32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_max(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
        }
    }
}

// Test case -  Fetch min for Int64
TEST(FamMinMaxAtomicMicrobench, FetchMinInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int64_t operand1Value = rand() % numeric_limits<int64_t>::max();
            int64_t operand2Value = rand() % numeric_limits<int64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_min(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int64(item, testOffset));
        }
    }
}

// Test case -  Fetch max for Int64
TEST(FamMinMaxAtomicMicrobench, FetchMaxInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int64_t operand1Value = rand() % numeric_limits<int64_t>::max();
            int64_t operand2Value = rand() % numeric_limits<int64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_max(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int64(item, testOffset));
        }
    }
}

// Test case -  Fetch min for UInt32
TEST(FamMinMaxAtomicMicrobench, FetchMinUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % numeric_limits<uint32_t>::max();
            uint32_t operand2Value = rand() % numeric_limits<uint32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_min(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  Fetch sub for UInt32
TEST(FamMinMaxAtomicMicrobench, FetchsubUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % numeric_limits<uint32_t>::max();
            uint32_t operand2Value = rand() % numeric_limits<uint32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_max(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  Fetch min for UInt64
TEST(FamMinMaxAtomicMicrobench, FetchMinUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % numeric_limits<uint64_t>::max();
            uint64_t operand2Value = rand() % numeric_limits<uint64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_min(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  Fetch max for UInt64
TEST(FamMinMaxAtomicMicrobench, FetchMaxUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % numeric_limits<uint64_t>::max();
            uint64_t operand2Value = rand() % numeric_limits<uint64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_max(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  Fetch min for Float
TEST(FamMinMaxAtomicMicrobench, FetchMinFloat) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(float));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            float operand1Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));
            float operand2Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_min(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_float(item, testOffset));
        }
    }
}

// Test case -  Fetch max for Float
TEST(FamMinMaxAtomicMicrobench, FetchMaxFloat) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(float));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            float operand1Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));
            float operand2Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_max(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_float(item, testOffset));
        }
    }
}

// Test case -  Fetch min for Double
TEST(FamMinMaxAtomicMicrobench, FetchMinDouble) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(double));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            double operand1Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));
            double operand2Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_min(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_double(item, testOffset));
        }
    }
}

// Test case -  Fetch max for Double
TEST(FamMinMaxAtomicMicrobench, FetchMaxDouble) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(double));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            double operand1Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));
            double operand2Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(
                my_fam->fam_fetch_max(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_double(item, testOffset));
        }
    }
}

// Test case -  NonFetch min for Int32
TEST(FamMinMaxAtomicMicrobench, NonFetchMinInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int32_t operand1Value = rand() % numeric_limits<int32_t>::max();
            int32_t operand2Value = rand() % numeric_limits<int32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_min(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
        }
    }
}

// Test case -  NonFetch max for Int32
TEST(FamMinMaxAtomicMicrobench, NonFetchMaxInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int32_t operand1Value = rand() % numeric_limits<int32_t>::max();
            int32_t operand2Value = rand() % numeric_limits<int32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_max(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
        }
    }
}

// Test case -  NonFetch min for Int64
TEST(FamMinMaxAtomicMicrobench, NonFetchMinInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int64_t operand1Value = rand() % numeric_limits<int64_t>::max();
            int64_t operand2Value = rand() % numeric_limits<int64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_min(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_int64(item, testOffset));
        }
    }
}

// Test case -  NonFetch max for Int64
TEST(FamMinMaxAtomicMicrobench, NonFetchMaxInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int64_t operand1Value = rand() % numeric_limits<int64_t>::max();
            int64_t operand2Value = rand() % numeric_limits<int64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_max(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_int64(item, testOffset));
        }
    }
}

// Test case -  NonFetch min for UInt32
TEST(FamMinMaxAtomicMicrobench, NonFetchMinUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % numeric_limits<uint32_t>::max();
            uint32_t operand2Value = rand() % numeric_limits<uint32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_min(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  NonFetch sub for UInt32
TEST(FamMinMaxAtomicMicrobench, NonFetchsubUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % numeric_limits<uint32_t>::max();
            uint32_t operand2Value = rand() % numeric_limits<uint32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_max(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  NonFetch min for UInt64
TEST(FamMinMaxAtomicMicrobench, NonFetchMinUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % numeric_limits<uint64_t>::max();
            uint64_t operand2Value = rand() % numeric_limits<uint64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_min(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  NonFetch max for UInt64
TEST(FamMinMaxAtomicMicrobench, NonFetchMaxUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % numeric_limits<uint64_t>::max();
            uint64_t operand2Value = rand() % numeric_limits<uint64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_max(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  NonFetch min for Float
TEST(FamMinMaxAtomicMicrobench, NonFetchMinFloat) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(float));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            float operand1Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));
            float operand2Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_min(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_float(item, testOffset));
        }
    }
}

// Test case -  NonFetch max for Float
TEST(FamMinMaxAtomicMicrobench, NonFetchMaxFloat) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(float));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            float operand1Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));
            float operand2Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_max(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_float(item, testOffset));
        }
    }
}

// Test case -  NonFetch min for Double
TEST(FamMinMaxAtomicMicrobench, NonFetchMinDouble) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(double));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            double operand1Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));
            double operand2Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_min(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_double(item, testOffset));
        }
    }
}

// Test case -  NonFetch max for Double
TEST(FamMinMaxAtomicMicrobench, NonFetchMaxDouble) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(double));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            double operand1Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));
            double operand2Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_max(item, testOffset, operand2Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_fetch_double(item, testOffset));
        }
    }
}

// Test case -  Fetch compare swap for Int32
TEST(FamCompareSwapAtomicMicrobench, FetchCompareSwapInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int32_t operand1Value = rand() % numeric_limits<int32_t>::max();
            int32_t operand2Value = rand() % numeric_limits<int32_t>::max();
            int32_t operand3Value = rand() % numeric_limits<int32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_compare_swap(
                item, testOffset, operand2Value, operand3Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
        }
    }
}

// Test case -  Fetch compare swap for Int64
TEST(FamCompareSwapAtomicMicrobench, FetchCompareSwapInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int64_t operand1Value = rand() % numeric_limits<int64_t>::max();
            int64_t operand2Value = rand() % numeric_limits<int64_t>::max();
            int64_t operand3Value = rand() % numeric_limits<int64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_compare_swap(
                item, testOffset, operand2Value, operand3Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int64(item, testOffset));
        }
    }
}

// Test case -  Fetch compare swap for UInt32
TEST(FamCompareSwapAtomicMicrobench, FetchCompareSwapUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % numeric_limits<uint32_t>::max();
            uint32_t operand2Value = rand() % numeric_limits<uint32_t>::max();
            uint32_t operand3Value = rand() % numeric_limits<uint32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_compare_swap(
                item, testOffset, operand2Value, operand3Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  Fetch compare swap for UInt64
TEST(FamCompareSwapAtomicMicrobench, FetchCompareSwapUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % numeric_limits<uint64_t>::max();
            uint64_t operand2Value = rand() % numeric_limits<uint64_t>::max();
            uint64_t operand3Value = rand() % numeric_limits<uint64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_compare_swap(
                item, testOffset, operand2Value, operand3Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  Fetch compare swap for Int128
TEST(FamCompareSwapAtomicMicrobench, FetchCompareSwapInt128) {

    union int128store {
        struct {
            uint64_t low;
            uint64_t high;
        };
        int64_t i64[2];
        int128_t i128;
    };

    int128store operand1Value, operand2Value, operand3Value;
    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int128_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            operand1Value.i64[0] = rand() % numeric_limits<uint64_t>::max();
            operand1Value.i64[1] = rand() % numeric_limits<uint64_t>::max();
            operand2Value.i64[0] = rand() % numeric_limits<uint64_t>::max();
            operand2Value.i64[1] = rand() % numeric_limits<uint64_t>::max();
            operand3Value.i64[0] = rand() % numeric_limits<uint64_t>::max();
            operand3Value.i64[1] = rand() % numeric_limits<uint64_t>::max();

            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset, operand1Value.i64[0]));
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset + 8, operand1Value.i64[1]));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_compare_swap(
                item, testOffset, operand2Value.i128, operand3Value.i128));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset + 8));
        }
    }
}

// Test case -  Fetch swap for Int32
TEST(FamSwapAtomicMicrobench, FetchSwapInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int32_t operand1Value = rand() % numeric_limits<int32_t>::max();
            int32_t operand2Value = rand() % numeric_limits<int32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_swap(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
        }
    }
}

// Test case -  Fetch swap for Int64
TEST(FamSwapAtomicMicrobench, FetchSwapInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(int64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            int64_t operand1Value = rand() % numeric_limits<int64_t>::max();
            int64_t operand2Value = rand() % numeric_limits<int64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_swap(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_int64(item, testOffset));
        }
    }
}

// Test case -  Fetch swap for UInt32
TEST(FamSwapAtomicMicrobench, FetchSwapUInt32) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint32_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint32_t operand1Value = rand() % numeric_limits<uint32_t>::max();
            uint32_t operand2Value = rand() % numeric_limits<uint32_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_swap(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint32(item, testOffset));
        }
    }
}

// Test case -  Fetch swap for UInt64
TEST(FamSwapAtomicMicrobench, FetchSwapUInt64) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(uint64_t));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t operand1Value = rand() % numeric_limits<uint64_t>::max();
            uint64_t operand2Value = rand() % numeric_limits<uint64_t>::max();

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_swap(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_uint64(item, testOffset));
        }
    }
}

// Test case -  Fetch swap for Float
TEST(FamSwapAtomicMicrobench, FetchSwapFloat) {

    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(float));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            float operand1Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));
            float operand2Value = static_cast<float>(rand()) /
                                  (static_cast<float>(RAND_MAX / 3.40282e+38));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_swap(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_float(item, testOffset));
        }
    }
}

// Test case -  Fetch swap for Double
TEST(FamSwapAtomicMicrobench, FetchSwapDouble) {
    int i, sm;

    for (sm = 1; sm <= 10; sm++) {

        uint64_t testOffset = rand() % (test_item_size - sizeof(double));

        for (i = 0; i < NUM_ITERATIONS; i++) {
            double operand1Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));
            double operand2Value =
                static_cast<double>(rand()) /
                (static_cast<double>(RAND_MAX / numeric_limits<double>::max()));

            EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand1Value));
            EXPECT_NO_THROW(my_fam->fam_quiet());

            EXPECT_NO_THROW(my_fam->fam_swap(item, testOffset, operand2Value));

            EXPECT_NO_THROW(my_fam->fam_fetch_double(item, testOffset));
        }
    }
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    for (int i = 1; i < argc; ++i) {
        printf("arg %2d = %s\n", i, (argv[i]));
    }
    if (argc == 3) {
        NUM_MM_ITERATIONS = atoi(argv[1]);
        DATA_ITEM_SIZE = atoi(argv[2]);
    }

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

// Note:this test can not be run with multiple memory server model, when memory
// server profiling is enabled.
#ifdef MEMSERVER_PROFILE
    char *openFamModel = NULL;
    EXPECT_NO_THROW(
        openFamModel = (char *)my_fam->fam_get_option(strdup("OPENFAM_MODEL")));
    if (strcmp(openFamModel, "memory_server") != 0) {
        EXPECT_NO_THROW(my_fam->fam_finalize("default"));
        std::cout << "Test case valid only in memory server model, "
                     "skipping with status : " << TEST_SKIP_STATUS << std::endl;
        return TEST_SKIP_STATUS;
    }
    char *cisServer = (char *)my_fam->fam_get_option(strdup("CIS_SERVER"));
    char *rpcPort = (char *)my_fam->fam_get_option(strdup("GRPC_PORT"));
    char *cisInterface =
        (char *)my_fam->fam_get_option(strdup("CIS_INTERFACE_TYPE"));
    if (strcmp(cisInterface, "rpc") == 0) {
        EXPECT_NO_THROW(cis = new Fam_CIS_Client(cisServer, atoi(rpcPort)));
    } else {
        EXPECT_NO_THROW(cis = new Fam_CIS_Direct(NULL, true, false));
    }
#endif

    EXPECT_NO_THROW(myPE = (int *)my_fam->fam_get_option(strdup("PE_ID")));

    EXPECT_NE((void *)NULL, myPE);

    const char *dataItem = get_uniq_str("firstGlobal", my_fam);
    const char *testRegion = get_uniq_str("testGlobal", my_fam);

    EXPECT_NO_THROW(desc = my_fam->fam_create_region(
                        testRegion, SMALL_REGION_SIZE, 0777, NULL));
    test_perm_mode = ALL_PERM;
    test_item_size = BLOCK_SIZE * NUM_ITERATIONS;
    // Allocating data global item
    EXPECT_NO_THROW(item = my_fam->fam_allocate(dataItem, test_item_size,
                                                test_perm_mode, desc));
    EXPECT_NE((void *)NULL, item);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    ret = RUN_ALL_TESTS();
    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    delete my_fam;
    return ret;
}
