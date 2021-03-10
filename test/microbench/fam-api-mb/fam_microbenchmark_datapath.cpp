/*
 * fam_microbenchmark_datapath.cpp
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
#include "cis/fam_cis_direct.h"
#include "common/fam_libfabric.h"
#include <fam/fam.h>

#include "common/fam_test_config.h"
//#define NUM_ITERATIONS 1000
#define ALL_PERM 0777
#define BIG_REGION_SIZE 1073741824
using namespace std;
using namespace openfam;

int NUM_ITERATIONS;
int *myPE;
Fam_CIS *cis;
fam *my_fam;
Fam_Options fam_opts;
Fam_Descriptor *item;
Fam_Region_Descriptor *desc;
mode_t test_perm_mode;
size_t test_item_size;

uint64_t gDataSize = 256;

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

// Test case -  Blocking put get test.
TEST(FamPutGet, BlockingFamPut) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    unsigned int offset = 0;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(
            my_fam->fam_put_blocking(local, item, offset, gDataSize));
    }
    free(local);
}

TEST(FamPutGet, BlockingFamPutNewDesc) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    unsigned int offset = 0;
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    RESET_PROFILE();

    for (int i = 0; i < NUM_ITERATIONS; i++) {

        Fam_Global_Descriptor globalDesc = item->get_global_descriptor();

        Fam_Descriptor *itemCopy = new Fam_Descriptor(globalDesc);
        EXPECT_NO_THROW(
            my_fam->fam_put_blocking(local, itemCopy, offset, gDataSize));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    GENERATE_PROFILE();

    free(local);
}

TEST(FamPutGet, BlockingFamGet) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    unsigned int offset = 0;

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
TEST(FamScatter, BlockingScatterIndex) {
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
    free(local);
}

TEST(FamScatter, BlockingGatherIndex) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    int count = 4;
    int64_t size = gDataSize / count;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
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
TEST(FamScatter, BlockingScatterIndexSize) {
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
    free(local);
}

TEST(FamScatter, BlockingGatherIndexSize) {
    int count = 4;
    int64_t *local = (int64_t *)malloc(gDataSize * count);
    int64_t size = gDataSize;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
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
    for (int i = 1; i < argc; ++i) {
        printf("arg %2d = %s\n", i, (argv[i]));
    }
    if (argc == 3) {
        gDataSize = atoi(argv[1]);
        NUM_ITERATIONS = atoi(argv[2]);
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
    if (strcmp(cisInterface, "rpc") != 0) {
        EXPECT_NO_THROW(cis = new Fam_CIS_Client(cisServer, atoi(rpcPort)));
    } else {
        EXPECT_NO_THROW(cis = new Fam_CIS_Direct(NULL, true, false));
    }
#endif

    const char *dataItem = get_uniq_str("firstGlobal", my_fam);
    const char *testRegion = get_uniq_str("testGlobal", my_fam);

    EXPECT_NO_THROW(myPE = (int *)my_fam->fam_get_option(strdup("PE_ID")));
    EXPECT_NO_THROW(desc = my_fam->fam_create_region(
                        testRegion, BIG_REGION_SIZE, 0777, RAID1));

    test_perm_mode = ALL_PERM;
    test_item_size = gDataSize * 4;
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(dataItem, test_item_size,
                                                test_perm_mode, desc));

    // EXPECT_NO_THROW(item = my_fam->fam_lookup(dataItem, testRegion));
    EXPECT_NE((void *)NULL, item);

    uint64_t testOffset = 0;
    for (int i = 0; i < 10; i++) {
        EXPECT_NO_THROW(my_fam->fam_fetch_int32(item, testOffset));
    }

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
#ifdef MEMSERVER_PROFILE
    EXPECT_NO_THROW(cis->reset_profile());
    EXPECT_NO_THROW(fabric_reset_profile());
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
#endif
    ret = RUN_ALL_TESTS();

    cout << "finished all testing" << endl;
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete item;
    delete desc;
    free((void *)dataItem);
    free((void *)testRegion);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    cout << "Finalize done : " << ret << endl;
    delete my_fam;
    return ret;
}
