/*
 * fam_scatter_gather_stride_nonblocking_with_validation.cpp
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

#include "common/fam_libfabric.h"
#include "common/fam_ops_libfabric.h"
#include <fam/fam.h>
#include <fam/fam_exception.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

int main() {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item, *item1;
    Fam_Global_Descriptor global_item;

    uint32_t pass = 0, fail = 0;

    init_fam_options(&fam_opts);
    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed" << endl;
        exit(1);
    }

    desc = my_fam->fam_create_region("sg_test", 1048576, 0777, NULL);
    if (desc == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    } else {
        Fam_Global_Descriptor global_reg = desc->get_global_descriptor();
        cout << " Fam_Region_Descriptor { Region ID : 0x" << hex << uppercase
             << global_reg.regionId << ", Offset : 0x" << global_reg.offset
             << " }" << endl;
    }

    // Allocating data items in the created region

    item = my_fam->fam_allocate("dataitem", 32768, 0777, desc);
    if (item == NULL) {
        cout << "fam allocation of dataitem 'dataitem' failed" << endl;
    } else {
        global_item = item->get_global_descriptor();
        cout << " Fam_Descriptor { Region ID : 0x" << hex << uppercase
             << global_item.regionId << ", Offset : 0x" << global_item.offset
             << " }" << endl;
    }

    item1 = my_fam->fam_allocate("dataitem1", 32768, 0777, desc);
    if (item1 == NULL) {
        cout << "fam allocation of dataitem 'dataitem1' failed" << endl;
    } else {
        global_item = item1->get_global_descriptor();
        cout << " Fam_Descriptor { Region ID : 0x" << hex << uppercase
             << global_item.regionId << ", Offset : 0x" << global_item.offset
             << " }" << endl;
    }

    // allocate an integer array and initialize it
    int newLocal[2048];

    for (int i = 0; i < 2048; i++) {
        newLocal[i] = i;
    }

    my_fam->fam_scatter_nonblocking(newLocal, item, 256, 10, 4, sizeof(int));

    try {
        my_fam->fam_quiet();
    } catch (Fam_Exception &e) {
        cout << "fam quiet failed:" << e.fam_error_msg() << endl;
        exit(1);
    }

    int getLocal[2048];
    int gatherLocal[2048];

    my_fam->fam_get_blocking(getLocal, item, 10 * sizeof(int),
                             sizeof(getLocal));

    my_fam->fam_gather_nonblocking(gatherLocal, item, 256, 10, 4, sizeof(int));

    try {
        my_fam->fam_quiet();
    } catch (Fam_Exception &e) {
        cout << "fam quiet failed:" << e.fam_error_msg() << endl;
        exit(1);
    }

    if (item != NULL)
        my_fam->fam_deallocate(item);

    // First put the data and gather the data and validate the correctness

    for (int i = 0; i < 2048; i++) {
        newLocal[i] = i;
    }

    my_fam->fam_put_blocking(newLocal, item1, 0, sizeof(newLocal));

    int gatherLocal_1[2048];

    my_fam->fam_gather_nonblocking(gatherLocal_1, item1, 256, 100, 3,
                                   sizeof(int));

    try {
        my_fam->fam_quiet();
    } catch (Fam_Exception &e) {
        cout << "fam quiet failed:" << e.fam_error_msg() << endl;
        exit(1);
    }

    if (item1 != NULL)
        my_fam->fam_deallocate(item1);

    int i, j;
    for (i = 0, j = 100; i < 256; i++) {
        if (gatherLocal_1[i] == newLocal[j]) {
            pass++;
        } else {
            fail++;
            printf(
                "[%d]-[%d]: Value read = %d, Value expected = %d::valueis did "
                "not match\n",
                i, j, gatherLocal_1[i], newLocal[j]);
        }
        j += 3;
    }

    // Destroying the region
    if (desc != NULL)
        my_fam->fam_destroy_region(desc);

    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;

    for (int i = 0; i < 256; i++) {
        if (gatherLocal[i] == newLocal[i]) {
            pass++;
        } else {
            fail++;
            printf("[%d]: Value read = %d, Value expected = %d::valueis did "
                   "not match\n",
                   i, gatherLocal[i], newLocal[i]);
        }
    }

    for (i = 0, j = 0; j < 256; j++) {
        if (getLocal[i] == newLocal[j]) {
            pass++;
        } else {
            fail++;
            printf(
                "[%d]-[%d]: Value read = %d, Value expected = %d::valueis did "
                "not match\n",
                i, j, getLocal[i], newLocal[j]);
        }
        i += 4;
    }

    if (fail) {
        printf("Test failed\n");
        return -1;
    } else {
        printf("Test passed\n");
        return 0;
    }
}
