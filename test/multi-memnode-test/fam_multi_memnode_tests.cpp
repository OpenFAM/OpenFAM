/*
 * fam_multi_memnode_tests.cpp
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
#include <stdlib.h>
#include <string.h>

#include <fam/fam.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

#define NUM_ITERATIONS 50
#define NAME_BUFF_SIZE 255

fam *my_fam;
Fam_Options fam_opts;

// Test case 1 - creating rgions with random names.
TEST(FamMultiMemNode, RandomNames) {
    Fam_Region_Descriptor *desc[5];
    Fam_Descriptor *item;
    char *local = strdup("Test message");
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *testRegion2 = get_uniq_str("hello", my_fam);
    const char *testRegion3 = get_uniq_str("world", my_fam);
    const char *testRegion4 = get_uniq_str("Welcome_to_OpenFAM@2019", my_fam);
    const char *testRegion5 = get_uniq_str("HPE@Bangalore", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc[0] = my_fam->fam_create_region(testRegion, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    cout << "Region with name " << testRegion
         << " created in memory node : " << desc[0]->get_memserver_id() << endl;

    EXPECT_NO_THROW(
        desc[1] = my_fam->fam_create_region(testRegion2, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    cout << "Region with name " << testRegion2
         << " created in memory node : " << desc[1]->get_memserver_id() << endl;

    EXPECT_NO_THROW(
        desc[2] = my_fam->fam_create_region(testRegion3, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    cout << "Region with name " << testRegion3
         << " created in memory node : " << desc[2]->get_memserver_id() << endl;

    EXPECT_NO_THROW(
        desc[3] = my_fam->fam_create_region(testRegion4, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    cout << "Region with name " << testRegion4
         << " created in memory node : " << desc[3]->get_memserver_id() << endl;

    EXPECT_NO_THROW(
        desc[4] = my_fam->fam_create_region(testRegion5, 8192, 0777, RAID1));
    EXPECT_NE((void *)NULL, desc);

    cout << "Region with name " << testRegion5
         << " created in memory node : " << desc[4]->get_memserver_id() << endl;

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 1024, 0777, desc[4]));
    EXPECT_NE((void *)NULL, item);

    EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item, 0, 13));

    // allocate local memory to receive 20 elements
    char *local2 = (char *)malloc(20);

    EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, item, 0, 13));

    EXPECT_STREQ(local, local2);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    for (int i = 0; i < 5; i++)
        EXPECT_NO_THROW(my_fam->fam_destroy_region(desc[i]));

    delete item;
    for (int i = 0; i < 5; i++)
        delete desc[i];

    free((void *)testRegion);
    free((void *)testRegion2);
    free((void *)testRegion3);
    free((void *)testRegion4);
    free((void *)testRegion5);
    free((void *)firstItem);
}

// Test case 2 - creating region with name having sequential suffixes.
TEST(FamMultiMemNode, SequentialSuffixName) {
    Fam_Region_Descriptor *desc[NUM_ITERATIONS];
    const char *testRegion = get_uniq_str("test", my_fam);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        char regionInfo[NAME_BUFF_SIZE];
        sprintf(regionInfo, "%s_%d", testRegion, i);
        EXPECT_NO_THROW(
            desc[i] = my_fam->fam_create_region(regionInfo, 8192, 0777, RAID1));
        EXPECT_NE((void *)NULL, desc);
        cout << "Region with name " << regionInfo
             << " created in memory node : " << desc[i]->get_memserver_id()
             << endl;
    }

    for (int i = 0; i < NUM_ITERATIONS; i++)
        EXPECT_NO_THROW(my_fam->fam_destroy_region(desc[i]));

    for (int i = 0; i < NUM_ITERATIONS; i++)
        delete desc[i];

    free((void *)testRegion);
}

// Test case 3 - creating regions with name having random suffixes.
TEST(FamMultiMemNode, RandomSuffixName) {
    Fam_Region_Descriptor *desc[NUM_ITERATIONS];
    const char *testRegion = get_uniq_str("test", my_fam);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        char regionInfo[NAME_BUFF_SIZE];
        sprintf(regionInfo, "%s_%ld", testRegion, random());
        EXPECT_NO_THROW(
            desc[i] = my_fam->fam_create_region(regionInfo, 8192, 0777, RAID1));
        EXPECT_NE((void *)NULL, desc);
        cout << "Region with name " << regionInfo
             << " created in memory node : " << desc[i]->get_memserver_id()
             << endl;
    }

    for (int i = 0; i < NUM_ITERATIONS; i++)
        EXPECT_NO_THROW(my_fam->fam_destroy_region(desc[i]));

    for (int i = 0; i < NUM_ITERATIONS; i++)
        delete desc[i];

    free((void *)testRegion);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
