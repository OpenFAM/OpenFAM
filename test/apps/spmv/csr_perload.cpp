/*
 * csr_perload.cpp
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
#include "fam_spmv.h"
#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <iostream>
//#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

// globals
fam *my_fam;

uint64_t filelength(const char *filpath) { // don't use that. Learn POSIX API
    struct stat st;
    if (stat(filpath, &st)) /*failure*/
        return -1;          // when file does not exist or is not accessible
    return (long)st.st_size;
}

Fam_Region_Descriptor *spmv_fam_initialize(void) {
    Fam_Region_Descriptor *region = NULL;
    Fam_Options fam_opts;
    uint64_t size = MATRIX_SIZE;

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

int spmv_fam_write(char *inpBuf, Fam_Descriptor *dataitem, uint64_t offset,
                   uint64_t size) {
    try {
        my_fam->fam_put_blocking(inpBuf, dataitem, offset, size);
    } catch (Fam_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
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
int spmv_initialize_output_dataitem(int rowCount, Fam_Region_Descriptor *region,
                                    mode_t perm) {
    Fam_Descriptor *dataitem = NULL;

    try {
        dataitem = my_fam->fam_allocate("result", (sizeof(float) * rowCount),
                                        perm, region);
        delete dataitem;
    } catch (...) {
        cout << "fam allocate failed:" << endl;
        return -1;
    }
    return 0;
}
int spmv_initialize_input_vector(int rowCount, Fam_Region_Descriptor *region,
                                 mode_t perm) {
    int64_t *local = NULL;
    Fam_Descriptor *dataitem = NULL;

    try {
        dataitem = my_fam->fam_allocate("vector", (sizeof(int64_t) * rowCount),
                                        perm, region);
    } catch (...) {
        cout << "fam allocate failed:" << endl;
        return -1;
    }
    local = (int64_t *)malloc((sizeof(int64_t) * rowCount));
    for (int i = 0; i < rowCount; i++)
        local[i] = (char)((i % 5) + 1);

    if (spmv_fam_write((char *)local, dataitem, 0,
                       (sizeof(int64_t) * rowCount)) < 0) {
        free(local);
        delete dataitem;
        return -1;
    }
    if (dataitem)
        delete dataitem;

    free(local);
    return 0;
}

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

    // Parse cmdline args
    if (argc != 2) {
        cout << "./spmv_fam <config_file>" << endl;
        exit(1);
    }
    strcpy(configFilename, argv[1]);
    spmv_read_config_file(configFilename, matRowCount, numSegments, inpPath);
    cout << "Matrix : " << matRowCount << " X " << matRowCount << endl;
    cout << "Num Segments : " << numSegments << endl;
    cout << "Data Path : " << inpPath << endl;

    // FAM initialize and create region
    region = spmv_fam_initialize();

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
        ret = spmv_load_file_to_dataitem(inpFilename, tmpFilename, region, perm,
                                         size);
        if (ret < 0) {
            cout << "Reading row data failed" << endl;
            exit(1);
        }
        // Load column
        sprintf(tmpFilename, "/column%d.bin", i);
        strcpy(inpFilename, inpPath);
        strcat(inpFilename, tmpFilename);
        size = (MAX_NZ_COUNT) * sizeof(int64_t);
        ret = spmv_load_file_to_dataitem(inpFilename, tmpFilename, region, perm,
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
        ret = spmv_load_file_to_dataitem(inpFilename, tmpFilename, region, perm,
                                         size);
        if (ret < 0) {
            cout << "Reading weight data failed" << endl;
            exit(1);
        }
    }

    // Create and initialize dataitem for vector
    cout << "Create dataitem for vector !!!" << endl;
    spmv_initialize_input_vector(matRowCount, region, perm);

    // Finally create dataitem for result
    cout << "Create dataitem for result !!!" << endl;
    spmv_initialize_output_dataitem(matRowCount, region, perm);

    cout << "Create dataitem for runtime header!!!" << endl;
    spmv_initialize_runtime_header(region, perm);

    cout << "CSR load complete !!!" << endl;
    delete region;
    my_fam->fam_finalize("default");
    delete my_fam;
}
