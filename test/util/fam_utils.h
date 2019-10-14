/*
 * fam_utils.h
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
#ifndef FAM_UTILS_H_
#define FAM_UTILS_H_

#include <fam/fam.h>

#define create_region(pass, my_fam, region, name, size, permission,            \
                      redundancy)                                              \
    try {                                                                      \
        region =                                                               \
            my_fam->fam_create_region(name, size, permission, redundancy);     \
    } catch (Fam_Exception & e) {                                              \
        pass = false;                                                          \
        cout << "Exception caught" << endl;                                    \
        cout << "Error msg: " << e.fam_error_msg() << endl;                    \
        cout << "Error: " << e.fam_error() << endl;                            \
    }

#define destroy_region(pass, my_fam, region)                                   \
    try {                                                                      \
        my_fam->fam_destroy_region(region);                                    \
    } catch (Fam_Exception & e) {                                              \
        pass = false;                                                          \
        cout << "Exception caught" << endl;                                    \
        cout << "Error msg: " << e.fam_error_msg() << endl;                    \
        cout << "Error: " << e.fam_error() << endl;                            \
    }

#define allocate(pass, my_fam, dataitem, name, size, permission, region)       \
    try {                                                                      \
        dataitem = my_fam->fam_allocate(name, size, permission, region);       \
    } catch (Fam_Exception & e) {                                              \
        pass = false;                                                          \
        cout << "Exception caught" << endl;                                    \
        cout << "Error msg: " << e.fam_error_msg() << endl;                    \
        cout << "Error: " << e.fam_error() << endl;                            \
    }

#define deallocate(pass, my_fam, dataitem)                                     \
    try {                                                                      \
        my_fam->fam_deallocate(dataitem);                                      \
    } catch (Fam_Exception & e) {                                              \
        pass = false;                                                          \
        cout << "Exception caught" << endl;                                    \
        cout << "Error msg: " << e.fam_error_msg() << endl;                    \
        cout << "Error: " << e.fam_error() << endl;                            \
    }

#define change_permission(pass, my_fam, descriptor, permission)                \
    try {                                                                      \
        my_fam->fam_change_permissions(descriptor, permission);                \
    } catch (Fam_Exception & e) {                                              \
        pass = false;                                                          \
        cout << "Exception caught" << endl;                                    \
        cout << "Error msg: " << e.fam_error_msg() << endl;                    \
        cout << "Error: " << e.fam_error() << endl;                            \
    }

#define lookup_region(pass, my_fam, region, name)                              \
    try {                                                                      \
        region = my_fam->fam_lookup_region(name);                              \
    } catch (Fam_Exception & e) {                                              \
        pass = false;                                                          \
        cout << "Exception caught" << endl;                                    \
        cout << "Error msg: " << e.fam_error_msg() << endl;                    \
        cout << "Error: " << e.fam_error() << endl;                            \
    }

#define lookup(pass, my_fam, dataitem, itemName, Regionname)                   \
    try {                                                                      \
        dataitem = my_fam->fam_lookup(itemName, Regionname);                   \
    } catch (Fam_Exception & e) {                                              \
        pass = false;                                                          \
        cout << "Exception caught" << endl;                                    \
        cout << "Error msg: " << e.fam_error_msg() << endl;                    \
        cout << "Error: " << e.fam_error() << endl;                            \
    }
#endif /* end of FAM_UTILS_H_ */
