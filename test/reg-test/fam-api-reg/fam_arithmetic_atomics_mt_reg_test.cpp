/*
 * fam_arithmetic_atomics_mt_reg_test.cpp
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
/* Test Case Description: Tests fam_add/fam_sub and certain fam_atomic operations for multithreaded model.
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

typedef struct {
    Fam_Descriptor *item;
    uint64_t offset;
    int32_t tid;
    int32_t deltaValue;
} ValueInfo;

#define NUM_THREADS 10
#define REGION_SIZE (8 * 1024 * 1024 * NUM_THREADS)
#define REGION_PERM 0777

void *thr_add_sub_int32_nonblocking(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(int32_t);

    //int tid =  addInfo->tid;
    uint64_t sm = (addInfo->offset);
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    int i, ofs;
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(int32_t) - 1)};
    int32_t baseValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    int32_t testAddValue[5] = {0x1, 0x1234, 0x54321, 0x1, 0x1};
    int32_t testExpectedValue[5] = {0x1, 0x2468, 0xA8642, 0x7fffffff, INT_MIN};
//    //cout << "THRD " << addInfo->tid << ", offset is " << offset <<  " testOffset: " << testOffset[0] << "," << testOffset[1] << "," << testOffset[2] << " for data item " << sm << endl;
    EXPECT_NE((void *)NULL, item);
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
    pthread_exit(NULL);
}

// Test case 1 - AddSubInt32NonBlock
TEST(FamArithmaticAtomics, AddSubInt32NonBlock) {
    int rc;
    Fam_Descriptor *item[3];
    pthread_t thr[NUM_THREADS];

    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 1; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(int32_t) ,
                                        test_perm_mode[sm], testRegionDesc));

        EXPECT_NE((void *)NULL, item[sm]);
        for (i = 0; i < NUM_THREADS; ++i) {
        tid = i;
        info[i] = {item[sm], (uint64_t)sm,tid, 0};
            if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_int32_nonblocking, &info[i]))) {
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
}


void *thr_add_sub_uint32_nonblocking(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(uint32_t);

    uint64_t sm = (addInfo->offset);

    int i, ofs;

    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(uint32_t))};
                                  //offset + (test_item_size[sm] - sizeof(uint32_t) - 1)};

    uint32_t baseValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    uint32_t testAddValue[5] = {0x1, 0x1234, 0x54321, 0x1, 0x1};
    uint32_t testExpectedValue[5] = {0x1, 0x2468, 0xA8642, 0x7fffffff,
                                     0x80000000};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                cout << "Testing fam_add: for ofs " << ofs << " and i: " << " sm: " << sm << i << "  item=" << item
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
    pthread_exit(NULL);
}

// Test case 2 - AddSubUInt32NonBlock
TEST(FamArithmaticAtomics, AddSubUInt32NonBlock) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item[3];
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(uint32_t),
                                        test_perm_mode[sm], testRegionDesc));
        EXPECT_NE((void *)NULL, item[sm]);
        for (i = 0; i < NUM_THREADS; ++i) {
	tid = i ;
	info[i] = {item[sm], (uint64_t)sm,tid, 0};
        if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_uint32_nonblocking, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
    //        return -1;
        }
        }

	for (i = 0; i < NUM_THREADS; ++i) {
		pthread_join(thr[i], NULL);
	}
        EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }
}


void *thr_add_sub_int64_nonblocking(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    //int tid =  addInfo->tid;
    uint64_t sm = (addInfo->offset);
    uint64_t offset = addInfo->tid * sizeof(int64_t);

    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(int64_t) - 1)};
    int64_t baseValue[5] = {0x0, 0x1234, 0x1111222233334321, 0x7ffffffffffffffe,
                            0x7fffffffffffffff};
    int64_t testAddValue[5] = {0x1, 0x1234, 0x1111222233334321, 0x1, 0x1};
    int64_t testExpectedValue[5] = {0x1, 0x2468, 0x2222444466668642,
                                    0x7fffffffffffffff, LONG_MIN};

    int i, ofs;
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

    pthread_exit(NULL);
}
// Test case 3 - AddSubInt64NonBlock
TEST(FamArithmaticAtomics, AddSubInt64NonBlock) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item[3];
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(int64_t),
                                        test_perm_mode[sm], testRegionDesc));

        EXPECT_NE((void *)NULL, item[sm]);

        for (i = 0; i < NUM_THREADS; ++i) {
	    tid = i;
            info[i] = {item[sm], (uint64_t)sm,tid, 0};
            if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_int64_nonblocking, &info[i]))) {
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
}



void *thr_add_sub_uint64_nonblocking(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    //int tid =  addInfo->tid;
    uint64_t offset = addInfo->tid * sizeof(uint64_t);

    uint64_t sm = (addInfo->offset);
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(uint64_t) - 1)};

    int i, ofs;

    uint64_t baseValue[5] = {0x0, 0x1234, 0x1111222233334321,
                             0x7ffffffffffffffe, 0x7fffffffffffffff};
    uint64_t testAddValue[5] = {0x1, 0x1234, 0x1111222233334321, 0x1, 0x1};
    uint64_t testExpectedValue[5] = {0x1, 0x2468, 0x2222444466668642,
                                     0x7fffffffffffffff, 0x8000000000000000};

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

    pthread_exit(NULL);
}

// Test case 4 - AddSubUInt64NonBlock
TEST(FamArithmaticAtomics, AddSubUInt64NonBlock) {

    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item[3];
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(uint64_t),
                                        test_perm_mode[sm], testRegionDesc));

        EXPECT_NE((void *)NULL, item[sm]);
        for (i = 0; i < NUM_THREADS; ++i) {
        tid = i;
        info[i] = {item[sm], (uint64_t)sm,tid, 0};
            if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_uint64_nonblocking, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
                pthread_join(thr[i], NULL);
         }
        EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }

}


void *thr_add_sub_float_nonblocking(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t sm = (addInfo->offset);
    uint64_t offset = addInfo->tid * sizeof(float);

    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(float) - 1)};
    int i, ofs;



    float baseValue[5] = {0.0f, 1234.12f, 54321.87f, 8888.33f, 99999.99f};
    float testAddValue[5] = {0.1f, 1234.12f, 0.12f, 1111.22f, 0.01f};
    float testExpectedValue[5] = {0.1f, 2468.24f, 54321.99f, 9999.55f,
                                  100000.00f};


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

    pthread_exit(NULL);
}
// Test case 5 - AddSubFloatNonBlock
TEST(FamArithmaticAtomics, AddSubFloatNonBlock) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item[3];
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(float),
                                        test_perm_mode[sm], testRegionDesc));

        EXPECT_NE((void *)NULL, item[sm]);

        for (i = 0; i < NUM_THREADS; ++i) {
	        tid = i;
        	info[i] = {item[sm], (uint64_t)sm,tid, 0};		
        	if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_float_nonblocking, &info[i]))) {
	            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        	    exit(1);
        	}
	}

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }
	EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }  
}


void *thr_add_sub_double_nonblocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(double);
    uint64_t sm = (addInfo->offset);
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(double) - 1)};
    int i, ofs;

    double baseValue[5] = {0.0, 1234.123, 987654321.8765, 2222555577778888.3333,
                           (DBL_MAX - 1.0)};
    double testAddValue[5] = {0.1, 1234.123, 0.1234, 1111.2222, 1.0};
    double testExpectedValue[5] = {0.1, 2468.246, 987654321.9999,
                                   2222555577779999.5555, DBL_MAX};

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

    pthread_exit(NULL);
}
// Test case 6 - AddSubDoubleNonBlock
TEST(FamArithmaticAtomics, AddSubDoubleNonBlock) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item[3];
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(double),
                                        test_perm_mode[sm], testRegionDesc));

        EXPECT_NE((void *)NULL, item[sm]);
        for (i = 0; i < NUM_THREADS; ++i) {
        tid = i;
        info[i] = {item[sm], (uint64_t)sm,tid, 0};
            if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_double_nonblocking, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
            }
        }
	
        for (i = 0; i < NUM_THREADS; ++i) 
		pthread_join(thr[i], NULL);
	EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }
}

// Test case 7 - AddSubInt32Blocking
void *thr_add_sub_int32_blocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(int32_t);
    uint64_t sm = (addInfo->offset);
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(double) - 1)};

    int i, ofs;


    int32_t baseValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    int32_t testAddValue[5] = {0x1, 0x1234, 0x54321, 0x1, 0x1};
    int32_t testExpectedValue[5] = {0x1, 0x2468, 0xA8642, 0x7fffffff, INT_MIN};

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
    
    pthread_exit(NULL);
}

TEST(FamArithmaticAtomics, AddSubInt32Blocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item[3];
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(int32_t),
                                        test_perm_mode[sm], testRegionDesc));

        EXPECT_NE((void *)NULL, item[sm]);
        for (i = 0; i < NUM_THREADS; ++i) {
		tid = i;
		info[i] = {item[sm], (uint64_t)sm,tid, 0};
		if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_int32_blocking, &info[i]))) {
		    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
		    exit(1);
	    //        return -1;
		}
    	}

	    for (i = 0; i < NUM_THREADS; ++i) {
		pthread_join(thr[i], NULL);
	    }
        EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
	}
}


void *thr_add_sub_uint32_blocking(void *arg) {

    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t sm = (addInfo->offset);
    uint64_t offset = addInfo->tid * sizeof(uint32_t);
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(uint32_t) - 1)};
    int i, ofs;

    uint32_t baseValue[5] = {0x0, 0x1234, 0x54321, 0x7ffffffe, 0x7fffffff};
    uint32_t testAddValue[5] = {0x1, 0x1234, 0x54321, 0x1, 0x1};
    uint32_t testExpectedValue[5] = {0x1, 0x2468, 0xA8642, 0x7fffffff,
                                     0x80000000};

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

    pthread_exit(NULL);
}

// Test case 8 - AddSubUInt32Blocking
TEST(FamArithmaticAtomics, AddSubUInt32Blocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item[3];
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(uint32_t),
                                        test_perm_mode[sm], testRegionDesc));

        EXPECT_NE((void *)NULL, item[sm]);
        for (i = 0; i < NUM_THREADS; ++i) {
		tid = i;
		info[i] = {item[sm], (uint64_t)sm,tid, 0};
		if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_uint32_blocking, &info[i]))) {
		    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
		    exit(1);
		}
        }

	for (i = 0; i < NUM_THREADS; ++i) {
		pthread_join(thr[i], NULL);
	}
	EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }
}
void *thr_add_sub_int64_blocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(int64_t);
    uint64_t sm = (addInfo->offset);
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(int64_t) - 1)};
    int i, ofs;

    int64_t baseValue[5] = {0x0, 0x1234, 0x1111222233334321, 0x7ffffffffffffffe,
                            0x7fffffffffffffff};
    int64_t testAddValue[5] = {0x1, 0x1234, 0x1111222233334321, 0x1, 0x1};
    int64_t testExpectedValue[5] = {0x1, 0x2468, 0x2222444466668642,
                                    0x7fffffffffffffff, LONG_MIN};


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

    pthread_exit(NULL);
}
// Test case 9 - AddSubInt64Blocking
TEST(FamArithmaticAtomics, AddSubInt64Blocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item[3];
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(int64_t),
                                        test_perm_mode[sm], testRegionDesc));

        EXPECT_NE((void *)NULL, item[sm]);
        for (i = 0; i < NUM_THREADS; ++i) {
		tid = i;
		info[i] = {item[sm], (uint64_t)sm,tid, 0};
		if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_int64_blocking, &info[i]))) {
		    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
		    exit(1);
		}
        }

        for (i = 0; i < NUM_THREADS; ++i) {
            pthread_join(thr[i], NULL);
        }
        EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }
}

void *thr_add_sub_uint64_blocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(uint64_t);
    uint64_t sm = (addInfo->offset);
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(uint64_t) - 1)};

    int i, ofs;

    uint64_t baseValue[5] = {0x0, 0x1234, 0x1111222233334321,
                             0x7ffffffffffffffe, 0x7fffffffffffffff};
    uint64_t testAddValue[5] = {0x1, 0x1234, 0x1111222233334321, 0x1, 0x1};
    uint64_t testExpectedValue[5] = {0x1, 0x2468, 0x2222444466668642,
                                     0x7fffffffffffffff, 0x8000000000000000};

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

    pthread_exit(NULL);
}
// Test case 10 - AddSubUInt64Blocking
TEST(FamArithmaticAtomics, AddSubUInt64Blocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item[3];
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(uint64_t),
                                        test_perm_mode[sm], testRegionDesc));

        EXPECT_NE((void *)NULL, item[sm]);
        for (i = 0; i < NUM_THREADS; ++i) {
		tid = i;
		info[i] = {item[sm], (uint64_t)sm,tid, 0};
		if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_uint64_blocking, &info[i]))) {
		    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
		    exit(1);
	    //        return -1;
		}
	    }

	    for (i = 0; i < NUM_THREADS; ++i) {
		pthread_join(thr[i], NULL);
	    }
        EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
	}
}


void *thr_add_sub_float_blocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t sm = (addInfo->offset);
    uint64_t offset = addInfo->tid * sizeof(float);
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(float) - 1)};

    int i, ofs;
    float baseValue[5] = {0.0f, 1234.12f, 54321.87f, 8888.33f, 99999.99f};
    float testAddValue[5] = {0.1f, 1234.12f, 0.12f, 1111.22f, 0.01f};
    float testExpectedValue[5] = {0.1f, 2468.24f, 54321.99f, 9999.55f,
                                  100000.00f};


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
    pthread_exit(NULL);
}
// Test case 11 - AddSubFloatBlocking
TEST(FamArithmaticAtomics, AddSubFloatBlocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item[3];
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
		// Allocating data items in the created region
		EXPECT_NO_THROW(
		    item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(float),
						test_perm_mode[sm], testRegionDesc));

		EXPECT_NE((void *)NULL, item[sm]);
		for (i = 0; i < NUM_THREADS; ++i) {
		tid = i;
		info[i] = {item[sm], (uint64_t)sm,tid, 0};
		if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_float_blocking,&info[i] ))) {
		    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
		    exit(1);
		}
	    }

	    for (i = 0; i < NUM_THREADS; ++i) {
		pthread_join(thr[i], NULL);
	    }
        EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }
}

void *thr_add_sub_double_blocking(void *arg) {
    ValueInfo *addInfo = (ValueInfo *)arg;
    Fam_Descriptor *item = addInfo->item;
    uint64_t offset = addInfo->tid * sizeof(double);
    int tid =  addInfo->tid;
    uint64_t sm = (addInfo->offset);
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    uint64_t testOffset[3] = {offset + 0, offset + (test_item_size[sm] / 2),
                                  offset + (test_item_size[sm] - sizeof(double) - 1)};

    int i, ofs;

    double baseValue[5] = {0.0, 1234.123, 987654321.8765, 2222555577778888.3333,
                           (DBL_MAX - 1.0)};
    double testAddValue[5] = {0.1, 1234.123, 0.1234, 1111.2222, 1.0};
    double testExpectedValue[5] = {0.1, 2468.246, 987654321.9999,
                                   2222555577779999.5555, DBL_MAX};

        for (ofs = 0; ofs < 3; ofs++) {
            for (i = 0; i < 5; i++) {
                // run_add_test<int32_t>(item, testOffset[ofs], baseValue[i],
                // testAddValue[i], testExpectedValue[i], INT32);
                cout << "Testing fam_add for thread " << tid << ": item=" << item
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

    pthread_exit(NULL);

}
// Test case 12 - AddSubDoubleBlocking
TEST(FamArithmaticAtomics, AddSubDoubleBlocking) {
    int rc;
    pthread_t thr[NUM_THREADS];
    Fam_Descriptor *item[3];
    const char *dataItem = get_uniq_str("first", my_fam);
    int i, sm ,tid;
    mode_t test_perm_mode[3] = {0777, 0644, 0600};
    size_t test_item_size[3] = {1024 * NUM_THREADS, 4096 * NUM_THREADS, 8192 * NUM_THREADS};
    ValueInfo *info;
    info = (ValueInfo *)malloc(sizeof(ValueInfo) * NUM_THREADS);

    for (sm = 0; sm < 3; sm++) {
        // Allocating data items in the created region
        EXPECT_NO_THROW(
            item[sm] = my_fam->fam_allocate(dataItem, test_item_size[sm] * sizeof(double),
                                        test_perm_mode[sm], testRegionDesc));

        EXPECT_NE((void *)NULL, item[sm]);
        for (i = 0; i < NUM_THREADS; ++i) {
	tid = i;
        info[i] = {item[sm], (uint64_t)sm,tid, 0};
            if ((rc = pthread_create(&thr[i], NULL, thr_add_sub_double_blocking, &info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
            }
        }

        for (i = 0; i < NUM_THREADS; ++i) {
        	pthread_join(thr[i], NULL);
         }
	EXPECT_NO_THROW(my_fam->fam_deallocate(item[sm]));
    }

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
