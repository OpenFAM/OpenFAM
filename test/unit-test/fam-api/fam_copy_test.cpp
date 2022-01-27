/*
 * fam_copy_test.cpp
 * Copyright (c) 2019-2021 Hewlett Packard Enterprise Development, LP. All
 * rights reserved. Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following conditions
 * are met:
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

#define MESSAGE_SIZE 12

using namespace std;
using namespace openfam;

int main() {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *srcDesc, *destDesc;
    Fam_Descriptor *srcItem;
    Fam_Descriptor *destItem[MESSAGE_SIZE];
    int fail = 0;

    init_fam_options(&fam_opts);
    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed" << endl;
        exit(1);
    }

    srcDesc = my_fam->fam_create_region("srcRegion", 8192, 0777, NULL);
    if (srcDesc == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    }
    // Allocating data items in the created region
    srcItem = my_fam->fam_allocate("first", 128, 0777, srcDesc);
    if (srcItem == NULL) {
        cout << "fam allocation of data item 'first' failed" << endl;
        exit(1);
    }

    destDesc = my_fam->fam_create_region("destRegion", 8192, 0777, NULL);
    if (destDesc == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    }

    for (int i = 0; i < MESSAGE_SIZE; i++) {
        // Allocating data items in the created region
        char itemInfo[255];
        sprintf(itemInfo, "second_%d", i);
        destItem[i] = my_fam->fam_allocate(itemInfo, 128, 0777, destDesc);
        if (destItem == NULL) {
            cout << "fam allocation of data item 'first' failed" << endl;
            exit(1);
        }
    }

    char *local = strdup("Test message");

    cout << "Content of source dataitem : " << local << endl;
    try {
        my_fam->fam_put_blocking(local, srcItem, 0, 13);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    void *waitObj[MESSAGE_SIZE];
    for (int i = 0; i < MESSAGE_SIZE; i++) {
        cout << "Copy " << i << " : Copying first " << i + 1 << " characters"
             << endl;
        try {
            waitObj[i] = my_fam->fam_copy(srcItem, 0, destItem[i], 0, i + 1);
        } catch (Fam_Exception &e) {
            cout << "Exception caught" << endl;
            cout << "Error msg: " << e.fam_error_msg() << endl;
            cout << "Error: " << e.fam_error() << endl;
        }
    }

    for (int i = MESSAGE_SIZE - 1; i >= 0; i--) {
        cout << "waiting for Copy : " << i << endl;
        try {
            my_fam->fam_copy_wait(waitObj[i]);
        } catch (Fam_Exception &e) {
            cout << "Exception caught" << endl;
            cout << "Error msg: " << e.fam_error_msg() << endl;
            cout << "Error: " << e.fam_error() << endl;
        }
    }

    // allocate local memory to receive 20 elements
    char *local2 = (char *)malloc(20);

    for (int i = 0; i < MESSAGE_SIZE; i++) {
        try {

            my_fam->fam_get_blocking(local2, destItem[i], 0, 13);

        } catch (Fam_Exception &e) {
            cout << "Exception caught" << endl;
            cout << "Error msg: " << e.fam_error_msg() << endl;
            cout << "Error: " << e.fam_error() << endl;
        }
        if (strncmp(local, local2, i + 1) == 0) {
            cout << "Copy " << i << " : " << local2 << " : CORRECT" << endl;
        } else {
            cout << "Copy " << i << " : " << local2 << " : WRONG" << endl;
            fail++;
            break;
        }
    }

    // Deallocating data items
    if (srcItem != NULL) {
        my_fam->fam_deallocate(srcItem);
    }

    for (int i = 0; i < MESSAGE_SIZE; i++) {
        if (destItem[i] != NULL)
            my_fam->fam_deallocate(destItem[i]);
    }

    // Destroying the region
    if (srcDesc != NULL) {
        my_fam->fam_destroy_region(srcDesc);
    }

    if (destDesc != NULL) {
        my_fam->fam_destroy_region(destDesc);
    }

    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;
    if (!fail) {
        printf("Test passed\n");
        return 0;
    } else {
        printf("Test failed\n");
        return -1;
    }
}
