/*
 * fam_put_get.cpp
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
#include <iostream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

int main() {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;

    memset((void *)&fam_opts, 0, sizeof(Fam_Options));

    fam_opts.memoryServer = strdup(TEST_MEMORY_SERVER);
    fam_opts.grpcPort = strdup(TEST_GRPC_PORT);
    fam_opts.libfabricPort = strdup(TEST_LIBFABRIC_PORT);
    fam_opts.allocator = strdup(TEST_ALLOCATOR);

    if (my_fam->fam_initialize("default", &fam_opts) < 0) {
        cout << "fam initialization failed" << endl;
        exit(1);
    } else {
        cout << "fam initialization successful" << endl;
    }

    desc = my_fam->fam_create_region("test", 8192, 0777, RAID1);
    if (desc == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    }
    // Allocating data items in the created region
    item = my_fam->fam_allocate("first", 1024, 0777, desc);
    if (item == NULL) {
        cout << "fam allocation of data item 'first' failed" << endl;
        exit(1);
    }
    int ret = 0;
    Fam_Stat *info = (Fam_Stat *)malloc(sizeof(Fam_Stat));
    my_fam->fam_stat(desc, info);

    uint64_t regionSize = info->size;

    cout << "Region size : " << regionSize << endl;
    my_fam->fam_stat(item, info);

    uint64_t itemSize = info->size;

    cout << "Item size : " << itemSize << endl;
    free(info);
    char *local = strdup("Test message");
    try {
        ret = my_fam->fam_put_blocking(local, item, 0, 13);
        if (ret < 0) {
            cout << "fam_put failed" << endl;
            exit(1);
        }
    } catch (Fam_Permission_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;

    } catch (Fam_Datapath_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    // allocate local memory to receive 20 elements
    char *local2 = (char *)malloc(20);
    try {

        ret = my_fam->fam_get_blocking(local2, item, 0, 13);
        if (ret < 0) {
            cout << "fam_get failed" << endl;
        }
    } catch (Fam_Permission_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;

    } catch (Fam_Datapath_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    // ... we now have a copy in local memory to work with
    cout << " Content of local2 : " << local2 << endl;
    // Deallocating data items
    if (item != NULL)
        my_fam->fam_deallocate(item);

    // Destroying the region
    if (desc != NULL)
        my_fam->fam_destroy_region(desc);

    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;
    if (strncmp(local, local2, 13) == 0) {
        cout << "Data read is same is as written" << endl;
        return 0;
    } else {
        cout << "Read and Written Data are different" << endl;
        return -1;
    }
}
