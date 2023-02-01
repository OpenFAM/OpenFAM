/*
 * fam_put_get_nb_mt_reg_test.cpp
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
/* Test Case Description: Tests non-blocking put/get operations for
 * multithreaded model.
 */

#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

#define NUM_THREADS 10
#define MSG_SIZE 30
#define REGION_SIZE (1024 * 1024 * NUM_THREADS)
#define REGION_PERM 0777

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t msg_size;
} ValueInfo;

typedef struct {
    Fam_Descriptor *item[3];
    uint64_t offset;
    int32_t tid;
    int32_t msg_size;
} ValueInfo2;

pthread_barrier_t barrier;
pthread_barrier_t barrier_half;

int rc;
fam *my_fam;
Fam_Options fam_opts;
Fam_Region_Descriptor *testRegionDesc;
Fam_Region_Descriptor *desc1, *desc2, *desc3;
const char *testRegionStr;
Fam_Descriptor *items[NUM_THREADS];

void *thr_func1(void *arg) {
    ValueInfo *valInfo = (ValueInfo *)arg;
    char *local = strdup("Test message");
    int tid = valInfo->tid;
    uint64_t offset = tid * valInfo->msg_size;
    // Put nonblocking
    EXPECT_NO_THROW(my_fam->fam_put_nonblocking(local, valInfo->item, offset,
                                                valInfo->msg_size));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    // allocate local memory to receive 20 elements
    char *local2 = (char *)malloc(MSG_SIZE);
    // Get nonblocking
    EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local2, valInfo->item, offset,
                                                valInfo->msg_size));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_STREQ(local, local2);
    free(local);
    free(local2);
    pthread_exit(NULL);
}
// Test case 1 - nonblocking put get test by multiple threads on same region and
// data item created before thread creation
TEST(FamPutGetMT, NonblockingPutGetMTSameRegionDataItem) {

    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    int i, tid = 0;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item = my_fam->fam_allocate(dataItem, 1024 * NUM_THREADS,
                                                0777, testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    for (i = 0; i < NUM_THREADS; ++i) {
        tid = i;
        info[i] = {item, 0, tid, MSG_SIZE};
        if ((rc = pthread_create(&thr[i], NULL, thr_func1, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
            //        return -1;
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    // Deallocating data items
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    delete (item);
    free((void *)dataItem);
    free(info);
}
void *thr_func2(void *arg) {
    ValueInfo2 *valInfo = (ValueInfo2 *)arg;
    char *local = strdup("Test message");
    char *local2 = (char *)malloc(MSG_SIZE);
    int tid = valInfo->tid;
    uint64_t offset = tid * valInfo->msg_size;
    // Put nonblocking
    EXPECT_NO_THROW(my_fam->fam_put_nonblocking(local, valInfo->item[0], offset,
                                                valInfo->msg_size));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    // allocate local memory to receive 20 elements
    EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local2, valInfo->item[0],
                                                offset, valInfo->msg_size));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_STREQ(local, local2);

    // Region 2
    // Put nonblocking
    EXPECT_NO_THROW(my_fam->fam_put_nonblocking(local, valInfo->item[1], offset,
                                                valInfo->msg_size));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    // allocate local memory to receive 20 elements
    EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local2, valInfo->item[1],
                                                offset, valInfo->msg_size));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_STREQ(local, local2);

    // Region 3

    // Put nonblocking
    EXPECT_NO_THROW(my_fam->fam_put_nonblocking(local, valInfo->item[2], offset,
                                                valInfo->msg_size));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    // allocate local memory to receive 20 elements
    EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local2, valInfo->item[2],
                                                offset, valInfo->msg_size));
    pthread_barrier_wait(&barrier);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier);
    EXPECT_STREQ(local, local2);

    free(local);
    free(local2);
    pthread_exit(NULL);
}
// Test case 2 - nonblocking put get test by multiple threads on multiple
// regions and data items created before thread creation

