/*
 * fam_noperm_test.cpp
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
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

int main() {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    int pass = 0, fail = 0;

    memset((void *)&fam_opts, 0, sizeof(Fam_Options));

    fam_opts.memoryServer = strdup(TEST_MEMORY_SERVER);
    fam_opts.grpcPort = strdup(TEST_GRPC_PORT);
    fam_opts.libfabricPort = strdup(TEST_LIBFABRIC_PORT);
    fam_opts.allocator = strdup(TEST_ALLOCATOR);
    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed" << endl;
        exit(1);
    }

    desc = my_fam->fam_create_region("test1", 8192, 0777, RAID1);
    if (desc == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    }
    // Allocating data items in the created region with READ perm only
    item = my_fam->fam_allocate("first", 1024, 0444, desc);
    if (item == NULL) {
        cout << "fam allocation of data item 'first' failed" << endl;
    }

    char *local = strdup("Test message");
    try {
        // write should fail as we have read-only perm
        my_fam->fam_put_blocking(local, item, 0, 13);
    } catch (Fam_Exception &e) {
        cout << "fam_put_blocking failed as expected with no permission"
             << endl;
        pass++;
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    // Deallocating data items should not fail
    if (item != NULL) {
        cout << "item deallocated" << endl;
        my_fam->fam_deallocate(item);
    } else {
        cout << "item is NULL" << endl;
        fail++;
    }

    // Destroying the region
    if (desc != NULL)
        my_fam->fam_destroy_region(desc);

    // Create region with read-only perm
    desc = my_fam->fam_create_region("test1", 8192, 0444, RAID1);
    if (desc == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    }
    // Allocating data items should pass
    item = my_fam->fam_allocate("first", 1024, 0777, desc);
    if (item == NULL) {
        fail++;
        cout << "fam allocation of data item failed" << endl;
    } else {
        pass++;
        cout << "fam allocation of data item succeeded as expected" << endl;
    }

    // Destroying the region should pass
    if (desc != NULL) {
        cout << "region deallocated" << endl;
        my_fam->fam_destroy_region(desc);
    }

    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;

    if (pass == 2) {
        cout << "Test passed. Pass=" << pass << endl;
        return 0;
    } else {
        cout << "Test failed. Pass=" << pass << endl;
        return -1;
    }
}
