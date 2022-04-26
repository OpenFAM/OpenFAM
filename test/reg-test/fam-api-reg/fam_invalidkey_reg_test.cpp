/*
 * fam_invalidkey_reg_test.cpp
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

#include "common/fam_libfabric.h"
#include "common/fam_ops_libfabric.h"
#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;

// Test case 1 - put get test.
TEST(FamInvalidKey, InvalidKeySuccess) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    char *message = strdup("Test message");
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    uint64_t invalidKey = -1;

    char *name = (char *)my_fam->fam_get_option(strdup("CIS_SERVER"));
    char *provider =
        (char *)my_fam->fam_get_option(strdup("LIBFABRIC_PROVIDER"));
    char *if_device = (char *)my_fam->fam_get_option(strdup("IF_DEVICE"));
    char *cisInterfaceType =
        (char *)my_fam->fam_get_option(strdup("CIS_INTERFACE_TYPE"));
    char *rpcPort = (char *)my_fam->fam_get_option(strdup("GRPC_PORT"));
    Fam_Allocator_Client *famAllocator = NULL;
    if (strcmp(cisInterfaceType, "rpc") == 0) {
        famAllocator = new Fam_Allocator_Client(name, atoi(rpcPort));
    } else {
        famAllocator = new Fam_Allocator_Client();
    }
    Fam_Ops_Libfabric *famOps =
        new Fam_Ops_Libfabric(false, provider, if_device, FAM_THREAD_MULTIPLE,
                              famAllocator, FAM_CONTEXT_DEFAULT);

    EXPECT_NO_THROW(famOps->initialize());

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 8192, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t nodeId = item->get_memserver_ids()[0];

    EXPECT_THROW(fabric_write(invalidKey, message, 25, 0,
                              (*famOps->get_fiAddrs())[nodeId],
                              famOps->get_defaultCtx(item)),
                 Fam_Exception);

    char *buff = (char *)malloc(1024);
    memset(buff, 0, 1024);

    EXPECT_THROW(fabric_read(invalidKey, buff, 25, 0,
                             (*famOps->get_fiAddrs())[nodeId],
                             famOps->get_defaultCtx(item)),
                 Fam_Exception);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    EXPECT_NO_THROW(famOps->finalize());

    delete item;
    delete desc;
    delete famAllocator;
    delete famOps;
    free((void *)testRegion);
    free((void *)firstItem);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    char *openFamModel =
        (char *)my_fam->fam_get_option(strdup("OPENFAM_MODEL"));

    if (strcmp(openFamModel, "memory_server") != 0) {
        my_fam->fam_finalize("default");
        std::cout << "Test case valid only in memory server model, "
                     "skipping with status : "
                  << TEST_SKIP_STATUS << std::endl;
        return TEST_SKIP_STATUS;
    }

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
