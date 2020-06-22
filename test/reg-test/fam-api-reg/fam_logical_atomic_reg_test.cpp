/*
 * fam_logical_atomics_reg_test.cpp
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

// Test case 1 - Fetch logical atomics for UInt32
TEST(FamLogicalAtomics, FetchLogicalUInt32) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint32_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    uint32_t operand2Value[5] = {0x0, 0x54321, 0x1234, 0x11111111, 0x100};
    uint32_t testAndExpectedValue[5] = {0x0, 0x220, 0x220, 0x11111110, 0x100};
    uint32_t testOrExpectedValue[5] = {0x0, 0x55335, 0x55335, 0x7FFFFFFF,
                                       0x7FFFFFFF};
    uint32_t testXorExpectedValue[5] = {0x0, 0x55115, 0x55115, 0x6EEEEEEF,
                                        0x7FFFFEFF};

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
                cout << "Testing logical atomics: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", Operand1=" << operand1Value[i]
                     << ", Operand2=" << operand2Value[i]
                     << ", expected(and)=" << testAndExpectedValue[i]
                     << ", expected(or)=" << testOrExpectedValue[i]
                     << ", expected(xor)=" << testXorExpectedValue[i] << endl;

                uint32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(result = my_fam->fam_fetch_and(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(testAndExpectedValue[i], result);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(result = my_fam->fam_fetch_or(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(testOrExpectedValue[i], result);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(result = my_fam->fam_fetch_xor(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(testXorExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 2 - Fetch logical atomics for UInt64
TEST(FamLogicalAtomics, FetchLogicalUInt64) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint64_t operand1Value[5] = {0x0, 0x1234, 0xffff222233334321,
                                 0x7ffffffffffffffe, 0x7fffffffffffffff};
    uint64_t operand2Value[5] = {0x0, 0x1234, 0x1111eeee33334321, 0x0, 0x100};
    uint64_t testAndExpectedValue[5] = {0x0, 0x1234, 0x1111222233334321, 0x0,
                                        0x100};
    uint64_t testOrExpectedValue[5] = {0x0, 0x1234, 0xFFFFEEEE33334321,
                                       0x7ffffffffffffffe, 0x7FFFFFFFFFFFFFFF};
    uint64_t testXorExpectedValue[5] = {0x0, 0x0, 0xEEEECCCC00000000,
                                        0x7FFFFFFFFFFFFFFE, 0x7FFFFFFFFFFFFEFF};

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
                cout << "Testing logical atomics: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", operand1=" << operand1Value[i]
                     << ", operand2=" << operand2Value[i]
                     << ", expected(and)=" << testAndExpectedValue[i]
                     << ", expected(or)=" << testOrExpectedValue[i]
                     << ", expected(xor)=" << testXorExpectedValue[i] << endl;
                uint64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(result = my_fam->fam_fetch_and(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testAndExpectedValue[i], result);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(result = my_fam->fam_fetch_or(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testOrExpectedValue[i], result);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(result = my_fam->fam_fetch_xor(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testXorExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 3 - Non-fetch logical atomics for UInt32
TEST(FamLogicalAtomics, NonfetchLogicalUInt32) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint32_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    uint32_t operand2Value[5] = {0x0, 0x54321, 0x1234, 0x11111111, 0x100};
    uint32_t testAndExpectedValue[5] = {0x0, 0x220, 0x220, 0x11111110, 0x100};
    uint32_t testOrExpectedValue[5] = {0x0, 0x55335, 0x55335, 0x7FFFFFFF,
                                       0x7FFFFFFF};
    uint32_t testXorExpectedValue[5] = {0x0, 0x55115, 0x55115, 0x6EEEEEEF,
                                        0x7FFFFEFF};

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
                cout << "Testing logical atomics: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", Operand1=" << operand1Value[i]
                     << ", Operand2=" << operand2Value[i]
                     << ", expected(and)=" << testAndExpectedValue[i]
                     << ", expected(or)=" << testOrExpectedValue[i]
                     << ", expected(xor)=" << testXorExpectedValue[i] << endl;

                uint32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    my_fam->fam_and(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(testAndExpectedValue[i], result);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    my_fam->fam_or(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(testOrExpectedValue[i], result);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    my_fam->fam_xor(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(testXorExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 4 - Non-fetch logical atomics for UInt64
TEST(FamLogicalAtomics, NonfetchLogicalUInt64) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint64_t operand1Value[5] = {0x0, 0x1234, 0xffff222233334321,
                                 0x7ffffffffffffffe, 0x7fffffffffffffff};
    uint64_t operand2Value[5] = {0x0, 0x1234, 0x1111eeee33334321, 0x0, 0x100};
    uint64_t testAndExpectedValue[5] = {0x0, 0x1234, 0x1111222233334321, 0x0,
                                        0x100};
    uint64_t testOrExpectedValue[5] = {0x0, 0x1234, 0xFFFFEEEE33334321,
                                       0x7ffffffffffffffe, 0x7FFFFFFFFFFFFFFF};
    uint64_t testXorExpectedValue[5] = {0x0, 0x0, 0xEEEECCCC00000000,
                                        0x7FFFFFFFFFFFFFFE, 0x7FFFFFFFFFFFFEFF};

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
                cout << "Testing logical atomics: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", operand1=" << operand1Value[i]
                     << ", operand2=" << operand2Value[i]
                     << ", expected(and)=" << testAndExpectedValue[i]
                     << ", expected(or)=" << testOrExpectedValue[i]
                     << ", expected(xor)=" << testXorExpectedValue[i] << endl;
                uint64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    my_fam->fam_and(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testAndExpectedValue[i], result);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    my_fam->fam_or(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testOrExpectedValue[i], result);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    my_fam->fam_xor(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());

                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint64(item, testOffset[ofs]));
                EXPECT_EQ(testXorExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 5 - Negative test cases with invalid permissions
TEST(FamLogicalAtomics, NonfetchLogicalNegativePerm) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int sm, ofs;

    mode_t test_perm_mode[4] = {0400, 0444, 0455, 0411};
    size_t test_item_size[4] = {1024, 4096, 8192, 16384};

    uint32_t operandUint32[2] = {0x76543210, 0x12345678};
    uint64_t operandUint64[2] = {0x1111222233334444, 0x5555666677778888};

    for (sm = 0; sm < 4; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(uint64_t) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            cout << "Testing fam logical atomic: item=" << item
                 << ", permission=" << test_perm_mode[sm]
                 << ", Dataitem size=" << test_item_size[sm]
                 << ", offset=" << testOffset[ofs] << endl;
#ifdef SHM
            // uint32 operations
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_and(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_or(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_xor(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

            // uint64 operations
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_and(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_or(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_xor(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);

#else
            // uint32 operations
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_and(item, testOffset[ofs], operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_or(item, testOffset[ofs], operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_xor(item, testOffset[ofs], operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            // uint64 operations
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_and(item, testOffset[ofs], operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_or(item, testOffset[ofs], operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_xor(item, testOffset[ofs], operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 6 - Negative test cases for with invalid permissions
TEST(FamLogicalAtomics, FetchLogicalNegativePerm) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int sm, ofs;

    mode_t test_perm_mode[4] = {0400, 0444, 0455, 0411};
    size_t test_item_size[4] = {1024, 4096, 8192, 16384};

    uint32_t operandUint32[2] = {0x76543210, 0x12345678};
    uint64_t operandUint64[2] = {0x1111222233334444, 0x5555666677778888};

    for (sm = 0; sm < 4; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(uint64_t) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            cout << "Testing fam logical atomic: item=" << item
                 << ", permission=" << test_perm_mode[sm]
                 << ", Dataitem size=" << test_item_size[sm]
                 << ", offset=" << testOffset[ofs] << endl;

#ifdef SHM
            // uint32 operations
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                Fam_Exception);
#else
            // uint32 operations
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif
            EXPECT_THROW(
                my_fam->fam_fetch_and(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_fetch_or(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_fetch_xor(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

#ifdef SHM
            // uint32 operations
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                Fam_Exception);
#else
            // uint64 operations
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif

            EXPECT_THROW(
                my_fam->fam_fetch_and(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_fetch_or(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_fetch_xor(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 7 - Negative test cases with invalid offset
TEST(FamLogicalAtomics, NonfetchLogicalNegativeInvalidOffset) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int sm, ofs;

    mode_t test_perm_mode[4] = {0600, 0644, 0755, 0711};
    size_t test_item_size[4] = {1024, 4096, 8192, 16384};

    uint32_t operandUint32[2] = {0x76543210, 0x12345678};
    uint64_t operandUint64[2] = {0x1111222233334444, 0x5555666677778888};

    for (sm = 0; sm < 4; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {test_item_size[sm], (2 * test_item_size[sm]),
                                  (test_item_size[sm] + 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            cout << "Testing fam logical atomic: item=" << item
                 << ", permission=" << test_perm_mode[sm]
                 << ", Dataitem size=" << test_item_size[sm]
                 << ", offset=" << testOffset[ofs] << endl;

#ifdef SHM
            // uint32 operations
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_and(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_or(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_xor(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

            // uint64 operations
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_and(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_or(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_xor(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);
#else
            // uint32 operations
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_and(item, testOffset[ofs], operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_or(item, testOffset[ofs], operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_xor(item, testOffset[ofs], operandUint32[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            // uint64 operations
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_and(item, testOffset[ofs], operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_or(item, testOffset[ofs], operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

            EXPECT_NO_THROW(
                my_fam->fam_xor(item, testOffset[ofs], operandUint64[1]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 8 - Negative test cases with invalid offset
TEST(FamLogicalAtomics, FetchLogicalNegativeInvalidOffset) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int sm, ofs;

    mode_t test_perm_mode[4] = {0600, 0644, 0755, 0711};
    size_t test_item_size[4] = {1024, 4096, 8192, 16384};

    uint32_t operandUint32[2] = {0x76543210, 0x12345678};
    uint64_t operandUint64[2] = {0x1111222233334444, 0x5555666677778888};

    for (sm = 0; sm < 4; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {test_item_size[sm], (2 * test_item_size[sm]),
                                  (test_item_size[sm] + 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            cout << "Testing fam logical atomic: item=" << item
                 << ", permission=" << test_perm_mode[sm]
                 << ", Dataitem size=" << test_item_size[sm]
                 << ", offset=" << testOffset[ofs] << endl;

#ifdef SHM
            // uint32 operations
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                Fam_Exception);
#else
            // uint32 operations
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif

            EXPECT_THROW(
                my_fam->fam_fetch_and(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_fetch_or(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_fetch_xor(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

#ifdef SHM
            // uint32 operations
            EXPECT_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                Fam_Exception);
#else
            // uint64 operations
            EXPECT_NO_THROW(
                my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
            EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
#endif

            EXPECT_THROW(
                my_fam->fam_fetch_and(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_fetch_or(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);

            EXPECT_THROW(
                my_fam->fam_fetch_xor(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);
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
