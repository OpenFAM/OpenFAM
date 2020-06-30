/*
 * api_fam_get_nonblocking.cpp
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
        if (myFam->fam_initialize("myApplication", fm) == 0) {
            printf("FAM initialized\n");
        }
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
        // look up the descriptor to a previously allocated data item
        // (a 50 element integer array)
        // Fam_Descriptor *descriptor = myFam->fam_lookup("myItem", "myRegion");
        // allocate local memory to receive 10 elements from different offsets
        // within the data item
        int *local1 = (int *)malloc(10 * sizeof(int));
        int *local2 = (int *)malloc(10 * sizeof(int));
        // copy elements 6-15 from FAM into local1 elements 0-9 in local memory
        myFam->fam_get_nonblocking(local1, descriptor, 6 * sizeof(int),
                                   10 * sizeof(int));
        // copy elements 26-35 from FAM into local2 elements 0-9 in local memory
        myFam->fam_get_nonblocking(local2, descriptor, 26 * sizeof(int),
                                   10 * sizeof(int));
        // wait for previously issued non-blocking operations to complete
        myFam->fam_quiet();
        // ... we now have copies in local memory to work with
        printf("fam_get_nonblocking success. All copies of data now in local "
               "memory\n");

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

    // ... Finalization code follows..
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
