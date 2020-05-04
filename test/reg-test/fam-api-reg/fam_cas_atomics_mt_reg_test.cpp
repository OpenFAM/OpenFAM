/*
 * fam_cas_atomics_mt_reg_test.cpp
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
/* Test Case Description: Tests fam_compare_swap operations for multithreaded
 * model.
 */

#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;

Fam_Region_Descriptor *testRegionDesc;
const char *testRegionStr;
int rc;
#define NUM_THREADS 10
#define REGION_SIZE (32 * 1024 * 1024 * NUM_THREADS)
#define REGION_PERM 0777

mode_t test_perm_mode[3] = {0777, 0644, 0600};
size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS,
                            8192 * NUM_THREADS};

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t deltaValue;
} ValueInfo;

void *thrd_cas_int32(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t sm = (addInfo->offset);
    int i, ofs;
    uint64_t offset = addInfo->tid * sizeof(int32_t);

    int32_t oldValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    int32_t cmpValue[5] = {0x0, 0x1234, 0x12345, 0x7ffffffe, 0x0};
    int32_t newValue[5] = {0x1, 0x4321, 0x12345, 0x7fffffff, 0x1};
    int32_t expValue[5] = {0x1, 0x4321, 0x54321, 0x7fffffff, 0x7fffffff};

    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(int32_t) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            int32_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], oldValue[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(my_fam->fam_compare_swap(item, testOffset[ofs],
                                                     cmpValue[i], newValue[i]));
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int32(item, testOffset[ofs]));
            EXPECT_EQ(expValue[i], result);
        }
    }
    pthread_exit(NULL);
}

