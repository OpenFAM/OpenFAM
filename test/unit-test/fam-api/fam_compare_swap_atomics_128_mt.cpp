/*
 * fam_compare_swap_atomics_128_mt.cpp
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
#include <pthread.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

#define NUM_THREADS 100

fam *my_fam;
Fam_Descriptor *item;

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
int128store valueInt128;

uint32_t pass = 0, fail = 0;

void *thr_func(void *arg) {
    int128store retValueInt128;
    oldValueInt128.i64[0] = 0xAAAAAAAABBBBBBBB;
    oldValueInt128.i64[1] = 0xCCCCCCCCDDDDDDDD;
    try {
        retValueInt128.i128 = my_fam->fam_compare_swap(
            item, 0, oldValueInt128.i128, newValueInt128.i128);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    if ((retValueInt128.i64[0] == oldValueInt128.i64[0]) &&
        (retValueInt128.i64[1] == oldValueInt128.i64[1])) {
        std::cout << "128 byte CAS test passed" << endl;
        std::cout << "Return value: " << hex << uppercase
                  << retValueInt128.i64[0] << " " << retValueInt128.i64[1]
                  << "\n";
        std::cout << "Old value: " << oldValueInt128.i64[0] << " "
                  << oldValueInt128.i64[1] << "\n";
        pass++;
    } else {
        std::cout << "128 byte CAS failed" << endl;
        std::cout << "Return value: " << hex << uppercase
                  << retValueInt128.i64[0] << " " << retValueInt128.i64[1]
                  << "\n";
        std::cout << "Old value: " << oldValueInt128.i64[0] << " "
                  << oldValueInt128.i64[1] << "\n";
        fail++;
    }

    pthread_exit(NULL);
}

int main() {
    my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *desc;

    pthread_t thr[NUM_THREADS];
    int i, rc;

    memset((void *)&fam_opts, 0, sizeof(Fam_Options));

    fam_opts.memoryServer = strdup(TEST_MEMORY_SERVER);
    fam_opts.grpcPort = strdup(TEST_GRPC_PORT);
    fam_opts.libfabricPort = strdup(TEST_LIBFABRIC_PORT);
    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed" << endl;
        exit(1);
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

    oldValueInt128.i128 = 0;
    newValueInt128.i128 = 0;
    valueInt128.i64[0] = 0xAAAAAAAABBBBBBBB;
    valueInt128.i64[1] = 0xCCCCCCCCDDDDDDDD;

    try {
        my_fam->fam_set(item, 0, valueInt128.i64[0]);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }
    try {
        my_fam->fam_set(item, sizeof(uint64_t), valueInt128.i64[1]);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    newValueInt128.i64[0] = 0x2222222233333333;
    newValueInt128.i64[1] = 0x4444444455555555;

    valueInt128.i64[0] = 0;
    valueInt128.i64[1] = 0;

    for (i = 0; i < NUM_THREADS; ++i) {
        if ((rc = pthread_create(&thr[i], NULL, thr_func, NULL))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            return -1;
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    // Deallocating data items
    if (item != NULL)
        my_fam->fam_deallocate(item);

    // Destroying the regioin
    if (desc != NULL)
        my_fam->fam_destroy_region(desc);

    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;

    if (pass == 1) {
        printf("Test passed\n");
        return 0;
    } else {
        printf("Test failed\n");
        return -1;
    }
}
