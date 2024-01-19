/*
 * fam_region_registration_basic.cpp
 * Copyright (c) 2023 Hewlett Packard Enterprise Development, LP. All rights
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

#define NUM_REGIONS 3
#define NUM_DATAITEMS 2
#define REGION_SIZE 1048576
#define DATAITEM_SIZE 1024

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;

// Test case 1 - put get test.
TEST(RegionRegistration, Basic) {
    Fam_Region_Descriptor *desc[NUM_REGIONS];
    Fam_Descriptor *item[NUM_DATAITEMS];

    // Local buffer with data
    char *local = strdup("Test message");

    // allocate local memory to receive 20 elements to recieve remote data
    char *local2 = (char *)malloc(20);

    string regionNames[NUM_REGIONS];
    string itemNames[NUM_DATAITEMS];

    for (int i = 0; i < NUM_REGIONS; i++) {
        regionNames[i] = "test" + to_string(i);
    }

    for (int i = 0; i < NUM_DATAITEMS; i++) {
        const char *name = get_uniq_str("item", my_fam);
        itemNames[i] = name;
        itemNames[i] = itemNames[i] + to_string(i);
    }

    int *myPE;
    EXPECT_NO_THROW(myPE = (int *)my_fam->fam_get_option(strdup("PE_ID")));
    EXPECT_NE((void *)NULL, myPE);

    // Only one PE(PE ID 0) creates all the regions
    if (*myPE == 0) {
        Fam_Region_Attributes *regionAttributes = new Fam_Region_Attributes();
        regionAttributes->permissionLevel = REGION;

        for (int i = 0; i < NUM_REGIONS; i++) {
            EXPECT_NO_THROW(desc[i] = my_fam->fam_create_region(
                                regionNames[i].c_str(), REGION_SIZE, 0777,
                                regionAttributes));
            EXPECT_NE((void *)NULL, desc[i]);
        }
        delete regionAttributes;
    }

    my_fam->fam_barrier_all();

    // Other PEs lookup for region
    if (*myPE != 0) {
        for (int i = 0; i < NUM_REGIONS; i++) {
            EXPECT_NO_THROW(
                desc[i] = my_fam->fam_lookup_region(regionNames[i].c_str()));
            EXPECT_NE((void *)NULL, desc[i]);
        }
    }

    // Allocation in each region
    for (int i = 0; i < NUM_REGIONS; i++) {
        for (int j = 0; j < NUM_DATAITEMS; j++) {
            // Allocating first data item in the created region
            EXPECT_NO_THROW(item[j] = my_fam->fam_allocate(itemNames[j].c_str(),
                                                           DATAITEM_SIZE, 0777,
                                                           desc[i]));
            EXPECT_NE((void *)NULL, item[j]);

            // Following data path operations need not do any additional
            // descriptor validation as the descriptor is already validated.
            EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item[j], 0, 13));

            EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, item[j], 0, 13));

            EXPECT_STREQ(local, local2);

            EXPECT_NO_THROW(item[j] = my_fam->fam_lookup(
                                itemNames[j].c_str(), regionNames[i].c_str()));
            EXPECT_NE((void *)NULL, item[j]);

            // Following data path operations goes through an additional
            // descriptor validation as the descriptor is not validated.
            EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item[j], 0, 13));

            EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, item[j], 0, 13));

            EXPECT_STREQ(local, local2);

            EXPECT_NO_THROW(my_fam->fam_deallocate(item[j]));
            delete item[j];
        }
    }

    my_fam->fam_barrier_all();

    if (*myPE == 0) {
        for (int i = 0; i < NUM_REGIONS; i++) {
            EXPECT_NO_THROW(my_fam->fam_destroy_region(desc[i]));
        }
    }

    for (int i = 0; i < NUM_REGIONS; i++) {
        delete desc[i];
    }
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
