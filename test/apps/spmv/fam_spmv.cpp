/*
 * fam_spmv.cpp 
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

using namespace std;
using namespace openfam;

// globals
fam *my_fam;
int myPE, numPEs;

Fam_Region_Descriptor *spmv_fam_initialize(void) {
    Fam_Region_Descriptor *region = NULL;
    Fam_Options fam_opts;
    int *val;
    // FAM initialize
    my_fam = new fam();
    memset((void *)&fam_opts, 0, sizeof(Fam_Options));
    fam_opts.openFamModel = strdup("SHM");
    fam_opts.runtime = strdup("NONE");

    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed:" << e.fam_error_msg() << endl;
        exit(1);
    }
    val = (int *)my_fam->fam_get_option(strdup("PE_ID"));
    myPE = *val;
    val = (int *)my_fam->fam_get_option(strdup("PE_COUNT"));
    numPEs = *val;

    cout << "Total PEs : " << numPEs << endl;
    cout << "My PE : " << myPE << endl;

    // Lookup region
    try {
        region = my_fam->fam_lookup_region(REGION_NAME);
    } catch (...) {
        // ignore
        cout << "fam lookup failed for region :" << REGION_NAME << endl;
        exit(1);
    }
    return region;
}

char *spmv_fam_read(Fam_Descriptor *dataitem, uint64_t offset, uint64_t size) {
    char *local = NULL;
    try {
        local = (char *)my_fam->fam_map(dataitem);
    } catch (...) {
        cout << "fam map failed" << endl;
        return NULL;
    }
    return local;
}

char *spmv_fam_read(const char *dataitemName, uint64_t offset, uint64_t size) {
    Fam_Descriptor *dataitem = NULL;
    // Lookup config dataitem
    try {
        dataitem = my_fam->fam_lookup(dataitemName, REGION_NAME);
    } catch (...) {
        // ignore
        cout << "fam lookup failed for dataitem :" << dataitemName << endl;
        exit(1);
    }
    return spmv_fam_read(dataitem, offset, size);
}

int spmv_fam_write(const char *dataitemName, char *inpBuf, uint64_t offset,
                   uint64_t size) {
    char *local = NULL;
    Fam_Descriptor *dataitem = NULL;
    // Lookup config dataitem
    try {
        dataitem = my_fam->fam_lookup(dataitemName, REGION_NAME);
    } catch (...) {
        // ignore
        cout << "fam lookup failed for dataitem :" << dataitemName << endl;
        return -1;
    }
    try {
        local = (char *)my_fam->fam_map(dataitem);
    } catch (...) {
        cout << "fam map failed" << endl;
        return -1;
    }
    memcpy(local + offset, inpBuf, size);
    return 0;
}

void spmv_read_config_data(char *configFilename, int &matRowCount,
                           int &numSegments, char *inpPath) {
    char *buf;
    Fam_Descriptor *dataitem = NULL;
    // Read Config file
    // matRowCount - Number of rows in the matrix
    // numSegments - number of file segments
    // inpPath - path of the input file segments

    // Lookup config dataitem
    try {
        dataitem = my_fam->fam_lookup(configFilename, REGION_NAME);
    } catch (...) {
        // ignore
        cout << "fam lookup failed for dataitem :" << configFilename << endl;
        exit(1);
    }
    buf = spmv_fam_read(dataitem, 0, CONFIG_FILE_SIZE);
    if (buf == NULL) {
        cout << "Incorrect config data" << endl;
        exit(1);
    }
    sscanf(buf, "%d", &matRowCount);
    buf = strchr(buf, '\n');
    if (buf == NULL) {
        cout << "Incorrect config data" << endl;
        exit(1);
    }
    buf++;
    sscanf(buf, "%d", &numSegments);
    buf = strchr(buf, '\n');
    if (buf == NULL) {
        cout << "Incorrect config data" << endl;
        exit(1);
    }
    buf++;
    sscanf(buf, "%s", inpPath);
}

void spmv_compute(int64_t startIdx, int64_t rowStart, int64_t rowEnd) {
    for (int64_t i = rowStart; i < rowEnd; i++) {
        float res = 0;
        for (int k = grow_off[i]; k < grow_off[i + 1]; k++) {
            res += gval[k] * (float)gvec[gcol[k]];
        }
        gresult[startIdx + i] = res;
    }
}

int spmv_store_dataitem_to_file(const char *outFilename,
                                const char *dataitemName,
                                Fam_Region_Descriptor *region, uint64_t size) {
    FILE *fp;
    char *buf, *addr;
    int64_t rem;

    buf = spmv_fam_read(dataitemName, 0, size);
    if (buf == NULL) {
        cout << "Reading failed" << endl;
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
    return 0;
}

int main(int argc, char **argv) {
    Fam_Region_Descriptor *region = NULL;
    char configFilename[32];
    int matRowCount;
    int numSegments;
    char inpPath[PATH_MAX];

    // FAM initialize
    // Open Preloaded region
    region = spmv_fam_initialize();
    strcpy(configFilename, argv[1]);
    spmv_read_config_data(configFilename, matRowCount, numSegments, inpPath);
    cout << "Matrix : " << matRowCount << " X " << matRowCount << endl;
    cout << "Num Segments : " << numSegments << endl;
    cout << "Data Path : " << inpPath << endl;

    gresult = (float *)malloc(sizeof(float) * matRowCount);
    if (gresult == NULL) {
        cout << "gresult alloc failed" << endl;
        exit(1);
    }
    // Read input vector
    gvec = (int64_t *)spmv_fam_read("vector", 0, sizeof(int64_t) * matRowCount);
    if (gvec == NULL) {
        cout << "Reading vector data failed" << endl;
        exit(1);
    }

    for (int i = 0; i < numSegments; i++) {
        uint64_t size;
        int ret;
        char dataitemName[PATH_MAX];
        int64_t nlocalverts;
	uint64_t rowPerSeg; 
	uint64_t offset; 

        // Load row
        sprintf(dataitemName, "/rowptr%d.bin", i);
        size = (MAX_ROW_COUNT + 5) * sizeof(int);
        grow_off = (int *)spmv_fam_read(dataitemName, 0, size);
        if (grow_off == NULL) {
            cout << "Reading row data failed" << endl;
            exit(1);
        }
        nlocalverts = *((int64_t *)grow_off);
        grow_off = (int *)((uint64_t)grow_off + sizeof(int64_t));
        //nlocaledges = *((int64_t *)grow_off);
        grow_off = (int *)((uint64_t)grow_off + sizeof(int64_t));

        // Load column
        sprintf(dataitemName, "/column%d.bin", i);
        size = (MAX_NZ_COUNT) * sizeof(int64_t);
        gcol = (int64_t *)spmv_fam_read(dataitemName, 0, size);
        if (gcol == NULL) {
            cout << "Reading column data failed" << endl;
            exit(1);
        }
        // Load weight
        sprintf(dataitemName, "/weight%d.bin", i);
        size = (MAX_NZ_COUNT) * sizeof(int64_t);
        gval = (float *)spmv_fam_read(dataitemName, 0, size);
        if (gval == NULL) {
            cout << "Reading weight data failed" << endl;
            exit(1);
        }
	rowPerSeg = nlocalverts-1;
	offset = (i * rowPerSeg)*sizeof(float);
        // Multiply
        spmv_compute(i * rowPerSeg, 0, nlocalverts);
        // Write output vector
        ret = spmv_fam_write("result", (char *)gresult+offset, offset,
                             sizeof(float) * rowPerSeg);
        if (ret < 0) {
            cout << "fam write failed to update result" << endl;
        }
    }
    cout << "Output written to fam_ovec.bin" << endl;
    if (spmv_store_dataitem_to_file("fam_ovec.bin", "result", region,
                                    sizeof(float) * matRowCount)) {
        cout << "Writing output vector to file failed" << endl;
    }
    my_fam->fam_finalize("default");
    delete my_fam;
    return 0;
}
