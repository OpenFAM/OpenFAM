/*
 * fam_cis_direct.h
 * Copyright (c) 2020-2021,2023 Hewlett Packard Enterprise Development, LP. All
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
#ifndef FAM_CIS_DIRECT_H_
#define FAM_CIS_DIRECT_H_

#include <string>
#include <sys/types.h>
#include <unordered_map>

#include "cis/fam_cis.h"
#include "common/fam_config_info.h"
#include "memory_service/fam_memory_service.h"
#include "memory_service/fam_memory_service_client.h"
#include "memory_service/fam_memory_service_direct.h"
#include "metadata_service/fam_metadata_service.h"
#include "metadata_service/fam_metadata_service_client.h"
#include "metadata_service/fam_metadata_service_direct.h"

#ifdef USE_THALLIUM
#include "memory_service/fam_memory_service_thallium_client.h"
#include "metadata_service/fam_metadata_service_thallium_client.h"
#include <thallium.hpp>
namespace tl = thallium;
#endif

using namespace metadata;

namespace openfam {

class Fam_CIS_Direct : public Fam_CIS {
  public:
    Fam_CIS_Direct(char *cisName, bool useAsyncCopy_ = false,
                   bool isSharedMemory = false);

    ~Fam_CIS_Direct();

    void reset_profile();

    void dump_profile();

    Fam_Memory_Service *get_memory_service(uint64_t memoryServerId);

    Fam_Metadata_Service *get_metadata_service(uint64_t metadataServerId);

    uint64_t get_num_memory_servers();
    Fam_Region_Item_Info create_region(string name, size_t nbytes,
                                       mode_t permission,
                                       Fam_Region_Attributes *regionAttributes,
                                       uint32_t uid, uint32_t gid);
    void destroy_region(uint64_t regionId, uint64_t memoryServerId,
                        uint32_t uid, uint32_t gid);
    void resize_region(uint64_t regionId, size_t nbytes,
                       uint64_t memoryServerId, uint32_t uid, uint32_t gid);

    Fam_Region_Item_Info allocate(string name, size_t nbytes, mode_t permission,
                                  uint64_t regionId, uint64_t memoryServerId,
                                  uint32_t uid, uint32_t gid);

    void deallocate(uint64_t regionId, uint64_t offset, uint64_t memoryServerId,
                    uint32_t uid, uint32_t gid);

    void change_region_permission(uint64_t regionId, mode_t permission,
                                  uint64_t memoryServerId, uint32_t uid,
                                  uint32_t gid);

    void change_dataitem_permission(uint64_t regionId, uint64_t offset,
                                    mode_t permission, uint64_t memoryServerId,
                                    uint32_t uid, uint32_t gid);
    bool check_region_permission(Fam_Region_Metadata region, bool op,
                                 Fam_Metadata_Service *metadataService,
                                 uint32_t uid, uint32_t gid);
    bool check_dataitem_permission(Fam_DataItem_Metadata dataitem, bool op,
                                   Fam_Metadata_Service *metadataService,
                                   uint32_t uid, uint32_t gid);
    Fam_Region_Item_Info lookup_region(string name, uint32_t uid, uint32_t gid);

    Fam_Region_Item_Info lookup(string itemName, string regionName,
                                uint32_t uid, uint32_t gid);

    Fam_Region_Item_Info
    check_permission_get_region_info(uint64_t regionId, uint64_t memoryServerId,
                                     uint32_t uid, uint32_t gid);

    Fam_Region_Item_Info check_permission_get_item_info(uint64_t regionId,
                                                        uint64_t offset,
                                                        uint64_t memoryServerId,
                                                        uint32_t uid,
                                                        uint32_t gid);

    Fam_Region_Item_Info get_stat_info(uint64_t regionId, uint64_t offset,
                                       uint64_t memoryServerId, uint32_t uid,
                                       uint32_t gid);

    void *copy(uint64_t srcRegionId, uint64_t srcOffset,
               uint64_t srcUsedMemsrvCnt, uint64_t srcCopyStart,
               uint64_t *srcKeys, uint64_t *srcBaseAddrList,
               uint64_t destRegionId, uint64_t destOffset,
               uint64_t destCopyStart, uint64_t size,
               uint64_t firstSrcMemserverId, uint64_t firstDestMemserverId,
               uint32_t uid, uint32_t gid);

    void wait_for_copy(void *waitObj);

    void *backup(uint64_t srcRegionId, uint64_t srcOffset,
                 uint64_t srcMemoryServerId, string BackupName, uint32_t uid,
                 uint32_t gid);
    void *restore(uint64_t destRegionId, uint64_t destOffset,
                  uint64_t destMemoryServerId, string BackupName, uint32_t uid,
                  uint32_t gid);
    string list_backup(std::string BackupName, uint64_t memoryServerId,
                       uint32_t uid, uint32_t gid);
    void *delete_backup(string BackupName, uint64_t memoryServerId,
                        uint32_t uid, uint32_t gid);
    Fam_Backup_Info get_backup_info(std::string BackupName,
                                    uint64_t memoryServerId, uint32_t uid,
                                    uint32_t gid);
    void wait_for_backup(void *waitObj);
    void wait_for_restore(void *waitObj);
    void wait_for_delete_backup(void *waitObj);
    void *fam_map(uint64_t regionId, uint64_t offset, uint64_t memoryServerId,
                  uint32_t uid, uint32_t gid);
    void fam_unmap(void *local, uint64_t regionId, uint64_t offset,
                   uint64_t memoryServerId, uint32_t uid, uint32_t gid);

    void acquire_CAS_lock(uint64_t offset, uint64_t memoryServerId);
    void release_CAS_lock(uint64_t offset, uint64_t memoryServerId);
    uint64_t get_dataitem_id(uint64_t offset, uint64_t memoryServerId);
    size_t get_addr_size(uint64_t memoryServerId);
    void get_addr(void *memServerFabricAddr, uint64_t memoryServerId);

    size_t get_memserverinfo_size();
    void get_memserverinfo(void *memServerInfoBuffer);

    configFileParams get_config_info(std::string filename);

    uint64_t align_to_address(uint64_t size, int multiple);
    bool validate_backup(Fam_Backup_Info info, uint32_t uid, uint32_t gid,
                         mode_t op);

    int get_atomic(uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset,
                   uint64_t nbytes, uint64_t key, uint64_t srcBaseAddr,
                   const char *nodeAddr, uint32_t nodeAddrSize,
                   uint64_t memoryServerId, uint32_t uid, uint32_t gid);

    int put_atomic(uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset,
                   uint64_t nbytes, uint64_t key, uint64_t srcBaseAddr,
                   const char *nodeAddr, uint32_t nodeAddrSize,
                   const char *data, uint64_t memoryServerId, uint32_t uid,
                   uint32_t gid);

    int scatter_strided_atomic(uint64_t regionId, uint64_t offset,
                               uint64_t nElements, uint64_t firstElement,
                               uint64_t stride, uint64_t elementSize,
                               uint64_t key, uint64_t srcBaseAddr,
                               const char *nodeAddr, uint32_t nodeAddrSize,
                               uint64_t memoryServerId, uint32_t uid,
                               uint32_t gid);

    int gather_strided_atomic(uint64_t regionId, uint64_t offset,
                              uint64_t nElements, uint64_t firstElement,
                              uint64_t stride, uint64_t elementSize,
                              uint64_t key, uint64_t srcBaseAddr,
                              const char *nodeAddr, uint32_t nodeAddrSize,
                              uint64_t memoryServerId, uint32_t uid,
                              uint32_t gid);

    int scatter_indexed_atomic(uint64_t regionId, uint64_t offset,
                               uint64_t nElements, const void *elementIndex,
                               uint64_t elementSize, uint64_t key,
                               uint64_t srcBaseAddr, const char *nodeAddr,
                               uint32_t nodeAddrSize, uint64_t memoryServerId,
                               uint32_t uid, uint32_t gid);

    int gather_indexed_atomic(uint64_t regionId, uint64_t offset,
                              uint64_t nElements, const void *elementIndex,
                              uint64_t elementSize, uint64_t key,
                              uint64_t srcBaseAddr, const char *nodeAddr,
                              uint32_t nodeAddrSize, uint64_t memoryServerId,
                              uint32_t uid, uint32_t gid);

    memoryServerMap *get_memory_service_map() { return memoryServers; }

    void open_region_with_registration(uint64_t regionId, uint32_t uid,
                                       uint32_t gid,
                                       std::vector<uint64_t> *memserverIds,
                                       Fam_Region_Memory_Map *regionMemoryMap);

    void open_region_without_registration(uint64_t regionId,
                                          std::vector<uint64_t> *memserverIds);

    void get_region_memory(uint64_t regionId, uint32_t uid, uint32_t gid,
                           Fam_Region_Memory_Map *regionMemoryMap);

    // get and set controlpath address definitions
    void set_controlpath_addr(string addr);
    string get_controlpath_addr();

    // get and set rpc_framework_type and protocol definitions
    void set_rpc_framework_protocol(configFileParams file_options);
    string get_rpc_framework_type();
    string get_rpc_protocol_type();

    void close_region(uint64_t regionId, std::vector<uint64_t> memserverIds);

  private:
    Fam_Async_QHandler *asyncQHandler;
    memoryServerMap *memoryServers;
    metadataServerMap *metadataServers;
    configFileParams file_options;
    uint64_t memoryServerCount;
    uint64_t memServerInfoSize;
    bool enableInterleaving;
    size_t interleaveSize;
    std::vector<std::tuple<uint64_t, size_t, void *> > *memServerInfoV;
    void *memServerInfoBuffer;
    bool useAsyncCopy;
    size_t metadataMaxKeyLen;
    string controlpath_addr, rpc_framework_type, rpc_protocol_type;
    Fam_CIS_Direct *direct_CIS;
    bool isSharedMemory;

    uint64_t generate_memory_server_id(const char *name) {
        std::uint64_t hashVal = std::hash<std::string> {}
        (name);
        return hashVal % memoryServerCount;
    }

    int create_region_failure_cleanup(
        std::vector<int> create_region_success_list,
        std::vector<Fam_Memory_Service *> memoryServiceList, uint64_t regionId);

    int allocate_failure_cleanup(
        std::vector<int> allocate_success_list,
        std::vector<Fam_Memory_Service *> memoryServiceList, uint64_t regionId,
        uint64_t *offsets);

    void register_region_memory(
        Fam_Region_Metadata region, Fam_Metadata_Service *metadataService,
        std::vector<Fam_Memory_Service *> regionMemoryServiceList, uint32_t uid,
        uint32_t gid);

    void register_dataitem_memory(
        Fam_DataItem_Metadata dataitem, Fam_Metadata_Service *metadataService,
        std::vector<Fam_Memory_Service *> memoryServiceList, uint32_t uid,
        uint32_t gid, Fam_Region_Item_Info *info);

    std::string rpc_type;
};

} // namespace openfam
#endif /* end of FAM_ALLOCATOR_H_ */