TEST(FamPutGetMT, NonblockingPutGetMTMultipleRegionDataItem) {

    Fam_Descriptor *item[3];

    pthread_t thr[NUM_THREADS];
    int i;

    const char *region1 = get_uniq_str("test111", my_fam);
    const char *region2 = get_uniq_str("test112", my_fam);
    const char *region3 = get_uniq_str("test113", my_fam);

    EXPECT_NO_THROW(desc1 = my_fam->fam_create_region(
                        region1, (8192 * NUM_THREADS), 0777, NULL));
    EXPECT_NE((void *)NULL, desc1);
    EXPECT_NO_THROW(desc2 = my_fam->fam_create_region(
                        region2, (8192 * NUM_THREADS), 0777, NULL));
    EXPECT_NE((void *)NULL, desc2);
    EXPECT_NO_THROW(desc3 = my_fam->fam_create_region(
                        region3, (8192 * NUM_THREADS), 0777, NULL));
    EXPECT_NE((void *)NULL, desc3);

    //    item = (Fam_Descriptor *)malloc(sizeof(Fam_Descriptor) * 3);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item[0] = my_fam->fam_allocate(
                        "first", (1024 * NUM_THREADS), 0777, desc1));
    EXPECT_NE((void *)NULL, item[0]);
    EXPECT_NO_THROW(item[1] = my_fam->fam_allocate(
                        "second", (1024 * NUM_THREADS), 0777, desc2));
    EXPECT_NE((void *)NULL, item[1]);
    EXPECT_NO_THROW(item[2] = my_fam->fam_allocate(
                        "third", (1024 * NUM_THREADS), 0777, desc3));
    EXPECT_NE((void *)NULL, item[2]);

    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);
    for (i = 0; i < NUM_THREADS; ++i) {
        info[i].item[0] = item[0];
        info[i].item[1] = item[1];
        info[i].item[2] = item[2];
        info[i].offset = (uint64_t)i;
        info[i].tid = i;
        info[i].msg_size = MSG_SIZE;

        if ((rc = pthread_create(&thr[i], NULL, thr_func2, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
            //        return -1;
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }
    // Deallocating data items
    EXPECT_NO_THROW(my_fam->fam_deallocate(item[0]));
    EXPECT_NO_THROW(my_fam->fam_deallocate(item[1]));
    EXPECT_NO_THROW(my_fam->fam_deallocate(item[2]));

    // Destroying the region
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc1));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc2));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc3));
    free(info);
}
void *thr_func3_put(void *arg) {
    ValueInfo *valInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = valInfo->item;
    int tid = valInfo->tid;
    uint64_t off = tid * valInfo->msg_size;
    char chararr[MSG_SIZE];
    sprintf(chararr, "Test message %d", tid);
    char *local = strdup(chararr);

    // Put nonblocking
    EXPECT_NO_THROW(my_fam->fam_put_nonblocking(local, item, (uint64_t)off,
                                                valInfo->msg_size));
    pthread_barrier_wait(&barrier_half);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier_half);
    free(local);
    pthread_exit(NULL);
}

