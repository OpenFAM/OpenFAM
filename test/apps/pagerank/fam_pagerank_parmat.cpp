/*
 * fam_pagerank_parmat.cpp
 * Copyright (c) 2019-2020 Hewlett Packard Enterprise Development, LP. All
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
#include "common/fam_test_config.h"
#include "fam_utils.h"
#include "pagerank_common.h"
#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <fstream>
#include <iostream>

using namespace std;
using namespace openfam;

#define fam_stream cout << "PE(" << myPE << "/" << numPEs << ") "
//
// Enable PRINT_OUTPUT to save pagerank output to OUTFILE.
// It can be compared to pagerank non fam version for correctness.
//
#define PRINT_OUTPUT 1
#define OUTFILE "pagerank_fam.out"

// Version used by Parmat. Here the rowCount is in int64_t
int pagerank_initialize_vector(int64_t rowCount, const char *vector_name) {
    double *local = NULL;
    Fam_Descriptor *vector0 = NULL;

    try {
        vector0 = my_fam->fam_lookup(vector_name, REGION_NAME);
    }
    catch (...) {
        // ignore
        fam_stream << "fam lookup failed for dataitem :" << vector_name << endl;
        exit(1);
    }

    local = (double *)malloc((sizeof(double) * rowCount));
    for (int64_t i = 0; i < rowCount; i++)
        local[i] = 1.0 / (double)rowCount;

    if (spmv_fam_write((char *)local, vector0, 0, (sizeof(double) * rowCount)) <
        0) {
        free(local);
        delete vector0;
        return -1;
    }
    if (vector0) {
        delete vector0;
    }

    free(local);
    return 0;
}

int main(int argc, char **argv) {
    int64_t nelements, nsegments = 1;
    Fam_Descriptor *rowDataItem[MAX_SEGMENTS];
    Fam_Descriptor *colDataItem[MAX_SEGMENTS];
    Fam_Descriptor *valDataItem[MAX_SEGMENTS];
    Fam_Descriptor *headerDataItem;
    Fam_Descriptor *resultDataItem;
    const char *vector_dataitem_name[2] = { "Vector0", "Vector1" };
    uint32_t rowsPerItr = 1024;
    int64_t matRowCount;
    int pagerank_Iter = 1;
    double *vector_in, *vector_out;
    SparseMatrixConfig config;
    // int64_t nlocaledges[MAX_SEGMENTS];
    int ret;

    // FAM initialize
    // Open Preloaded region
    // region = spmv_fam_initialize();
    (void)spmv_fam_initialize();
    // Read config
    if (spmv_fam_read((char *)&config, "config", 0,
                      sizeof(struct SparseMatrixConfig)) < 0) {
        cerr << "Reading SparseMatrixConfig failed" << endl;
        exit(-1);
    }
    std::cout << "Size : " << config.nrows
              << " nelements : " << config.nelements
              << " Segments : " << config.nsegments << endl;
    if (argc != 3) {
        if (myPE == 0) {
            cout << "usage" << endl;
            cout << argv[0] << " <rows per iter> <Iter>"
                 << endl;
        }
        exit(1);
    }
    matRowCount = config.nrows;
    nelements = config.nelements;
    nsegments = config.nsegments;
    rowsPerItr = atoi(argv[1]);
    pagerank_Iter = atoi(argv[2]);

    if (myPE == 0) {
        fam_stream << "Matrix : " << matRowCount << " X " << matRowCount
                   << endl;
        fam_stream << "Num of elements : " << nelements << endl;
        fam_stream << "Pagerank iteration : " << pagerank_Iter << endl;
        fam_stream << "Partition size : " << rowsPerItr << endl;
        if (nsegments != 1)
            fam_stream << "num segments: " << nsegments << endl;
    }

    headerDataItem = spmv_get_dataitem("Global_header");
    if (myPE == 0) {
        char buf[1024] = { 0 };
        int ret = spmv_fam_write((char *)&buf, headerDataItem, 0, 1024);
        if (ret < 0) {
            fam_stream << "fam write failed to update header" << endl;
            exit(1);
        }
        fam_stream << "Global header initialized !!!" << endl;
    }

    // initialize vector_in local vector
    vector_in = (double *)spmv_malloc(sizeof(double) * matRowCount);
    if (vector_in == NULL) {
        fam_stream << "Input Vector alloc failed" << endl;
        exit(1);
    }

    // initialize vector_out local vector
    vector_out = (double *)spmv_malloc(sizeof(double) * matRowCount);
    if (vector_out == NULL) {
        fam_stream << "Output vector alloc failed" << endl;
        exit(1);
    }
    pagerank_initialize_vector(matRowCount, vector_dataitem_name[0]);

    // Initialize row, coloumn and weight dataitems
    for (int i = 0; i < nsegments; i = i + 1) {
        rowDataItem[i] = spmv_get_dataitem("rowptr" + std::to_string(i));
        colDataItem[i] = spmv_get_dataitem("colptr" + std::to_string(i));
        valDataItem[i] = spmv_get_dataitem("valptr" + std::to_string(i));
	if(rowDataItem[i] == NULL || colDataItem[i] == NULL || valDataItem[i] == NULL) {
		std::cout<<"Lookup of rowptr or colptr or valptr dataitem failed "<<std::endl;
		exit(-1);
	}
    }
    my_fam->fam_barrier_all();
    uint64_t start_time = fam_test_get_time();

    for (int iter = 0; iter < pagerank_Iter; iter++) {
        // Read input vector, Vector0 or Vector1 based on iteration
        ret = spmv_fam_read((char *)vector_in, vector_dataitem_name[iter % 2],
                            0, sizeof(double) * matRowCount);
        if (ret < 0) {
            fam_stream << "Reading vector data failed" << endl;
            exit(1);
        }

        // Set the result Dataitem
        resultDataItem =
            spmv_get_dataitem(vector_dataitem_name[(iter + 1) % 2]);

#ifdef TRACE
        if (myPE == 0)
            fam_stream << "Reading input  vector, pagerank iter = " << iter
                       << endl;
#endif

        for (int i = 0; i < nsegments; i = i + 1) {
            // Each PE to read rows_per_iter rows

            int64_t rowBegin;
            while ((rowBegin = spmv_get_next_row(headerDataItem, 0,
                                                 rowsPerItr)) < matRowCount) {
                rowtype *local_row = NULL;
                int64_t *local_col = NULL;
                valtype *local_val = NULL;
                int64_t entries;
                int64_t curRows;
#ifdef TRACE
                fam_stream << "Multiplying row  " << rowBegin << " "
                           << rowsPerItr << " iter = " << iter << endl;
#endif
                int64_t rowEnd = rowBegin + rowsPerItr;
                if (rowEnd > matRowCount)
                    rowEnd = matRowCount;

                curRows = rowEnd - rowBegin;
                int64_t rowSize = (curRows + 1) * sizeof(rowtype);
#ifdef TRACE
                std::cout << "Allocating : " << rowSize << " for row "
                          << std::endl;
#endif
                // Init local pointers
                local_row = (rowtype *)spmv_malloc(rowSize);
                if (local_row == NULL) {
                    fam_stream << "Row alloc failed" << endl;
                    exit(1);
                }
		int64_t rowOff = rowBegin;
#ifdef graph500
		rowOff = rowBegin + 4;
#endif
                // We now know rows to start multiplication
                // It is grow to growEnd
                //
                // Read grow to growEnd
                ret = spmv_fam_read((char *)local_row, rowDataItem[i],
                                    (rowOff) * sizeof(rowtype),
                                    (curRows + 1) * sizeof(rowtype));
                if (ret < 0) {
                    fam_stream << "Reading rowptr failed" << endl;
                }
#ifdef TRACE
		for (int t1 = 0; t1 <= curRows; t1++) {
			std::cout<<" row "<<t1<<" "<<local_row[t1]<<std::endl;
		}
#endif

                // Load column into local_col
                int64_t colOff = local_row[0] * sizeof(int64_t);
                entries = local_row[curRows] - local_row[0];
#ifdef graph500
		entries = entries - 1;
#endif
                //int64_t colSize = (entries) * sizeof(int64_t);
		int64_t colSize = (entries) * sizeof(int64_t);
#ifdef TRACE
                // init local column pointer
                fam_stream << "Allocating : " << colSize
                           << " for col , off : " << colOff
			   << " entries : "<< entries
                           << " rowBegin: " << rowBegin << " " << local_row[0]
                           << ":" << local_row[curRows - 1] << std::endl;
#endif
                local_col = (int64_t *)spmv_malloc(colSize);
                if (local_col == NULL) {
                    fam_stream << "Column ptr alloc failed" << endl;
                    exit(1);
                }

                ret = spmv_fam_read((char *)local_col, colDataItem[i], colOff,
                                    colSize);

                if (ret < 0) {
                    fam_stream << "Reading column data failed" << endl;
                    exit(1);
                }
// init local value pointer
#ifdef TRACE
                fam_stream << "Allocating : " << colSize << " for val "
                           << std::endl;
#endif

		int64_t valSize = (entries) * sizeof(valtype);
		int64_t valOff =  local_row[0] * sizeof(valtype);
                local_val = (valtype *)spmv_malloc(valSize);
                if (local_val == NULL) {
                    fam_stream << "Value ptr alloc failed" << endl;
                    exit(1);
                }
                // Load value into local_val
                ret = spmv_fam_read((char *)local_val, valDataItem[i], valOff,
                                    valSize);
                if (ret < 0) {
                    fam_stream << "Reading value data failed" << endl;
                    exit(1);
                }
#ifdef TRACE
                std::cout << "Reading values successful!!" << std::endl;
                std::cout << "#### Row : " << rowBegin << " : " << rowEnd
                          << " colptr : " << local_row[0] << " : "
                          << local_row[curRows] << std::endl;

                for (int64_t r = 0; r < curRows; r++) {
                    std::cout << "Row " << r + rowBegin << "(" << r
                              << ") : " << local_row[r] << " : "
                              << local_row[r + 1] << std::endl;
                    int64_t rowOff = local_row[0];
                    for (int64_t k = local_row[r]; k < local_row[r + 1]; k++) {
                        std::cout << "Col: " << local_col[k - rowOff]
                                  << " Val: " << local_val[k - rowOff]
                                  << std::endl;
                    }
                }
#endif                
                // Multiply
                spmv_compute(local_row, local_col, local_val, 0, curRows,
                             rowBegin, vector_in, vector_out);

                // Write output vector
                ret = spmv_fam_write((char *)vector_out+rowBegin * sizeof(double), resultDataItem,
                                     rowBegin * sizeof(double),
                                     sizeof(double) * curRows);
                if (ret < 0) {
                    fam_stream << "fam write failed to update result" << endl;
                }
            }
        }
        my_fam->fam_barrier_all();
        if (myPE == 0) {
            // For the next iteration read rows from the beginning
            spmv_set_row_zero(headerDataItem);
            // fam_stream << "Completed !!!" << endl;
        }
        my_fam->fam_barrier_all();
    }
    uint64_t final_time = fam_test_get_time() - start_time;
    cout << "Pagerank time for PE=" << myPE << " for no. of iteration "
         << pagerank_Iter << " in nanoseconds = " << final_time << endl;
#ifdef PRINT_OUTPUT
    if (myPE == 0) {
        ofstream outfile;
        outfile.open(OUTFILE);

        ret = spmv_fam_read((char *)vector_in, vector_dataitem_name[0], 0,
                            sizeof(double) * matRowCount);
        if (ret < 0) {
            fam_stream << "Reading vector data failed" << endl;
            exit(1);
        }
        ret = spmv_fam_read((char *)vector_out, vector_dataitem_name[1], 0,
                            sizeof(double) * matRowCount);
        if (ret < 0) {
            fam_stream << "Reading vector data failed" << endl;
            exit(1);
        }

        for (int i = 0; i < matRowCount; i++) {
            outfile << "Row : " << i << " Vector0 : " << vector_in[i]
                    << " Vector1 : " << vector_out[i] << std::endl;
        }
        outfile.close();
    }
#endif

    my_fam->fam_finalize("default");
    delete my_fam;

    return 0;
}
