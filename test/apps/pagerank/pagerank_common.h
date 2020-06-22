/*
 * pagerank_common.h 
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
#ifndef __PAGERANK_COMMON_H__
#define __PAGERANK_COMMON_H__ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sched.h>
#include <unistd.h>
#include <limits.h>
#include "common/fam_options.h"
#include "common/fam_test_config.h"
#include <fam/fam.h>
#include <fam/fam_exception.h>

/* You might need to increase MAX_ROW_COUNT and MAX_NZ_COUNT for
 * larger matrix size. You can choose to decrease for smaller
 * matrix size. It works for Matrix with 1B rows, and nnz per row 48.
 */
#define MAX_CPU_PER_SOCK   32
#define MAX_ROW_COUNT      2097152
#define MAX_NZ_COUNT       16777216
#define MAX_SEGMENTS       1024
#define MATRIX_SIZE (16 * 1024UL * 1024UL * 1024UL)
#define REGION_NAME "pagerank"
#define CONFIG_FILE_SIZE 1024

using namespace std;
using namespace openfam;

typedef int64_t rowtype;
typedef int64_t valtype;

fam *my_fam;
int myPE, numPEs;
#define fam_stream cout << "PE(" << myPE << "/" << numPEs << ") "

double *gresult; // shmem symmetric variable to store global results.
int *grow_off; // rowptr global memory
float *gval; // value global memory 
int64_t *gcol; // column global memory
int64_t *gvec; // vector global memory

struct SparseMatrixConfig {
   int64_t nrows;
   int64_t nelements;
   int64_t nsegments;
};

uint64_t filelength(const char *filpath) { // don't use that. Learn POSIX API
    struct stat st;
    if (stat(filpath, &st)) /*failure*/
        return -1;          // when file does not exist or is not accessible
    return (long)st.st_size;
}


Fam_Region_Descriptor *spmv_fam_initialize(void) {
    Fam_Region_Descriptor *region = NULL;
    Fam_Options fam_opts;
    int *val;
    // FAM initialize
    my_fam = new fam();
    init_fam_options(&fam_opts);
    // fam_opts.allocator = strdup("NVMM");
    // fam_opts.runtime = strdup("NONE");

    try {
        my_fam->fam_initialize("default", &fam_opts);
    }
    catch (Fam_Exception &e) {
        fam_stream << "fam initialization failed:" << e.fam_error_msg() << endl;
        exit(1);
    }
    val = (int *)my_fam->fam_get_option(strdup("PE_ID"));
    myPE = *val;
    val = (int *)my_fam->fam_get_option(strdup("PE_COUNT"));
    numPEs = *val;

    if (myPE == 0) {
        fam_stream << "Total PEs : " << numPEs << endl;
        // fam_stream << "My PE : " << myPE << endl;
    }

    // Lookup region
    try {
        region = my_fam->fam_lookup_region(REGION_NAME);
    }
    catch (...) {
        // ignore
        fam_stream << "fam lookup failed for region :" << REGION_NAME << endl;
        exit(1);
    }
    return region;
}

// Version of initialize with size as input, it will destroy existing region and 
// create new region
Fam_Region_Descriptor *spmv_fam_initialize(size_t size) {
    Fam_Region_Descriptor *region = NULL;
    Fam_Options fam_opts;
    //uint64_t size = MATRIX_SIZE;

    // FAM initialize
    my_fam = new fam();
    memset((void *)&fam_opts, 0, sizeof(Fam_Options));
    fam_opts.runtime = strdup("NONE");

    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed:" << e.fam_error_msg() << endl;
        exit(1);
    }

    // Create/Open region
    try {
        region = my_fam->fam_lookup_region(REGION_NAME);
        if (region != NULL) {
            try {
                my_fam->fam_destroy_region(region);
            } catch (Fam_Exception &e) {
                cout << "Error msg: " << e.fam_error_msg() << endl;
                exit(1);
            }
        }
    } catch (...) {
        // ignore
    }
    try {
        region = my_fam->fam_create_region(REGION_NAME, size, 0777, NONE);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed:" << e.fam_error_msg() << endl;
        exit(1);
    }
    return region;
}

void *spmv_malloc(uint64_t size) {
    void *addr = malloc(size);
    if (addr == NULL) {
        cout << "malloc failed(" << size << ")" << endl;
        exit(1);
    }
    return addr;
}

