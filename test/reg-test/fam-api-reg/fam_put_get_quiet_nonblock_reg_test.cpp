/*
 * fam_put_get_quiet_nonblock_reg_test.cpp
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

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;

// Test case 1 - put get test.
TEST(FamPutGetNonblock, PutGetNonblockSuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item[10];
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    for (int i = 0; i < 10; i++) {
        char itemInfo[255];
        sprintf(itemInfo, "%s_%d", firstItem, i);
        EXPECT_NO_THROW(item[i] =
                            my_fam->fam_allocate(itemInfo, 1024, 0777, desc));
        EXPECT_NE((void *)NULL, item[i]);
    }

    char buff[50];
    char *local[10];
    for (int i = 0; i < 10; i++) {
        sprintf(buff, "Test message-%d", i);
        local[i] = strdup(buff);
        EXPECT_NO_THROW(my_fam->fam_put_nonblocking(local[i], item[i], 0, 15));
    }

    EXPECT_NO_THROW(my_fam->fam_quiet());

    // allocate local memory to receive 20 elements

    char *local2[10];
    for (int i = 0; i < 10; i++) {
        local2[i] = (char *)malloc(20);
        EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local2[i], item[i], 0, 15));
    }

    EXPECT_NO_THROW(my_fam->fam_quiet());

    for (int i = 0; i < 10; i++) {
        EXPECT_STREQ(local[i], local2[i]);
    }

    for (int i = 0; i < 10; i++) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(item[i]));
    }
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    for (int i = 0; i < 10; i++)
        delete item[i];
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
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
