/*
 * fam_options_reg_test.cpp
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
#include <sstream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>
#include <fam/fam_exception.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;

#ifndef OPENFAM_VERSION
#define OPENFAM_VERSION "0.0.0"
#endif

// Test case1 - Get Defualt option list
TEST(FamOption, GetDefaultOptionList) {
    const char **optList;

    optList = my_fam->fam_list_options();

    EXPECT_NE((void *)NULL, optList);
    if (optList != NULL) {
        EXPECT_STREQ(optList[1], "DEFAULT_REGION_NAME");
        EXPECT_STREQ(optList[2], "CIS_SERVER");
        EXPECT_STREQ(optList[3], "GRPC_PORT");
        EXPECT_STREQ(optList[4], "LIBFABRIC_PROVIDER");
        EXPECT_STREQ(optList[5], "FAM_THREAD_MODEL");
        EXPECT_STREQ(optList[6], "CIS_INTERFACE_TYPE");
        EXPECT_STREQ(optList[7], "OPENFAM_MODEL");
        EXPECT_STREQ(optList[8], "FAM_CONTEXT_MODEL");
        EXPECT_STREQ(optList[9], "PE_COUNT");
        EXPECT_STREQ(optList[10], "PE_ID");
        EXPECT_STREQ(optList[11], "RUNTIME");
        EXPECT_STREQ(optList[12], "NUM_CONSUMER");
        EXPECT_STREQ(optList[13], "FAM_DEFAULT_MEMORY_TYPE");
        EXPECT_STREQ(optList[14], "IF_DEVICE");
    }
}

// Test case 2 - Get defualt option values
TEST(FamOption, GetDefaultOptionValue) {
    char *optValue;
    char *opt;
    int *peCnt;

    opt = strdup("VERSION");
    optValue = (char *)my_fam->fam_get_option(opt);
    EXPECT_NE((char *)NULL, optValue);
    EXPECT_STREQ(optValue, OPENFAM_VERSION);
    free(opt);
    free(optValue);

    opt = strdup("GRPC_PORT");
    optValue = (char *)my_fam->fam_get_option(opt);
    EXPECT_NE((char *)NULL, optValue);
    free(opt);
    free(optValue);

    opt = strdup("PE_COUNT");
    peCnt = (int *)my_fam->fam_get_option(opt);
    free(opt);
    free(peCnt);
}

// Test case 3 - Query with invalid option.
TEST(FamOption, QueryWithInavlidOption) {
    char *opt;

    opt = strdup("NotAnOption");
    EXPECT_THROW(my_fam->fam_get_option(opt), Fam_Exception);
    free(opt);
}

// Test case 4 - Register a client buffer to the default fam class
TEST(FamOption, ClientBufferRegistrationToDefaultFam) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(desc = my_fam->fam_create_region(testRegion, 8192, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    char *opt;
    opt = strdup("FAM_CLIENT_BUFFER");

    Fam_Client_Buffer *bufRegInfo1 = new Fam_Client_Buffer();
    bufRegInfo1->buffer = (void *)strdup("Test message");
    bufRegInfo1->bufferSize = strlen((char *)bufRegInfo1->buffer) + 1;
    bufRegInfo1->op = REGISTER;

    EXPECT_NO_THROW(my_fam->fam_set_option(opt, (void *)bufRegInfo1, sizeof(Fam_Client_Buffer)));

    EXPECT_NO_THROW(my_fam->fam_put_blocking(bufRegInfo1->buffer, item, 0, 13));

    // allocate local memory to receive the test message
    Fam_Client_Buffer *bufRegInfo2 = new Fam_Client_Buffer();
    bufRegInfo2->buffer = (void *)malloc(20);
    bufRegInfo2->bufferSize = 20;
    bufRegInfo2->op = REGISTER;

    EXPECT_NO_THROW(my_fam->fam_set_option(opt, (void *)bufRegInfo2, sizeof(Fam_Client_Buffer)));

    EXPECT_NO_THROW(my_fam->fam_get_blocking(bufRegInfo2->buffer, item, 0, 13));

    EXPECT_STREQ((char *)bufRegInfo1->buffer, (char *)bufRegInfo2->buffer);

    bufRegInfo1->op = DEREGISTER;
    bufRegInfo2->op = DEREGISTER;
    EXPECT_NO_THROW(my_fam->fam_set_option(opt, (void *)bufRegInfo1, sizeof(Fam_Client_Buffer)));
    EXPECT_NO_THROW(my_fam->fam_set_option(opt, (void *)bufRegInfo2, sizeof(Fam_Client_Buffer)));

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);

    free(bufRegInfo1->buffer);
    free(bufRegInfo2->buffer);

    delete bufRegInfo1;
    delete bufRegInfo2;
}

// Test case 5 - Register a client buffer to the new fam context
TEST(FamOption, ClientBufferRegistrationToFamContext) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(desc = my_fam->fam_create_region(testRegion, 8192, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(firstItem, 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    fam_context *ctx = NULL;
    EXPECT_NO_THROW(ctx = my_fam->fam_context_open());
    EXPECT_NE((void *)NULL, ctx);

    char *opt;
    opt = strdup("FAM_CLIENT_BUFFER");

    Fam_Client_Buffer *bufRegInfo1 = new Fam_Client_Buffer();
    bufRegInfo1->buffer = (void *)strdup("Test message");
    bufRegInfo1->bufferSize = strlen((char *)bufRegInfo1->buffer) + 1;
    bufRegInfo1->ctx = ctx;
    bufRegInfo1->op = REGISTER;

    EXPECT_NO_THROW(my_fam->fam_set_option(opt, (void *)bufRegInfo1, sizeof(Fam_Client_Buffer)));

    EXPECT_NO_THROW(ctx->fam_put_blocking(bufRegInfo1->buffer, item, 0, 13));

    // allocate local memory to receive the test message
    Fam_Client_Buffer *bufRegInfo2 = new Fam_Client_Buffer();
    bufRegInfo2->buffer = (void *)malloc(20);
    bufRegInfo2->bufferSize = 20;
    bufRegInfo2->ctx = ctx;
    bufRegInfo2->op = REGISTER;

    EXPECT_NO_THROW(my_fam->fam_set_option(opt, (void *)bufRegInfo2, sizeof(Fam_Client_Buffer)));

    EXPECT_NO_THROW(ctx->fam_get_blocking(bufRegInfo2->buffer, item, 0, 13));

    EXPECT_STREQ((char *)bufRegInfo1->buffer, (char *)bufRegInfo2->buffer);

    bufRegInfo1->op = DEREGISTER;
    bufRegInfo2->op = DEREGISTER;
    EXPECT_NO_THROW(my_fam->fam_set_option(opt, (void *)bufRegInfo1, sizeof(Fam_Client_Buffer)));
    EXPECT_NO_THROW(my_fam->fam_set_option(opt, (void *)bufRegInfo2, sizeof(Fam_Client_Buffer)));

    EXPECT_NO_THROW(my_fam->fam_context_close(ctx));

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);

    free(bufRegInfo1->buffer);
    free(bufRegInfo2->buffer);

    delete bufRegInfo1;
    delete bufRegInfo2;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    my_fam = new fam();
    int ret;
    init_fam_options(&fam_opts);
    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));
    ret = RUN_ALL_TESTS();
    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    return ret;
}
