/*
 * fam_dataitem_interleave_advanced_test.cpp
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
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

#define NUM_ELEMENTS 4
#define ELEMENT_SIZE 100
fam *my_fam;
Fam_Options fam_opts;

TEST(DataitemInterleavingNegative, PutGetInvalidOffset) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;

    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 16777216, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t INTERLEAVE_SIZE = item->get_interleave_size();

    char *local = (char *)malloc(6 * INTERLEAVE_SIZE);
    memset(local, 'a', INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + INTERLEAVE_SIZE), 'b', INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 2 * INTERLEAVE_SIZE), 'c',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 3 * INTERLEAVE_SIZE), 'd',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 4 * INTERLEAVE_SIZE), 'e',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 5 * INTERLEAVE_SIZE), 'f',
           INTERLEAVE_SIZE);

    EXPECT_THROW(my_fam->fam_put_blocking(local, item,
                                          16777216 - INTERLEAVE_SIZE,
                                          6 * INTERLEAVE_SIZE),
                 Fam_Exception);

    // allocate local memory to receive 20 elements
    char *local2 = (char *)malloc(6 * INTERLEAVE_SIZE);

    EXPECT_THROW(my_fam->fam_get_blocking(local2, item,
                                          16777216 - INTERLEAVE_SIZE,
                                          6 * INTERLEAVE_SIZE),
                 Fam_Exception);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleavingNegative, PutGetNBInvalidOffset) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;

    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 16777216, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t INTERLEAVE_SIZE = item->get_interleave_size();

    char *local = (char *)malloc(6 * INTERLEAVE_SIZE);
    memset(local, 'a', INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + INTERLEAVE_SIZE), 'b', INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 2 * INTERLEAVE_SIZE), 'c',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 3 * INTERLEAVE_SIZE), 'd',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 4 * INTERLEAVE_SIZE), 'e',
           INTERLEAVE_SIZE);
    memset((void *)((uint64_t)local + 5 * INTERLEAVE_SIZE), 'f',
           INTERLEAVE_SIZE);
#ifdef CHECK_OFFSETS
    EXPECT_THROW(my_fam->fam_put_nonblocking(local, item,
                                             16777216 - INTERLEAVE_SIZE,
                                             6 * INTERLEAVE_SIZE),
                 Fam_Exception);

#else
    EXPECT_NO_THROW(my_fam->fam_put_nonblocking(
        local, item, 16777216 - INTERLEAVE_SIZE, 6 * INTERLEAVE_SIZE));
    EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif
    // allocate local memory to receive 20 elements
    char *local2 = (char *)malloc(6 * INTERLEAVE_SIZE);

#ifdef CHECK_OFFSETS
    EXPECT_THROW(my_fam->fam_get_nonblocking(
        local2, item, 16777216 - INTERLEAVE_SIZE, 6 * INTERLEAVE_SIZE), Fam_Exception);
#else
    EXPECT_NO_THROW(my_fam->fam_get_nonblocking(
        local2, item, 16777216 - INTERLEAVE_SIZE, 6 * INTERLEAVE_SIZE));
    EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));

    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleaving, ScatterGatherStrideBlockInvalidOffset) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 10485760, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t INTERLEAVE_SIZE = item->get_interleave_size();
    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                      26, 27, 28, 29, 30, 31, 32, 33, 34, 35};

    EXPECT_THROW(my_fam->fam_scatter_blocking(newLocal, item, 5,
                                              16777216 - INTERLEAVE_SIZE, 4096,
                                              sizeof(int)),
                 Fam_Exception);

    int *local2 = (int *)malloc(10 * sizeof(int));

    EXPECT_THROW(my_fam->fam_gather_blocking(local2, item, 5,
                                             16777216 - INTERLEAVE_SIZE, 4096,
                                             sizeof(int)),
                 Fam_Exception);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleaving, ScatterGatherStrideNonblockInvalidOffset) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 10485760, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t INTERLEAVE_SIZE = item->get_interleave_size();
    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                      26, 27, 28, 29, 30, 31, 32, 33, 34, 35};
#ifdef CHECK_OFFSETS
    EXPECT_THROW(my_fam->fam_scatter_nonblocking(
        newLocal, item, 5, 16777216 - INTERLEAVE_SIZE, 4096, sizeof(int)), Fam_Exception);
#else
    EXPECT_NO_THROW(my_fam->fam_scatter_nonblocking(
        newLocal, item, 5, 16777216 - INTERLEAVE_SIZE, 4096, sizeof(int)));

    EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif
    int *local2 = (int *)malloc(10 * sizeof(int));
#ifdef CHECK_OFFSETS
    EXPECT_THROW(my_fam->fam_gather_nonblocking(
        local2, item, 5, 16777216 - INTERLEAVE_SIZE, 4096, sizeof(int)), Fam_Exception);

#else
    EXPECT_NO_THROW(my_fam->fam_gather_nonblocking(
        local2, item, 5, 16777216 - INTERLEAVE_SIZE, 4096, sizeof(int)));

    EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleaving, ScatterGatherIndexBlockInvalidOffset) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 10485760, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t INTERLEAVE_SIZE = item->get_interleave_size();
    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    uint64_t indexes[] = {
        16777216 - INTERLEAVE_SIZE, 16777216, 16777216 + INTERLEAVE_SIZE,
        16777216 - 2 * INTERLEAVE_SIZE, 16777216 - 3 * INTERLEAVE_SIZE};
    EXPECT_THROW(
        my_fam->fam_scatter_blocking(newLocal, item, 10, indexes, sizeof(int)),
        Fam_Exception);

    int *local2 = (int *)malloc(10 * sizeof(int));
    EXPECT_THROW(
        my_fam->fam_gather_blocking(local2, item, 10, indexes, sizeof(int)),
        Fam_Exception);

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleaving, ScatterGatherIndexNonblockInvalidOffset) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 10485760, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t INTERLEAVE_SIZE = item->get_interleave_size();
    // allocate an integer array and initialize it
    int newLocal[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    uint64_t indexes[] = {
        16777216 - INTERLEAVE_SIZE, 16777216, 16777216 + INTERLEAVE_SIZE,
        16777216 - 2 * INTERLEAVE_SIZE, 16777216 - 3 * INTERLEAVE_SIZE};
#ifdef CHECK_OFFSETS
    EXPECT_THROW(my_fam->fam_scatter_nonblocking(newLocal, item, 10, indexes,
                                                    sizeof(int)), Fam_Exception);

#else
    EXPECT_NO_THROW(my_fam->fam_scatter_nonblocking(newLocal, item, 10, indexes,
                                                    sizeof(int)));
    EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif
    int *local2 = (int *)malloc(10 * sizeof(int));
#ifdef CHECK_OFFSETS
    EXPECT_THROW(
        my_fam->fam_gather_nonblocking(local2, item, 10, indexes, sizeof(int)), Fam_Exception);
#else
    EXPECT_NO_THROW(
        my_fam->fam_gather_nonblocking(local2, item, 10, indexes, sizeof(int)));
    EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleavingAdvanced, ScatterGatherStrideElementSpanServer) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 10485760, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t INTERLEAVE_SIZE = item->get_interleave_size();

    char elementArray[NUM_ELEMENTS][ELEMENT_SIZE];
    char buffer[NUM_ELEMENTS][ELEMENT_SIZE];
    int asciiVal = 65; // ASCII value for 'A'
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        memset(elementArray[i], char(asciiVal), ELEMENT_SIZE - 1);
        memset((char *)((uint64_t)(elementArray[i]) + ELEMENT_SIZE - 1), '\0',
               1);  // terminate the string with null char
        asciiVal++; // Increment the ASCII value to next char
    }

    uint64_t stride = (INTERLEAVE_SIZE / ELEMENT_SIZE);

    EXPECT_NO_THROW(my_fam->fam_scatter_blocking(
        elementArray, item, NUM_ELEMENTS, stride, stride, ELEMENT_SIZE));

    EXPECT_NO_THROW(my_fam->fam_gather_blocking(buffer, item, NUM_ELEMENTS,
                                                stride, stride, ELEMENT_SIZE));

    for (int i = 0; i < NUM_ELEMENTS; i++) {
        EXPECT_STREQ(elementArray[i], buffer[i]);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleavingAdvanced, ScatterGatherIndexElementSpanServer) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 10485760, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t INTERLEAVE_SIZE = item->get_interleave_size();

    char elementArray[NUM_ELEMENTS][ELEMENT_SIZE];
    char buffer[NUM_ELEMENTS][ELEMENT_SIZE];
    int asciiVal = 65; // ASCII value for 'A'
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        memset(elementArray[i], char(asciiVal), ELEMENT_SIZE - 1);
        memset((char *)((uint64_t)(elementArray[i]) + ELEMENT_SIZE - 1), '\0',
               1);  // terminate the string with null char
        asciiVal++; // Increment the ASCII value to next char
    }

    uint64_t stride = (INTERLEAVE_SIZE / ELEMENT_SIZE);
    uint64_t indeces[NUM_ELEMENTS];
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        indeces[i] = i * stride;
    }

    // EXPECT_NO_THROW(
    //    my_fam->fam_scatter_blocking(elementArray, item, NUM_ELEMENTS,
    //    indeces, ELEMENT_SIZE));
    try {
        my_fam->fam_scatter_blocking(elementArray, item, NUM_ELEMENTS, indeces,
                                     ELEMENT_SIZE);
    } catch (Fam_Exception &e) {
        cout << e.fam_error_msg() << endl;
    }

    EXPECT_NO_THROW(my_fam->fam_gather_blocking(buffer, item, NUM_ELEMENTS,
                                                indeces, ELEMENT_SIZE));

    for (int i = 0; i < NUM_ELEMENTS; i++) {
        EXPECT_STREQ(elementArray[i], buffer[i]);
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

TEST(DataitemInterleavingAdvanced, AtomicSpanServer) {
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    const char *testRegion = get_uniq_str("test", my_fam);
    const char *firstItem = get_uniq_str("first", my_fam);

    EXPECT_NO_THROW(
        desc = my_fam->fam_create_region(testRegion, 104857600, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    // Allocating data items in the created region
    EXPECT_NO_THROW(item =
                        my_fam->fam_allocate(firstItem, 10485760, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    uint64_t INTERLEAVE_SIZE = item->get_interleave_size();
    uint64_t usedMemservCnt = item->get_used_memsrv_cnt();

    if (usedMemservCnt == 1) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

        delete item;
        delete desc;

        free((void *)testRegion);
        free((void *)firstItem);

        GTEST_SKIP();
    }
    char *local = (char *)malloc(10485760);
    memset(local, 0, 10485760);

    EXPECT_NO_THROW(my_fam->fam_put_blocking(local, item, 0, 10485760));

    int32_t value_i32 = 1;
    int64_t value_i64 = 1;
    int128_t value_i128 = 1;
    uint32_t value_u32 = 1;
    uint64_t value_u64 = 1;
    float value_f = 1.0;
    double value_d = 1.0;

    EXPECT_THROW(my_fam->fam_set(item, INTERLEAVE_SIZE - 1, value_i32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, INTERLEAVE_SIZE - 1, value_i64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, INTERLEAVE_SIZE - 1, value_i128),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, INTERLEAVE_SIZE - 1, value_f),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_set(item, INTERLEAVE_SIZE - 1, value_d),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_add(item, INTERLEAVE_SIZE - 1, value_i32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_add(item, INTERLEAVE_SIZE - 1, value_i64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_add(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_add(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_add(item, INTERLEAVE_SIZE - 1, value_f),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_add(item, INTERLEAVE_SIZE - 1, value_d),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_subtract(item, INTERLEAVE_SIZE - 1, value_i32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_subtract(item, INTERLEAVE_SIZE - 1, value_i64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_subtract(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_subtract(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_subtract(item, INTERLEAVE_SIZE - 1, value_f),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_subtract(item, INTERLEAVE_SIZE - 1, value_d),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_min(item, INTERLEAVE_SIZE - 1, value_i32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_min(item, INTERLEAVE_SIZE - 1, value_i64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_min(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_min(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_min(item, INTERLEAVE_SIZE - 1, value_f),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_min(item, INTERLEAVE_SIZE - 1, value_d),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_max(item, INTERLEAVE_SIZE - 1, value_i32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, INTERLEAVE_SIZE - 1, value_i64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, INTERLEAVE_SIZE - 1, value_f),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, INTERLEAVE_SIZE - 1, value_d),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_and(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_and(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_or(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_or(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_xor(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_xor(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_fetch_int32(item, INTERLEAVE_SIZE - 1),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_int64(item, INTERLEAVE_SIZE - 1),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_int128(item, INTERLEAVE_SIZE - 1),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_uint32(item, INTERLEAVE_SIZE - 1),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_uint64(item, INTERLEAVE_SIZE - 1),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_float(item, INTERLEAVE_SIZE - 1),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_double(item, INTERLEAVE_SIZE - 1),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_swap(item, INTERLEAVE_SIZE - 1, value_i32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_swap(item, INTERLEAVE_SIZE - 1, value_i64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_swap(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_swap(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_swap(item, INTERLEAVE_SIZE - 1, value_f),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_swap(item, INTERLEAVE_SIZE - 1, value_d),
                 Fam_Exception);

    int32_t oldValue_i32 = 1, newValue_i32 = 2;
    int64_t oldValue_i64 = 1, newValue_i64 = 2;
    uint32_t oldValue_u32 = 1, newValue_u32 = 2;
    uint64_t oldValue_u64 = 1, newValue_u64 = 2;
    int128_t oldValue_i128 = 1, newValue_i128 = 2;

    EXPECT_THROW(my_fam->fam_compare_swap(item, INTERLEAVE_SIZE - 1,
                                          oldValue_i32, newValue_i32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_compare_swap(item, INTERLEAVE_SIZE - 1,
                                          oldValue_i64, newValue_i64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_compare_swap(item, INTERLEAVE_SIZE - 1,
                                          oldValue_i128, newValue_i128),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_compare_swap(item, INTERLEAVE_SIZE - 1,
                                          oldValue_u32, newValue_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_compare_swap(item, INTERLEAVE_SIZE - 1,
                                          oldValue_u64, newValue_u64),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_fetch_add(item, INTERLEAVE_SIZE - 1, value_i32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_add(item, INTERLEAVE_SIZE - 1, value_i64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_add(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_add(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_add(item, INTERLEAVE_SIZE - 1, value_f),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_add(item, INTERLEAVE_SIZE - 1, value_d),
                 Fam_Exception);

    EXPECT_THROW(
        my_fam->fam_fetch_subtract(item, INTERLEAVE_SIZE - 1, value_i32),
        Fam_Exception);
    EXPECT_THROW(
        my_fam->fam_fetch_subtract(item, INTERLEAVE_SIZE - 1, value_i64),
        Fam_Exception);
    EXPECT_THROW(
        my_fam->fam_fetch_subtract(item, INTERLEAVE_SIZE - 1, value_u32),
        Fam_Exception);
    EXPECT_THROW(
        my_fam->fam_fetch_subtract(item, INTERLEAVE_SIZE - 1, value_u64),
        Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_subtract(item, INTERLEAVE_SIZE - 1, value_f),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_subtract(item, INTERLEAVE_SIZE - 1, value_d),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_fetch_min(item, INTERLEAVE_SIZE - 1, value_i32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_min(item, INTERLEAVE_SIZE - 1, value_i64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_min(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_min(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_min(item, INTERLEAVE_SIZE - 1, value_f),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_min(item, INTERLEAVE_SIZE - 1, value_d),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_fetch_max(item, INTERLEAVE_SIZE - 1, value_i32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_max(item, INTERLEAVE_SIZE - 1, value_i64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_max(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_max(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_max(item, INTERLEAVE_SIZE - 1, value_f),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_max(item, INTERLEAVE_SIZE - 1, value_d),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_fetch_and(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_and(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_fetch_or(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_or(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);

    EXPECT_THROW(my_fam->fam_fetch_xor(item, INTERLEAVE_SIZE - 1, value_u32),
                 Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_xor(item, INTERLEAVE_SIZE - 1, value_u64),
                 Fam_Exception);
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));

    delete item;
    delete desc;

    free((void *)testRegion);
    free((void *)firstItem);
}

int main(int argc, char **argv) {
    int ret = 0;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
