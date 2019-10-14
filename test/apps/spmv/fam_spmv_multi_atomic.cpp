/*
 * fam_spmv_multi_atomic.cpp
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
#include "common/fam_test_config.h"
#include "fam_spmv.h"
#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <iostream>

using namespace std;
using namespace openfam;

#define fam_stream cout << "PE(" << myPE << "/" << numPEs << ") "
// globals
fam *my_fam;
int myPE, numPEs;

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
    } catch (Fam_Exception &e) {
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
    } catch (...) {
        // ignore
        fam_stream << "fam lookup failed for region :" << REGION_NAME << endl;
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
    int ret;
    try {
        ret = my_fam->fam_get_blocking(buf, dataitem, offset, size);
        if (ret < 0) {
            fam_stream << "fam_get failed" << endl;
            return -1;
        }
    } catch (...) {
        fam_stream << "fam get failed" << endl;
        return -1;
    }
    return 0;
}

Fam_Descriptor *spmv_get_dataitem(const char *dataitemName) {
    Fam_Descriptor *dataitem = NULL;
    // Lookup config dataitem
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

int spmv_fam_read(char *buf, const char *dataitemName, uint64_t offset,
                  uint64_t size) {
    Fam_Descriptor *dataitem = NULL;
    // Lookup config dataitem
    try {
        dataitem = my_fam->fam_lookup(dataitemName, REGION_NAME);
    } catch (...) {
        // ignore
        fam_stream << "fam lookup failed for dataitem :" << dataitemName
                   << endl;
        exit(1);
    }
    return spmv_fam_read(buf, dataitem, offset, size);
}

int spmv_fam_write(char *inpBuf, const char *dataitemName, uint64_t offset,
                   uint64_t size) {
    Fam_Descriptor *dataitem = NULL;
    int ret;

    // Lookup config dataitem
    try {
        dataitem = my_fam->fam_lookup(dataitemName, REGION_NAME);
    } catch (...) {
        // ignore
        fam_stream << "fam lookup failed for dataitem :" << dataitemName
                   << endl;
        return -1;
    }
    try {
        ret = my_fam->fam_put_blocking(inpBuf, dataitem, offset, size);
        if (ret < 0) {
            fam_stream << "fam_put failed" << endl;
            exit(1);
        }
    } catch (Fam_Exception &e) {
        fam_stream << "Exception caught" << endl;
        fam_stream << "Error msg: " << e.fam_error_msg() << endl;
        fam_stream << "Error: " << e.fam_error() << endl;
        return -1;
    }
    return 0;
}
int spmv_fam_write(char *inpBuf, Fam_Descriptor *dataitem, uint64_t offset,
                   uint64_t size) {
    int ret;

    try {
        ret = my_fam->fam_put_blocking(inpBuf, dataitem, offset, size);
        if (ret < 0) {
            fam_stream << "fam_put failed" << endl;
            exit(1);
        }
    } catch (Fam_Exception &e) {
        fam_stream << "Exception caught" << endl;
        fam_stream << "Error msg: " << e.fam_error_msg() << endl;
        fam_stream << "Error: " << e.fam_error() << endl;
        return -1;
    }
    return 0;
}

void spmv_read_config_data(char *configFilename, int &matRowCount,
                           int &numSegments, char *inpPath) {
    char *buf, *addr;
    Fam_Descriptor *dataitem = NULL;
    int ret;
    // Read Config file
    // matRowCount - Number of rows in the matrix
    // numSegments - number of file segments
    // inpPath - path of the input file segments

    // Lookup config dataitem
    try {
        dataitem = my_fam->fam_lookup(configFilename, REGION_NAME);
    } catch (...) {
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

void spmv_compute(int64_t rowStart, int64_t rowEnd) {
    int64_t offShift = grow_off[rowStart];
    for (int64_t i = rowStart; i < rowEnd; i++) {
        float res = 0;
        for (int k = grow_off[i]; k < grow_off[i + 1]; k++) {
            res += gval[k - offShift] * (float)gvec[gcol[k - offShift]];
        }
        gresult[i] = res;
    }
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

int spmv_get_next_row(Fam_Descriptor *item, uint64_t offset, uint32_t val) {
    int res;
    try {
        res = (int)my_fam->fam_fetch_add(item, 0, val);
    } catch (...) {
        cout << "fam_fetch_add failed" << endl;
        exit(1);
    }
    return res;
}

int main(int argc, char **argv) {
    // Fam_Region_Descriptor *region = NULL;
    char configFilename[32];
    int matRowCount;
    int numSegments;
    char inpPath[PATH_MAX];
    Fam_Descriptor *rowDataItem[MAX_SEGMENTS];
    Fam_Descriptor *colDataItem[MAX_SEGMENTS];
    Fam_Descriptor *valDataItem[MAX_SEGMENTS];
    Fam_Descriptor *headerDataItem;
    Fam_Descriptor *resultDataItem;
    int64_t nlocalverts[MAX_SEGMENTS];
    int growPerItr = 1024;
    // int64_t nlocaledges[MAX_SEGMENTS];
    int ret;

    // FAM initialize
    // Open Preloaded region
    // region = spmv_fam_initialize();
    (void)spmv_fam_initialize();
    if (argc != 3) {
        if (myPE == 0) {
            cout << "usage" << endl;
            cout << argv[0] << "config name <rows per iter>" << endl;
        }
        exit(1);
    }
    strcpy(configFilename, argv[1]);
    spmv_read_config_data(configFilename, matRowCount, numSegments, inpPath);
    if (myPE == 0) {
        fam_stream << "Matrix : " << matRowCount << " X " << matRowCount
                   << endl;
        fam_stream << "Num Segments : " << numSegments << endl;
        fam_stream << "Data Path : " << inpPath << endl;
    }

    growPerItr = atoi(argv[2]);
    resultDataItem = spmv_get_dataitem("result");
    gresult = (float *)spmv_malloc(sizeof(float) * matRowCount);
    if (gresult == NULL) {
        fam_stream << "gresult alloc failed" << endl;
        exit(1);
    }

    // Read input vector
    if (myPE == 0)
        fam_stream << "Reading input  vector" << endl;

    gvec = (int64_t *)spmv_malloc(sizeof(int64_t) * matRowCount);
    ret =
        spmv_fam_read((char *)gvec, "vector", 0, sizeof(int64_t) * matRowCount);
    if (ret < 0) {
        fam_stream << "Reading vector data failed" << endl;
        exit(1);
    }

    // Initialize row, coloumn and weight dataitems
    for (int i = 0; i < numSegments; i = i + 1) {
        char dataitemName[PATH_MAX];

        sprintf(dataitemName, "/rowptr%d.bin", i);
        rowDataItem[i] = spmv_get_dataitem(dataitemName);
        sprintf(dataitemName, "/column%d.bin", i);
        colDataItem[i] = spmv_get_dataitem(dataitemName);
        sprintf(dataitemName, "/weight%d.bin", i);
        valDataItem[i] = spmv_get_dataitem(dataitemName);
    }
    {
        uint64_t size;
        // Allocate local buffers for row, col, val
        size = (MAX_ROW_COUNT + 5) * sizeof(int);
        grow_off = (int *)spmv_malloc(size);
        size = (MAX_NZ_COUNT) * sizeof(int64_t);
        gcol = (int64_t *)spmv_malloc(size);
        size = (MAX_NZ_COUNT) * sizeof(int64_t);
        gval = (float *)spmv_malloc(size);
    }
    for (int i = 0; i < numSegments; i++) {
        int64_t buf[2] = {0};

        // Load segment header
        ret = spmv_fam_read((char *)&buf, rowDataItem[i], 0, sizeof(buf));
        if (ret < 0) {
            fam_stream << "Reading row data failed" << endl;
            exit(1);
        }
        nlocalverts[i] = buf[0];
    }
    headerDataItem = spmv_get_dataitem("Global_header");
    if (myPE == 0) {
        char buf[1024] = {0};
        int ret = spmv_fam_write((char *)&buf, headerDataItem, 0, 1024);
        if (ret < 0) {
            fam_stream << "fam write failed to update header" << endl;
            exit(1);
        }
        fam_stream << "Global header initialized !!!" << endl;
    }
    my_fam->fam_barrier_all();
    int gRowCorrection = 0;
    int gRowsPerSeg = (int)nlocalverts[0] - 1;
    int grow;
    while ((grow = spmv_get_next_row(headerDataItem, 0, growPerItr)) <
           matRowCount) {
        int growEnd = grow + growPerItr;
        if (growEnd > matRowCount)
            growEnd = matRowCount;

        for (; grow < (growEnd); grow = grow + growPerItr - gRowCorrection) {
            int ret;
            uint64_t rowOff = (2 * sizeof(int64_t));
            uint64_t colOff = 0;
            uint64_t valOff = 0;
            uint64_t rowSize;
            uint64_t colSize;
            uint64_t valSize;
            int gtotalRows;
            int gSegRowIdx;
            int segIdx;
            int colEntries;

            gRowCorrection = 0;
            segIdx = grow / (gRowsPerSeg + 1);
            gSegRowIdx = grow % (gRowsPerSeg + 1);
            rowOff += (gSegRowIdx) * sizeof(int);
            ret = spmv_fam_read((char *)&colEntries, rowDataItem[segIdx],
                                rowOff, sizeof(int));
            if (ret < 0) {
                fam_stream << "Reading  &colEntriesdata failed" << endl;
                exit(1);
            }
            colOff = colEntries * sizeof(int64_t);
            valOff = colEntries * sizeof(float);
            gtotalRows = growPerItr;
            if (gSegRowIdx + gtotalRows > nlocalverts[segIdx]) {
                int totalRows = (int)nlocalverts[segIdx] - gSegRowIdx;
                gRowCorrection = gtotalRows - totalRows;
                gtotalRows -= gRowCorrection;
            }
            {
                int entries;

                if ((gSegRowIdx + gtotalRows) >= nlocalverts[segIdx]) {
                    gtotalRows = (int)nlocalverts[segIdx] - gSegRowIdx - 1;
                }
                // Load row
                rowSize = (gtotalRows + 1) * sizeof(int);
                ret = spmv_fam_read((char *)grow_off, rowDataItem[segIdx],
                                    rowOff, rowSize);
                if (ret < 0) {
                    fam_stream << "Reading row data failed" << endl;
                    exit(1);
                }
                entries = grow_off[gtotalRows] - grow_off[0];
                if (!entries) {
                    continue;
                }

                // Load column
                colSize = entries * sizeof(int64_t);
                ret = spmv_fam_read((char *)gcol, colDataItem[segIdx], colOff,
                                    colSize);
                if (ret < 0) {
                    fam_stream << "Reading column data failed" << endl;
                    exit(1);
                }

                // Load weight
                valSize = entries * sizeof(float);
                ret = spmv_fam_read((char *)gval, valDataItem[segIdx], valOff,
                                    valSize);
                if (ret < 0) {
                    fam_stream << "Reading weight data failed" << endl;
                    exit(1);
                }

                // Multiply
                spmv_compute(0, gtotalRows);

                // Write output vector
                ret = spmv_fam_write((char *)gresult, resultDataItem,
                                     ((segIdx * gRowsPerSeg) + gSegRowIdx) *
                                         sizeof(float),
                                     sizeof(float) * gtotalRows);
                if (ret < 0) {
                    fam_stream << "fam write failed to update result" << endl;
                }
            }
        }
    }
    my_fam->fam_barrier_all();
    if (myPE == 0)
        fam_stream << "Completed !!!" << endl;
    free(grow_off);
    free(gcol);
    free(gval);
    my_fam->fam_finalize("default");
    delete my_fam;
    return 0;
}
