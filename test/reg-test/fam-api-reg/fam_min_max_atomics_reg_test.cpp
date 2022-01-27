/*
 * fam_min_max_atomics_reg_test.cpp
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

char *openFamModel;

Fam_Region_Descriptor *testRegionDesc;
const char *testRegionStr;

#define REGION_SIZE (1024 * 1024)
#define REGION_PERM 0777

#define SHM_CHECK (strcmp(openFamModel, "shared_memory") == 0)
/*
// Test case 1 - MinMaxInt32NonBlock
TEST(FamMinMaxAtomics, MinMaxInt32NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    int32_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffff,
                                (int32_t)0x87654321};
    int32_t operand2Value[5] = {(int32_t)0xf0000000, 0x1234, 0x7fffffff, 0x0,
                                (int32_t)0xffffffff};
    int32_t operand3Value[5] = {0x0, (int32_t)0xffff0000, 0x54321, 0x1,
                                0x7fffffff};
    int32_t testMinExpectedValue[5] = {(int32_t)0xf0000000, 0x1234, 0x54321,
                                       0x0, (int32_t)0x87654321};
    int32_t testMaxExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x1, 0x7fffffff};

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
                cout << "Testing min max atomics: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", operand1Value=" << operand1Value[i]
                     << ", operand2Value=" << operand2Value[i]
                     << ", operand3Value=" << operand3Value[i]
                     << ", expected(min)=" << testMinExpectedValue[i]
                     << ", expected(max)=" << testMaxExpectedValue[i] << endl;
                int32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int32(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int32(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 2 - MinMaxUInt32NonBlock
TEST(FamMinMaxAtomics, MinMaxUInt32NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint32_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffff, 0x87654321};
    uint32_t operand2Value[5] = {0xf0000000, 0x1234, 0x7fffffff, 0x0,
                                 0xffffffff};
    uint32_t operand3Value[5] = {0x0, 0xffff0000, 0x54321, 0x1, 0x7fffffff};
    uint32_t testMinExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x0, 0x87654321};
    uint32_t testMaxExpectedValue[5] = {0x0, 0xffff0000, 0x54321, 0x1,
                                        0x87654321};

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
                cout << "Testing min max atomics: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", operand1Value=" << operand1Value[i]
                     << ", operand2Value=" << operand2Value[i]
                     << ", operand3Value=" << operand3Value[i]
                     << ", expected(min)=" << testMinExpectedValue[i]
                     << ", expected(max)=" << testMaxExpectedValue[i] << endl;
                uint32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 3 - MinMaxInt64NonBlock
TEST(FamMinMaxAtomics, MinMaxInt64NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    int64_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffffffffffff,
                                (int64_t)0xfedcba9876543210};
    int64_t operand2Value[5] = {(int64_t)0xf000000000000000, 0x1234,
                                0x7fffffffffffffff, 0x0,
                                (int64_t)0xffffffffffffffff};
    int64_t operand3Value[5] = {0x0, (int64_t)0xffffffff00000000, 0x54321, 0x1,
                                0x7fffffffffffffff};
    int64_t testMinExpectedValue[5] = {(int64_t)0xf000000000000000, 0x1234,
                                       0x54321, 0x0,
                                       (int64_t)0xfedcba9876543210};
    int64_t testMaxExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x1,
                                       0x7fffffffffffffff};

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
                cout << "Testing min max atomics: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", operand1Value=" << operand1Value[i]
                     << ", operand2Value=" << operand2Value[i]
                     << ", operand3Value=" << operand3Value[i]
                     << ", expected(min)=" << testMinExpectedValue[i]
                     << ", expected(max)=" << testMaxExpectedValue[i] << endl;
                int64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 4 - MinMaxUInt64NonBlock
TEST(FamMinMaxAtomics, MinMaxUInt64NonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint64_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffffffffffff,
                                 0xfedcba9876543210};
    uint64_t operand2Value[5] = {0xf000000000000000, 0x1234, 0x7fffffffffffffff,
                                 0x0, 0xffffffffffffffff};
    uint64_t operand3Value[5] = {0x0, 0xffffffff00000000, 0x54321, 0x1,
                                 0x7fffffffffffffff};
    uint64_t testMinExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x0,
                                        0xfedcba9876543210};
    uint64_t testMaxExpectedValue[5] = {0x0, 0xffffffff00000000, 0x54321, 0x1,
                                        0xfedcba9876543210};

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
                cout << "Testing min max atomics: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", operand1Value=" << operand1Value[i]
                     << ", operand2Value=" << operand2Value[i]
                     << ", operand3Value=" << operand3Value[i]
                     << ", expected(min)=" << testMinExpectedValue[i]
                     << ", expected(max)=" << testMaxExpectedValue[i] << endl;
                uint64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 5 - MinMaxFloatNonBlock
TEST(FamMinMaxAtomics, MinMaxFloatNonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    float operand1Value[5] = {0.0f, 1234.12f, 54321.87f, 8888.33f, 99999.99f};
    float operand2Value[5] = {0.1f, 1234.12f, 0.12f, 9999.22f, 0.01f};
    float operand3Value[5] = {0.5f, 1234.23f, 0.01f, 5432.10f, 0.05f};
    float testMinExpectedValue[5] = {0.0f, 1234.12f, 0.12f, 8888.33f, 0.01f};
    float testMaxExpectedValue[5] = {0.5f, 1234.23f, 0.12f, 8888.33f, 0.05f};

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
                cout << "Testing min max atomics: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", operand1Value=" << operand1Value[i]
                     << ", operand2Value=" << operand2Value[i]
                     << ", operand3Value=" << operand3Value[i]
                     << ", expected(min)=" << testMinExpectedValue[i]
                     << ", expected(max)=" << testMaxExpectedValue[i] << endl;
                float result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_float(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_float(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 6 - MinMaxDoubleNonBlock
TEST(FamMinMaxAtomics, MinMaxDoubleNonBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    double operand1Value[5] = {0.0, 1234.123, 987654321.8765,
                               2222555577778888.3333, (DBL_MAX - 1.0)};
    double operand2Value[5] = {0.1, 1234.123, 0.1234, 1111.2222, 1.0};
    double operand3Value[5] = {1.5, 5432.123, 0.0123, 2222555577778888.3333,
                               DBL_MAX - 1};
    double testMinExpectedValue[5] = {0.0, 1234.123, 0.1234, 1111.2222, 1.0};
    double testMaxExpectedValue[5] = {1.5, 5432.123, 0.1234,
                                      2222555577778888.3333, DBL_MAX - 1};

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
                cout << "Testing min max atomics: item=" << item
                     << ", offset=" << testOffset[ofs]
                     << ", operand1Value=" << operand1Value[i]
                     << ", operand2Value=" << operand2Value[i]
                     << ", operand3Value=" << operand3Value[i]
                     << ", expected(min)=" << testMinExpectedValue[i]
                     << ", expected(max)=" << testMaxExpectedValue[i] << endl;
                double result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operand2Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_double(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operand3Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_double(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}
*/
// Test case 7 - MinMaxInt32Block
TEST(FamMinMaxAtomics, MinMaxInt32Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    int32_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffff,
                                (int32_t)0x87654321};
    int32_t operand2Value[5] = {(int32_t)0xf0000000, 0x1234, 0x7fffffff, 0x0,
                                (int32_t)0xffffffff};
    int32_t operand3Value[5] = {0x0, (int32_t)0xffff0000, 0x54321, 0x1,
                                0x7fffffff};
    int32_t testMinExpectedValue[5] = {(int32_t)0xf0000000, 0x1234, 0x54321,
                                       0x0, (int32_t)0x87654321};
    int32_t testMaxExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x1, 0x7fffffff};

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
                int32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int32(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                    item, testOffset[ofs], operand3Value[i]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int32(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 8 - MinMaxUInt32Block
TEST(FamMinMaxAtomics, MinMaxUInt32Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint32_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffff, 0x87654321};
    uint32_t operand2Value[5] = {0xf0000000, 0x1234, 0x7fffffff, 0x0,
                                 0xffffffff};
    uint32_t operand3Value[5] = {0x0, 0xffff0000, 0x54321, 0x1, 0x7fffffff};
    uint32_t testMinExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x0, 0x87654321};
    uint32_t testMaxExpectedValue[5] = {0x0, 0xffff0000, 0x54321, 0x1,
                                        0x87654321};

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
                uint32_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                    item, testOffset[ofs], operand3Value[i]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_uint32(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 9 - MinMaxInt64Block
TEST(FamMinMaxAtomics, MinMaxInt64Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    int64_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffffffffffff,
                                (int64_t)0xfedcba9876543210};
    int64_t operand2Value[5] = {(int64_t)0xf000000000000000, 0x1234,
                                0x7fffffffffffffff, 0x0,
                                (int64_t)0xffffffffffffffff};
    int64_t operand3Value[5] = {0x0, (int64_t)0xffffffff00000000, 0x54321, 0x1,
                                0x7fffffffffffffff};
    int64_t testMinExpectedValue[5] = {(int64_t)0xf000000000000000, 0x1234,
                                       0x54321, 0x0,
                                       (int64_t)0xfedcba9876543210};
    int64_t testMaxExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x1,
                                       0x7fffffffffffffff};

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
                int64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                    item, testOffset[ofs], operand3Value[i]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 10 - MinMaxUInt64Block
TEST(FamMinMaxAtomics, MinMaxUInt64Block) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    uint64_t operand1Value[5] = {0x0, 0x1234, 0x54321, 0x7fffffffffffffff,
                                 0xfedcba9876543210};
    uint64_t operand2Value[5] = {0xf000000000000000, 0x1234, 0x7fffffffffffffff,
                                 0x0, 0xffffffffffffffff};
    uint64_t operand3Value[5] = {0x0, 0xffffffff00000000, 0x54321, 0x1,
                                 0x7fffffffffffffff};
    uint64_t testMinExpectedValue[5] = {0x0, 0x1234, 0x54321, 0x0,
                                        0xfedcba9876543210};
    uint64_t testMaxExpectedValue[5] = {0x0, 0xffffffff00000000, 0x54321, 0x1,
                                        0xfedcba9876543210};

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
                uint64_t result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                    item, testOffset[ofs], operand3Value[i]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_int64(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 11 - MinMaxFloatBlock
TEST(FamMinMaxAtomics, MinMaxFloatBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    float operand1Value[5] = {0.0f, 1234.12f, 54321.87f, 8888.33f, 99999.99f};
    float operand2Value[5] = {0.1f, 1234.12f, 0.12f, 9999.22f, 0.01f};
    float operand3Value[5] = {0.5f, 1234.23f, 0.01f, 5432.10f, 0.05f};
    float testMinExpectedValue[5] = {0.0f, 1234.12f, 0.12f, 8888.33f, 0.01f};
    float testMaxExpectedValue[5] = {0.5f, 1234.23f, 0.12f, 8888.33f, 0.05f};

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
                float result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_float(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                    item, testOffset[ofs], operand3Value[i]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_float(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 12 - MinMaxDoubleBlock
TEST(FamMinMaxAtomics, MinMaxDoubleBlock) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm, ofs;

    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024, 4096, 8192};

    double operand1Value[5] = {0.0, 1234.123, 987654321.8765,
                               2222555577778888.3333, (DBL_MAX - 1.0)};
    double operand2Value[5] = {0.1, 1234.123, 0.1234, 1111.2222, 1.0};
    double operand3Value[5] = {1.5, 5432.123, 0.0123, 2222555577778888.3333,
                               DBL_MAX - 1};
    double testMinExpectedValue[5] = {0.0, 1234.123, 0.1234, 1111.2222, 1.0};
    double testMaxExpectedValue[5] = {1.5, 5432.123, 0.1234,
                                      2222555577778888.3333, DBL_MAX - 1};

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
                double result;
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operand1Value[i]));
                EXPECT_NO_THROW(my_fam->fam_quiet());
                EXPECT_NO_THROW(result = my_fam->fam_fetch_min(
                                    item, testOffset[ofs], operand2Value[i]));
                EXPECT_EQ(operand1Value[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_double(item, testOffset[ofs]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(result = my_fam->fam_fetch_max(
                                    item, testOffset[ofs], operand3Value[i]));
                EXPECT_EQ(testMinExpectedValue[i], result);
                EXPECT_NO_THROW(
                    result = my_fam->fam_fetch_double(item, testOffset[ofs]));
                EXPECT_EQ(testMaxExpectedValue[i], result);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 13 - Min Max Negative test case with invalid permissions
TEST(FamMinMaxAtomics, MinMaxNegativeBlockPerm) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int sm, ofs;

    mode_t test_perm_mode[4] = {0400, 0444, 0455, 0411};
    size_t test_item_size[4] = {1024, 4096, 8192, 16384};

    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};

    for (sm = 0; sm < 4; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(double) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt32[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt32[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandInt32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandInt32[1]),
                Fam_Exception);
            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);
            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt64[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt64[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandInt64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandInt64[1]),
                Fam_Exception);

            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);

            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandFloat[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandFloat[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandFloat[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandFloat[1]),
                Fam_Exception);

            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandDouble[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandDouble[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandDouble[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandDouble[1]),
                Fam_Exception);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}
#ifdef ENABLE_KNOWN_ISSUES

// Test case 14 - Min Max Negative test case with invalid permissions
TEST(FamMinMaxAtomics, MinMaxNegativeNonblockPerm) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int sm, ofs;

    mode_t test_perm_mode[4] = {0400, 0444, 0455, 0411};
    size_t test_item_size[4] = {1024, 4096, 8192, 16384};

    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};

    for (sm = 0; sm < 4; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {0, (test_item_size[sm] / 2),
                                  (test_item_size[sm] - sizeof(double) - 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt32[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandInt32[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandInt32[1]),
                    Fam_Exception);

                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandUint32[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandUint32[1]),
                    Fam_Exception);

                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt64[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandInt64[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandInt64[1]),
                    Fam_Exception);

                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandUint64[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandUint64[1]),
                    Fam_Exception);

                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandFloat[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandFloat[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandFloat[1]),
                    Fam_Exception);

                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandDouble[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandDouble[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandDouble[1]),
                    Fam_Exception);

            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt32[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandInt32[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandInt32[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandUint32[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandUint32[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt64[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandInt64[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandInt64[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandUint64[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandUint64[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandFloat[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandFloat[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandFloat[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandDouble[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandDouble[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandDouble[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 15 - Min Max Negative test case with invalid offset
TEST(FamMinMaxAtomics, MinMaxNegativeNonblockInvalidOffset) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int sm, ofs;

    mode_t test_perm_mode[4] = {0600, 0644, 0755, 0711};
    size_t test_item_size[4] = {1024, 4096, 8192, 16384};

    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};

    for (sm = 0; sm < 4; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {test_item_size[sm], (2 * test_item_size[sm]),
                                  (test_item_size[sm] + 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt32[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandInt32[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandInt32[1]),
                    Fam_Exception);

                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandUint32[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandUint32[1]),
                    Fam_Exception);

                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt64[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandInt64[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandInt64[1]),
                    Fam_Exception);

                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandUint64[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandUint64[1]),
                    Fam_Exception);

                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandFloat[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandFloat[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandFloat[1]),
                    Fam_Exception);

                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandDouble[0]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandDouble[1]),
                    Fam_Exception);
                EXPECT_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandDouble[1]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt32[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandInt32[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandInt32[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandUint32[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandUint32[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt64[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandInt64[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandInt64[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandUint64[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandUint64[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandFloat[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandFloat[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandFloat[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);

                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandDouble[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_min(item, testOffset[ofs], operandDouble[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
                EXPECT_NO_THROW(
                    my_fam->fam_max(item, testOffset[ofs], operandDouble[1]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}
#endif
// Test case 13 - Min Max Negative test case with invalid offset
TEST(FamMinMaxAtomics, MinMaxNegativeBlockInvalidOffset) {
    Fam_Descriptor *item;
    const char *dataItem = get_uniq_str("first", my_fam);
    int sm, ofs;

    mode_t test_perm_mode[4] = {0600, 0644, 0755, 0711};
    size_t test_item_size[4] = {1024, 4096, 8192, 16384};

    int32_t operandInt32[2] = {0x1234, (int32_t)0xffffffff};
    uint32_t operandUint32[2] = {0x1234, 0xffffffff};
    int64_t operandInt64[2] = {0x12345678, (int32_t)0xffffffffffffffff};
    uint64_t operandUint64[2] = {0x12345678, 0xffffffffffffffff};
    float operandFloat[2] = {0.1f, 1234.56f};
    double operandDouble[2] = {123456.78, 999999.99};

    for (sm = 0; sm < 4; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item = my_fam->fam_allocate(dataItem, test_item_size[sm],
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item);

        uint64_t testOffset[3] = {test_item_size[sm], (2 * test_item_size[sm]),
                                  (test_item_size[sm] + 1)};

        for (ofs = 0; ofs < 3; ofs++) {
            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt32[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt32[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandInt32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandInt32[1]),
                Fam_Exception);

            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint32[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint32[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandUint32[1]),
                Fam_Exception);

            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt64[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandInt64[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandInt64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandInt64[1]),
                Fam_Exception);

            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint64[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandUint64[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandUint64[1]),
                Fam_Exception);

            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandFloat[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandFloat[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandFloat[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandFloat[1]),
                Fam_Exception);

            if (SHM_CHECK) {
                EXPECT_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandDouble[0]),
                    Fam_Exception);
            } else {
                EXPECT_NO_THROW(
                    my_fam->fam_set(item, testOffset[ofs], operandDouble[0]));
                EXPECT_THROW(my_fam->fam_quiet(), Fam_Exception);
            }
            EXPECT_THROW(
                my_fam->fam_fetch_min(item, testOffset[ofs], operandDouble[1]),
                Fam_Exception);
            EXPECT_THROW(
                my_fam->fam_fetch_max(item, testOffset[ofs], operandDouble[1]),
                Fam_Exception);
        }

        EXPECT_NO_THROW(my_fam->fam_deallocate(item));
        delete item;
    }
    free((void *)dataItem);
}

// Test case 16 - Min Max Negative test case with invalid descriptor
TEST(FamMinMaxAtomics, MinMaxNegativeInvalidDescriptor) {
    Fam_Descriptor *item = NULL;

    EXPECT_THROW(my_fam->fam_min(item, 0, uint32_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, 0, uint32_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_min(item, 0, uint64_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, 0, uint64_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_min(item, 0, int32_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, 0, int32_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_min(item, 0, int64_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, 0, int64_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_min(item, 0, float(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, 0, float(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_min(item, 0, double(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, 0, double(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_min(item, 0, double(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_max(item, 0, double(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_min(item, 0, uint32_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_max(item, 0, uint32_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_min(item, 0, uint64_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_max(item, 0, uint64_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_min(item, 0, int32_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_max(item, 0, int32_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_min(item, 0, int64_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_max(item, 0, int64_t(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_min(item, 0, float(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_max(item, 0, float(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_min(item, 0, double(0)), Fam_Exception);
    EXPECT_THROW(my_fam->fam_fetch_max(item, 0, double(0)), Fam_Exception);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();

    init_fam_options(&fam_opts);

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    testRegionStr = get_uniq_str("test", my_fam);

    openFamModel = (char *)my_fam->fam_get_option(strdup("OPENFAM_MODEL"));

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
