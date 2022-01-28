/*
 * fam_invalidkey_test.cpp
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
#include <iostream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>

#include "common/fam_libfabric.h"
#include "common/fam_ops_libfabric.h"

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

int main() {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    Fam_Global_Descriptor global;

    memset((void *)&fam_opts, 0, sizeof(Fam_Options));

    Fam_Ops_Libfabric *famOps = new Fam_Ops_Libfabric;

    int ret = 0;

    char *buff = (char *)malloc(1024);
    memset(buff, 0, 1024);

    // Initialize the libfabric ops
    char *message = strdup("This is the datapath test");

    init_fam_options(&fam_opts);
    ret = famOps->initialize(name, service, false, provider);
    if (ret < 0) {
        cout << "famOps initialization failed" << endl;
        exit(1);
    }

    // Initialize openFAM API
    fam_opts.memoryServer = strdup(TEST_MEMORY_SERVER);
    fam_opts.grpcPort = strdup(TEST_GRPC_PORT);
    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed" << endl;
        exit(1);
    }

    // Create Region
    desc = my_fam->fam_create_region("test", 8192, 0777, NULL);
    if (desc == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    } else {
        global = desc->get_global_descriptor();
        cout << " Fam_Region_Descriptor { Region ID : 0x" << hex << uppercase
             << global.regionId << ", Offset : 0x" << global.offset << " }"
             << endl;
    }

    // Allocating data items in the created region
    item = my_fam->fam_allocate("first", 1024, 0777, desc);
    if (item == NULL) {
        cout << "fam allocation of dataitem 'first' failed" << endl;
    } else {
        global = item->get_global_descriptor();
        cout << " Fam_Descriptor { Region ID : 0x" << hex << uppercase
             << global.regionId << ", Offset : 0x" << global.offset
             << ", Key : 0x" << item->get_key() << " }" << endl;
    }

    // Write with valid key
    ret =
        fabric_write(item->get_key(), message, 25, 0,
                     (*famOps->get_fiAddrs())[0], famOps->get_defaultCtx(item));

    if (ret < 0) {
        cout << "fabric write failed" << endl;
        exit(1);
    }

    // Read with valid key
    ret = fabric_read(item->get_key(), buff, 25, 0, (*famOps->get_fiAddrs())[0],
                      famOps->get_defaultCtx(item));
    if (ret < 0) {
        cout << "fabric read failed" << endl;
        exit(1);
    }

    cout << "With valid key : " << endl;
    cout << "key = " << item->get_key() << endl;
    cout << "buffer = " << buff << endl;

    uint64_t invalidKey = -1;
    // Write with invalid key
    ret = fabric_write(invalidKey, message, 25, 0, (*famOps->get_fiAddrs())[0],
                       famOps->get_defaultCtx(item));

    if (ret < 0) {
        cout << "fabric write failed" << endl;
        exit(1);
    }

    // Read with invalid key
    ret = fabric_read(invalidKey, buff, 25, 0, (*famOps->get_fiAddrs())[0],
                      famOps->get_defaultCtx(item));
    if (ret < 0) {
        cout << "fabric read failed" << endl;
        exit(1);
    }

    cout << "With invalid key : " << endl;
    cout << "key = " << item->get_key() << endl;
    cout << "buffer = " << buff << endl;

    // Deallocating data items
    if (item != NULL)
        my_fam->fam_deallocate(item);

    // Destroying the regioin
    if (desc != NULL)
        my_fam->fam_destroy_region(desc);

    famOps->finalize();
    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;
}
