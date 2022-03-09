/* fam_context_reg_test.cpp
 * Copyright (c) 2022 Hewlett Packard Enterprise Development, LP. All rights
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
/* Test Case Description: Tests fam_context_open and fam_context_close
 * operations for single threaded model.
 */

#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;
fam *my_fam;
Fam_Options fam_opts;
Fam_Region_Descriptor *testRegionDesc;
const char *testRegionStr;
#define NUM_ITERATIONS 100
#define NUM_IO_ITERATIONS 5
#define NUM_CONTEXTS 256
typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t deltaValue;
} ValueInfo;

// Test case 1 - FamContextOpenCloseStressTest
TEST(FamContext, FamContextOpenCloseStressTest) {
    fam_context *ctx;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        EXPECT_NO_THROW(ctx = my_fam->fam_context_open());
        EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
    }
}
// Test case 2 - FamContextOpenCloseWithIOOperation
TEST(FamContext, FamContextOpenCloseWithIOOperation) {
    Fam_Region_Descriptor *rd = NULL;
    Fam_Descriptor *descriptor = NULL;
    fam_context *ctx;
    EXPECT_NO_THROW(ctx = my_fam->fam_context_open());

    // create a 100 MB region with 0777 permissions
    EXPECT_NO_THROW(rd = my_fam->fam_create_region(
                        "myRegion", (uint64_t)10000000, 0777, NULL));
    // use the created region...
    EXPECT_NE((void *)NULL, rd);
    EXPECT_NO_THROW(descriptor = my_fam->fam_allocate(
                        "myItem", (uint64_t)(50 * sizeof(int)), 0600, rd));

    EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)1));
    EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)2));
    EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)3));
    EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)4));
    EXPECT_NO_THROW(my_fam->fam_or(descriptor, 0, (uint32_t)4));
    EXPECT_NO_THROW(ctx->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_quiet());

    // We are done with the operations. Destroy the region and everything in it
    EXPECT_NO_THROW(my_fam->fam_destroy_region(rd));

    EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
}

// Test case 3 - FamContextOpenCloseWithIOOperationStressTest
TEST(FamContext, FamContextOpenCloseWithIOOperationStressTest) {
    Fam_Region_Descriptor *rd = NULL;
    Fam_Descriptor *descriptor = NULL;
    fam_context *ctx;

    // create a 100 MB region with 0777 permissions
    EXPECT_NO_THROW(rd = my_fam->fam_create_region(
                        "myRegion", (uint64_t)10000000, 0777, NULL));
    // use the created region...

    EXPECT_NE((void *)NULL, rd);
    EXPECT_NO_THROW(descriptor = my_fam->fam_allocate(
                        "myItem", (uint64_t)(50 * sizeof(int)), 0600, rd));

    for (int i = 0; i < NUM_IO_ITERATIONS; i++) {
        EXPECT_NO_THROW(ctx = my_fam->fam_context_open());

        EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)1));
        EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)2));
        EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)3));
        EXPECT_NO_THROW(ctx->fam_or(descriptor, 0, (uint32_t)4));
        EXPECT_NO_THROW(my_fam->fam_or(descriptor, 0, (uint32_t)4));
        EXPECT_NO_THROW(ctx->fam_quiet());
        EXPECT_NO_THROW(my_fam->fam_quiet());

        // We are done with the operations. Destroy the region and everything in
        // it
        EXPECT_NO_THROW(my_fam->fam_destroy_region(rd));

        EXPECT_NO_THROW(my_fam->fam_context_close(ctx));
    }
}
// Test case 4 - FamContextSimultaneousOpenCloseStressTest
TEST(FamContext, FamContextSimultaneousOpenCloseStressTest) {

    fam_context *ctx[NUM_CONTEXTS];
    for (int i = 0; i < NUM_CONTEXTS; i++) {
        EXPECT_NO_THROW(ctx[i] = my_fam->fam_context_open());
    }
    for (int i = 1; i < NUM_CONTEXTS; i++) {
        EXPECT_NO_THROW(my_fam->fam_context_close(ctx[i]));
    }
}

int main(int argc, char **argv) {
    int ret = 0;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
