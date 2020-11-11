/*
 * fam_allocate_map_nvmm.cpp
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
    Fam_Region_Descriptor *desc = NULL;
    Fam_Descriptor *item = NULL;

    init_fam_options(&fam_opts);
    cout << " Calling fam_initialize" << endl;
    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed" << endl;
        exit(1);
    }

    char *openFamModel =
        (char *)my_fam->fam_get_option(strdup("OPENFAM_MODEL"));

    if (strcmp(openFamModel, "shared_memory") != 0) {
        my_fam->fam_finalize("default");
        std::cout << "Test case valid only in shared memory model, "
                     "skipping with status : "
                  << TEST_SKIP_STATUS << std::endl;
        return TEST_SKIP_STATUS;
    }

    try {
        desc = my_fam->fam_create_region("test", 8192, 0777, RAID1);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    cout << "Create region successful" << endl;
    // Allocating data items in the created region
    try {
        item = my_fam->fam_allocate("first", 1024, 0777, desc);
    } catch (Fam_Exception &e) {
        my_fam->fam_destroy_region(desc);
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    cout << "Allocated memory" << endl;

    int *fam_array = NULL;
    try {
        fam_array = (int *)my_fam->fam_map(item);
    } catch (Fam_Exception &e) {
        my_fam->fam_destroy_region(desc);
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    // now we can directly manipulate the data in FAM
    for (int i = 0; i < 10; i++) {
        printf("%d\n", fam_array[i]);
        fam_array[i] = i;
    }

    try {
        my_fam->fam_unmap(fam_array, item);
    } catch (Fam_Exception &e) {
        my_fam->fam_destroy_region(desc);
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    cout << "Mapping again" << endl;
    int *fam_array_new = (int *)my_fam->fam_map(item);

    for (int i = 0; i < 10; i++) {
        printf("%d\n", fam_array_new[i]);
        if (fam_array_new[i] != i) {
            cerr << "error: Unexpected value found after mapping" << endl;
            my_fam->fam_destroy_region(desc);
            exit(1);
        }
    }

    // Deallocating data items
    if (item != NULL) {
        try {
            my_fam->fam_deallocate(item);
        } catch (Fam_Exception &e) {
            my_fam->fam_destroy_region(desc);
            cout << "Exception caught" << endl;
            cout << "Error msg: " << e.fam_error_msg() << endl;
            cout << "Error: " << e.fam_error() << endl;
        }
        cout << "Deallocated item" << endl;
    }
    // Destroying the region
    if (desc != NULL) {
        try {
            my_fam->fam_destroy_region(desc);
        } catch (Fam_Exception &e) {
            cout << "Exception caught" << endl;
            cout << "Error msg: " << e.fam_error_msg() << endl;
            cout << "Error: " << e.fam_error() << endl;
        }
        cout << "Destoryed region" << endl;
    }
    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;
}
