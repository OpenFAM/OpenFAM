/*
 * test/microbench/fam-api-mb/fam_region_spanning.cpp
 * Copyright (c) 2021 Hewlett Packard Enterprise Development, LP. All rights
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
#include <chrono>

#include <fam/fam.h>

#include "cis/fam_cis_client.h"
#include "common/fam_test_config.h"
#include "common/fam_libfabric.h"

#define BIG_REGION_SIZE (21474836480 * 4)
using namespace std::chrono;

using namespace std;
using namespace openfam;

uint64_t gDataSize = 1048576;
fam *my_fam;
Fam_Options fam_opts;
Fam_Descriptor **itemLocal;
Fam_Region_Descriptor *descLocal;

int *myPE;
int NUM_DATAITEMS = 1;
int NUM_IO_ITERATIONS = 1;
int nodesperPE = 1;
int msrvcnt = 1;
mode_t test_perm_mode;
size_t test_item_size = 1024;
int64_t operand1Value = 0x1fffffffffffffff;
int64_t operand2Value = 0x1fffffffffffffff;
uint64_t operand1UValue = 0x1fffffffffffffff;
uint64_t operand2UValue = 0x1fffffffffffffff;


#define warmup_function(funcname, ...) { \
    int64_t *local = (int64_t *)malloc(gDataSize); \
    for (int i = 0; i < NUM_DATAITEMS; i++) { \
                my_fam->funcname(__VA_ARGS__); \
    } \
    my_fam->fam_barrier_all(); \
    fabric_reset_profile(); \
    free(local); \
}

// With microbenchmark tests, please use fam_reset_profile, if any warmup calls are being made.
// fam_reset_profile is a method defined in fam class in fam.cpp, which is used to ensure the warmup calls are not considered
// while collecting profiling data.
// In addition, to be able to use fam_reset_profile, we need to add this method in include/fam.h.
// Add the below mentioned function to warmup_function.
// my_fam->fam_reset_profile();

const int MAX = 26;
const int DATAITEM_NAME_LEN = 20;

// Returns a string of random alphabets of
// length n.
string getRandomString() {
    char alphabet[MAX] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
                          'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
                          's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

    string res = "";
    for (int i = 0; i < DATAITEM_NAME_LEN; i++)
        res = res + alphabet[rand() % MAX];

    return res;
}

// Following set of functions are used to generate dataitems such that
// // PEs create data items in the same switch as that of memory server on specific cluster.
int* get_memsrv(int peId, int totalPEs, int *numsrv) {
    int *memsrv = (int *)calloc(1, sizeof(int) * 8);
    *numsrv = 0;

    int switch_cnt = 8;
    if (nodesperPE == 1) {
    switch(msrvcnt) {
    case 1: 	 memsrv[0] = 0;
		 *numsrv = 1;
		break;
    case 2 :        memsrv[0] = peId/switch_cnt;
                    *numsrv = msrvcnt/2;
                    break;
    case 4 :
    case 8 :
    case 16:
                    *numsrv = msrvcnt/2;

                    if ( peId < switch_cnt){
                            for (int i = 0; i < msrvcnt/2; i++)
                                memsrv[i] = i;
                    }
                    else {
                            for (int j=0, i = msrvcnt/2; i<msrvcnt; j++, i++)
                                memsrv[j] = i;
                    }
                    break;

    }
    }

    return memsrv;
}
string getDataitemNameForMemsrv(int memsrv) {
    size_t hashVal;
    string dataitemName;
    size_t val = memsrv ;
    do {
        dataitemName = getRandomString() ;
        hashVal = hash<string>{}(dataitemName) % (msrvcnt);
    } while (hashVal != (size_t)val );
    return dataitemName;
}

string getDataitemNameForMemsrvList(int *memsrv, int mcnt) {
    size_t hashVal;
    string dataitemName;
    int found = 0;
    do {
        dataitemName = getRandomString() ;
        hashVal = hash<string>{}(dataitemName) % (msrvcnt);
        for (int i = 0; i< mcnt; i++)
                if (hashVal == (size_t)memsrv[i])
                    {found = 1; break;}
    } while (!found);
    return dataitemName;
}

string getDataitemName(int peId, int totalPEs) {
    size_t hashVal;
    size_t val = 0;
    string dataitemName;
    int switch_cnt = 8;
    switch(msrvcnt) {
        case 1:         val = peId;
                        break;
        case 2 :        val = peId/switch_cnt;
                        break;
        case 4 :
                        if ( peId < switch_cnt)
                                val =  peId % 2;
                        else
                                val = msrvcnt / 2 + peId % 2;
                        break;
        case 8 :
                        if ( peId < switch_cnt)
                                val = peId/switch_cnt + peId % 4;
                        else
                                val = msrvcnt / 2 + peId % 4;
                        break;
        case 16:
                        val = peId;
                        break;

    } 

    do {
        dataitemName = getRandomString() ;
        hashVal = hash<string>{}(dataitemName) % totalPEs;
    } while (hashVal != (size_t)val);
    return dataitemName;
}
// Test case -  All Fetch atomics (Int64)
TEST(FamArithmaticAtomicmicrobench, FetchInt64) {
    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

	        EXPECT_NO_THROW(my_fam->fam_fetch_int64(itemLocal[i], testOffset));
	}
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchAddInt64) {
    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
        	EXPECT_NO_THROW(my_fam->fam_fetch_add(itemLocal[i], testOffset, operand2Value));
    	}
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchSubInt64) {
    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        EXPECT_NO_THROW(
            my_fam->fam_fetch_subtract(itemLocal[i], testOffset, operand2Value));
    	}
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchAndInt64) {
    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for ( i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        EXPECT_NO_THROW(
            my_fam->fam_fetch_and(itemLocal[i], testOffset, operand2UValue));
    	}
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchOrInt64) {
    int i = 0;

    uint64_t testOffset = 0;


    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        EXPECT_NO_THROW(my_fam->fam_fetch_or(itemLocal[i], testOffset, operand2UValue));
    	}
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchXorInt64) {
    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        EXPECT_NO_THROW(
            my_fam->fam_fetch_xor(itemLocal[i], testOffset, operand2UValue));
    	}
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchMinInt64) {
    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for ( i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        EXPECT_NO_THROW(my_fam->fam_fetch_min(itemLocal[i], testOffset, operand2Value));
    	}
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchMaxInt64) {
    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for ( i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        EXPECT_NO_THROW(my_fam->fam_fetch_max(itemLocal[i], testOffset, operand2Value));
    	}
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchCmpswapInt64) {
    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        EXPECT_NO_THROW(my_fam->fam_compare_swap(itemLocal[i], testOffset,
                                                 operand1Value, operand2Value));
    	}
    }
}

TEST(FamArithmaticAtomicmicrobench, FetchSwapInt64) {
    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        EXPECT_NO_THROW(my_fam->fam_swap(itemLocal[i], testOffset, operand2Value));
    }
    }
}

// Test case -  fam_set for Int64
TEST(FamArithmaticAtomicmicrobench, SetInt64) {
    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
        EXPECT_NO_THROW(my_fam->fam_set(itemLocal[i], testOffset, operand2Value));
    	}
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case -  NonFetch add for Int64
TEST(FamArithmaticAtomicmicrobench, NonFetchAddInt64) {

    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        EXPECT_NO_THROW(my_fam->fam_add(itemLocal[i], testOffset, operand2Value));
    	}
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case -  NonFetch subtract for Int64
TEST(FamArithmaticAtomicmicrobench, NonFetchSubInt64) {

    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for ( i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        EXPECT_NO_THROW(my_fam->fam_subtract(itemLocal[i], testOffset, operand2Value));
    	}
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case - NonFetch and for UInt64
TEST(FamLogicalAtomics, NonFetchAndUInt64) {

    int i = 0;

    uint64_t testOffset = 0;

    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for ( i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {


        EXPECT_NO_THROW(my_fam->fam_and(itemLocal[i], testOffset, operand2UValue));
    	}
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case - NonFetch or for UInt64
TEST(FamLogicalAtomics, NonFetchOrUInt64) {

    int i = 0;

    uint64_t testOffset = 0;
    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        EXPECT_NO_THROW(my_fam->fam_or(itemLocal[i], testOffset, operand2UValue));
    	}
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case - NonFetch xor for UInt64
TEST(FamLogicalAtomics, NonFetchXorUInt64) {

    int i = 0;

    uint64_t testOffset = 0;
    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for ( i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

       		EXPECT_NO_THROW(my_fam->fam_xor(itemLocal[i], testOffset, operand2UValue));
    	}
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case -  NonFetch min for Int64
TEST(FamMinMaxAtomicMicrobench, NonFetchMinInt64) {

    int i = 0;

    uint64_t testOffset = 0;
    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
        	EXPECT_NO_THROW(my_fam->fam_min(itemLocal[i], testOffset, operand2Value));
    	}
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}

// Test case -  NonFetch max for Int64
TEST(FamMinMaxAtomicMicrobench, NonFetchMaxInt64) {

    int i = 0;
    uint64_t testOffset = 0;
    warmup_function(fam_fetch_int32,itemLocal[i], testOffset);

    for (i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

        	EXPECT_NO_THROW(my_fam->fam_max(itemLocal[i], testOffset, operand2Value));
	}
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
}



int main(int argc, char **argv) {
    int ret;
    std::string config_type = "specific";
    ::testing::InitGoogleTest(&argc, argv);

    if (argc >= 2) {
        config_type = argv[1];
        if ( ( config_type.compare("-h") == 0 ) || ( config_type.compare("--help") == 0) ) {
                cout << "config_type: even/specific/random depending on how we want the data item distribution to happen" << endl;
                cout << "num_dataitems: number of data items to be allocated by PE" << endl;
                cout << "num_io_iters: number of I/O iterations to be done on each data item" << endl;
                cout << "data_size: Data Item size" << endl;
                cout << "num_msrv: Number of memory servers " << endl;
                cout << "nodesperPE: Number of nodes per PE" << endl;

                exit(-1);
        }
    }

    if (argc >= 3) {
        NUM_DATAITEMS = atoi(argv[2]);
    }

    if (argc >= 4) {
        NUM_IO_ITERATIONS = atoi(argv[3]);
    }

    if (argc >= 5) {
        gDataSize = atoi(argv[4]);
    }

    if (argc >= 6) {
        msrvcnt = atoi(argv[5]);
    }
    if (argc >= 7) {
        nodesperPE = atoi(argv[6]);
    }


    my_fam = new fam();

    init_fam_options(&fam_opts);
    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));
    itemLocal = new Fam_Descriptor *[NUM_DATAITEMS];
    Fam_Region_Descriptor *descLocal;
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);

    EXPECT_NO_THROW(myPE = (int *)my_fam->fam_get_option(strdup("PE_ID")));

    EXPECT_NE((void *)NULL, myPE);
    int *numPEs;
    EXPECT_NO_THROW(numPEs = (int *)my_fam->fam_get_option(strdup("PE_COUNT")));
    EXPECT_NE((void *)NULL, numPEs);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    if (*myPE == 0) {
        for (int i = 1; i < argc; ++i) {
            printf("arg %2d = %s\n", i, (argv[i]));
        }
	uint64_t regionSize = gDataSize * NUM_DATAITEMS * *numPEs;
	regionSize = regionSize < BIG_REGION_SIZE ? BIG_REGION_SIZE : regionSize;
	EXPECT_NO_THROW(descLocal = 
		  my_fam->fam_create_region("test0", regionSize , 0777, NULL));
        EXPECT_NE((void *)NULL, descLocal);
    }
   
    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    if (*myPE != 0) {
        EXPECT_NO_THROW(descLocal = my_fam->fam_lookup_region("test0"));
        EXPECT_NE((void *)NULL, descLocal);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    int mcnt = 0;

    int *memsrv =  get_memsrv(*myPE, *numPEs, &mcnt);

    int number_of_dataitems_per_pe = NUM_DATAITEMS;
    if ( config_type.compare("even") == 0 ) {
	    int mid = *myPE % mcnt;
	    for (int i = 0; i < number_of_dataitems_per_pe; i++) {
		 if (mid == mcnt)
			mid = 0;
		std::string name = getDataitemNameForMemsrv(memsrv[mid++]);
		const char *itemInfo = strdup(name.c_str());

		// Allocating data items

	        EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_allocate(
				    itemInfo, gDataSize * 1, 0777, descLocal));
		EXPECT_NE((void *)NULL, itemLocal[i]);
                // cout << "PE" <<  *myPE << "," << itemInfo << "," <<
                // itemLocal[i]->get_memserver_id() << endl;
            }

    } else if ( config_type.compare("specific") == 0 ) {
	    for (int i = 0; i < number_of_dataitems_per_pe; i++) {

		std::string name;
			name = getDataitemName(*myPE,*numPEs);
		const char *itemInfo = strdup(name.c_str());
		// Allocating data items
		EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_allocate(
				    itemInfo, gDataSize * 1 , 0777, descLocal));
		EXPECT_NE((void *)NULL, itemLocal[i]);
                // cout << "PE" <<  *myPE << "," << itemInfo << "," <<
                // itemLocal[i]->get_memserver_id() << endl;
            }

    } else {


	    for (int i = 0; i < number_of_dataitems_per_pe; i++) {
		std::string name = getDataitemNameForMemsrvList(memsrv,mcnt);
		const char *itemInfo = strdup(name.c_str());
		// Allocating data items
		EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_allocate(
				    itemInfo, gDataSize * 1, 0777, descLocal));
		EXPECT_NE((void *)NULL, itemLocal[i]);
                // cout << "PE" <<  *myPE << "," << itemInfo << "," <<
                // itemLocal[i]->get_memserver_id() << endl;
            }
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    ret = RUN_ALL_TESTS();
    // Deallocate dataitems
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    for (int i = 0; i < number_of_dataitems_per_pe; i++) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(itemLocal[i]));
    }

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    if ( *myPE == 0 ) {
            EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal));
     }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    free((void *)testRegionLocal);
    free((void *)firstItemLocal);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    delete my_fam;
    return ret;
}