int spmv_fam_read(char *buf, Fam_Descriptor *dataitem, uint64_t offset,
                  uint64_t size) {
    try {
        my_fam->fam_get_blocking(buf, dataitem, offset, size);
    }
    catch (Fam_Exception &e) {
        cout << "FAM Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
        return -1;
    }
    catch (...) {
        fam_stream << "fam get failed" << endl;
        return -1;
    }
    return 0;
}

Fam_Descriptor *spmv_get_dataitem(std::string dataitemName) {
    Fam_Descriptor *dataitem = NULL;
    // Lookup config dataitem
    try {
        dataitem = my_fam->fam_lookup(dataitemName.c_str(), REGION_NAME);
    }
    catch (...) {
        // ignore
        fam_stream << "fam lookup failed for dataitem :" << dataitemName
                   << endl;
        exit(1);
    }
    return dataitem;
}

int spmv_fam_read(char *buf, const char *dataitemName, uint64_t offset,
                  uint64_t size) {
    Fam_Descriptor *dataitem = NULL;
    // Lookup config dataitem
    try {
        dataitem = my_fam->fam_lookup(dataitemName, REGION_NAME);
    }
    catch (...) {
        // ignore
        fam_stream << "fam lookup failed for dataitem :" << dataitemName
                   << endl;
        exit(1);
    }
    return spmv_fam_read(buf, dataitem, offset, size);
}

Fam_Descriptor *spmv_get_dataitem(const char *dataitemName) {
    Fam_Descriptor *dataitem = NULL;
    // Lookup config dataitem
    try {
        dataitem = my_fam->fam_lookup(dataitemName, REGION_NAME);
    }
    catch (...) {
        // ignore
        fam_stream << "fam lookup failed for dataitem :" << dataitemName
                   << endl;
        exit(1);
    }
    return dataitem;
}

int spmv_fam_write(char *inpBuf, const char *dataitemName, uint64_t offset,
                   uint64_t size) {
    Fam_Descriptor *dataitem = NULL;

    // Lookup config dataitem
    try {
        dataitem = my_fam->fam_lookup(dataitemName, REGION_NAME);
    }
    catch (...) {
        // ignore
        fam_stream << "fam lookup failed for dataitem :" << dataitemName
                   << endl;
        return -1;
    }
    try {
        my_fam->fam_put_blocking(inpBuf, dataitem, offset, size);
    }
    catch (Fam_Exception &e) {
        fam_stream << "Exception caught" << endl;
        fam_stream << "Error msg: " << e.fam_error_msg() << endl;
        fam_stream << "Error: " << e.fam_error() << endl;
        return -1;
    }
    return 0;
}

int spmv_fam_write(char *inpBuf, Fam_Descriptor *dataitem, uint64_t offset,
                   uint64_t size) {
    try {
        my_fam->fam_put_blocking(inpBuf, dataitem, offset, size);
    }
    catch (Fam_Exception &e) {
        fam_stream << "Exception caught" << endl;
        fam_stream << "Error msg: " << e.fam_error_msg() << endl;
        fam_stream << "Error: " << e.fam_error() << endl;
        return -1;
    }
    return 0;
}

// This function uses grow as input matrix. Used with graph500 version
void spmv_compute(int64_t rowStart, int64_t rowEnd, double *vector_in,
                  double *vector_out) {
    int64_t offShift = grow_off[rowStart];
    for (int64_t i = rowStart; i < rowEnd; i++) {
        double res = 0;
        for (int k = grow_off[i]; k < grow_off[i + 1]; k++) {
            res += gval[k - offShift] * (double)vector_in[gcol[k - offShift]];
        }
        vector_out[i] = res;
    }
}

void spmv_compute(rowtype *local_row, int64_t *local_col, valtype *local_val,
                  int64_t rowBegin, int64_t rowEnd, int64_t real_row,
                  double *vector_in, double *vector_out) {
    int64_t offShift = local_row[rowBegin];
    for (int64_t i = rowBegin; i < rowEnd; i++) {
        double sum = 0;
        for (int64_t k = local_row[i]; k < local_row[i + 1]; k++) {
            sum += (double)local_val[k - offShift] *
                   vector_in[local_col[k - offShift]];
#ifdef TRACE
            fam_stream << " SpMV: " << i << " Actual row: " << real_row + i
                       << " OffShift: " << offShift << " colIdx: " << k << " : "
                       << local_row[i + 1]
                       << " Col: " << local_col[k - offShift]
                       << " Val: " << local_val[k - offShift]
                       << " Sum: " << sum<< endl;
#endif
        }

        vector_out[i + real_row] = sum;
#ifdef TRACE
        fam_stream<<" Multiply "<<i+real_row<<": Vector_in: "<<vector_in[i + real_row]
                 <<" Vector_out: "<<vector_out[i + real_row]<<std::endl;
#endif
    }
}

