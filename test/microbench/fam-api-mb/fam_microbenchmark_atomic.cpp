/*
 * fam_microbenchmark_atomic.cpp
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

#include "cis/fam_cis_client.h"
#include "common/fam_libfabric.h"
#include <fam/fam.h>

#include "common/fam_test_config.h"
#define NUM_ITERATIONS 10000
#define BIG_REGION_SIZE 8192
#define BLOCK_SIZE 1024
#define ALL_PERM 0777

using namespace std;
using namespace openfam;

int *myPE;
Fam_CIS_Client *cis;
fam *my_fam;
Fam_Options fam_opts;
Fam_Descriptor *item;
Fam_Region_Descriptor *desc;
mode_t test_perm_mode;
size_t test_item_size;
int64_t operand1Value = 0x1fffffffffffffff;
int64_t operand2Value = 0x1fffffffffffffff;
uint64_t operand1UValue = 0x1fffffffffffffff;
uint64_t operand2UValue = 0x1fffffffffffffff;

// Test case -  All Fetch atomics (Int64)
TEST(FamArithmaticAtomicmicrobench, FetchInt64) {
    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_fetch_int64(item, testOffset));
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchAddInt64) {
    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_fetch_add(item, testOffset, operand2Value));
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchSubInt64) {
    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_fetch_subtract(item, testOffset, operand2Value));
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchAndInt64) {
    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_fetch_and(item, testOffset, operand2UValue));
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchOrInt64) {
    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_fetch_or(item, testOffset, operand2UValue));
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchXorInt64) {
    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_fetch_xor(item, testOffset, operand2UValue));
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchMinInt64) {
    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_fetch_min(item, testOffset, operand2Value));
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchMaxInt64) {
    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_fetch_max(item, testOffset, operand2Value));
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchCmpswapInt64) {
    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_compare_swap(item, testOffset,
                                                 operand1Value, operand2Value));
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchSwapInt64) {
    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_swap(item, testOffset, operand2Value));
    }
}

// Test case -  fam_set for Int64
TEST(FamArithmaticAtomicmicrobench, SetInt64) {
    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_set(item, testOffset, operand2Value));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case -  NonFetch add for Int64
TEST(FamArithmaticAtomicmicrobench, NonFetchAddInt64) {

    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {

        EXPECT_NO_THROW(my_fam->fam_add(item, testOffset, operand2Value));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case -  NonFetch subtract for Int64
TEST(FamArithmaticAtomicmicrobench, NonFetchSubInt64) {

    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {

        EXPECT_NO_THROW(my_fam->fam_subtract(item, testOffset, operand2Value));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case - NonFetch and for UInt64
TEST(FamLogicalAtomics, NonFetchAndUInt64) {

    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {

        EXPECT_NO_THROW(my_fam->fam_and(item, testOffset, operand2UValue));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case - NonFetch or for UInt64
TEST(FamLogicalAtomics, NonFetchOrUInt64) {

    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {

        EXPECT_NO_THROW(my_fam->fam_or(item, testOffset, operand2UValue));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case - NonFetch xor for UInt64
TEST(FamLogicalAtomics, NonFetchXorUInt64) {

    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {

        EXPECT_NO_THROW(my_fam->fam_xor(item, testOffset, operand2UValue));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case -  NonFetch min for Int64
TEST(FamMinMaxAtomicMicrobench, NonFetchMinInt64) {

    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {

        EXPECT_NO_THROW(my_fam->fam_min(item, testOffset, operand2Value));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case -  NonFetch max for Int64
TEST(FamMinMaxAtomicMicrobench, NonFetchMaxInt64) {

    int i;

    uint64_t testOffset = 0;

    for (i = 0; i < NUM_ITERATIONS; i++) {

        EXPECT_NO_THROW(my_fam->fam_max(item, testOffset, operand2Value));
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    uint64_t testOffset = 0;
    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    const char *dataItem = get_uniq_str("firstGlobal", my_fam);
    const char *testRegion = get_uniq_str("testGlobal", my_fam);

#if !defined(SHM) && defined(MEMSERVER_PROFILE)
    const char *memoryServers = strdup(TEST_MEMSERVER_IP);
    std::string delimiter1 = ",";
    std::string delimiter2 = ":";
    MemServerMap memoryServerList =
        parse_memserver_list(memoryServers, delimiter1, delimiter2);
    EXPECT_NO_THROW(
        cis = new Fam_CIS_Client(memoryServerList, atoi(TEST_GRPC_PORT)));
#endif
    EXPECT_NO_THROW(myPE = (int *)my_fam->fam_get_option(strdup("PE_ID")));
    EXPECT_NO_THROW(desc = my_fam->fam_create_region(
                        testRegion, BIG_REGION_SIZE, 0777, RAID1));
    test_perm_mode = ALL_PERM;
    test_item_size = BLOCK_SIZE;
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(dataItem, test_item_size,
                                                test_perm_mode, desc));
    EXPECT_NE((void *)NULL, item);

    int64_t *local = (int64_t *)malloc(test_item_size);
    for (int i = 0; i < 10; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_put_blocking(local, item, 0, test_item_size));
    }
    // Need to disbale profiling code for fam_fetch_int32, so that
    // the profiling does not capture data for this dummy fetch_int32 call.
    // We do not profile for this APIs in the above test cases.
    for (int i = 0; i < 10; i++) {
        EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
    }

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
#if !defined(SHM) && defined(MEMSERVER_PROFILE)
    EXPECT_NO_THROW(cis->reset_profile(0));
    EXPECT_NO_THROW(fabric_reset_profile());
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
#endif

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete item;
    delete desc;
    free((void *)dataItem);
    free((void *)testRegion);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    delete my_fam;
    return ret;
}
