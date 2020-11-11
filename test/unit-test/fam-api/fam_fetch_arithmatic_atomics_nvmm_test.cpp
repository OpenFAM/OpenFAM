/*
 * fam_fetch_arithmatic_atomics_test.cpp
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
#include <unistd.h>

#include "common/fam_test_config.h"
#include <fam/fam.h>
#include <fam/fam_exception.h>

using namespace std;
using namespace openfam;

uint32_t pass = 0, fail = 0;

template <class T> void validate_result(T a, T b) {
    if (a == b) {
        pass++;
        cout << "result obtained : " << hex << uppercase << a
             << ", result expected : " << b << "::results matched" << endl;
    } else {
        fail++;
        cout << "result obtained : " << a << ", result expected : " << b
             << "::results not matched" << endl;
    }
}

int main() {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;

    init_fam_options(&fam_opts);

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

    // atomic tests for int32
    int32_t valueInt32 = 0xAAAA;
    try {
        my_fam->fam_set(item, 0, valueInt32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    valueInt32 = 0x1;
    try {
        valueInt32 = my_fam->fam_fetch_add(item, 0, valueInt32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int32_t>(valueInt32, 0xAAAA);

    try {
        valueInt32 = my_fam->fam_fetch_int32(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int32_t>(valueInt32, 0xAAAB);

    valueInt32 = 0x1;
    try {
        valueInt32 = my_fam->fam_fetch_subtract(item, 0, valueInt32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int32_t>(valueInt32, 0xAAAB);

    try {
        valueInt32 = my_fam->fam_fetch_int32(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int32_t>(valueInt32, 0xAAAA);

    // Atomic tests for int64
    int64_t valueInt64 = 0xBBBBBBBBBBBBBBBB;
    try {
        my_fam->fam_set(item, 0, valueInt64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    valueInt64 = 0x1;
    try {
        valueInt64 = my_fam->fam_fetch_add(item, 0, valueInt64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int64_t>(valueInt64, 0xBBBBBBBBBBBBBBBB);

    try {
        valueInt64 = my_fam->fam_fetch_int64(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int64_t>(valueInt64, 0xBBBBBBBBBBBBBBBC);

    valueInt64 = 0x1;
    try {
        valueInt64 = my_fam->fam_fetch_subtract(item, 0, valueInt64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int64_t>(valueInt64, 0xBBBBBBBBBBBBBBBC);

    try {
        valueInt64 = my_fam->fam_fetch_int64(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int64_t>(valueInt64, 0xBBBBBBBBBBBBBBBB);

    // Deallocating data items
    if (item != NULL)
        my_fam->fam_deallocate(item);

    // Destroying the regioin
    if (desc != NULL)
        my_fam->fam_destroy_region(desc);

    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;

    if (fail) {
        printf("Test failed\n");
        return -1;
    } else {
        printf("Test passed\n");
        return 0;
    }
}
