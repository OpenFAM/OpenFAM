/*
 * fam_region_registration_resize.cpp
 * Copyright (c) 2023 Hewlett Packard Enterprise Development, LP. All rights
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
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <fam/fam.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

#define NUM_THREADS 3

fam *my_fam;
Fam_Options fam_opts;
pthread_barrier_t barrier;

typedef struct {
    int32_t tid;
} ValueInfo;

void *thr_func1(void *arg) {
    ValueInfo *valInfo = (ValueInfo *)arg;
    int tid = valInfo->tid;

    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item1, *item2, *item3;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);
    const char *secondItem = get_uniq_str("second", my_fam);
    const char *thirdItem = get_uniq_str("third", my_fam);

    if (tid == 0) {
        Fam_Region_Attributes *regionAttributes = new Fam_Region_Attributes();

        regionAttributes->permissionLevel = REGION;
        regionAttributes->redundancyLevel = NONE;
        regionAttributes->interleaveEnable = ENABLE;
        regionAttributes->memoryType = VOLATILE;

        EXPECT_NO_THROW(desc = my_fam->fam_create_region(
                            testRegion, 8589934592, 0777, regionAttributes));
        EXPECT_NE((void *)NULL, desc);

        // First data item allocation succeeds
        EXPECT_NO_THROW(
            item1 = my_fam->fam_allocate(firstItem, 4294967296, 0777, desc));
        EXPECT_NE((void *)NULL, item1);
        // Second dataitem allocation fails as there is no space
        EXPECT_THROW(
            item2 = my_fam->fam_allocate(secondItem, 4294967296, 0777, desc),
            Fam_Exception);

        char *local = (char *)malloc(4294967296);
        memset(local, 0, 4294967296);

        // Write operation on already validated descriptor
        EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item1, 0, 4294967296));

        // Lookup operation creates an uninitialized descriptor
        EXPECT_NO_THROW(item1 = my_fam->fam_lookup(firstItem, testRegion));

        // This operation goes through additional descriptor validation
        EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item1, 0, 4294967296));

        char *local2 = (char *)malloc(4294967296);

        EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, item1, 0, 4294967296));
	EXPECT_EQ(0, memcmp(local, local2, 4294967296));

        // Resize the region
        EXPECT_NO_THROW(my_fam->fam_resize_region(desc, 17179869184));

        free(local);
        free(local2);
    }

    pthread_barrier_wait(&barrier);

    // Second thread
    if (tid == 1) {
        EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(testRegion));

        // Attempt to create second dataitem, this time it succeeds as region is
        // resized
        EXPECT_NO_THROW(
            item2 = my_fam->fam_allocate(secondItem, 4294967296, 0777, desc));
        EXPECT_NE((void *)NULL, item2);

        // Third dataitem allocation fails as there is no space
        EXPECT_THROW(
            item3 = my_fam->fam_allocate(thirdItem, 6442450944, 0777, desc),
            Fam_Exception);

        char *local = (char *)malloc(4294967296);
        memset(local, 0, 4294967296);

        // Write operation on already validated descriptor
        EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item2, 0, 4294967296));

        // Lookup operation creates an uninitialized descriptor
        EXPECT_NO_THROW(item2 = my_fam->fam_lookup(secondItem, testRegion));

        // This operation goes through additional descriptor validation
        EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item2, 0, 4294967296));

        char *local3 = (char *)malloc(4294967296);

        EXPECT_NO_THROW(my_fam->fam_get_blocking(local3, item2, 0, 4294967296));
        EXPECT_EQ(0, memcmp(local, local3, 4294967296));

        // Resize the region second time
        EXPECT_NO_THROW(my_fam->fam_resize_region(desc, 34359738368));

        free(local);
        free(local3);
    }

    pthread_barrier_wait(&barrier);

    // Third thread
    if (tid == 2) {
        EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(testRegion));
        EXPECT_NO_THROW(
            item3 = my_fam->fam_allocate(thirdItem, 6442450944, 0777, desc));
        EXPECT_NE((void *)NULL, item3);

        char *local = (char *)malloc(6442450944);
        memset(local, 0, 6442450944);
        // Write operation on already validated descriptor
        EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item3, 0, 6442450944));

        // Lookup operation creates an uninitialized descriptor
        EXPECT_NO_THROW(item3 = my_fam->fam_lookup(thirdItem, testRegion));

        // This operation goes through additional descriptor validation
        EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item3, 0, 6442450944));

        char *local3 = (char *)malloc(6442450944);

        EXPECT_NO_THROW(my_fam->fam_get_blocking(local3, item3, 0, 6442450944));
	EXPECT_EQ(0, memcmp(local, local3, 6442450944));

        free(local);
        free(local3);
    }

    pthread_barrier_wait(&barrier);

    // Second thread
    if (tid == 1) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(item2));
        delete item2;
    }

    // Third thread
    if (tid == 2) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(item3));
        delete item3;
    }

    pthread_barrier_wait(&barrier);

    // First thread
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(item1));
        EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
        delete item1;
    }
    delete desc;
    free((void *)testRegion);
    free((void *)firstItem);
    free((void *)secondItem);
    free((void *)thirdItem);

    pthread_exit(NULL);
}

TEST(FamRegionRegistration, RegionRegistrationResize) {
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int rc;
    int i, tid = 0;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    // Create threads
    for (i = 0; i < NUM_THREADS; ++i) {
        tid = i;
        info[i] = {tid};
        if ((rc = pthread_create(&thr[i], NULL, thr_func1, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    // Join all threads
    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);
    fam_opts.famThreadModel = strdup("FAM_THREAD_MULTIPLE");

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    pthread_barrier_init(&barrier, NULL, NUM_THREADS);

    ret = RUN_ALL_TESTS();

    pthread_barrier_destroy(&barrier);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
