/*
 * fam_mm_maxconfig_test.cpp
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
#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

#include "fam_utils.h"
#include "common/fam_test_config.h"

#define REG_MAX_SIZE 1099511627776

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;

TEST(FamMMTest, FamCreateRegionMaxSizeSuccess) {
    Fam_Region_Descriptor *desc = NULL;
    const char *testRegion = get_uniq_str("mmtest", my_fam);

    EXPECT_NO_THROW(desc = my_fam->fam_create_region(testRegion, REG_MAX_SIZE,
                                                     0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    free((void *)testRegion);
}

TEST(FamMMNegativeTest, FamCreateRegionGreaterMaxSizeFailure) {
    const char *testRegion = get_uniq_str("mm_negative_test", my_fam);

    EXPECT_THROW(
        my_fam->fam_create_region(testRegion, 2199023255552, 0777, RAID1),
        Fam_Exception);
    free((void *)testRegion);
}

TEST(FamMMTest, FamAllocateMaxSizeSuccess) {
    Fam_Region_Descriptor *desc = NULL;
    Fam_Descriptor *item = NULL;
    const char *testRegion = get_uniq_str("mm_test", my_fam);
    const char *dataItem = get_uniq_str("mm_data", my_fam);

    EXPECT_NO_THROW(desc = my_fam->fam_create_region(testRegion, REG_MAX_SIZE,
                                                     0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_NO_THROW(
        item = my_fam->fam_allocate(dataItem, 549755813888, 0444, desc));
    EXPECT_NE((void *)NULL, item);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    delete item;
    free((void *)testRegion);
    free((void *)dataItem);
}

TEST(FamMMNegativeTest, FamAllocateGreaterMaxSizeFailure) {
    Fam_Region_Descriptor *desc = NULL;
    const char *testRegion = get_uniq_str("mm_negative_test", my_fam);
    const char *dataItem = get_uniq_str("mm_data", my_fam);

    EXPECT_NO_THROW(desc = my_fam->fam_create_region(testRegion, REG_MAX_SIZE,
                                                     0777, RAID1));
    EXPECT_NE((void *)NULL, desc);
    EXPECT_THROW(my_fam->fam_allocate(dataItem, 824633720832, 0444, desc),
                 Fam_Exception);
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    free((void *)testRegion);
    free((void *)dataItem);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    delete my_fam;

    return ret;
}
