/*
 *   fam_metadata_rpc_test.cpp
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

#include "../../src/metadata/fam_metadata_service.h"
#include "common/fam_test_config.h"
#include "metadata/fam_metadata_service_client.h"

#include <fam/fam.h>
#include <string.h>
#include <unistd.h>

using namespace famradixtree;
using namespace nvmm;
using namespace metadata;
using namespace openfam;

int main(int argc, char *argv[]) {

    Fam_Metadata_Service *manager =
        new Fam_Metadata_Service_Client(METADATA_SERVER, METADATA_RPC_PORT);

    int ret;
    uint64_t count = 0, fail = 0;

    fam *my_fam = new fam();
    Fam_Options fam_opts;

    init_fam_options(&fam_opts);
    fam_opts.runtime = strdup("NONE");

    try {
        my_fam->fam_initialize("default", &fam_opts);
    } catch (Fam_Exception &e) {
        cout << "fam initialization failed" << endl;
        exit(1);
    }

    Fam_Region_Descriptor *desc[2];
    Fam_Region_Metadata node;
    Fam_Region_Metadata *regnode = new Fam_Region_Metadata();

    Fam_DataItem_Metadata dinode;
    Fam_DataItem_Metadata *datanode = new Fam_DataItem_Metadata();
    memset(regnode, 0, sizeof(Fam_Region_Metadata));
    memset(datanode, 0, sizeof(Fam_DataItem_Metadata));

    uint64_t i = 0, j = 5000;

    for (i = 0; i < 2; i++) {

        string name;
        uint64_t regionId;

        desc[i] = my_fam->fam_create_region(to_string(i).c_str(),
                                            16 * 1024 * 1024, 0744, RAID1);
        if (desc[i] == NULL) {
            cout << "fam create region failed" << endl;
            exit(1);
        }

        if (!manager->metadata_find_region(to_string(i), node)) {
            printf("Region lookup failed: reg name=%s, ret=%d\n", name.c_str(),
                   ret);
            fail++;
        }
        count++;

        name = node.name;
        regionId = node.regionId;
        memcpy(regnode, &node, sizeof(Fam_Region_Metadata));

        try {
            manager->metadata_insert_region(regionId, name.c_str(), regnode);
        } catch (Fam_Exception &e) {
            cout << "Error : " << e.fam_error() << endl;
            cout << "Error msg : " << e.fam_error_msg() << endl;
        }
        count++;
        if (!manager->metadata_find_region(regionId, node)) {
            printf("Region lookup failed: regid=%lu, ret=%d\n", regionId, ret);
            fail++;
        }
        count++;
        if (!manager->metadata_find_region(to_string(i), node)) {
            printf("Region lookup failed: reg name=%s, ret=%d\n", name.c_str(),
                   ret);
            fail++;
        }
        count++;

        try {
            manager->metadata_modify_region(to_string(i), regnode);
        } catch (Fam_Exception &e) {
            cout << "Error : " << e.fam_error() << endl;
            cout << "Error msg : " << e.fam_error_msg() << endl;
            fail++;
        }
        count++;
        try {
            manager->metadata_modify_region(regionId, regnode);
        } catch (Fam_Exception &e) {
            cout << "Error : " << e.fam_error() << endl;
            cout << "Error msg : " << e.fam_error_msg() << endl;
            fail++;
        }
        count++;

        try {
            manager->metadata_insert_region(regionId, name.c_str(), regnode);
        } catch (Fam_Exception &e) {
            cout << "Error : " << e.fam_error() << endl;
            cout << "Error msg : " << e.fam_error_msg() << endl;
        }
        count++;
        try {
            manager->metadata_delete_region(regionId);
        } catch (Fam_Exception &e) {
            cout << "Error : " << e.fam_error() << endl;
            cout << "Error msg : " << e.fam_error_msg() << endl;
            fail++;
        }
        count++;
        try {
            manager->metadata_modify_region(regionId, regnode);
        } catch (Fam_Exception &e) {
            cout << "Error : " << e.fam_error() << endl;
            cout << "Error msg : " << e.fam_error_msg() << endl;
        }
        count++;
        try {
            manager->metadata_insert_region(regionId, name.c_str(), regnode);
        } catch (Fam_Exception &e) {
            cout << "Error : " << e.fam_error() << endl;
            cout << "Error msg : " << e.fam_error_msg() << endl;
            fail++;
        }
        count++;
        try {
            manager->metadata_delete_region(name.c_str());
        } catch (Fam_Exception &e) {
            cout << "Error : " << e.fam_error() << endl;
            cout << "Error msg : " << e.fam_error_msg() << endl;
            fail++;
        }
        count++;
        try {
            manager->metadata_insert_region(regionId, name.c_str(), regnode);
        } catch (Fam_Exception &e) {
            cout << "Error : " << e.fam_error() << endl;
            cout << "Error msg : " << e.fam_error_msg() << endl;
            fail++;
        }
        count++;

        memset(datanode, 0, sizeof(Fam_DataItem_Metadata));
        for (j = 5000; j < 5002; j++) {
            try {
                manager->metadata_insert_dataitem(j, regionId, datanode);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_delete_dataitem(j, regionId);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;

            try {
                manager->metadata_insert_dataitem(j, regionId, datanode);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_insert_dataitem(j, regionId, datanode);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
            }
            count++;
            try {
                manager->metadata_delete_dataitem(j, regionId);
            } catch (Fam_Exception &e) {
                cout << "delete dataitem" << endl;
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            if (!manager->metadata_find_dataitem(j, regionId, dinode)) {
                printf("Dataitem lookup failed: id=%lu:%lu , ret=%d\n",
                       regionId, j, ret);
            }
            count++;
            try {
                manager->metadata_modify_dataitem(j, regionId, datanode);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
            }
            count++;
            try {
                manager->metadata_insert_dataitem(j, regionId, datanode);
            } catch (Fam_Exception &e) {
                cout << "insert dataitem" << endl;
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            if (!manager->metadata_find_dataitem(j, regionId, dinode)) {
                printf("Dataitem find failed: id=%lu:%lu , ret=%d\n", regionId,
                       j, ret);
                cout << "find dataitem failed" << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_modify_dataitem(j, regionId, datanode);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_insert_region(regionId, name.c_str(),
                                                regnode);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
            }
            count++;
            try {
                manager->metadata_delete_region(regionId);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_insert_dataitem(j, regionId, datanode);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
            }
            count++;
            try {
                manager->metadata_delete_dataitem(j, regionId);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
            }
            count++;
            try {
                manager->metadata_insert_region(regionId, name.c_str(),
                                                regnode);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }

            count++;
        }
        for (j = 5050; j < 5052; j++) {
            string dataname = to_string(j);
            strcpy(datanode->name, dataname.c_str());
            try {
                manager->metadata_insert_dataitem(j, regionId, datanode,
                                                  to_string(j));
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_insert_dataitem(j, regionId, datanode);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
            }
            count++;
            try {
                manager->metadata_delete_dataitem(to_string(j), regionId);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_modify_dataitem(to_string(j), regionId,
                                                  datanode);
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
            }
            count++;
            try {
                manager->metadata_insert_dataitem(j, to_string(i), datanode,
                                                  to_string(j));
            } catch (Fam_Exception &e) {
                cout << "insert dataitem failed---" << endl;
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            if (!manager->metadata_find_dataitem(j, to_string(i), dinode)) {
                printf("Dataitem lookup failed: id=%lu:%lu , ret=%d\n",
                       regionId, j, ret);
                fail++;
            }
            count++;

            if (!manager->metadata_find_dataitem(to_string(j), regionId,
                                                 dinode)) {
                printf("Dataitem lookup failed: id=%lu:%lu , ret=%d\n",
                       regionId, j, ret);
                fail++;
            }
            count++;

            if (!manager->metadata_find_dataitem(to_string(j), to_string(i),
                                                 dinode)) {
                printf("Dataitem lookup failed: id=%lu:%lu , ret=%d\n",
                       regionId, j, ret);
                fail++;
            }
            count++;
            try {
                manager->metadata_modify_dataitem(to_string(j), regionId,
                                                  datanode);
            } catch (Fam_Exception &e) {
                cout << "Failed to modify_dataitemi 1" << endl;
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_modify_dataitem(to_string(j), to_string(i),
                                                  datanode);
            } catch (Fam_Exception &e) {
                cout << "Failed to modify_dataitem 2" << endl;
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_modify_dataitem(j, to_string(i), datanode);
            } catch (Fam_Exception &e) {
                cout << "Failed to modify_dataitem 3" << endl;
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_delete_dataitem(j, to_string(i));
            } catch (Fam_Exception &e) {
                cout << "Failed to delete_dataitem" << endl;
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_insert_dataitem(j, regionId, datanode,
                                                  to_string(j));
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
            count++;
            try {
                manager->metadata_delete_dataitem(to_string(j), to_string(i));
            } catch (Fam_Exception &e) {
                cout << "Error : " << e.fam_error() << endl;
                cout << "Error msg : " << e.fam_error_msg() << endl;
                fail++;
            }
        }
    }
    for (i = 0; i < 2; i++) {
        my_fam->fam_destroy_region(desc[i]);
    }

    delete datanode;
    delete regnode;

    if (fail == 0)
        std::cout << "fam_metadata_test test passed:" << std::endl;
    else
        std::cout << "fam_metadata_test test failed:" << std::endl;

    std::cout << "Total tests run    : " << count << std::endl;
    std::cout << "Total tests passed : " << count - fail << std::endl;
    std::cout << "Total tests failed : " << fail << std::endl;

    if (fail) {
        return 1;
    } else
        return 0;
}
