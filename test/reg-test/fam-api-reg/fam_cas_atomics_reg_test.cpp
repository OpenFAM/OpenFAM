/*
 * fam_cas_atomics_reg_test.cpp
 * Copyright (c) 2019, 2022 Hewlett Packard Enterprise Development, LP. All
 * rights reserved. Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following conditions
 * are met:
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

#define REGION_SIZE (1024 * 1024)
#define REGION_PERM 0777

// Test case 1 - Compare And Swap Int32
TEST(FamCASAtomics, CASInt32) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    int32_t oldValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    int32_t cmpValue[5] = {0x0, 0x1234, 0x12345, 0x7ffffffe, 0x0};
    int32_t newValue[5] = {0x1, 0x4321, 0x12345, 0x7fffffff, 0x1};
    int32_t expValue[5] = {0x1, 0x4321, 0x54321, 0x7fffffff, 0x7fffffff};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - 2 * sizeof(int32_t))};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_compare_and_swap: item=" << item
                     << ", offset=" << hex << testOffset[ofs]
                     << ", oldValue=" << hex << oldValue[i]
                     << ", cmpValue=" << hex << cmpValue[i]
                     << ", newValue=" << hex << newValue[i]
                     << ", expected=" << hex << expValue[i] << endl;

                int32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], oldValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(my_fam->fam_compare_swap(
                    item, testOffset[ofs], cmpValue[i], newValue[i]));
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int32(item, testOffset[ofs]));
                EXPECT_EQ(expValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 2 - Compare And Swap UInt32
TEST(FamCASAtomics, CASUInt32) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint32_t oldValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0xefffffff};
    uint32_t cmpValue[5] = {0x0, 0x1234, 0x12345, 0x7ffffffe, 0x0};
    uint32_t newValue[5] = {0x1, 0x4321, 0x12345, 0x80000000, 0x1};
    uint32_t expValue[5] = {0x1, 0x4321, 0x54321, 0x80000000, 0xefffffff};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - 2 * sizeof(uint32_t))};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_compare_and_swap: item=" << item
                     << ", offset=" << hex << testOffset[ofs]
                     << ", oldValue=" << hex << oldValue[i]
                     << ", cmpValue=" << hex << cmpValue[i]
                     << ", newValue=" << hex << newValue[i]
                     << ", expected=" << hex << expValue[i] << endl;

                uint32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], oldValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(my_fam->fam_compare_swap(
                    item, testOffset[ofs], cmpValue[i], newValue[i]));
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(expValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 3 - Compare And Swap Int64
TEST(FamCASAtomics, CASInt64) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    int64_t oldValue[5] = {0x0, 0x1234, 0x1111222233334444, 0x7ffffffffffffffe,
                           0x7fffffffffffffff};
    int64_t cmpValue[5] = {0x0, 0x1234, 0x11112222ffff4321, 0x7ffffffffffffffe,
                           0x0000000000000000};
    int64_t newValue[5] = {0x1, 0x4321, 0x2111222233334321, 0x0000000000000000,
                           0x1111111111111111};
    int64_t expValue[5] = {0x1, 0x4321, 0x1111222233334444, 0x0000000000000000,
                           0x7fffffffffffffff};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - 2 * sizeof(int64_t))};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_compare_and_swap: item=" << item
                     << ", offset=" << hex << testOffset[ofs]
                     << ", oldValue=" << hex << oldValue[i]
                     << ", cmpValue=" << hex << cmpValue[i]
                     << ", newValue=" << hex << newValue[i]
                     << ", expected=" << hex << expValue[i] << endl;

                int64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], oldValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(my_fam->fam_compare_swap(
                    item, testOffset[ofs], cmpValue[i], newValue[i]));
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(expValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 4 - Compare And Swap UInt64
TEST(FamCASAtomics, CASUInt64) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint64_t oldValue[5] = {0x0, 0x1234, 0x1111222233334444, 0x7ffffffffffffffe,
                            0x7fffffffffffffff};
    uint64_t cmpValue[5] = {0x0, 0x1234, 0x11112222ffff4321, 0x7ffffffffffffffe,
                            0x7fffffffffffffff};
    uint64_t newValue[5] = {0x1, 0x4321, 0x2111222233334321, 0x8000000000000000,
                            0xefffffffffffffff};
    uint64_t expValue[5] = {0x1, 0x4321, 0x1111222233334444, 0x8000000000000000,
                            0xefffffffffffffff};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - 2 * sizeof(uint64_t))};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_compare_and_swap: item=" << item
                     << ", offset=" << hex << testOffset[ofs]
                     << ", oldValue=" << hex << oldValue[i]
                     << ", cmpValue=" << hex << cmpValue[i]
                     << ", newValue=" << hex << newValue[i]
                     << ", expected=" << hex << expValue[i] << endl;

                uint64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], oldValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(my_fam->fam_compare_swap(
                    item, testOffset[ofs], cmpValue[i], newValue[i]));
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(expValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 5 - Compare And Swap Int128
TEST(FamCASAtomics, CASInt128) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

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

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - 2 * sizeof(int64_t))};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_compare_and_swap: item=" << item
                     << ", offset=" << hex << testOffset[ofs]
                     << ", oldValue=" << hex << oldValue[i].i64[0]
                     << oldValue[i].i64[1] << ", cmpValue=" << hex
                     << cmpValue[i].i64[0] << cmpValue[i].i64[1]
                     << ", newValue=" << hex << newValue[i].i64[0]
                     << newValue[i].i64[1] << ", expected=" << hex
                     << expValue[i].i64[0] << expValue[i].i64[1] << endl;

                int64_t result0, result1;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], oldValue[i].i64[0]));
                EXPECT_NO_THROW(my_fam->fam_set(item, testOffset[ofs] + 8,
                                                oldValue[i].i64[1]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(my_fam->fam_compare_swap(
                    item, testOffset[ofs], cmpValue[i].i128, newValue[i].i128));
                EXPECT_NO_THROW(
                    result0 = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(expValue[i].i64[0], result0);
                EXPECT_NO_THROW(result1 = my_fam->fam_fetch_int64(
                                    item, testOffset[ofs] + 8));
                EXPECT_EQ(expValue[i].i64[1], result1);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    testRegionStr = get_uniq_str("test", my_fam);

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        testRegionStr, REGION_SIZE, REGION_PERM, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));
    delete testRegionDesc;
    free((void *)testRegionStr);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
