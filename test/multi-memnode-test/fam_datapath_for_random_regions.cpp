/*
 * fam_datapath_for_random_regions.cpp
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
/* Test Case Description: Test program in which each PE creates 15 regions, 
 * a data item in each of these regions, and does datapath operations on 
 * these data items.Regions are created randomly in any of the memory servers.
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
#define NUM_REGIONS 15

fam *my_fam;
Fam_Options fam_opts;
Fam_Region_Descriptor *testRegionDesc[NUM_REGIONS];
const char *testRegionStr;
const char *firstItem;
Fam_Descriptor *item[NUM_REGIONS];

#define REGION_SIZE 1073741824
#define REGION_PERM 0777
#define NUM_ITERATIONS 1000
uint64_t gDataSize = 256;

// Test case#1 - Blocking Put/Get calls are invoked for each region for NUM_ITERATIONS.
TEST(FamDatapath, BlockingPutGet) {
    int j = 0;
    unsigned offset = 0;
    char *local = strdup("MSG: ");
    // allocate local memory to receive gDataSize  elements
    char *msg = (char *)malloc(gDataSize);
    char *local2 = (char *)malloc(gDataSize);
    sprintf(msg, "%s", local);
    while (j < NUM_ITERATIONS) {
        offset = (gDataSize * j) % REGION_SIZE;
        for (int i = 0; i < NUM_REGIONS; i++) {
            EXPECT_NO_THROW(
                my_fam->fam_put_blocking(msg, item[i], offset, gDataSize));
        }

        j++;
    }
    j = 0;
    while (j < NUM_ITERATIONS) {
        offset = (gDataSize * j) % REGION_SIZE;
        for (int i = 0; i < NUM_REGIONS; i++) {

            EXPECT_NO_THROW(
                my_fam->fam_get_blocking(local2, item[i], offset, gDataSize));
        }

        j++;
    }
    free(msg);
    free(local2);
}

// Test case -  Non-Blocking put calls are invoked for each region for NUM_ITERATIONS.
TEST(FamDatapath, NonBlockingFamPut) {
    int i, j = 0;
    unsigned offset = 0;
    char *msg = strdup("MSG: ");
    while (j < NUM_ITERATIONS) {
        offset = (gDataSize * j) % REGION_SIZE;
        for (i = 0; i < NUM_REGIONS; i++) {
            EXPECT_NO_THROW(
                my_fam->fam_put_nonblocking(msg, item[i], offset, gDataSize));
        }
        j++;
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}
//
// Test case -  Non-Blocking get calls are invoked for each region for NUM_ITERATIONS.
//
TEST(FamDatapath, NonBlockingFamGet) {
    int i, j = 0;
    unsigned offset = 0;
    char *local2 = (char *)malloc(gDataSize);
    while (j < NUM_ITERATIONS) {
        for (i = 0; i < NUM_REGIONS; i++) {
            offset = (gDataSize * j) % REGION_SIZE;
            EXPECT_NO_THROW(my_fam->fam_get_nonblocking(
                (void *)(local2), item[i], offset, gDataSize));
        }
        j++;
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (argc == 2)
        gDataSize = atoi(argv[1]);

    my_fam = new fam();
    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));
    int i = 0;
    firstItem = get_uniq_str("first", my_fam);
    testRegionStr = get_uniq_str("test", my_fam);
    while (i < NUM_REGIONS) {

        char regionStr[20];
        char itemStr[20];

        sprintf(regionStr, "%s_%d", testRegionStr, i);
        sprintf(itemStr, "%s_%d", firstItem, i);
        EXPECT_NO_THROW(
            testRegionDesc[i] = my_fam->fam_create_region(
                regionStr, REGION_SIZE + 1048576, REGION_PERM, RAID1));
        EXPECT_NE((void *)NULL, testRegionDesc[i]);
        cout << "Region with name " << regionStr << " created in memory node : "
             << testRegionDesc[i]->get_memserver_id() <<  endl;

        // Allocating data items in the created region

        EXPECT_NO_THROW(item[i] = my_fam->fam_allocate(
                            itemStr, REGION_SIZE, 0777, testRegionDesc[i]));
        EXPECT_NE((void *)NULL, item[i]);
        i++;
    }
    my_fam->fam_barrier_all();
    int ret = RUN_ALL_TESTS();
    i = 0;
    while (i < NUM_REGIONS) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(item[i]));

        EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc[i]));
        delete testRegionDesc[i];
        i++;
    }
    free((void *)testRegionStr);
    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    delete my_fam;
    return ret;
}
