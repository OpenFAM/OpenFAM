/*
 * fam_dump_dataitem.cpp
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
char *regionName;
Fam_Region_Descriptor *spmv_fam_initialize(const char *regionName) {
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
        region = my_fam->fam_lookup_region(regionName);
    } catch (...) {
        // ignore
        cout << "fam lookup failed for region :" << regionName << endl;
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
        dataitem = my_fam->fam_lookup(dataitemName, regionName);
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
        dataitem = my_fam->fam_lookup(dataitemName, regionName);
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
    // ./fam_dump_dataitem <filename> <dataitem name> <region name> <size>

    if (argc != 5) {
        cout << "Invalid Options !!!" << endl;
        cout << "    usage : " << endl;
        cout << "    ./fam_dump_dataitem <filename> <dataitem name> <region "
                "name> <size>"
             << endl;
        cout << endl;
        exit(1);
    }
    regionName = (char *)argv[3];
    // FAM initialize
    // Open Preloaded region
    region = spmv_fam_initialize((const char *)argv[3]);
    if (spmv_store_dataitem_to_file((const char *)argv[1],
                                    (const char *)argv[2], region,
                                    atol(argv[4]))) {
        cout << "Writing output vector to file failed" << endl;
    }
    my_fam->fam_finalize("default");
    delete my_fam;
    return 0;
}
