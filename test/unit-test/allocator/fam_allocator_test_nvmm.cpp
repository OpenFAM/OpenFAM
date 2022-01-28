/*
 * fam_allocator_test_nvmm.cpp
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

#include <fam/fam.h>

#include "common/fam_options.h"
#include "common/fam_test_config.h"
#include <fam/fam_exception.h>

using namespace std;
using namespace openfam;

/**
 * 1. Initialize using NVMM.
 * 2. Create region with permission 0444 and name test
 *    Store regionID in global descriptor - global
 * 3. Create a data item, it will fail because of permission error.
 * 4. Change permission of region to 0777
 * 5. Create dataitem with name first with permission 0444
 *    Store regionID and offset in global descriptor - global_dataitem
 * 5. Map the data item, it will fail now
 * 6. Change permission of dataitem to 0777
 * 7. Map the data item.
 * 8. Do a lookup on region name test
 * 9. Do a lookup on data item first
 * 10. Compare the regionId, offset obtained using lookup
 *    and in global descriptor.
 */
int main() {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    Fam_Global_Descriptor global;
    Fam_Global_Descriptor globalCopy;
    Fam_Global_Descriptor global_dataitem;
    Fam_Global_Descriptor globalCopy_dataitem;

    init_fam_options(&fam_opts);
    char *buff = (char *)malloc(1024);
    memset(buff, 0, 1024);

    // Initialize the libfabric ops
    // char *message = strdup("This is the datapath test");

    // Initialize openFAM API
    fam_opts.openFamModel = strdup("shared_memory");
    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed" << endl;
        exit(1);
    }

    // Create Region
    desc = my_fam->fam_create_region("test", 8192, 0444, NULL);
    if (desc == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    } else {
        global = desc->get_global_descriptor();
        cout << " Fam_Region_Descriptor { Region ID : 0x" << hex << uppercase
             << global.regionId << ", Offset : 0x" << global.offset << " }"
             << endl;
    }

    // Change the permission of region
    try {
        my_fam->fam_change_permissions(desc, 0777);
    } catch (Fam_Exception &e) {
        cout << "fam change region permission failed" << endl;
        my_fam->fam_destroy_region(desc);
        exit(1);
    }

    // Allocating data items in the created region
    item = my_fam->fam_allocate("first", 1024, 0444, desc);
    if (item == NULL) {
        cout << "fam allocation of dataitem 'first' failed" << endl;
        my_fam->fam_destroy_region(desc);
        exit(1);
    } else {
        global_dataitem = item->get_global_descriptor();
        cout << " Fam_Descriptor { Region ID : 0x" << hex << uppercase
             << global_dataitem.regionId << ", Offset : 0x"
             << global_dataitem.offset << ", Key : 0x" << item->get_key()
             << " }" << endl;
    }
    void *local;

    // Below map should fail as permission is not there.
    try {
        local = my_fam->fam_map(item);
        // Below code will run if there is no exception.
        // We are expecting an exception here.
        my_fam->fam_destroy_region(desc);
        exit(1);
    } catch (Fam_Exception &e) {
        cout << "fam_map needs write permission" << endl;
    }

    // Change the permission of dataitem
    try {
        my_fam->fam_change_permissions(item, 0777);
    } catch (Fam_Exception &e) {
        cout << "fam change region permission failed" << endl;
        my_fam->fam_destroy_region(desc);
        exit(1);
    }

    // Lookup for region
    Fam_Region_Descriptor *regionCopy = my_fam->fam_lookup_region("test");
    globalCopy = regionCopy->get_global_descriptor();

    cout << " Region copy : " << endl;

    cout << " Fam_Region_Descriptor { Region ID : 0x" << hex << uppercase
         << globalCopy.regionId << ", Offset : 0x" << globalCopy.offset << " }"
         << endl;

    // lookup for dataitem
    Fam_Descriptor *itemCopy = my_fam->fam_lookup("first", "test");
    globalCopy_dataitem = itemCopy->get_global_descriptor();

    cout << " Dataitem copy: " << endl;
    cout << " Fam_Descriptor { Region ID : 0x" << hex << uppercase
         << globalCopy_dataitem.regionId << ", Offset : 0x"
         << globalCopy_dataitem.offset << ", Key : 0x" << itemCopy->get_key()
         << " }" << endl;

    if (global.regionId != globalCopy.regionId) {
        cout << "Test failed. regionId from lookup does not match expected "
                "regionId "
             << endl;
        my_fam->fam_destroy_region(desc);
        exit(1);
    }
    if (global.regionId != globalCopy_dataitem.regionId) {
        cout << "Test failed. regionId from lookup does not match expected "
                "regionId "
             << endl;
        my_fam->fam_destroy_region(desc);
        exit(1);
    }

    if (global_dataitem.offset != globalCopy_dataitem.offset) {
        cout << "Test failed. regionId from lookup does not match expected "
                "regionId "
             << endl;
        my_fam->fam_destroy_region(desc);
        exit(1);
    }

    local = my_fam->fam_map(itemCopy);
    if (local == NULL) {
        cout << "fam_map was not supposed to fail. Something wrong" << endl;
        my_fam->fam_destroy_region(desc);
        exit(1);
    }
    my_fam->fam_unmap(local, itemCopy);

    // Deallocating data items
    if (item != NULL)
        my_fam->fam_deallocate(item);

    // Destroying the regioin
    if (desc != NULL)
        my_fam->fam_destroy_region(desc);

    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;
}
