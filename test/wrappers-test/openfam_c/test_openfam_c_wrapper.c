/*
 * test_openfam_c_wrapper.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fam/fam_c.h>


int main() {
    //create a fam object
    c_fam* fam_inst = c_fam_create();
    if(fam_inst == NULL) { 
        printf ("Unable to create fam object! Exiting...\n");
        exit(1);
    }

    // populate the fam_options before doing fam_initialize
    c_fam_options fam_opts;
    memset((void*)&fam_opts, 0, sizeof(c_fam_options));

    const char* groupName = strdup("CWrappers");
    const char* regionName = strdup("CWrapperRegion");
    const char* dataitem1 = strdup("dataitem1");
    size_t regionSize = 1024 * 1024 * 1024LLU;
    int rlevel = 1;
    // fam initialize
    int ret = c_fam_initialize(fam_inst, groupName, &fam_opts);
    if (ret != 0) {
        printf ("OpenFAM error: %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    }

    // create a fam region
    c_fam_Region_Attributes regionAttributes;
    c_fam_region_desc* region_desc = c_fam_create_region(fam_inst, regionName, regionSize, 0777, &regionAttributes);
    if(region_desc == NULL) {
        printf ("OpenFAM error: %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    }

    // allocate the fam dataitem
    c_fam_desc* fd1 = c_fam_allocate_named(fam_inst, dataitem1, 1024, 0777, region_desc);
    if(fd1 == NULL) {
        printf ("OpenFAM error: %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    }
    c_fam_desc* fd2 = c_fam_allocate(fam_inst, 16, 0777, region_desc);
    if(fd2 == NULL) {
        printf ("OpenFAM error: %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    }

    // write value into dataitem
    uint64_t inputVal = 2000;
    ret = c_fam_put_blocking(fam_inst, (void*)&inputVal, fd1, 0, sizeof(uint64_t));
    if(ret != 0) {
        printf ("OpenFAM error: %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    }

    // read fam dataitem
    uint64_t outputVal;
    ret = c_fam_get_blocking(fam_inst, (void*)&outputVal, fd1, 0, sizeof(uint64_t));
    if(ret != 0) {
        printf ("OpenFAM error: %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    }

    // check data item is read correctly
    if(inputVal == outputVal) {
        printf ("Dataitem read correctly!\n");
    } else {
        printf ("Dataitem read incorrectly!\n");
    }

    // check 128 bit atomic operations
    int128_t inputval = 6000;
    ret = c_fam_set_int128(fam_inst, fd2, 0, inputval);
    if(ret != 0) {
        printf("128 bit set failed! %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    }
    ret = c_fam_quiet(fam_inst);
    if(ret != 0) {
        printf ("Fam quiet failed! %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    }

    // compare and swap
    int128_t retval, newval = 9000;
    ret = c_fam_compare_swap_int128(fam_inst, fd2, 0, inputval, newval, &retval);
    if (ret != 0) {
        printf ("Compare swap failed! %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    } else {
        if (retval != inputval) {
            printf ("Comparison failed!\n");
        }
    }

    // fetch the value
    int128_t getval;
    ret = c_fam_fetch_int128(fam_inst, fd2, 0, &getval);
    if (ret != 0) {
        printf ("Fam fetch 128 failed! %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    }

    if (getval == newval) {
        printf ("All 128 operations successful!\n");
    } else {
        printf ("All 128 operations did not succeed!\n");
    }

    // Using fam context
    c_fam_context *ctx = c_fam_context_open(fam_inst);

    int64_t arr[16] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};
    uint64_t indexes[] = {0, 7, 3, 5, 8, 10, 15, 14, 11, 9};

    ret = c_fam_scatter_blocking_index(ctx, &arr, fd1, 10, indexes, sizeof(int64_t));
    if(ret != 0) {
        printf ("Fam scatter blocking index failed: %d : %s\n", c_fam_error(), c_fam_error_msg()); 
        exit(1);
    }

    c_fam_context_close(fam_inst, ctx);

    // destroy region
    ret = c_fam_destroy_region(fam_inst, region_desc);
    if(ret != 0) {
        printf ("OpenFAM error: %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    }

    ret = c_fam_finalize(fam_inst, groupName);
    if(ret != 0) {
        printf ("OpenFAM error: %d : %s\n", c_fam_error(), c_fam_error_msg());
        exit(1);
    }

    return 0;
}

