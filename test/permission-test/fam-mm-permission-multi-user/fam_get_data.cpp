/*
 * fam_get_data.cpp
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

// ./fam_get_data <RegionName> <DataItemName> <offset> <size> 

#include <fam/fam_exception.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fam/fam.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

int main(int argc, char *argv[]) 
{
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Descriptor *item;

    init_fam_options(&fam_opts);
    fam_opts.runtime=strdup("NONE");

    try {
        my_fam->fam_initialize("default", &fam_opts);
    }
    catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
	exit(1);
    }
    
    try {
        item = my_fam->fam_lookup(argv[2], argv[1]);
    }
    catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
	return(e.fam_error());
    }

    ssize_t data_size = atoi(argv[4]);
    char *local2 = (char *)malloc(data_size);
    try 
    {
    	my_fam->fam_get_blocking(local2, item, atoi(argv[3]), data_size);
    }
    catch (Fam_Exception &e) 
    {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
	return(e.fam_error());
    }
    
    try {
        my_fam->fam_finalize("default");
    }
    catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
	exit(3);
    }

    return 0;
}
