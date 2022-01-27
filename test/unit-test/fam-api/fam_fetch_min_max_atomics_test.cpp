/*
 * fam_fetch_min_max_atomics_test.cpp
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

    desc = my_fam->fam_create_region("test", 8192, 0777, NULL);
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

    // Atomic min and max operations for int32
    int32_t valueInt32 = 0xBBBBBBBB;
    try {
        my_fam->fam_set(item, 0, valueInt32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        my_fam->fam_quiet();
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    valueInt32 = 0xAAAAAAAA;
    try {
        valueInt32 = my_fam->fam_fetch_min(item, 0, valueInt32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint32_t>(valueInt32, 0xBBBBBBBB);

    try {
        valueInt32 = my_fam->fam_fetch_uint32(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint32_t>(valueInt32, 0xAAAAAAAA);

    valueInt32 = 0xCCCCCCCC;
    try {
        valueInt32 = my_fam->fam_fetch_max(item, 0, valueInt32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint32_t>(valueInt32, 0xAAAAAAAA);

    try {
        valueInt32 = my_fam->fam_fetch_uint32(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint32_t>(valueInt32, 0xCCCCCCCC);

    // Atomic min and max operations for int64
    int64_t valueInt64 = 0xBBBBBBBBBBBBBBBB;
    try {
        my_fam->fam_set(item, 0, valueInt64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        my_fam->fam_quiet();
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    valueInt64 = 0xAAAAAAAAAAAAAAAA;
    try {
        valueInt64 = my_fam->fam_fetch_min(item, 0, valueInt64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int64_t>(valueInt64, 0xBBBBBBBBBBBBBBBB);

    try {
        valueInt64 = my_fam->fam_fetch_uint64(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int64_t>(valueInt64, 0xAAAAAAAAAAAAAAAA);

    valueInt64 = 0xCCCCCCCCCCCCCCCC;
    try {
        valueInt64 = my_fam->fam_fetch_max(item, 0, valueInt64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int64_t>(valueInt64, 0xAAAAAAAAAAAAAAAA);

    try {
        valueInt64 = my_fam->fam_fetch_uint64(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<int64_t>(valueInt64, 0xCCCCCCCCCCCCCCCC);

    // Atomic min and max operations for uint32
    uint32_t valueUint32 = 0xBBBBBBBB;
    try {
        my_fam->fam_set(item, 0, valueUint32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        my_fam->fam_quiet();
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    valueUint32 = 0x11111111;
    try {
        valueUint32 = my_fam->fam_fetch_min(item, 0, valueUint32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint32_t>(valueUint32, 0xBBBBBBBB);

    try {
        valueUint32 = my_fam->fam_fetch_uint32(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint32_t>(valueUint32, 0x11111111);

    valueUint32 = 0xCCCCCCCC;
    try {
        valueUint32 = my_fam->fam_fetch_max(item, 0, valueUint32);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint32_t>(valueUint32, 0x11111111);

    try {
        valueUint32 = my_fam->fam_fetch_uint32(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint32_t>(valueUint32, 0xCCCCCCCC);

    // Atomic min and max operations for uint64
    uint64_t valueUint64 = 0xAAAAAAAAAAAAAAAA;
    try {
        my_fam->fam_set(item, 0, valueUint64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        my_fam->fam_quiet();
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    valueUint64 = 0x1111111111111111;
    try {
        valueUint64 = my_fam->fam_fetch_min(item, 0, valueUint64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint64_t>(valueUint64, 0xAAAAAAAAAAAAAAAA);

    try {
        valueUint64 = my_fam->fam_fetch_uint64(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint64_t>(valueUint64, 0x1111111111111111);

    valueUint64 = 0xCCCCCCCCCCCCCCCC;
    try {
        valueUint64 = my_fam->fam_fetch_max(item, 0, valueUint64);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint64_t>(valueUint64, 0x1111111111111111);

    try {
        valueUint64 = my_fam->fam_fetch_uint64(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<uint64_t>(valueUint64, 0xCCCCCCCCCCCCCCCC);

    // Atomic min and max operations for float
    float valueFloat = 3.3f;
    try {
        my_fam->fam_set(item, 0, valueFloat);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        my_fam->fam_quiet();
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    valueFloat = 1.2f;
    try {
        valueFloat = my_fam->fam_fetch_min(item, 0, valueFloat);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<float>(valueFloat, 3.3f);

    try {
        valueFloat = my_fam->fam_fetch_float(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<float>(valueFloat, 1.2f);

    valueFloat = 5.6f;
    try {
        valueFloat = my_fam->fam_fetch_max(item, 0, valueFloat);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<float>(valueFloat, 1.2f);

    try {
        valueFloat = my_fam->fam_fetch_float(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<float>(valueFloat, 5.6f);

    // Atomic min and max operations for double
    double valueDouble = 3.3e+38;
    try {
        my_fam->fam_set(item, 0, valueDouble);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        my_fam->fam_quiet();
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    valueDouble = 1.2e+38;
    try {
        valueDouble = my_fam->fam_fetch_min(item, 0, valueDouble);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<double>(valueDouble, 3.3e+38);

    try {
        valueDouble = my_fam->fam_fetch_double(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<double>(valueDouble, 1.2e+38);

    valueDouble = 5.6e+38;
    try {
        valueDouble = my_fam->fam_fetch_max(item, 0, valueDouble);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<double>(valueDouble, 1.2e+38);

    try {
        valueDouble = my_fam->fam_fetch_double(item, 0);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    validate_result<double>(valueDouble, 5.6e+38);

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
