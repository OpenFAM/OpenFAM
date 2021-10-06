/*Modifications copyright (C) 2021 Advanced Micro Devices, Inc. All rights
 * reserved.*/

#ifndef __GUPS_COMMON_H__
#define __GUPS_COMMON_H__

#include "common/fam_options.h"
#include "common/fam_test_config.h"
#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#define REGION_NAME "GUPS"

using namespace std;
using namespace openfam;

fam *my_fam;
int myPE, numPEs;
#define fam_stream cout << "PE(" << myPE << "/" << numPEs << ") "

Fam_Region_Descriptor *GUPS_fam_initialize(void) {
    Fam_Region_Descriptor *region = NULL;
    Fam_Options fam_opts;
    int *val;
    // FAM initialize
    my_fam = new fam();
    //init_fam_options(&fam_opts);
    memset((void *)&fam_opts, 0, sizeof(Fam_Options));
    //fam_opts.allocator = strdup("NVMM");
    fam_opts.openFamModel = strdup("shared_memory");
    fam_opts.runtime = strdup("NONE");

    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        fam_stream << "fam initialization failed 1:" << e.fam_error_msg() << endl;
        exit(1);
    }

    val = (int *)my_fam->fam_get_option(strdup("PE_ID"));
    myPE = *val;
    val = (int *)my_fam->fam_get_option(strdup("PE_COUNT"));
    numPEs = *val;

    if (myPE == -1) {
        fam_stream << "Total PEs : " << numPEs << endl;
        fam_stream << "My PE : " << myPE << endl;
    }

    try {
        region = my_fam->fam_lookup_region(REGION_NAME);
    } catch (...) {
        fam_stream << "fam lookup failed for region :" << REGION_NAME << endl;
        exit(1);
    }
    return region;
}

Fam_Region_Descriptor *GUPS_fam_initialize(size_t size) {
    Fam_Region_Descriptor *region = NULL;
    Fam_Options fam_opts;
    // FAM initialize
    my_fam = new fam();
    memset((void *)&fam_opts, 0, sizeof(Fam_Options));
    fam_opts.runtime = strdup("NONE");

    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed 2:" << e.fam_error_msg() << endl;
        exit(1);
    }

    //fam_opts.allocator = strdup("NVMM");
    fam_opts.openFamModel = strdup("shared_memory");

    int *val;
    val = (int *)my_fam->fam_get_option(strdup("PE_ID"));
    myPE = *val;
    val = (int *)my_fam->fam_get_option(strdup("PE_COUNT"));
    numPEs = *val;

    try {
        region = my_fam->fam_lookup_region(REGION_NAME);
        if (region != NULL) {
            try {
                my_fam->fam_destroy_region(region);
            } catch (Fam_Exception &e) {
                cout << "Error msg: yes " << e.fam_error_msg() << endl;
                exit(1);
            }
        }
    } catch (...) {
    }

    try {
        region = my_fam->fam_create_region(REGION_NAME, size, 0777, NONE);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed 3:" << e.fam_error_msg() << endl;
        exit(1);
    }
    return region;
}

Fam_Descriptor *gups_get_dataitem(const char *dataitemName) {
    Fam_Descriptor *dataitem = NULL;
    // Lookup  dataitem
    try {
        dataitem = my_fam->fam_lookup(dataitemName, REGION_NAME);
    } catch (...) {
        // ignore
        fam_stream << "fam lookup failed for dataitem :" << dataitemName
                   << endl;
        exit(1);
    }
    return dataitem;
}

#endif
