/*
 * fam_control_path_reg_test.cpp
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
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>

#include "common/fam_test_config.h"
#define BIG_REGION_SIZE 214748364
#define BLOCK_SIZE 1048576
#define RESIZE_REGION_SIZE 214748364
#define NAME_BUFF_SIZE 255
#define DATA_ITEM_SIZE 1048576

using namespace std;
using namespace openfam;

int *myPE;
fam *my_fam;
Fam_Options fam_opts;
int NUM_MM_ITERATIONS = 100;

// Test case 1 - fam_copy and fam_copy_wait test (success).
TEST(FamControlPath, CreateDestroyRegion) {
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
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    free((void *)testRegionLocal);
}

TEST(FamControlPath, FamAllocateDellocateMultiple1) {
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
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(itemLocal[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal));
    free((void *)testRegionLocal);
    free((void *)firstItemLocal);
}

// Test case 4 - Region Permission Change in loop
TEST(FamControlPath, RegionChangePermission) {
    Fam_Region_Descriptor **descLocal =
        new Fam_Region_Descriptor *[NUM_MM_ITERATIONS];
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);

    mode_t perm;
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char regionInfo[NAME_BUFF_SIZE];
        sprintf(regionInfo, "%s_%d", testRegionLocal, i);
        EXPECT_NO_THROW(descLocal[i] = my_fam->fam_create_region(
                            regionInfo, BLOCK_SIZE, 0777, NULL));
        EXPECT_NE((void *)NULL, descLocal[i]);
        perm = (mode_t)i;
        my_fam->fam_change_permissions(descLocal[i], perm);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    free((void *)testRegionLocal);
}

// Test case 4 - Data Item Permission Change in loop
TEST(FamControlPath, DataItemChangePermission) {
    Fam_Descriptor **itemLocal = new Fam_Descriptor *[NUM_MM_ITERATIONS];
    Fam_Region_Descriptor *descLocal;
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);

    EXPECT_NO_THROW(descLocal = my_fam->fam_create_region(
                        testRegionLocal, BIG_REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, descLocal);

    mode_t perm;
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        char itemInfo[NAME_BUFF_SIZE];
        sprintf(itemInfo, "%s_%d", firstItemLocal, i);
        // Allocating data items
        EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_allocate(
                            itemInfo, DATA_ITEM_SIZE, 0777, descLocal));
        EXPECT_NE((void *)NULL, itemLocal[i]);
        perm = (mode_t)i;
        my_fam->fam_change_permissions(itemLocal[i], perm);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    for (int i = 0; i < NUM_MM_ITERATIONS; i++) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(itemLocal[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal));
    free((void *)testRegionLocal);
    free((void *)firstItemLocal);
}
TEST(FamControlPath, FamResizeRegion) {
    Fam_Region_Descriptor *descLocal;
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);
    EXPECT_NO_THROW(descLocal = my_fam->fam_create_region(
                        testRegionLocal, RESIZE_REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, descLocal);

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    EXPECT_NO_THROW(my_fam->fam_stat(descLocal, info));
    EXPECT_NO_THROW(my_fam->fam_resize_region(descLocal, info->size * 2));
    free(info);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal));
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
