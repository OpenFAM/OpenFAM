/*
 * fam_microbenchmark_datapath_multiple_memnodes.cpp
 * Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All rights
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
/* Test Case Description: Test Program in which a region is created in each
 * memory server and each PE will create a data item in one of these regions.
 * Each PE will do put/getscatter/gather operations on the data items they have
 * created. Based on number of memory servers, first half of PEs will create 
 * regions, and other PEs lookup for created regions.
 */

#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>

#include "common/fam_test_config.h"
#define NUM_ITERATIONS 1000
#define ALL_PERM 0777
#define BIG_REGION_SIZE 1073741824
using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;
Fam_Descriptor *item;
Fam_Region_Descriptor *desc;
mode_t test_perm_mode;
size_t test_item_size;

uint64_t gDataSize = 256;

const int MAX = 26;
const int REGION_NAME_LEN = 10;

// Returns a string of random alphabets of
// length n.
string getRandomString() {
    char alphabet[MAX] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
                          'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
                          's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

    string res = "";
    for (int i = 0; i < REGION_NAME_LEN; i++)
        res = res + alphabet[rand() % MAX];

    return res;
}

// Function that creates a region name such that each memory node
// gets a region.
// Input1: memServerId - memory node ID in which region has to be created
// Input2: totalMemservers - number of memorynodes avaialble
// Returns region name
const char *getRegionName(int memServerId, int totalMemservers) {
    size_t hashVal;
    std::string regionName;
    do {
        // regionName = getRandomString()  + to_string(memServerId );
        regionName = getRandomString();
        hashVal = hash<string>{}(regionName) % totalMemservers;
    } while (hashVal != (size_t)memServerId);
    cout << "[ " << memServerId << ", " << totalMemservers
         << " ] = " << regionName << endl;
    return (strdup(regionName.c_str()));
}

// Test case -  Blocking put get test.
TEST(FamPutGet, BlockingFamPutGet) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    unsigned int offset = 0;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_put_blocking(local, item, offset, gDataSize));
    }

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_get_blocking(local, item, offset, gDataSize));
    }
    free(local);
}

// Test case -  Non-Blocking get test.
TEST(FamPutGet, NonBlockingFamGet) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    unsigned int offset = 0;
    // Put all data first
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_get_nonblocking(local, item, offset, gDataSize));
    }
    // Wait for I/os to complete
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local);
}

// Test case -  Non-Blocking put test.
TEST(FamPutGet, NonBlockingFamPut) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    unsigned int offset = 0;
    // Put all data first
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_put_nonblocking(local, item, offset, gDataSize));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local);
}

// Test case -  Blocking scatter and gather (Index) test.
TEST(FamScatter, BlockingScatterGatherIndex) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    int count = 4;
    int64_t size = gDataSize / count;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_scatter_blocking(local, item, count, indexes, size));
    }
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_gather_blocking(local, item, count, indexes, size));
    }
    free(local);
}

// Test case -  Non-Blocking scatter (Index) test.
TEST(FamScatter, NonBlockingScatterIndex) {

    int64_t *local = (int64_t *)malloc(gDataSize);
    int count = 4;
    int64_t size = gDataSize / count;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_scatter_nonblocking(local, item, count, indexes, size));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local);
}

// Test case -  Non-Blocking gather (Index) test.
TEST(FamScatter, NonBlockingGatherIndex) {

    int64_t *local = (int64_t *)malloc(gDataSize);
    int count = 4;
    int64_t size = gDataSize / count;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_gather_nonblocking(local, item, count, indexes, size));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local);
}

// Test case -  Blocking scatter and gather (Index) test (Full size).
TEST(FamScatter, BlockingScatterGatherIndexSize) {
    int count = 4;
    int64_t *local = (int64_t *)malloc(gDataSize * count);
    int64_t size = gDataSize;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_scatter_blocking(local, item, count, indexes, size));
    }
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_gather_blocking(local, item, count, indexes, size));
    }
    free(local);
}

// Test case -  Non-Blocking scatter (Index) test (Full size).
TEST(FamScatter, NonBlockingScatterIndexSize) {

    int count = 4;
    int64_t *local = (int64_t *)malloc(gDataSize * count);
    int64_t size = gDataSize;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_scatter_nonblocking(local, item, count, indexes, size));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local);
}

// Test case -  Non-Blocking gather (Index) test (Full size).
TEST(FamScatter, NonBlockingGatherIndexSize) {

    int count = 4;
    int64_t *local = (int64_t *)malloc(gDataSize * count);
    int64_t size = gDataSize;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_gather_nonblocking(local, item, count, indexes, size));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);
    char *peIdOpt;
    int *peId;
    int numsrv = 1;

    for (int i = 1; i < argc; ++i) {
        printf("arg %2d = %s\n", i, (argv[i]));
    }

    if (argc >= 2) {
        gDataSize = atoi(argv[1]);
        numsrv = atoi(argv[2]);
    }

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    // Following piece of code is used to get region name such that each memory
    // node gets a region to work with.
    peIdOpt = strdup("PE_ID");
    peId = (int *)my_fam->fam_get_option(peIdOpt);
    srand((*peId % numsrv));

    const char *testRegion = getRegionName((*peId % numsrv), numsrv);
    // First half of compute nodes/PEs create region, rest of them lookup for
    // the same region.
    if ((*peId % numsrv) == *peId) {
        EXPECT_NO_THROW(desc = my_fam->fam_create_region(
                            testRegion, BIG_REGION_SIZE, 0777, RAID1));
        my_fam->fam_barrier_all();

    } else {

        my_fam->fam_barrier_all();
        EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(testRegion));
    }
    cout << "PE ID: " << *peId << "Region with name " << testRegion
         << " created in memory node : " << desc->get_memserver_id() << endl;

    // Create data item for each compute node(PE)
    const char *dataItem = get_uniq_str("firstGlobal", my_fam);
    test_perm_mode = ALL_PERM;
    test_item_size = gDataSize * 4;
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(dataItem, test_item_size,
                                                test_perm_mode, desc));
    EXPECT_NE((void *)NULL, item);
    my_fam->fam_barrier_all();
    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    // Ensure all data items are deallocated before destroying the regions.
    my_fam->fam_barrier_all();

    if ((*peId % numsrv) == *peId) {
        EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    }
    delete item;
    delete desc;
    free((void *)dataItem);
    free((void *)testRegion);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    delete my_fam;
    return ret;
}
