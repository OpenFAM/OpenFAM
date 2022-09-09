/*
 * fam_csr_preload_parmat.cpp
 * Copyright (c) 2019-2022 Hewlett Packard Enterprise Development, LP. All
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
#include "common/fam_options.h"
#include "fam_utils.h"
#include "pagerank_common.h"
#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <fstream>
#include <iostream>
//#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

// Loading PARMAT graph to FAM
int LoadGraphMatrix(std::istream& istream, size_t num_rows, size_t num_elements, 
                    Fam_Region_Descriptor *region, mode_t perm)
{
    // load words
    int count = 0;
    int64_t vertex_from;
    int64_t vertex_to;
    int64_t prev_vertex_from = -1;
    int64_t* rowptr = new int64_t[num_rows+1];
    int64_t* colptr = new int64_t[num_elements];
    int64_t* valptr = new int64_t[num_elements];

    while (istream >> vertex_from >> vertex_to)
    {
        if (vertex_from != prev_vertex_from) {
            // fill up any empty rows
            for (int64_t row = prev_vertex_from+1; row <= vertex_from; row++) {
                rowptr[row] = count;
            }
            prev_vertex_from = vertex_from;
        }
        //col_ind_val[count] = { vertex_to, 1 };    
        colptr[count] = vertex_to;
        valptr[count] = 1;
        if (++count % 1000000 == 0) {
            std::cout << "Loaded " << count << " edges" << std::endl;
        }
    }
    // fill up remaining empty rows
    for (uint64_t row = vertex_from+1; row < (num_rows+1); row++) {
        rowptr[row] = count;
    }
    // write rowptr to fam
    if (load_dataitem((char*)rowptr, "rowptr0", 0, 
                       (num_rows+1)*sizeof(int64_t), region, perm) < 0) {
        delete rowptr;
        delete colptr;
        delete valptr;
        return -1;
    }
    // write colptr to fam
    if (load_dataitem((char*)colptr, "colptr0", 0,
                       (count)*sizeof(int64_t), region, perm) < 0) {
        delete rowptr;
        delete colptr;
        delete valptr;
        return -1;
    }
    // write valptr to fam
    if (load_dataitem((char*)valptr, "valptr0", 0,
                       (count)*sizeof(int64_t), region, perm) < 0) {
        delete rowptr;
        delete colptr;
        delete valptr;
        return -1;
    } 
               
    return 0;
}

//
// This file loads sorted parmat output into fam
// @input : argv[1] - parmat file
// @input : argv[2] - nrows
// @input : argv[3] - nnz
//
// Note: Parmat file has to be created using :
// ./PaRMAT -nVertices 16384 -nEdges 16384*4 -sorted 
//
int main(int argc, char **argv) {
    Fam_Region_Descriptor *region = NULL;
    std::string parmat_file("parmat.out");
    int matRowCount;
    int nnz;
    int nelements;
    mode_t perm = 0777;
    SparseMatrixConfig config;

    // Parse cmdline args
    if (argc != 4) {
        cout << "./parmat_csr_load parmat_file nrows nnz" << endl;
        exit(1);
    }
    parmat_file = argv[1];  
    matRowCount = atoi(argv[2]);
    nnz = atoi(argv[3]);
   
    cout << "Matrix : " << matRowCount << " X " << matRowCount << endl;
    cout << "nnz : " << nnz << endl;
    cout << "Data Path : " << parmat_file << endl;

    uint64_t start_time = fam_test_get_time();
    nelements = nnz * matRowCount;

    // FAM initialize and create region
    // TODO: If region creation fails , consider changing the size of the region
    region = spmv_fam_initialize(
        (nelements * 3 * sizeof(int64_t) + 2 * matRowCount * sizeof(double)) *
        4);

    // Set config
    config.nrows = matRowCount;
    config.nelements = nnz * matRowCount;
    config.nsegments = 1;
    
    load_dataitem((char*)&config, "config", 0,
                       sizeof(struct SparseMatrixConfig), region, perm);

    // Create CSR to 3 dataitems, rowptr, colptr and valptr
    cout << "Loading CSR data" << endl;
    std::ifstream infile(parmat_file);
    std::istream &istream = infile;
    LoadGraphMatrix(istream, matRowCount, nelements, region, perm);

    // Create and initialize dataitem for vector
    cout << "Create dataitem for vector !!!" << endl;
    pagerank_initialize_vectors(matRowCount, region, perm);

    cout << "Create dataitem for runtime header!!!" << endl;
    spmv_initialize_runtime_header(region, perm);

    cout << "CSR load complete !!!" << endl;
    delete region;
    my_fam->fam_finalize("default");
    delete my_fam;

    uint64_t final_time = fam_test_get_time() - start_time;
    cout << dec << "load time in nanoseconds = " << final_time << endl;
}