void *thr_func3_get(void *arg) {
    ValueInfo *valInfo = (ValueInfo *)arg;
    char chararr[MSG_SIZE];
    int tid = valInfo->tid;
    // int msg_no = (NUM_THREADS / 2);
    int msg_no = (tid - NUM_THREADS / 2);
    uint64_t off;
    off = (uint64_t)(valInfo->msg_size * msg_no);
    Fam_Descriptor *item = valInfo->item;
    sprintf(chararr, "Test message %d", msg_no);
    char *local = strdup(chararr);
    // Get nonblocking
    char *local2 = (char *)malloc(MSG_SIZE);

    EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local2, item, (uint64_t)off,
                                                valInfo->msg_size));
    pthread_barrier_wait(&barrier_half);
    if (tid == NUM_THREADS / 2) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier_half);
    EXPECT_STREQ(local, local2);
    free(local);
    free(local2);
    pthread_exit(NULL);
}
// Test case 3 - nonblocking put get test by multiple threads on same region and
// data item created before thread creation
TEST(FamPutGetMT, NonblockingPutGetMTSameRegionDataItemPutGet) {

    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    pthread_t thr[NUM_THREADS];
    int i, tid = 0;
    ValueInfo *info;
    mode_t test_perm_mode = 0777;
    size_t test_item_size = (1024 * NUM_THREADS);

    // Allocating data items in the created region

    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(dataItem, test_item_size,
                                             test_perm_mode, testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    for (i = 0; i < NUM_THREADS / 2; ++i) {
        tid = i;
        info[i] = {item, 0, tid, MSG_SIZE};
        if ((rc = pthread_create(&thr[i], NULL, thr_func3_put, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }
    for (i = 0; i < NUM_THREADS / 2; ++i) {
        pthread_join(thr[i], NULL);
    }

    for (i = NUM_THREADS / 2; i < NUM_THREADS; ++i) {
        tid = i;
        info[i] = {item, 0, tid, MSG_SIZE};
        if ((rc = pthread_create(&thr[i], NULL, thr_func3_get, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (i = NUM_THREADS / 2; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    delete item;
    free((void *)dataItem);

    free(info);
}
void *thr_func4_put(void *arg) {
    ValueInfo *valInfo = (ValueInfo *)arg;
    Fam_Descriptor *item;
    int tid = valInfo->tid;
    uint64_t off = tid * MSG_SIZE;
    char chararr[MSG_SIZE];
    sprintf(chararr, "Test message %d", tid);
    char data[20];
    sprintf(data, "first_%d", tid);
    const char *dataItem = get_uniq_str(data, my_fam);
    char *local = strdup(chararr);
    mode_t test_perm_mode = 0777;
    size_t test_item_size = (1024 * NUM_THREADS);
    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(dataItem, test_item_size,
                                             test_perm_mode, testRegionDesc));
    EXPECT_NE((void *)NULL, item);
    items[tid] = item;
    *valInfo = {item, 0, 0, 0};
    // Put nonblocking
    EXPECT_NO_THROW(my_fam->fam_put_nonblocking(local, items[tid],
                                                (uint64_t)off, MSG_SIZE));
    pthread_barrier_wait(&barrier_half);
    if (tid == 0) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier_half);
    free(local);
    free((void *)dataItem);
    pthread_exit(NULL);
}

void *thr_func4_get(void *arg) {
    ValueInfo *valInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = valInfo->item;
    int tid = valInfo->tid;
    EXPECT_NE((void *)NULL, item);
    char chararr[MSG_SIZE];
    int msg_no = (tid - NUM_THREADS / 2);
    uint64_t off;
    off = (uint64_t)(MSG_SIZE * msg_no);
    sprintf(chararr, "Test message %d", msg_no);
    char *local = strdup(chararr);
    // Get nonblocking
    char *local2 = (char *)malloc(MSG_SIZE);

    EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local2, items[msg_no],
                                                (uint64_t)off, MSG_SIZE));
    pthread_barrier_wait(&barrier_half);
    if (tid == NUM_THREADS / 2) {
        EXPECT_NO_THROW(my_fam->fam_quiet());
    }
    pthread_barrier_wait(&barrier_half);
    EXPECT_STREQ(local, local2);
    free(local);
    free(local2);
    pthread_exit(NULL);
}
// Test case 4 - nonblocking put get test by multiple threads on same region and
// data item created before thread creation
TEST(FamPutGetMT, NonblockingPutGetMTSameRegionPerThreadDataItemPutGet) {

    pthread_t thr[NUM_THREADS];
    int i, tid = 0;
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);
    for (i = 0; i < NUM_THREADS / 2; ++i) {
        tid = i;
        info[i] = {0, 0, tid, MSG_SIZE};
        if ((rc = pthread_create(&thr[i], NULL, thr_func4_put, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }
    for (i = 0; i < NUM_THREADS / 2; ++i) {
        pthread_join(thr[i], NULL);
    }

    for (i = NUM_THREADS / 2; i < NUM_THREADS; ++i) {
        tid = i;
        info[i] = {info[i - NUM_THREADS / 2].item, 0, tid, MSG_SIZE};
        if ((rc = pthread_create(&thr[i], NULL, thr_func4_get, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }
    for (i = NUM_THREADS / 2; i < NUM_THREADS; ++i)
        pthread_join(thr[i], NULL);
    EXPECT_NO_THROW(my_fam->fam_deallocate(info[i - NUM_THREADS / 2].item));
    free(info);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);
    my_fam = new fam();

    init_fam_options(&fam_opts);
    fam_opts.famThreadModel = strdup("FAM_THREAD_MULTIPLE");

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));
    testRegionStr = get_uniq_str("test", my_fam);
    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, REGION_PERM, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    pthread_barrier_init(&barrier, NULL, NUM_THREADS);
    pthread_barrier_init(&barrier_half, NULL, NUM_THREADS / 2);
    ret = RUN_ALL_TESTS();
    pthread_barrier_destroy(&barrier);
    pthread_barrier_destroy(&barrier_half);
    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));
    delete testRegionDesc;
    free((void *)testRegionStr);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
