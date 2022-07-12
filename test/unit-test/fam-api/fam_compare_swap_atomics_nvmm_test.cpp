/*
 * fam_compare_swap_atomics_nvmm_test.cpp
 * Copyright (c) 2019,2022 Hewlett Packard Enterprise Development, LP. All rights
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

    desc = my_fam->fam_create_region("test", 8192, 0777, NULL);
    if (desc == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    }
    std::cout << "Fam Region created"
              << "\n";
    // Allocating data items in the created region
    item = my_fam->fam_allocate("first", 1024, 0777, desc);
    if (item == NULL) {
        cout << "fam allocation of data item 'first' failed" << endl;
        my_fam->fam_destroy_region(desc);
        exit(1);
    }

    int32_t valueInt32 = 0xAAAAAAAA;
    try {
        my_fam->fam_set(item, 8, valueInt32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    int32_t oldValueInt32 = 0xAAAAAAAA;
    int32_t newValueInt32 = 0xBBBBBBBB;

    try {
        my_fam->fam_compare_swap(item, 8, oldValueInt32, newValueInt32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        valueInt32 = my_fam->fam_fetch_int32(item, 8);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int32_t>(valueInt32, 0xBBBBBBBB);

    // Compare atomic operation for int64
    int64_t valueInt64 = 0xBBBBBBBBBBBBBBBB;
    try {
        my_fam->fam_set(item, 8, valueInt64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    int64_t oldValueInt64 = 0xBBBBBBBBBBBBBBBB;
    int64_t newValueInt64 = 0xCCCCCCCCCCCCCCCC;

    try {
        my_fam->fam_compare_swap(item, 8, oldValueInt64, newValueInt64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        valueInt64 = my_fam->fam_fetch_int64(item, 8);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int64_t>(valueInt64, 0xCCCCCCCCCCCCCCCC);

    // Compare atomic operations for uint32
    uint32_t valueUint32 = 0xBBBBBBBB;
    try {
        my_fam->fam_set(item, 8, valueUint32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    uint32_t oldValueUint32 = 0xBBBBBBBB;
    uint32_t newValueUint32 = 0xCCCCCCCC;

    try {
        my_fam->fam_compare_swap(item, 8, oldValueUint32, newValueUint32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        valueUint32 = my_fam->fam_fetch_uint32(item, 8);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint32_t>(valueUint32, 0xCCCCCCCC);

    // Compare atomic operation for uint64
    uint64_t valueUint64 = 0xBBBBBBBBBBBBBBBB;
    try {
        my_fam->fam_set(item, 8, valueUint64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    uint64_t oldValueUint64 = 0xBBBBBBBBBBBBBBBB;
    uint64_t newValueUint64 = 0xCCCCCCCCCCCCCCCC;

    try {
        my_fam->fam_compare_swap(item, 8, oldValueUint64, newValueUint64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        valueUint64 = my_fam->fam_fetch_uint64(item, 8);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint64_t>(valueUint64, 0xCCCCCCCCCCCCCCCC);

    // Failure test case
    valueUint64 = 0xBBBBBBBBBBBBBBBB;
    try {
        my_fam->fam_set(item, 8, valueUint64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    oldValueUint64 = 0xAAAAAAAAAAAAAAAA;
    newValueUint64 = 0xCCCCCCCCCCCCCCCC;

    try {
        my_fam->fam_compare_swap(item, 8, oldValueUint64, newValueUint64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        valueUint64 = my_fam->fam_fetch_uint64(item, 8);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint64_t>(valueUint64, 0xBBBBBBBBBBBBBBBB);

    // Declare the int128store union here.
    union int128store {
        struct {
            uint64_t low;
            uint64_t high;
        };
        int64_t i64[2];
        int128_t i128;
    };
    int128store oldValueInt128;
    int128store newValueInt128;
    int128store ValueInt128;
    oldValueInt128.i128 = 0;
    newValueInt128.i128 = 0;
    ValueInt128.i64[0] = 0xAAAAAAAAAAAAAAAA;
    ValueInt128.i64[1] = 0xAAAAAAAAAAAAAAAA;

    try {
        my_fam->fam_set(item, 0, ValueInt128.i128);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    oldValueInt128.i64[0] = 0xAAAAAAAAAAAAAAAA;
    oldValueInt128.i64[1] = 0xAAAAAAAAAAAAAAAA;

    newValueInt128.i64[0] = 0xBBBBBBBBBBBBBBBB;
    newValueInt128.i64[1] = 0xBBBBBBBBBBBBBBBB;

    try {
        my_fam->fam_compare_swap(item, 0, oldValueInt128.i128,
                                 newValueInt128.i128);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        ValueInt128.i128 = my_fam->fam_fetch_int128(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    // Validate result did not work with int128_t, Create our own validate
    // validate_result<int128_t>(ResultValueInt128.i128, ValueInt128.i128);
    if (newValueInt128.i128 == ValueInt128.i128)
        std::cout << "128 byte test passed"
                  << "\n";
    else {
        std::cout << "128 byte test failed"
                  << "\n";
        std::cout << "Result value: " << ValueInt128.i64[0] << " "
                  << ValueInt128.i64[1] << "\n";
        fail = 1;
    }
    // Deallocating data items
    if (item != NULL)
        my_fam->fam_deallocate(item);

    // Destroying the region
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
