/*
 * fam_options_test.cpp
 * Copyright (c) 2019,2023 Hewlett Packard Enterprise Development, LP. All rights
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
#include <fam/fam_exception.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

int main() {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    const char **optList;
    int i = 0;
    init_fam_options(&fam_opts);
    //fam_opts.grpcPort = strdup("9500");
    //fam_opts.runtime = strdup("NONE");

    try {
        // Throws grpc exception because of wrong grpc port.
        // Continue the test, as this is only to test fam_options.
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed with fam_error:" << e.fam_error()
             << ":" << e.fam_error_msg()
             << ", continue to test fam_list_options." << endl;
    }

    optList = my_fam->fam_list_options();

    if (optList != NULL) {
        cout << "List of supported options" << endl;
        while (optList[i])
            cout << optList[i++] << ",";
        cout << endl;
    }

    // Read the default options values
    i = 0;
    while (optList[i]) {
        char *opt = strdup(optList[i]);
        if (strncmp(opt, "PE", 2) == 0) {
            try{
                int *optIntValue = (int *)my_fam->fam_get_option(opt);
                cout << optList[i] << ":" << *optIntValue << endl;
                free(optIntValue);
            } catch (Fam_Exception &e) {
                cout << "Exception caught" << endl;
                cout << "Error msg: " << e.fam_error_msg() << endl;
                cout << "Error: " << e.fam_error() << endl;
            }
        } else {
            try{
                char *optValue = (char *)my_fam->fam_get_option(opt);
                cout << optList[i] << ":" << optValue << endl;
                free(optValue);
            } catch (Fam_Exception &e) {
                cout << "Exception caught" << endl;
                cout << "Error msg: " << e.fam_error_msg() << endl;
                cout << "Error: " << e.fam_error() << endl;
            }
        }
        i++;
        free(opt);
    }

    try {
        char *badOpt = strdup("badOption");
        char *optValue = (char *)my_fam->fam_get_option(badOpt);
        cout << optList[i] << ":" << optValue << endl;
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    char bufRegOpt[20] = "FAM_CLIENT_BUFFER";

    Fam_Client_Buffer *bufRegInfo1 = new Fam_Client_Buffer();
    bufRegInfo1->buffer = malloc(4096);
    bufRegInfo1->bufferSize = 4096;
    bufRegInfo1->op = REGISTER;

    try {
        my_fam->fam_set_option(bufRegOpt, (void *)bufRegInfo1, sizeof(Fam_Client_Buffer));
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    bufRegInfo1->op = DEREGISTER;
    try {
        my_fam->fam_set_option(bufRegOpt, (void *)bufRegInfo1, sizeof(Fam_Client_Buffer));
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    free(bufRegInfo1->buffer);
    delete bufRegInfo1;

    Fam_Client_Buffer *bufRegInfo2 = new Fam_Client_Buffer();
    bufRegInfo2->buffer = malloc(4096);
    bufRegInfo2->bufferSize = 4096;
    bufRegInfo2->ctx = my_fam->fam_context_open();
    bufRegInfo2->op = REGISTER;

    try {
        my_fam->fam_set_option(bufRegOpt, (void *)bufRegInfo2, sizeof(Fam_Client_Buffer));
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    bufRegInfo2->op = DEREGISTER;
    try {
        my_fam->fam_set_option(bufRegOpt, (void *)bufRegInfo2, sizeof(Fam_Client_Buffer));
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    try {
        my_fam->fam_context_close(bufRegInfo2->ctx);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
    }

    free(bufRegInfo2->buffer);
    delete bufRegInfo2;

    free(optList);

    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;
}
