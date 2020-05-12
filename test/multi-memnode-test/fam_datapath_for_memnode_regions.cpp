/*
 * fam_datapath_for_memnode_regions.cpp
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
 * memory server and each PE will create a data item in these regions.
 * Each PE will do put/getscatter/gather operations on these regions.
 * Based on number of memory servers, first PEs will create regions, and
 * other PEs lookup for created regions.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>
#include <fam/fam_exception.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;
#define NUM_REGIONS 10

fam *my_fam;
Fam_Options fam_opts;
Fam_Region_Descriptor *testRegionDesc[NUM_REGIONS];
const char *testRegionStr;
const char *firstItem;
Fam_Descriptor *item[NUM_REGIONS];

#define REGION_PERM 0777
#define NUM_ITERATIONS 1000
#define REGION_SIZE 4294967296
uint64_t gDataSize = 256;
int numsrv;

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

// Test case#1 - Blocking Put/Get operations in each memory server for  NUM_ITERATIONS.
TEST(FamDatapath, BlockingPutGet) {
    int j = 0;
    char *local = strdup("MSG: ");
    char *msg = (char *)malloc(gDataSize);
    // allocate local memory to receive 20 elements
    char *local2 = (char *)malloc(gDataSize);
    uint64_t offset = 0;
    while (j < NUM_ITERATIONS) {
        sprintf(msg, "%s %d", local, j);
        for (int i = 0; i < numsrv; i++) {
            EXPECT_NO_THROW(
                my_fam->fam_put_blocking(msg, item[i], offset, gDataSize));
        }

        j++;
    }
    j = 0;
    offset = 0;
    while (j < NUM_ITERATIONS) {
        for (int i = 0; i < numsrv; i++) {
            sprintf(msg, "%s %d", local, j);
            EXPECT_NO_THROW(
                my_fam->fam_get_blocking(local2, item[i], offset, gDataSize));
        }

        j++;
    }
    free(msg);
    free(local2);
}

// Test case -  Non-Blocking puts in each memory server for  NUM_ITERATIONS.
TEST(FamDatapath, NonBlockingFamPut) {
    int i, j = 0;
    char *local = strdup("MSG: ");
    char *msg = (char *)malloc(gDataSize);
    uint64_t offset = 0;
    while (j < NUM_ITERATIONS) {
        sprintf(msg, "%s %d", local, j);
        for (i = 0; i < numsrv; i++) {
            my_fam->fam_put_nonblocking(msg, item[i], offset, gDataSize);
        }
        j++;
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}
//
// Test case -  Non-Blocking get in each memory server for  NUM_ITERATIONS.
//
TEST(FamDatapath, NonBlockingFamGet) {
    int i, j = 0;
    uint64_t offset = 0;
    char *local2 = (char *)malloc(gDataSize);    
    while (j < NUM_ITERATIONS) {
        for (i = 0; i < numsrv; i++) {
            EXPECT_NO_THROW(my_fam->fam_get_nonblocking(
                (void *)(local2), item[i], offset, gDataSize));
        }
        j++;
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local2);

}
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    char *peIdOpt;
    int *peId;
    numsrv = 1;

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
    int i = 0;

    peIdOpt = strdup("PE_ID");
    peId = (int *)my_fam->fam_get_option(peIdOpt);
    srand((numsrv));
    // PE 0 will create a region in each memory node.
    // Other PEs lookup for the regions before using them.
    if ((*peId) == 0) {
        while (i < numsrv) {
            const char *testRegion = getRegionName(i, numsrv);

            EXPECT_NO_THROW(testRegionDesc[i] = my_fam->fam_create_region(
                                testRegion, REGION_SIZE, REGION_PERM, RAID1));
            EXPECT_NE((void *)NULL, testRegionDesc[i]);
            cout << "PE ID: " << *peId << "Region with name " << testRegion
                 << " created in memory node : "
                 << testRegionDesc[i]->get_memserver_id() << endl;
            i++;
        }
        my_fam->fam_barrier_all();

    } else {

        my_fam->fam_barrier_all();
        i = 0;
        while (i < numsrv) {
            const char *testRegion = getRegionName(i, numsrv);

            EXPECT_NO_THROW(testRegionDesc[i] =
                                my_fam->fam_lookup_region(testRegion));
            EXPECT_NE((void *)NULL, testRegionDesc[i]);
            cout << "PE ID: " << *peId << "Region with name " << testRegion
                 << " looked up in memory node : "
                 << testRegionDesc[i]->get_memserver_id() << endl;
            i++;
        }
    }
    i = 0;
    // After all regions are created/looked up, create data item in each region
    while (i < numsrv) {

        char itemStr[20];
        firstItem = get_uniq_str("first", my_fam);
        sprintf(itemStr, "%s_%d", firstItem, i);
        EXPECT_NO_THROW(item[i] = my_fam->fam_allocate(
                            itemStr, (gDataSize), 0777, testRegionDesc[i]));
        EXPECT_NE((void *)NULL, item[i]);
        //	cout << "PE " << *peId << " allocated data item " << itemStr <<
        //" in memserver " << testRegionDesc[i]->get_memserver_id() << endl;
        i++;
    }
    my_fam->fam_barrier_all();
    int ret = RUN_ALL_TESTS();
    i = 0;
    while (i < numsrv) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(item[i]));
        i++;
    }
    // Wait for all data items to be deallocated before sestroying the regions.
    my_fam->fam_barrier_all();
    if (*peId == 0) {
        i = 0;
        while (i < numsrv) {
            EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc[i]));
            delete testRegionDesc[i];
            i++;
        }
    }
    free((void *)testRegionStr);
    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    delete my_fam;
    return ret;
}
