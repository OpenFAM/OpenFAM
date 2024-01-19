/*
 * fam_cis_thallium_client.h
 * Copyright (c) 2023 Hewlett Packard Enterprise Development, LP. All
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
#ifndef FAM_CIS_THALLIUM_CLIENT_H_
#define FAM_CIS_THALLIUM_CLIENT_H_

#include <sys/types.h>

#include <iostream>
#include <map>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <unistd.h>

#include "cis/fam_cis.h"
#include "common/fam_config_info.h"

using namespace std;
namespace tl = thallium;

namespace openfam {

class Fam_CIS_Thallium_Client : public Fam_CIS {
  public:
    Fam_CIS_Thallium_Client(tl::engine engine_, const char *, uint64_t port);

    ~Fam_CIS_Thallium_Client();

    void reset_profile();

    void dump_profile();

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
    Fam_Backup_Info get_backup_info(std::string BackupName,
                                    uint64_t memoryServerId, uint32_t uid,
                                    uint32_t gid);
    string list_backup(std::string BackupName, uint64_t memoryServerId,
                       uint32_t uid, uint32_t gid);
    void *delete_backup(string BackupName, uint64_t memoryServerId,
                        uint32_t uid, uint32_t gid);

    void wait_for_backup(void *waitObj);
    void wait_for_restore(void *waitObj);
    void wait_for_delete_backup(void *waitObj);

    void *fam_map(uint64_t regionId, uint64_t offset, uint64_t memoryServerId,
                  uint32_t uid, uint32_t gid);
    void fam_unmap(void *local, uint64_t regionId, uint64_t offset,
                   uint64_t memoryServerId, uint32_t uid, uint32_t gid);

    void acquire_CAS_lock(uint64_t offset, uint64_t memoryServerId);
    void release_CAS_lock(uint64_t offset, uint64_t memoryServerId);

    size_t get_addr_size(uint64_t memoryServerId) { return 0; };
    void get_addr(void *memServerFabricAddr, uint64_t memoryServerId) {}
    size_t get_memserverinfo_size();
    void get_memserverinfo(void *memServerInfo);

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

    void get_region_memory(uint64_t regionId, uint32_t uid, uint32_t gid,
                           Fam_Region_Memory_Map *regionMemoryMap);

    void open_region_with_registration(uint64_t regionId, uint32_t uid,
                                       uint32_t gid,
                                       std::vector<uint64_t> *memserverIds,
                                       Fam_Region_Memory_Map *regionMemoryMap);

    void open_region_without_registration(uint64_t regionId,
                                          std::vector<uint64_t> *memserverIds);

    void close_region(uint64_t regionId, std::vector<uint64_t> memserverIds);

    // set and get controlpath address functions
    void set_controlpath_addr(string addr) {}

    string get_controlpath_addr() { return std::string(); }

  private:
    std::unique_ptr<Fam_CIS_Rpc::Stub> stub;
    tl::engine myEngine;
    tl::provider_handle ph;
    tl::remote_procedure rp_reset_profile, rp_dump_profile,
        rp_get_num_memory_servers, rp_create_region, rp_destroy_region,
        rp_resize_region, rp_allocate, rp_deallocate,
        rp_change_region_permission, rp_change_dataitem_permission,
        rp_lookup_region, rp_lookup, rp_check_permission_get_region_info,
        rp_check_permission_get_item_info, rp_get_stat_info, rp_copy, rp_backup,
        rp_restore, rp_acquire_CAS_lock, rp_release_CAS_lock,
        rp_get_backup_info, rp_list_backup, rp_delete_backup,
        rp_get_memserverinfo_size, rp_get_memserverinfo, rp_get_atomic,
        rp_put_atomic, rp_scatter_strided_atomic, rp_gather_strided_atomic,
        rp_scatter_indexed_atomic, rp_gather_indexed_atomic,
        rp_get_region_memory, rp_open_region_with_registration,
        rp_open_region_without_registration, rp_close_region;
    tl::endpoint server;
    void connect(const char *name, uint64_t port);
    char *get_fabric_addr(const char *name, uint64_t port);
};

} // namespace openfam
#endif /* end of FAM_ALLOCATOR_H_ */
