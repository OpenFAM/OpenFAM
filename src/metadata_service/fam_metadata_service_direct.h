/*
 *   fam_metadata_service_direct.h
 *   Copyright (c) 2019-2020,2023 Hewlett Packard Enterprise Development, LP.
 * All rights reserved. Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the following conditions
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

#ifndef FAM_METADATA_SERVICE_DIRECT_H
#define FAM_METADATA_SERVICE_DIRECT_H

#include <iostream>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

#include "radixtree/kvs.h"
#include "radixtree/radix_tree.h"

#include "common/fam_config_info.h"
#include "fam_metadata_service.h"
#include "nvmm/epoch_manager.h"
#include "nvmm/fam.h"
#include "nvmm/memory_manager.h"
#define MIN_HEAP_SIZE (64 * (1UL << 20))

#define MAX_DATAITEM_NUM_HINT 1000000
#define MAX_INTERLEAVE_SIZE 1073741824

using namespace radixtree;
using namespace nvmm;
using namespace std;

namespace metadata {

class Fam_Metadata_Service_Direct : public Fam_Metadata_Service {
  public:
    void Start(bool use_meta_reg, bool enable_region_spanning,
               size_t region_span_size_per_memoryserver,
               size_t dataitem_span_size_per_memoryserver,
               const char *metadata_path, bool use_fam_path);
    void Stop();

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
    configFileParams get_config_info(std::string filename);
    uint64_t align_to_address(uint64_t size, int multiple);
    void metadata_update_memoryserver(
        int nmemServersPersistent,
        std::vector<uint64_t> memsrv_persistent_id_list,
        int nmemServersVolatile, std::vector<uint64_t> memsrv_volatile_id_list);
    void metadata_validate_and_create_region(
        const std::string regionname, size_t size, uint64_t *regionid,
        Fam_Region_Attributes *regionAttributes,
        std::list<int> *memory_server_list, int user_policy);
    void
    metadata_validate_and_destroy_region(const uint64_t regionId, uint32_t uid,
                                         uint32_t gid,
                                         std::list<int> *memory_server_list);
    void metadata_validate_and_allocate_dataitem(
        const std::string dataitemName, const uint64_t regionId, uint32_t uid,
        uint32_t gid, size_t size, std::list<int> *memory_server_list,
        size_t *interleaveSize, Fam_Permission_Level *permissionLevel,
        mode_t *regionPermission, int user_policy);

    void metadata_validate_and_deallocate_dataitem(
        const uint64_t regionId, const uint64_t dataitemId, uint32_t uid,
        uint32_t gid, Fam_DataItem_Metadata &dataitem);
    void metadata_find_region_and_check_permissions(
        metadata_region_item_op_t op, const uint64_t regionId, uint32_t uid,
        uint32_t gid, Fam_Region_Metadata &region);

    void metadata_find_region_and_check_permissions(
        metadata_region_item_op_t op, const std::string regionName,
        uint32_t uid, uint32_t gid, Fam_Region_Metadata &region);

    void metadata_find_dataitem_and_check_permissions(
        metadata_region_item_op_t op, const uint64_t dataitemId,
        const uint64_t regionId, uint32_t uid, uint32_t gid,
        Fam_DataItem_Metadata &dataitem);

    void metadata_find_dataitem_and_check_permissions(
        metadata_region_item_op_t op, const std::string dataitemName,
        const std::string regionName, uint32_t uid, uint32_t gid,
        Fam_DataItem_Metadata &dataitem);

    std::list<int> get_memory_server_list(uint64_t regionId);

    Fam_Metadata_Service_Direct(bool use_fam_path, bool use_meta_reg = 0);
    void metadata_reset_bitmap(uint64_t regionID);
    ~Fam_Metadata_Service_Direct();
    // get and set controlpath address definitions
    void set_controlpath_addr(string addr);
    string get_controlpath_addr();

    // get and set get_rpc_framework_type and protocol definitions
    void set_rpc_framework_protocol(configFileParams file_options);
    string get_rpc_framework_type();
    string get_rpc_protocol_type();

  private:
    class Impl_;
    Impl_ *pimpl_;
    string controlpath_addr, rpc_framework_type, rpc_protocol_type;
    size_t controlpath_FabricAddrSize;
    void *controlpath_FabricAddr;
};

} // namespace metadata
#endif
