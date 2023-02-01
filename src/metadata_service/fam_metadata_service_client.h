/*
 *   fam_metadata_service_client.h
 *   Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All rights
 *   reserved. Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following conditions
 *   are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
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

#ifndef FAM_METADATA_SERVICE_CLIENT_H
#define FAM_METADATA_SERVICE_CLIENT_H

#include <grpc/impl/codegen/log.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <map>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

#include "fam_metadata_rpc.grpc.pb.h"
#include "fam_metadata_service.h"

using namespace std;

namespace metadata {

class Fam_Metadata_Service_Client : public Fam_Metadata_Service {
  public:
    void reset_profile();
    void dump_profile();

    void metadata_insert_region(const uint64_t regionId,
                                const std::string regionName,
                                Fam_Region_Metadata *region);

    void metadata_delete_region(const uint64_t regionId);
    void metadata_delete_region(const std::string regionName);

    bool metadata_find_region(const uint64_t regionId,
                              Fam_Region_Metadata &region);
    bool metadata_find_region(const std::string regionName,
                              Fam_Region_Metadata &region);

    void metadata_modify_region(const uint64_t regionID,
                                Fam_Region_Metadata *region);
    void metadata_modify_region(const std::string regionName,
                                Fam_Region_Metadata *region);

    void metadata_insert_dataitem(const uint64_t dataitemId,
                                  const uint64_t regionId,
                                  Fam_DataItem_Metadata *dataitem,
                                  std::string dataitemName = "");

    void metadata_insert_dataitem(const uint64_t dataitemId,
                                  const std::string regionName,
                                  Fam_DataItem_Metadata *dataitem,
                                  std::string dataitemName = "");

    void metadata_delete_dataitem(const uint64_t dataitemId,
                                  const std::string regionName);
    void metadata_delete_dataitem(const uint64_t dataitemId,
                                  const uint64_t regionId);
    void metadata_delete_dataitem(const std::string dataitemName,
                                  const std::string regionName);
    void metadata_delete_dataitem(const std::string dataitemName,
                                  const uint64_t regionId);

    bool metadata_find_dataitem(const uint64_t dataitemId,
                                const uint64_t regionId,
                                Fam_DataItem_Metadata &dataitem);
    bool metadata_find_dataitem(const uint64_t dataitemId,
                                const std::string regionName,
                                Fam_DataItem_Metadata &dataitem);
    bool metadata_find_dataitem(const std::string dataitemName,
                                const uint64_t regionId,
                                Fam_DataItem_Metadata &dataitem);
    bool metadata_find_dataitem(const std::string dataitemName,
                                const std::string regionName,
                                Fam_DataItem_Metadata &dataitem);

    void metadata_modify_dataitem(const uint64_t dataitemId,
                                  const uint64_t regionId,
                                  Fam_DataItem_Metadata *dataitem);
    void metadata_modify_dataitem(const uint64_t dataitemId,
                                  const std::string regionName,
                                  Fam_DataItem_Metadata *dataitem);
    void metadata_modify_dataitem(const std::string dataitemName,
                                  const uint64_t regionId,
                                  Fam_DataItem_Metadata *dataitem);
    void metadata_modify_dataitem(const std::string dataitemName,
                                  const std::string regionName,
                                  Fam_DataItem_Metadata *dataitem);

    bool metadata_check_permissions(Fam_DataItem_Metadata *dataitem,
                                    metadata_region_item_op_t op, uint32_t uid,
                                    uint32_t gid);

    bool metadata_check_permissions(Fam_Region_Metadata *region,
                                    metadata_region_item_op_t op, uint32_t uid,
                                    uint32_t gid);
    size_t metadata_maxkeylen();
    void metadata_update_memoryserver(
        int nmemServersPersistent,
        std::vector<uint64_t> memsrv_persistent_id_list,
        int nmemServersVolatile, std::vector<uint64_t> memsrv_volatile_id_list);
    void metadata_reset_bitmap(uint64_t regionId);
    void metadata_validate_and_create_region(
        const std::string regionname, size_t size, uint64_t *regionid,
        Fam_Region_Attributes *regionAttributes,
        std::list<int> *memory_server_list, int user_policy);

    void
    metadata_validate_and_destroy_region(const uint64_t regionId, uint32_t uid,
                                         uint32_t gid,
                                         std::list<int> *memory_server_list);
    void metadata_validate_and_deallocate_dataitem(
        const uint64_t regionId, const uint64_t dataitemId, uint32_t uid,
        uint32_t gid, Fam_DataItem_Metadata &dataitem);
    void metadata_validate_and_allocate_dataitem(
        const std::string dataitemName, const uint64_t regionId, uint32_t uid,
        uint32_t gid, size_t size, std::list<int> *memory_server_list,
        size_t *interleaveSize, int user_policy = 0);

    void metadata_find_region_and_check_permissions(
        metadata_region_item_op_t op, const uint64_t regionId, uint32_t uid,
        uint32_t gid, Fam_Region_Metadata &region);

    void metadata_find_dataitem_and_check_permissions(
        metadata_region_item_op_t op, const uint64_t dataitemId,
        const uint64_t regionId, uint32_t uid, uint32_t gid,
        Fam_DataItem_Metadata &dataitem);

    void metadata_find_region_and_check_permissions(
        metadata_region_item_op_t op, const std::string regionName,
        uint32_t uid, uint32_t gid, Fam_Region_Metadata &region);

    void metadata_find_dataitem_and_check_permissions(
        metadata_region_item_op_t op, const std::string dataitemName,
        const std::string regionName, uint32_t uid, uint32_t gid,
        Fam_DataItem_Metadata &dataitem);

    std::list<int> get_memory_server_list(uint64_t region);
    Fam_Metadata_Service_Client(const char *name, uint64_t port);
    ~Fam_Metadata_Service_Client();

  private:
    uint32_t uid;
    uint32_t gid;
    std::unique_ptr<Fam_Metadata_Rpc::Stub> stub;
};

} // namespace metadata
#endif
