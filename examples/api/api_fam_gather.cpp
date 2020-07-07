/*
 * api_fam_gather.cpp
 * Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All rights
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
#include <string.h>
using namespace std;
using namespace openfam;

int main(void) {
    int ret = 0;
    fam *myFam = new fam();
    Fam_Region_Descriptor *region = NULL;
    Fam_Descriptor *descriptor = NULL;
    Fam_Options *fm = (Fam_Options *)malloc(sizeof(Fam_Options));
    memset((void *)fm, 0, sizeof(Fam_Options));
    // assume that no specific options are needed by the implementation
    fm->runtime = strdup("NONE");
    try {
        myFam->fam_initialize("myApplication", fm);
        printf("FAM initialized\n");
    } catch (Fam_Exception &e) {
        printf("FAM Initialization failed: %s\n", e.fam_error_msg());
        myFam->fam_abort(-1); // abort the program
        // note that fam_abort currently returns after signaling
        // so we must terminate with the same value
        return -1;
    }

    // ... Initialization code here

    try {
        // create a 100 MB region with 0777 permissions and RAID5 redundancy
        region = myFam->fam_create_region("myRegion", (uint64_t)10000000, 0777,
                                          RAID5);
        // create 50 element unnamed integer array in FAM with 0600
        // (read/write by owner) permissions in myRegion
        descriptor = myFam->fam_allocate("myItem", (uint64_t)(50 * sizeof(int)),
                                         0600, region);
        // use the created region and data item...
        // ... continuation code here
        //
    } catch (Fam_Exception &e) {
        printf("Create region/Allocate Data item failed: %d: %s\n",
               e.fam_error(), e.fam_error_msg());
        return -1;
    }

    try {
        // Fam_Descriptor *descriptor = myFam->fam_lookup("myItem", "myRegion");
        // allocate a 25-element integer array in local memory
        int *local = (int *)malloc(25 * sizeof(int));
        // gather all odd elements from myItem into local memory
        // first element is myItem[1]; collect 25 elements with stride 2
        myFam->fam_gather_blocking(local, descriptor, 25, (uint64_t)1, 2,
                                   sizeof(int));
        // gather myItem[1,7,13,15,16]
        int *indexedLocal = (int *)malloc(5 * sizeof(int));
        uint64_t indexes[] = {1, 7, 13, 15, 16};
        myFam->fam_gather_blocking(indexedLocal, descriptor, 5, indexes,
                                   sizeof(int));
        // ... we now have the correct elements in local memory
        printf("We now have correct elements in local memory\n");
        printf("fam_gather_blocking API successfull!!\n");

    } catch (Fam_Exception &e) {
        printf("fam API failed: %d: %s\n", e.fam_error(), e.fam_error_msg());
        ret = -1;
    }

    try {
        // we are finished. Destroy the region and everything in it
        myFam->fam_destroy_region(region);
        // printf("fam_destroy_region successfull\n");
    } catch (Fam_Exception &e) {
        printf("Destroy region failed: %d: %s\n", e.fam_error(),
               e.fam_error_msg());
        ret = -1;
    }

    // ... Finalization code follows
    try {
        myFam->fam_finalize("myApplication");
        printf("FAM finalized\n");
    } catch (Fam_Exception &e) {
        printf("FAM Finalization failed: %s\n", e.fam_error_msg());
        myFam->fam_abort(-1); // abort the program
        // note that fam_abort currently returns after signaling
        // so we must terminate with the same value
        return -1;
    }
    return (ret);
}