// Test case 1 - Compare And Swap Int32
TEST(FamCASAtomics, CASInt32) {
    Fam_Descriptor *item[3];
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, tid;
    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item[sm] = my_fam->fam_allocate(
                            dataItem,
                            test_item_size[sm] * sizeof(int32_t) * NUM_THREADS,
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            tid = i;
            info[i] = {item[sm], (uint64_t)sm, tid, 0};
            if ((rc =
                     pthread_create(&thr[i], NULL, thrd_cas_int32, &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }
    // delete item[sm];
    free((void *)info);
    free((void *)dataItem);
}

void *thrd_cas_uint32(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t sm = (addInfo->offset);
    int i, ofs;
    uint64_t offset = addInfo->tid * sizeof(uint32_t);
    uint32_t oldValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0xefffffff};
    uint32_t cmpValue[5] = {0x0, 0x1234, 0x12345, 0x7ffffffe, 0x0};
    uint32_t newValue[5] = {0x1, 0x4321, 0x12345, 0x80000000, 0x1};
    uint32_t expValue[5] = {0x1, 0x4321, 0x54321, 0x80000000, 0xefffffff};

    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(uint32_t) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            uint32_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], oldValue[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(my_fam->fam_compare_swap(item, testOffset[ofs],
                                                     cmpValue[i], newValue[i]));
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int32(item, testOffset[ofs]));
            EXPECT_EQ(expValue[i], result);
        }
    }
    pthread_exit(NULL);
}
// Test case 2 - Compare And Swap UInt32
TEST(FamCASAtomics, CASUInt32) {
    Fam_Descriptor *item[3];
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, tid;
    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item[sm] = my_fam->fam_allocate(
                            dataItem,
                            test_item_size[sm] * NUM_THREADS * sizeof(uint32_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            tid = i;
            info[i] = {item[sm], (uint64_t)sm, tid, 0};
            if ((rc = pthread_create(&thr[i], NULL, thrd_cas_uint32,
                                     &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }
        EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }
    // delete item[sm];
    free((void *)info);
    free((void *)dataItem);
}

// Test case 3 - Compare And Swap Int64
void *thrd_cas_int64(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t sm = (addInfo->offset);
    int i, ofs;
    uint64_t offset = addInfo->tid * sizeof(int64_t);
    int64_t oldValue[5] = {0x0, 0x1234, 0x1111222233334444, 0x7ffffffffffffffe,
                           0x7fffffffffffffff};
    int64_t cmpValue[5] = {0x0, 0x1234, 0x11112222ffff4321, 0x7ffffffffffffffe,
                           0x0000000000000000};
    int64_t newValue[5] = {0x1, 0x4321, 0x2111222233334321, 0x0000000000000000,
                           0x1111111111111111};
    int64_t expValue[5] = {0x1, 0x4321, 0x1111222233334444, 0x0000000000000000,
                           0x7fffffffffffffff};

    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(int64_t) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            int64_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], oldValue[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(my_fam->fam_compare_swap(item, testOffset[ofs],
                                                     cmpValue[i], newValue[i]));
            EXPECT_NO_THROW(result =
                                my_fam->fam_fetch_int64(item, testOffset[ofs]));
            EXPECT_EQ(expValue[i], result);
        }
    }
    pthread_exit(NULL);
}
TEST(FamCASAtomics, CASInt64) {
    Fam_Descriptor *item[3];
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, tid;
    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item[sm] = my_fam->fam_allocate(
                            dataItem,
                            test_item_size[sm] * NUM_THREADS * sizeof(int64_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            tid = i;
            info[i] = {item[sm], (uint64_t)sm, tid, 0};
            if ((rc =
                     pthread_create(&thr[i], NULL, thrd_cas_int64, &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }
    // delete item[sm];
    free((void *)info);

    free((void *)dataItem);
}

// Test case 4 - Compare And Swap UInt64
void *thrd_cas_uint64(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t sm = (addInfo->offset);
    int i, ofs;
    uint64_t offset = addInfo->tid * sizeof(uint64_t);
    uint64_t oldValue[5] = {0x0, 0x1234, 0x1111222233334444, 0x7ffffffffffffffe,
                            0x7fffffffffffffff};
    uint64_t cmpValue[5] = {0x0, 0x1234, 0x11112222ffff4321, 0x7ffffffffffffffe,
                            0x7fffffffffffffff};
    uint64_t newValue[5] = {0x1, 0x4321, 0x2111222233334321, 0x8000000000000000,
                            0xefffffffffffffff};
    uint64_t expValue[5] = {0x1, 0x4321, 0x1111222233334444, 0x8000000000000000,
                            0xefffffffffffffff};

    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(uint64_t) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {

            uint64_t result;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], oldValue[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(my_fam->fam_compare_swap(item, testOffset[ofs],
                                                     cmpValue[i], newValue[i]));
            EXPECT_NO_THROW(
                result = my_fam->fam_fetch_uint64(item, testOffset[ofs]));
            EXPECT_EQ(expValue[i], result);
        }
    }
    pthread_exit(NULL);
}
TEST(FamCASAtomics, CASUInt64) {
    Fam_Descriptor *item[3];
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, tid;
    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item[sm] = my_fam->fam_allocate(
                            dataItem,
                            test_item_size[sm] * NUM_THREADS * sizeof(uint64_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            tid = i;
            info[i] = {item[sm], (uint64_t)sm, tid, 0};
            if ((rc = pthread_create(&thr[i], NULL, thrd_cas_uint64,
                                     &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }
    //    delete item[sm];
    free((void *)info);
    free((void *)dataItem);
}

// Test case 5 - Compare And Swap Int128
void *thrd_cas_int128(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t sm = (addInfo->offset);
    int i, ofs;
    uint64_t offset = addInfo->tid * 2 * sizeof(int64_t);
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - 2 * sizeof(int64_t))};

    union int128store {
        struct {
            uint64_t low;
            uint64_t high;
        };
        int64_t i64[2];
        int128_t i128;
    };
    int128store oldValue[5], cmpValue[5], newValue[5], expValue[5];

    oldValue[0].i64[0] = cmpValue[0].i64[0] = 0x0;
    oldValue[0].i64[1] = cmpValue[0].i64[1] = 0x1000;
    newValue[0].i64[0] = expValue[0].i64[0] = 0x1;
    newValue[0].i64[1] = expValue[0].i64[1] = 0x1234;

    oldValue[1].i64[0] = cmpValue[1].i64[0] = 0x1234;
    oldValue[1].i64[1] = cmpValue[1].i64[1] = 0x1234AA;
    newValue[1].i64[0] = expValue[1].i64[0] = 0x4321;
    newValue[1].i64[1] = expValue[1].i64[1] = 0x11223344;

    oldValue[2].i64[0] = expValue[2].i64[0] = 0x1111222233334444;
    oldValue[2].i64[1] = expValue[2].i64[1] = 0xAAAA22223333DDDD;
    newValue[2].i64[0] = 0x2111222233334321;
    newValue[2].i64[1] = 0xeffffffffffffffe;
    cmpValue[2].i64[0] = 0x11112222ffff4321;
    cmpValue[2].i64[1] = 0xAAAA222233334444;

    oldValue[3].i64[0] = cmpValue[3].i64[0] = 0x7ffffffffffffffe;
    oldValue[3].i64[1] = cmpValue[3].i64[1] = 0x123456789ABCDEF0;
    newValue[3].i64[0] = expValue[3].i64[0] = 0x8000000000000000;
    newValue[3].i64[1] = expValue[3].i64[1] = 0xABCDEFABCDEF1111;

    oldValue[4].i64[0] = expValue[4].i64[0] = 0x1111222233334444;
    oldValue[4].i64[1] = expValue[4].i64[1] = 0x1234;
    newValue[4].i64[0] = 0x2111222233334321;
    newValue[4].i64[1] = 0xeffffffffffffffe;
    cmpValue[4].i64[0] = 0x123123123123;
    cmpValue[4].i64[1] = 0xAAAA222233334444;

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            int64_t result0, result1;
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], oldValue[i].i64[0]));
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs] + 8, oldValue[i].i64[1]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(my_fam->fam_compare_swap(
                item, testOffset[ofs], cmpValue[i].i128, newValue[i].i128));
            EXPECT_NO_THROW(result0 =
                                my_fam->fam_fetch_int64(item, testOffset[ofs]));
            EXPECT_EQ(expValue[i].i64[0], result0);
            EXPECT_NO_THROW(
                result1 = my_fam->fam_fetch_int64(item, testOffset[ofs] + 8));
            EXPECT_EQ(expValue[i].i64[1], result1);
        }
    }
    pthread_exit(NULL);
}
TEST(FamCASAtomics, CASInt128) {
    Fam_Descriptor *item[3];
    pthread_t thr[NUM_THREADS];
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, tid;
    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(item[sm] = my_fam->fam_allocate(
                            dataItem,
                            test_item_size[sm] * NUM_THREADS * sizeof(int128_t),
                            test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            tid = i;
            info[i] = {item[sm], (uint64_t)sm, tid, 0};
            if ((rc = pthread_create(&thr[i], NULL, thrd_cas_int128,
                                     &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }
    free(info);
    ;
    free((void *)dataItem);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));
    fam_opts.famThreadModel = strdup("FAM_THREAD_MULTIPLE");
    //    fam_opts.runtime = strdup("NONE");
    testRegionStr = get_uniq_str("test", my_fam);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, REGION_PERM, RAID1));
    EXPECT_NE((void *)NULL, testRegionDesc);

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));
    delete testRegionDesc;
    free((void *)testRegionStr);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
