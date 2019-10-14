/*
 * fam_csr_preload_graph500.cpp 
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
#include "common/fam_options.h"
#include "pagerank_common.h"
#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <iostream>
//#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

int main(int argc, char **argv) {
    Fam_Region_Descriptor *region = NULL;
    char configFilename[32];
    int matRowCount;
    int numSegments;
    char inpPath[PATH_MAX];
    char inpFilename[PATH_MAX];
    char tmpFilename[PATH_MAX];
    int ret;
    mode_t perm = 0777;
    SparseMatrixConfig config;

    // Parse cmdline args
    if (argc != 2) {
        cout << argv[0]<<" <config_file>" << endl;
        exit(1);
    }
    strcpy(configFilename, argv[1]);
    spmv_read_config_file(configFilename, matRowCount, numSegments, inpPath);
    cout << "Matrix : " << matRowCount << " X " << matRowCount << endl;
    cout << "Num Segments : " << numSegments << endl;
    cout << "Data Path : " << inpPath << endl;

    // FAM initialize and create region
    region = spmv_fam_initialize(MATRIX_SIZE);

    // Set config
    config.nrows = matRowCount;
    config.nelements = -1;
    config.nsegments = numSegments;

    load_dataitem((char*)&config, "config", 0,
                       sizeof(struct SparseMatrixConfig), region, perm);

    // Write config file to FAM
    ret = spmv_load_file_to_dataitem(configFilename, configFilename, region,
                                     perm, CONFIG_FILE_SIZE);
    if (ret < 0) {
        cout << "writing config data failed" << endl;
        exit(1);
    }
    // Create data item for each segment and load contents
    cout << "Loading CSR data" << endl;
    for (int i = 0; i < numSegments; i++) {
        uint64_t size;
        int ret;

        // Load row
        sprintf(tmpFilename, "/rowptr%d.bin", i);
        strcpy(inpFilename, inpPath);
        strcat(inpFilename, tmpFilename);
        size = (MAX_ROW_COUNT + 5) * sizeof(int);
	char* rowptrDataitemName = (char *)("rowptr"+std::to_string(i)).c_str();
        ret = spmv_load_file_to_dataitem(inpFilename, rowptrDataitemName, region, perm,
                                         size);
        if (ret < 0) {
            cout << "Reading row data failed" << endl;
            exit(1);
        }
        // Load column
        sprintf(tmpFilename, "/column%d.bin", i);
        strcpy(inpFilename, inpPath);
        strcat(inpFilename, tmpFilename);
	char* columnDataitemName = (char *)("colptr"+std::to_string(i)).c_str();
        size = (MAX_NZ_COUNT) * sizeof(int64_t);
        ret = spmv_load_file_to_dataitem(inpFilename, columnDataitemName, region, perm,
                                         size);
        if (ret < 0) {
            cout << "Reading column data failed" << endl;
            exit(1);
        }
        // Load weight
        sprintf(tmpFilename, "/weight%d.bin", i);
        strcpy(inpFilename, inpPath);
        strcat(inpFilename, tmpFilename);
        size = (MAX_NZ_COUNT) * sizeof(int64_t);
	char* valueDataitemName = (char *)("valptr"+std::to_string(i)).c_str();
        ret = spmv_load_file_to_dataitem(inpFilename, valueDataitemName, region, perm,
                                         size);
        if (ret < 0) {
            cout << "Reading weight data failed" << endl;
            exit(1);
        }
    }

    // Create and initialize dataitem for vector
    cout << "Create dataitem for vector !!!" << endl;
    pagerank_initialize_vectors(matRowCount, region, perm);

    cout << "Create dataitem for runtime header!!!" << endl;
    spmv_initialize_runtime_header(region, perm);

    cout << "CSR load complete !!!" << endl;
    delete region;
    my_fam->fam_finalize("default");
    delete my_fam;
}
