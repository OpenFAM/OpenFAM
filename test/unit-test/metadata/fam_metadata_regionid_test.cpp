/*
 *   fam_metadata_manager.cpp
 *   Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All
 *   rights reserved.
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *   1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the name of the copyright holder nor the names of its
 *      contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 *      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 *      IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *      BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *      FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 *      SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *      INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *      DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *      OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *      INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *      CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *      OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *      IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * See https://spdx.org/licenses/BSD-3-Clause
 *
 */

#include "../../src/metadata/fam_metadata_manager.h"
#include "common/fam_test_config.h"

#include <fam/fam.h>
#include <string.h>
#include <unistd.h>

using namespace famradixtree;
using namespace nvmm;
using namespace metadata;
using namespace openfam;

int main(int argc, char *argv[]) {

    FAM_Metadata_Manager *manager = FAM_Metadata_Manager::GetInstance(true);

    uint64_t fail = 0;

    fam *my_fam = new fam();
    Fam_Options fam_opts;

    memset((void *)&fam_opts, 0, sizeof(Fam_Options));

    fam_opts.memoryServer = strdup("0:127.0.0.1");
    fam_opts.libfabricPort = strdup(TEST_LIBFABRIC_PORT);
    fam_opts.grpcPort = strdup(TEST_GRPC_PORT);
    fam_opts.allocator = strdup(TEST_ALLOCATOR);
    fam_opts.runtime = strdup("NONE");
    if (my_fam->fam_initialize("default", &fam_opts) < 0) {
        cout << "fam initialization failed" << endl;
        exit(1);
    } else {
        cout << "fam initialization successful" << endl;
    }

    uint64_t regionID[100] = {0};
    for (int i = 0; i < 100; i++) {
        int status = manager->metadata_get_RegionID(&regionID[i]);
        std::cout << "Region id :" << regionID[i] << " status : " << status
                  << std::endl;
        if (status != META_NO_ERROR)
            fail++;
    }
    for (int i = 0; i < 100; i++) {
        int status = manager->metadata_reset_RegionID(regionID[i]);
        std::cout << "Region id :" << regionID[i] << " status : " << status
                  << std::endl;
        if (status != META_NO_ERROR)
            fail++;
    }

    if (fail == 0)
        std::cout << "fam_metadata_test test passed" << std::endl;
    else
        std::cout << "fam_metadata_test test failed:" << fail << std::endl;

    if (fail) {
        return 1;
    } else
        return 0;
}
