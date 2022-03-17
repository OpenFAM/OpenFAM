/*
 * test_fam_create_region.cpp
 * Copyright (c) 2022 Hewlett Packard Enterprise Development, LP. All rights
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
/*
 * @Args:
 * argv[1] - Region name
 * argv[2] - Region size in MB
 * argv[3] - memory type - Volatile (0) or persistent (1)
 * argv[4] - Interleave enable (0) or disable (1)
 * argv[5] - Expected outcome from create_region Success (0) or Failure (1)
 */
int main(int argc, char *argv[]) {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *desc1 = NULL;
    Fam_Region_Attributes *regionAttributes =
        (Fam_Region_Attributes *)malloc(sizeof(Fam_Region_Attributes));
    memset(regionAttributes, 0, sizeof(Fam_Region_Attributes));

    if (atoi(argv[3]) == 0)
        regionAttributes->memoryType = VOLATILE;
    else if (atoi(argv[3]) == 1)
        regionAttributes->memoryType = PERSISTENT;
    else {
        std::cout << "Wrong option specified for memory type, test failed"
                  << std::endl;
        return -1;
    }

    if (atoi(argv[4]) == 0)
        regionAttributes->interleaveEnable = ENABLE;
    else if (atoi(argv[4]) == 1)
        regionAttributes->interleaveEnable = DISABLE;
    else {
        std::cout << "Wrong option specified for interleave enable, test failed"
                  << std::endl;
        return -1;
    }
    if (atoi(argv[5]) == 0)
        regionAttributes->redundancyLevel = NONE;
    else if (atoi(argv[5]) == 1)
        regionAttributes->redundancyLevel = REDUNDANCY_LEVEL_DEFAULT;

    init_fam_options(&fam_opts);
    fam_opts.runtime = strdup("NONE");

    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
        exit(1);
    }

    try {
        printf("PNS: regionAttributes: %d %d\n", atoi(argv[3]), atoi(argv[4]));
        desc1 = my_fam->fam_create_region(argv[1], atoi(argv[2]), 0777,
                                          regionAttributes);
        cout << "Created\n" << endl;
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
        return e.fam_error();
    }

    if (desc1 != NULL)
        my_fam->fam_destroy_region(desc1);

    try {
        my_fam->fam_finalize("default");
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
        exit(2);
    }

    return 0;
}
