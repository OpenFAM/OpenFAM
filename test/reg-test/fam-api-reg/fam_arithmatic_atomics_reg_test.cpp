/*
 * fam_arithmatic_atomics_reg_test.cpp
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

// Test case 1 - AddSubInt32NonBlock
TEST(FamArithmaticAtomics, AddSubInt32NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    int32_t baseValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    int32_t testAddValue[5] = {0x1, 0x1234, 0x54321, 0x1, 0x1};
    int32_t testExpectedValue[5] = {0x1, 0x2468, 0xA8642, 0x7fffffff, INT_MIN};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(int32_t) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                int32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_add(item, testOffset[ofs], testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int32(item, testOffset[ofs]));
                EXPECT_EQ(testExpectedValue[i], result);
                EXPECT_NO_THROW(my_fam->fam_subtract(item, testOffset[ofs],
                                                     testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int32(item, testOffset[ofs]));
                EXPECT_EQ(baseValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 2 - AddSubUInt32NonBlock
TEST(FamArithmaticAtomics, AddSubUInt32NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint32_t baseValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    uint32_t testAddValue[5] = {0x1, 0x1234, 0x54321, 0x1, 0x1};
    uint32_t testExpectedValue[5] = {0x1, 0x2468, 0xA8642, 0x7fffffff,
                                     0x80000000};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(uint32_t) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                uint32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_add(item, testOffset[ofs], testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(testExpectedValue[i], result);
                EXPECT_NO_THROW(my_fam->fam_subtract(item, testOffset[ofs],
                                                     testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(baseValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 3 - AddSubInt64NonBlock
TEST(FamArithmaticAtomics, AddSubInt64NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    int64_t baseValue[5] = {0x0, 0x1234, 0x1111222233334321, 0x7ffffffffffffffe,
                            0x7fffffffffffffff};
    int64_t testAddValue[5] = {0x1, 0x1234, 0x1111222233334321, 0x1, 0x1};
    int64_t testExpectedValue[5] = {0x1, 0x2468, 0x2222444466668642,
                                    0x7fffffffffffffff, LONG_MIN};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(int64_t) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                int64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_add(item, testOffset[ofs], testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(testExpectedValue[i], result);
                EXPECT_NO_THROW(my_fam->fam_subtract(item, testOffset[ofs],
                                                     testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(baseValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 4 - AddSubUInt64NonBlock
TEST(FamArithmaticAtomics, AddSubUInt64NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint64_t baseValue[5] = {0x0, 0x1234, 0x1111222233334321,
                             0x7ffffffffffffffe, 0x7fffffffffffffff};
    uint64_t testAddValue[5] = {0x1, 0x1234, 0x1111222233334321, 0x1, 0x1};
    uint64_t testExpectedValue[5] = {0x1, 0x2468, 0x2222444466668642,
                                     0x7fffffffffffffff, 0x8000000000000000};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(uint64_t) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                // run_add_test<int32_t>(item, testOffset[ofs], baseValue[i],
                // testAddValue[i], testExpectedValue[i], INT32);
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                uint64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_add(item, testOffset[ofs], testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testExpectedValue[i], result);
                EXPECT_NO_THROW(my_fam->fam_subtract(item, testOffset[ofs],
                                                     testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(baseValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 5 - AddSubFloatNonBlock
TEST(FamArithmaticAtomics, AddSubFloatNonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    float baseValue[5] = {0.0f, 1234.12f, 54321.87f, 8888.33f, 99999.99f};
    float testAddValue[5] = {0.1f, 1234.12f, 0.12f, 1111.22f, 0.01f};
    float testExpectedValue[5] = {0.1f, 2468.24f, 54321.99f, 9999.55f,
                                  100000.00f};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(float) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                // run_add_test<int32_t>(item, testOffset[ofs], baseValue[i],
                // testAddValue[i], testExpectedValue[i], INT32);
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                float result = 0.0f;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_add(item, testOffset[ofs], testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_float(item, testOffset[ofs]));
                EXPECT_FLOAT_EQ(testExpectedValue[i], result);
                EXPECT_NO_THROW(my_fam->fam_subtract(item, testOffset[ofs],
                                                     testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_float(item, testOffset[ofs]));
                EXPECT_FLOAT_EQ(baseValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 6 - AddSubDoubleNonBlock
TEST(FamArithmaticAtomics, AddSubDoubleNonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    double baseValue[5] = {0.0, 1234.123, 987654321.8765, 2222555577778888.3333,
                           (DBL_MAX - 1.0)};
    double testAddValue[5] = {0.1, 1234.123, 0.1234, 1111.2222, 1.0};
    double testExpectedValue[5] = {0.1, 2468.246, 987654321.9999,
                                   2222555577779999.5555, DBL_MAX};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(double) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                // run_add_test<int32_t>(item, testOffset[ofs], baseValue[i],
                // testAddValue[i], testExpectedValue[i], INT32);
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                double result = 0.0;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_add(item, testOffset[ofs], testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_double(item, testOffset[ofs]));
                EXPECT_DOUBLE_EQ(testExpectedValue[i], result);
                EXPECT_NO_THROW(my_fam->fam_subtract(item, testOffset[ofs],
                                                     testAddValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_double(item, testOffset[ofs]));
                EXPECT_DOUBLE_EQ(baseValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 7 - AddSubInt32Blocking
TEST(FamArithmaticAtomics, AddSubInt32Blocking) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    int32_t baseValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    int32_t testAddValue[5] = {0x1, 0x1234, 0x54321, 0x1, 0x1};
    int32_t testExpectedValue[5] = {0x1, 0x2468, 0xA8642, 0x7fffffff, INT_MIN};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(int32_t) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                int32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_add(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_EQ(baseValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_subtract(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_EQ(testExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 8 - AddSubUInt32Blocking
TEST(FamArithmaticAtomics, AddSubUInt32Blocking) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint32_t baseValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    uint32_t testAddValue[5] = {0x1, 0x1234, 0x54321, 0x1, 0x1};
    uint32_t testExpectedValue[5] = {0x1, 0x2468, 0xA8642, 0x7fffffff,
                                     0x80000000};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(uint32_t) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                uint32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_add(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_EQ(baseValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_subtract(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_EQ(testExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 9 - AddSubInt64Blocking
TEST(FamArithmaticAtomics, AddSubInt64Blocking) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    int64_t baseValue[5] = {0x0, 0x1234, 0x1111222233334321, 0x7ffffffffffffffe,
                            0x7fffffffffffffff};
    int64_t testAddValue[5] = {0x1, 0x1234, 0x1111222233334321, 0x1, 0x1};
    int64_t testExpectedValue[5] = {0x1, 0x2468, 0x2222444466668642,
                                    0x7fffffffffffffff, LONG_MIN};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(int64_t) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                int64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_add(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_EQ(baseValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_subtract(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_EQ(testExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 10 - AddSubUInt64Blocking
TEST(FamArithmaticAtomics, AddSubUInt64Blocking) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint64_t baseValue[5] = {0x0, 0x1234, 0x1111222233334321,
                             0x7ffffffffffffffe, 0x7fffffffffffffff};
    uint64_t testAddValue[5] = {0x1, 0x1234, 0x1111222233334321, 0x1, 0x1};
    uint64_t testExpectedValue[5] = {0x1, 0x2468, 0x2222444466668642,
                                     0x7fffffffffffffff, 0x8000000000000000};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(uint64_t) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                // run_add_test<int32_t>(item, testOffset[ofs], baseValue[i],
                // testAddValue[i], testExpectedValue[i], INT32);
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                uint64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_add(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_EQ(baseValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_subtract(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_EQ(testExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 11 - AddSubFloatBlocking
TEST(FamArithmaticAtomics, AddSubFloatBlocking) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    float baseValue[5] = {0.0f, 1234.12f, 54321.87f, 8888.33f, 99999.99f};
    float testAddValue[5] = {0.1f, 1234.12f, 0.12f, 1111.22f, 0.01f};
    float testExpectedValue[5] = {0.1f, 2468.24f, 54321.99f, 9999.55f,
                                  100000.00f};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(float) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                // run_add_test<int32_t>(item, testOffset[ofs], baseValue[i],
                // testAddValue[i], testExpectedValue[i], INT32);
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                float result = 0.0f;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_add(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_FLOAT_EQ(baseValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_subtract(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_FLOAT_EQ(testExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 12 - AddSubDoubleBlocking
TEST(FamArithmaticAtomics, AddSubDoubleBlocking) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    double baseValue[5] = {0.0, 1234.123, 987654321.8765, 2222555577778888.3333,
                           (DBL_MAX - 1.0)};
    double testAddValue[5] = {0.1, 1234.123, 0.1234, 1111.2222, 1.0};
    double testExpectedValue[5] = {0.1, 2468.246, 987654321.9999,
                                   2222555577779999.5555, DBL_MAX};

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(double) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                // run_add_test<int32_t>(item, testOffset[ofs], baseValue[i],
                // testAddValue[i], testExpectedValue[i], INT32);
                cout << "Testing fam_add: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", baseValue=" << baseValue[i]
                     << ", incr=" << testAddValue[i]
                     << ", expected=" << testExpectedValue[i] << endl;
                double result = 0.0;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], baseValue[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_add(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_DOUBLE_EQ(baseValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_subtract(
                                    item, testOffset[ofs], testAddValue[i]));
                EXPECT_DOUBLE_EQ(testExpectedValue[i], result);
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
                        testRegionStr, REGION_SIZE, REGION_PERM, RAID1));
    EXPECT_NE((void *)NULL, testRegionDesc);

    ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));
    delete testRegionDesc;
    free((void *)testRegionStr);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    return ret;
}
