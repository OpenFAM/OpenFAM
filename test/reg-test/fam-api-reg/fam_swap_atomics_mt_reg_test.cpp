/*
 * fam_swap_atomics_reg_test.cpp
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
/* Test Case Description: Tests swap operations for multithreaded model.
 * (test case has been commented as there are some issues with negative test
 * scenarios with non-blocking calls.)
 *
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
    int itemId;
} ValueInfo2;

#define NUM_THREADS 10
#define REGION_SIZE (1024 * 1024 * NUM_THREADS)
#define REGION_PERM 0777

// Test case 1 - Swap Int32

void *thrd_swap_int32(void *arg) {

    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    int i, sm, ofs;
    sm = addInfo->itemId;
    uint64_t offset = addInfo->tid * sizeof(int32_t);
    size_t test_item_size[3] = {1024, 4096, 8192};
    int32_t oldValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    int32_t newValue[5] = {0x1, 0x4321, 0x12345, 0x7fffffff, 0x1};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(int32_t) - 1)};
    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {

            int32_t result;
            EXPECT_NO_THROW(my_fam->fam_set(addInfo->item[sm], testOffset[ofs],
                                            oldValue[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_swap(addInfo->item[sm],
                                                 testOffset[ofs], newValue[i]));
            EXPECT_EQ(oldValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_int32(addInfo->item[sm],
                                                             testOffset[ofs]));
            EXPECT_EQ(newValue[i], result);
        }
    }

    pthread_exit(NULL);
}
TEST(FamSwapAtomics, SwapInt32) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm;
    pthread_t thr[NUM_THREADS];

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {sizeof(int32_t) * 1024, sizeof(int32_t) * 4096,
                                sizeof(int32_t) * 8192};

    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);
    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
            if ((rc = pthread_create(&thr[i], NULL, thrd_swap_int32,
                                     &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    }
    delete item;
    free((void *)dataItem);
    free((void *)info);
}

// Test case 2 - Swap UInt32
void *thrd_swap_uint32(void *arg) {
    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    int i, sm, ofs;
    sm = addInfo->itemId;
    uint64_t offset = addInfo->tid * sizeof(uint32_t);
    size_t test_item_size[3] = {1024, 4096, 8192};
    uint32_t oldValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0xefffffff};
    uint32_t newValue[5] = {0x1, 0x4321, 0x12345, 0x80000000, 0x1};

    uint64_t testOffset[3] = {
        offset + 0, offset + (test_item_size[sm] / 2),
        (offset + test_item_size[sm] - sizeof(uint32_t) - 1)};
    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            uint32_t result;
            EXPECT_NO_THROW(my_fam->fam_set(addInfo->item[sm], testOffset[ofs],
                                            oldValue[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_swap(addInfo->item[sm],
                                                 testOffset[ofs], newValue[i]));
            EXPECT_EQ(oldValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_uint32(addInfo->item[sm],
                                                              testOffset[ofs]));
            EXPECT_EQ(newValue[i], result);
        }
    }

    pthread_exit(NULL);
}
TEST(FamSwapAtomics, SwapUInt32) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm;
    pthread_t thr[NUM_THREADS];

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {sizeof(uint32_t) * 1024,
                                sizeof(uint32_t) * 4096,
                                sizeof(uint32_t) * 8192};

    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);
    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
            if ((rc = pthread_create(&thr[i], NULL, thrd_swap_uint32,
                                     &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
                //        return -1;
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }
        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
    free((void *)info);
}

// Test case 3 - Swap Int64
void *thrd_swap_int64(void *arg) {

    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    int i, sm, ofs;
    sm = addInfo->itemId;
    uint64_t offset = addInfo->tid * sizeof(int64_t);
    size_t test_item_size[3] = {1024, 4096, 8192};
    int64_t oldValue[5] = {0x0, 0x1234, 0x1111222233334444, 0x7ffffffffffffffe,
                           0x7fffffffffffffff};
    int64_t newValue[5] = {0x1, 0x4321, 0x2111222233334321, 0x0000000000000000,
                           0x1111111111111111};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(int64_t) - 1)};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            int64_t result;
            EXPECT_NO_THROW(my_fam->fam_set(addInfo->item[sm], testOffset[ofs],
                                            oldValue[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_swap(addInfo->item[sm],
                                                 testOffset[ofs], newValue[i]));
            EXPECT_EQ(oldValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_int64(addInfo->item[sm],
                                                             testOffset[ofs]));
            EXPECT_EQ(newValue[i], result);
        }
    }

    pthread_exit(NULL);
}
TEST(FamSwapAtomics, SwapInt64) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm;

    pthread_t thr[NUM_THREADS];
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {sizeof(int64_t) * 1024, sizeof(int64_t) * 4096,
                                sizeof(int64_t) * 8192};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
            if ((rc = pthread_create(&thr[i], NULL, thrd_swap_int64,
                                     &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
    free((void *)info);
}

// Test case 4 - Swap UInt64
void *thrd_swap_uint64(void *arg) {

    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    int i, sm, ofs;
    sm = addInfo->itemId;
    uint64_t offset = addInfo->tid * sizeof(uint64_t);
    size_t test_item_size[3] = {1024, 4096, 8192};
    uint64_t oldValue[5] = {0x0, 0x1234, 0x1111222233334444, 0x7ffffffffffffffe,
                            0x7fffffffffffffff};
    uint64_t newValue[5] = {0x1, 0x4321, 0x2111222233334321, 0x8000000000000000,
                            0xefffffffffffffff};

    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(uint64_t) - 1)};
    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            uint64_t result;
            EXPECT_NO_THROW(my_fam->fam_set(addInfo->item[sm], testOffset[ofs],
                                            oldValue[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_swap(addInfo->item[sm],
                                                 testOffset[ofs], newValue[i]));
            EXPECT_EQ(oldValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_uint64(addInfo->item[sm],
                                                              testOffset[ofs]));
            EXPECT_EQ(newValue[i], result);
        }
    }
    pthread_exit(NULL);
}
TEST(FamSwapAtomics, SwapUInt64) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm;
    pthread_t thr[NUM_THREADS];
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {sizeof(uint64_t) * 1024,
                                sizeof(uint64_t) * 4096,
                                sizeof(uint64_t) * 8192};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
            if ((rc = pthread_create(&thr[i], NULL, thrd_swap_uint64,
                                     &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }
        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
    free((void *)info);
}

// Test case 5 - Swap Float
void *thrd_swap_float(void *arg) {

    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    int i, sm, ofs;
    sm = addInfo->itemId;
    uint64_t offset = addInfo->tid * sizeof(double);
    size_t test_item_size[3] = {1024, 4096, 8192};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(float) - 1)};
    float oldValue[5] = {0.0f, 1234.12f, 54321.87f, 8888.33f, 999990.99f};
    float newValue[5] = {0.1f, 2468.24f, 54321.99f, 9999.55f, 100000.00f};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            float result = 0.0;
            EXPECT_NO_THROW(my_fam->fam_set(addInfo->item[sm], testOffset[ofs],
                                            oldValue[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_swap(addInfo->item[sm],
                                                 testOffset[ofs], newValue[i]));
            EXPECT_FLOAT_EQ(oldValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_float(addInfo->item[sm],
                                                             testOffset[ofs]));
            EXPECT_FLOAT_EQ(newValue[i], result);
        }
    }

    pthread_exit(NULL);
}
TEST(FamSwapAtomics, SwapFloat) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm;
    pthread_t thr[NUM_THREADS];
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {sizeof(float) * 1024, sizeof(float) * 4096,
                                sizeof(float) * 8192};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
            if ((rc = pthread_create(&thr[i], NULL, thrd_swap_float,
                                     &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
    free((void *)info);
}

// Test case 6 - Swap Double
void *thrd_swap_double(void *arg) {

    ValueInfo2 *addInfo = (ValueInfo2 *)arg;
    int i, sm, ofs;
    uint64_t offset = addInfo->tid * sizeof(double);
    sm = addInfo->itemId;
    size_t test_item_size[3] = {1024, 4096, 8192};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                              offset +
                                  (test_item_size[sm] - sizeof(double) - 1)};
    double oldValue[5] = {0.0f, 1234.12f, 987654321.8765f,
                          2222555577778888.3333, DBL_MAX};
    double newValue[5] = {0.1f, 2468.24f, 987854321.7899f,
                          3333444466669999.5555, DBL_MIN};

    for (ofs = 0; ofs < 3; ofs++) {
        for (i = 0; i < 5; i++) {
            double result = 0.0;
            EXPECT_NO_THROW(my_fam->fam_set(addInfo->item[sm], testOffset[ofs],
                                            oldValue[i]));
            EXPECT_NO_THROW(my_fam->fam_quiet());
            EXPECT_NO_THROW(result =
                                my_fam->fam_swap(addInfo->item[sm],
                                                 testOffset[ofs], newValue[i]));
            EXPECT_DOUBLE_EQ(oldValue[i], result);
            EXPECT_NO_THROW(result = my_fam->fam_fetch_double(addInfo->item[sm],
                                                              testOffset[ofs]));
            EXPECT_DOUBLE_EQ(newValue[i], result);
        }
    }

    pthread_exit(NULL);
}
TEST(FamSwapAtomics, SwapDouble) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {sizeof(double) * 1024, sizeof(double) * 4096,
                                sizeof(double) * 8192};

    pthread_t thr[NUM_THREADS];
    ValueInfo2 *info = (ValueInfo2 *)malloc(sizeof(ValueInfo2) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);
        for (i = 0; i < NUM_THREADS; ++i) {
            info[i].item[sm] = item;
            info[i].offset = (uint64_t)i;
            info[i].tid = i;
            info[i].itemId = sm;
            if ((rc = pthread_create(&thr[i], NULL, thrd_swap_double,
                                     &info[i]))) {
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
    free((void *)info);
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
                        testRegionStr, REGION_SIZE, REGION_PERM, RAID1));
    EXPECT_NE((void *)NULL, testRegionDesc);

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));
    delete testRegionDesc;
    free((void *)testRegionStr);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
