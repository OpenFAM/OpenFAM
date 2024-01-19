/*
 * fam_region_registration_mt.cpp
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

#define NUM_THREADS 32

fam *my_fam;
Fam_Options fam_opts;
pthread_barrier_t barrier;
const char *testRegion;

typedef struct {
    int32_t tid;
} ValueInfo;

void *thr_func1(void *arg) {
    ValueInfo *valInfo = (ValueInfo *)arg;
    int tid = valInfo->tid;

    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item1, *item2;

    const char *name = get_uniq_str("first", my_fam);
    string itemName = name;
    itemName = itemName + to_string(tid);

    EXPECT_NO_THROW(desc = my_fam->fam_lookup_region(testRegion));
    EXPECT_NE((void *)NULL, desc);

    // First data item allocation
    EXPECT_NO_THROW(
        item1 = my_fam->fam_allocate(itemName.c_str(), 1024, 0777, desc));
    EXPECT_NE((void *)NULL, item1);

    pthread_barrier_wait(&barrier);

    char *local = strdup("Test Message");

    // Lookup operation creates an uninitialized descriptor
    EXPECT_NO_THROW(item2 = my_fam->fam_lookup(itemName.c_str(), testRegion));
    EXPECT_NE((void *)NULL, item2);

    // Write operation with uninitialized descriptor. Hence, need additional
    // validation
    EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item2, 0, 13));

    char *local2 = (char *)malloc(20);

    EXPECT_NO_THROW(my_fam->fam_get_blocking(local2, item2, 0, 13));
    EXPECT_STREQ(local, local2);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item1));
    delete item1;
    delete item2;
    delete desc;
    free((void *)name);
    free(local);
    free(local2);

    pthread_exit(NULL);
}

TEST(FamRegionRegistration, MultiThread) {
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int rc;
    int i, tid = 0;

    testRegion = get_uniq_str("test", my_fam);

    Fam_Region_Descriptor *desc;
    Fam_Region_Attributes *regionAttributes = new Fam_Region_Attributes();

    regionAttributes->permissionLevel = REGION;
    regionAttributes->redundancyLevel = NONE;
    regionAttributes->interleaveEnable = ENABLE;
    regionAttributes->memoryType = VOLATILE;

    EXPECT_NO_THROW(desc = my_fam->fam_create_region(testRegion, 1048576, 0777,
                                                     regionAttributes));
    EXPECT_NE((void *)NULL, desc);

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
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete desc;
    free((void *)testRegion);
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