int spmv_get_next_row(Fam_Descriptor *item, uint64_t offset, uint32_t val) {
    int res;
    try {
        res = (int)my_fam->fam_fetch_add(item, 0, val);
    }
    catch (...) {
        cout << "fam_fetch_add failed" << endl;
        exit(1);
    }
    return res;
}

// Version used by graph500
int pagerank_initialize_vector(int rowCount, const char *vector_name) {
   double *local = NULL;
   Fam_Descriptor *vector0 = NULL;

   try {
        vector0 = my_fam->fam_lookup(vector_name, REGION_NAME);
    }
    catch (...) {
        // ignore
        fam_stream << "fam lookup failed for dataitem :" << vector_name
                   << endl;
        exit(1);
    }

    local = (double *)malloc((sizeof(double) * rowCount));
    for (int i = 0; i < rowCount; i++)
        local[i] = 1.0 / rowCount;

    if (spmv_fam_write((char *)local, vector0, 0,
                       (sizeof(double) * rowCount)) < 0) {
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

// Used to initialize Vector0 and Vector1 during preload
int pagerank_initialize_vectors(int rowCount, Fam_Region_Descriptor *region,
                                 mode_t perm) {
   double *local = NULL;
   Fam_Descriptor *vector0 = NULL;
   Fam_Descriptor *vector1 = NULL;

   try {
        vector0 = my_fam->fam_allocate("Vector0", (sizeof(double) * rowCount),
                                        perm, region);
    } catch (...) {
        cout << "fam allocate failed:" << endl;
        return -1;
    }

    try {
        vector1 = my_fam->fam_allocate("Vector1", (sizeof(double) * rowCount),
                                        perm, region);
    } catch (...) {
        cout << "fam allocate failed:" << endl;
        return -1;
    }
    local = (double *)malloc((sizeof(double) * rowCount));
    for (int i = 0; i < rowCount; i++)
        local[i] = 1.0 / rowCount;

    if (spmv_fam_write((char *)local, vector0, 0,
                       (sizeof(double) * rowCount)) < 0) {
        free(local);
        delete vector0;
        delete vector1;
        return -1;
    }
    if (vector0) {
        delete vector0;
    }
    if (vector1) {
        delete vector1;
    }

    free(local);
    return 0;

}

int spmv_initialize_runtime_header(Fam_Region_Descriptor *region,
                                    mode_t perm) {
    Fam_Descriptor *dataitem = NULL;

    try {
        dataitem = my_fam->fam_allocate("Global_header", 1024,
                                        perm, region);
        delete dataitem;
    } catch (...) {
        cout << "fam allocate failed:" << endl;
        return -1;
    }
    return 0;
}

void spmv_read_config_file(char *configFilename, int &matRowCount,
                           int &numSegments, char *inpPath) {
    FILE *fp;
    char *str = (char *)malloc(72);
    // Read Config file
    // matRowCount - Number of rows in the matrix
    // numSegments - number of file segments
    // inpPath - path of the input file segments

    fp = fopen(configFilename, "r");
    if (fp == NULL) {
        printf("\nCouldn't locate the config file..Exiting");
        exit(1);
    }
    memset(str, 0, 72);
    str = fgets(str, 72, fp);
    if (str == NULL) {
        printf("\nInvalid config data..Exiting");
        exit(1);
    }
    sscanf(str, "%d", &matRowCount);
    memset(str, 0, 72);
    str = fgets(str, 72, fp);
    if (str == NULL) {
        printf("\nInvalid config data..Exiting");
        exit(1);
    }
    sscanf(str, "%d", &numSegments);
    str = fgets(str, 72, fp);
    if (str == NULL) {
        printf("\nInvalid config data..Exiting");
        exit(1);
    }
    sscanf(str, "%s", inpPath);
    free(str);
    fclose(fp);
}

int load_dataitem(char *inpBuf, const char *dataitemName, uint64_t offset,
                   uint64_t size, Fam_Region_Descriptor *region, mode_t perm) {
    Fam_Descriptor *dataitem = NULL;

    try {
        dataitem = my_fam->fam_allocate(dataitemName, size, perm, region);
    } catch (...) {
        cout << "fam allocate failed:" << endl;
        return -1;
    }
    if(spmv_fam_write(inpBuf, dataitem, offset,
                       size) < 0) {
        cerr << "SpMV fam write failed" <<endl;
        return -1;
    }
    return 0;
}

int spmv_load_file_to_dataitem(char *inpFilename, char *dataitemName,
                               Fam_Region_Descriptor *region, mode_t perm,
                               uint64_t size) {
    FILE *fp;
    char *buf, *addr;
    uint64_t fileSize, rem;
    Fam_Descriptor *dataitem;

    fileSize = filelength(inpFilename);
    if (fileSize > size) {
        return -1;
    }
    fp = fopen(inpFilename, "rb");
    if (fp == NULL) {
        printf("File not found:%s\n", inpFilename);
        return -1;
    }
    try {
        dataitem = my_fam->fam_allocate(dataitemName, size, perm, region);
    } catch (...) {
        cout << "fam allocate failed:" << endl;
        return -1;
    }
    buf = (char *)malloc(fileSize);
    if (buf == NULL) {
        cout << "malloc failed" << endl;
        delete dataitem;
        return -1;
    }
    addr = buf;
    rem = fileSize;
    while (!feof(fp) && (rem > 0)) {
        int64_t res = 0;
        res = fread(addr, 1, rem, fp);
        addr += res;
        rem -= res;
    }
    if (spmv_fam_write(buf, dataitem, 0, fileSize) < 0) {
        free(buf);
        delete dataitem;
        return -1;
    }
    free(buf);
    delete dataitem;
    fclose(fp);
    return 0;
}


void spmv_read_config_data(char *configFilename, int &matRowCount,
                           int &numSegments, char *inpPath) {
    char *buf, *addr;
    Fam_Descriptor *dataitem = NULL;
    Fam_Region_Descriptor *region = NULL;
    int ret;
    // Read Config file
    // matRowCount - Number of rows in the matrix
    // numSegments - number of file segments
    // inpPath - path of the input file segments

    // Lookup config dataitem
    try {
        region = my_fam->fam_lookup_region(REGION_NAME);
    }
    catch (...) {
        // ignore
        fam_stream << "fam lookup failed for dataitem :" << configFilename
                   << endl;
        exit(1);
    }
    std::cout << "Region : " << region->get_global_descriptor().regionId
              << std::endl;

    // Lookup config dataitem
    try {
        dataitem = my_fam->fam_lookup(configFilename, REGION_NAME);
    }
    catch (...) {
        // ignore
        fam_stream << "fam lookup failed for dataitem :" << configFilename
                   << endl;
        exit(1);
    }
    addr = (char *)spmv_malloc(CONFIG_FILE_SIZE);
    ret = spmv_fam_read(addr, dataitem, 0, CONFIG_FILE_SIZE);
    if (ret < 0) {
        fam_stream << "Incorrect config data" << endl;
        exit(1);
    }
    buf = addr;
    sscanf(buf, "%d", &matRowCount);
    buf = strchr(buf, '\n');
    if (buf == NULL) {
        free(addr);
        fam_stream << "Incorrect config data" << endl;
        exit(1);
    }
    buf++;
    sscanf(buf, "%d", &numSegments);
    buf = strchr(buf, '\n');
    if (buf == NULL) {
        free(addr);
        fam_stream << "Incorrect config data" << endl;
        exit(1);
    }
    buf++;
    sscanf(buf, "%s", inpPath);
    free(addr);
}

int spmv_store_dataitem_to_file(const char *outFilename,
                                const char *dataitemName,
                                Fam_Region_Descriptor *region, uint64_t size) {
    FILE *fp;
    char *buf, *addr;
    int64_t rem;
    int ret;

    buf = (char *)spmv_malloc(size);
    ret = spmv_fam_read(buf, dataitemName, 0, size);
    if (ret < 0) {
        fam_stream << "Reading failed" << endl;
        return -1;
    }
    fp = fopen(outFilename, "wb");
    if (fp == NULL) {
        printf("File not found:%s\n", outFilename);
        return -1;
    }
    addr = buf;
    rem = size;
    while (rem > 0) {
        int64_t res = 0;
        res = fwrite(addr, 1, rem, fp);
        addr += res;
        rem -= res;
    }
    fclose(fp);
    free(buf);
    return 0;
}

bool spmv_set_row_zero(Fam_Descriptor *item) {
    bool res = true;
    try {
        my_fam->fam_set(item, 0, 0);
    }
    catch (...) {
        res = false;
        cout << "fam_set failed" << endl;
        exit(1);
    }
    return res;
}

#endif // __PAGERANK_COMMON_H__

