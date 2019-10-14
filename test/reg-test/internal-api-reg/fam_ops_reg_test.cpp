/*
 * fam_ops_reg_test.cpp
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

#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>
#include <fam/fam_exception.h>

#include "common/fam_ops.h"
#include "common/fam_ops_libfabric.h"
#include "common/fam_ops_nvmm.h"

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

Fam_Ops *famOps;

// Test case#1
TEST(FamOpsLibfabricInternal, FamUnimplemented) {
    const char *name = "127.0.0.1";
    const char *service = "1500";
    char *provider = strdup("sockets");

    famOps =
        new Fam_Ops_Libfabric(name, service, true, provider,
                              FAM_THREAD_MULTIPLE, NULL, FAM_CONTEXT_DEFAULT);

    EXPECT_THROW(famOps->abort(0), Fam_Unimplemented_Exception);

    delete famOps;
    free(provider);
}

// Test case#2
TEST(FamOpsNvmmInternal, FamUnimplemented) {

    famOps =
        new Fam_Ops_NVMM(FAM_THREAD_MULTIPLE, FAM_CONTEXT_DEFAULT, NULL, 1);

    EXPECT_THROW(famOps->abort(0), Fam_Unimplemented_Exception);

    EXPECT_THROW(famOps->fence(NULL), Fam_Unimplemented_Exception);

    delete famOps;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
