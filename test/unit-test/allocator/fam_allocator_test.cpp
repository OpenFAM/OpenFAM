/*
 * fam_allocator_test.cpp
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
#include "common/fam_options.h"
#include "fam_utils.h"

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

int main() {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *desc = NULL;
    Fam_Descriptor *item = NULL;

    memset((void *)&fam_opts, 0, sizeof(Fam_Options));

    // Initialize openFAM API
    fam_opts.memoryServer = strdup(TEST_MEMORY_SERVER);
    fam_opts.grpcPort = strdup(TEST_GRPC_PORT);
    // fam_opts.allocator = strdup(TEST_ALLOCATOR);
    fam_opts.allocator = strdup("NVMM");
    fam_opts.runtime = strdup("NONE");
    if (my_fam->fam_initialize("default", &fam_opts) < 0) {
        cout << "fam initialization failed" << endl;
        exit(1);
    } else {
        cout << "fam initialization successful" << endl;
    }

    // Create Region
    bool pass = true;
    create_region(pass, my_fam, desc, "test", 8192, 0444, RAID1);
    if (!pass) {
        if (desc != NULL)
            destroy_region(pass, my_fam, desc);
        exit(1);
    }

    // Change the permission of region
    change_permission(pass, my_fam, desc, 0777);
    if (!pass) {
        if (item != NULL)
            destroy_region(pass, my_fam, desc);
        exit(1);
    }

    // Allocating data items in the created region
    allocate(pass, my_fam, item, "first", 1024, 0444, desc);
    if (!pass) {
        destroy_region(pass, my_fam, desc);
        exit(1);
    }

    // Change the permission of dataitem
    change_permission(pass, my_fam, item, 0777);
    if (!pass) {
        if (item != NULL)
            deallocate(pass, my_fam, item);
        if (desc != NULL)
            destroy_region(pass, my_fam, desc);
        exit(1);
    }

    // Lookup for region
    lookup_region(pass, my_fam, desc, "test");
    if (!pass) {
        if (item != NULL)
            deallocate(pass, my_fam, item);
        if (desc != NULL)
            destroy_region(pass, my_fam, desc);
        exit(1);
    }

    // lookup for dataitem
    lookup(pass, my_fam, item, "first", "test");
    if (!pass) {
        if (item != NULL)
            deallocate(pass, my_fam, item);
        if (desc != NULL)
            destroy_region(pass, my_fam, desc);
        exit(1);
    }

    // Deallocating data items
    if (item != NULL)
        deallocate(pass, my_fam, item);

    // Destroying the regioin
    if (desc != NULL)
        destroy_region(pass, my_fam, desc);

    // fam finalize
    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;
}
