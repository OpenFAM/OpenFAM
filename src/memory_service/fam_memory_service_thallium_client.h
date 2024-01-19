/*
 * fam_memory_service_thallium_client.h
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
#ifndef FAM_MEMORY_SERVICE_THALLIUM_CLIENT_H_
#define FAM_MEMORY_SERVICE_THALLIUM_CLIENT_H_

#include <sys/types.h>

#include <grpc/impl/codegen/log.h>
#include <grpcpp/grpcpp.h>
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
#include "memory_service/fam_memory_service.h"
#include "memory_service/fam_memory_service_rpc.grpc.pb.h"

using namespace std;
namespace tl = thallium;

namespace openfam {

class Fam_Memory_Service_Thallium_Client : public Fam_Memory_Service {
  public:
    Fam_Memory_Service_Thallium_Client(tl::engine engine_, const char *name,
                                       uint64_t port);

    ~Fam_Memory_Service_Thallium_Client();

    void reset_profile();

    void dump_profile();

    void create_region(uint64_t regionId, size_t nbytes);

    void destroy_region(uint64_t regionId, uint64_t *resourceStatus);

    void resize_region(uint64_t regionId, size_t nbytes);

    Fam_Region_Item_Info allocate(uint64_t regionId, size_t nbytes);

    void deallocate(uint64_t regionId, uint64_t offset);

    void copy(uint64_t srcRegionId, uint64_t *srcOffsets,
              uint64_t srcUsedMemsrvCnt, uint64_t srcCopyStart,
              uint64_t srcCopyEnd, uint64_t *srcKeys, uint64_t *srcBaseAddrList,
              uint64_t destRegionId, uint64_t destOffset,
              uint64_t destUsedMemsrvCnt, uint64_t *srcMemserverIds,
              uint64_t srcInterleaveSize, uint64_t destInterleaveSize,
              uint64_t size);

    void backup(uint64_t srcRegionId, uint64_t srcOffset, uint64_t size,
                uint64_t chunkSize, uint64_t usedMemserverCnt,
                uint64_t fileStartPos, const string BackupName, uint32_t uid,
                uint32_t gid, mode_t mode, const string dataitemName,
                uint64_t itemSize, bool writeMetadata);

    void restore(uint64_t destRegionId, uint64_t destOffset, uint64_t size,
                 uint64_t chunkSize, uint64_t usedMemserverCnt,
                 uint64_t fileStartPos, string BackupName);
    Fam_Backup_Info get_backup_info(std::string BackupName, uint32_t uid,
                                    uint32_t gid, uint32_t mode);
    std::string list_backup(std::string BackupName, uint32_t uid, uint32_t gid,
                            uint32_t mode);
    void delete_backup(std::string BackupName);

    void *get_local_pointer(uint64_t regionId, uint64_t offset);

    void acquire_CAS_lock(uint64_t offset);
    void release_CAS_lock(uint64_t offset);

    size_t get_addr_size();
    void *get_addr();
    Fam_Memory_Type get_memtype();

    void register_region_memory(uint64_t regionId, bool accessType);

    Fam_Region_Memory get_region_memory(uint64_t regionId, bool accessType);

    Fam_Dataitem_Memory get_dataitem_memory(uint64_t regionId, uint64_t offset,
                                            uint64_t size, bool accessType);

    void get_atomic(uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset,
                    uint64_t nbytes, uint64_t key, uint64_t srcBaseAddr,
                    const char *nodeAddr, uint32_t nodeAddrSize);

    void put_atomic(uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset,
                    uint64_t nbytes, uint64_t key, uint64_t srcBaseAddr,
                    const char *nodeAddr, uint32_t nodeAddrSize,
                    const char *data);

    void scatter_strided_atomic(uint64_t regionId, uint64_t offset,
                                uint64_t nElements, uint64_t firstElement,
                                uint64_t stride, uint64_t elementSize,
                                uint64_t key, uint64_t srcBaseAddr,
                                const char *nodeAddr, uint32_t nodeAddrSize);

    void gather_strided_atomic(uint64_t regionId, uint64_t offset,
                               uint64_t nElements, uint64_t firstElement,
                               uint64_t stride, uint64_t elementSize,
                               uint64_t key, uint64_t srcBaseAddr,
                               const char *nodeAddr, uint32_t nodeAddrSize);

    void scatter_indexed_atomic(uint64_t regionId, uint64_t offset,
                                uint64_t nElements, const void *elementIndex,
                                uint64_t elementSize, uint64_t key,
                                uint64_t srcBaseAddr, const char *nodeAddr,
                                uint32_t nodeAddrSize);

    void gather_indexed_atomic(uint64_t regionId, uint64_t offset,
                               uint64_t nElements, const void *elementIndex,
                               uint64_t elementSize, uint64_t key,
                               uint64_t srcBaseAddr, const char *nodeAddr,
                               uint32_t nodeAddrSize);

    void update_memserver_addrlist(void *memServerInfoBuffer,
                                   size_t memServerInfoSize,
                                   uint64_t memoryServerCount);

    Fam_Region_Memory open_region_with_registration(uint64_t regionId,
                                                    bool accessType);

    void open_region_without_registration(uint64_t regionId);

    uint64_t close_region(uint64_t regionId);

    uint64_t get_memory_server_id() { return memoryServerId; }

    // set and get controlpath address functions
    void set_controlpath_addr(string addr) {}

    string get_controlpath_addr() { return std::string(); }

    void create_region_failure_cleanup(uint64_t regionId);

  private:
    std::unique_ptr<Fam_Memory_Service_Rpc::Stub> stub;
    size_t memServerFabricAddrSize;
    char *memServerFabricAddr;
    Fam_Memory_Type memServermemType;
    tl::engine myEngine;
    tl::provider_handle ph;
    tl::remote_procedure rp_reset_profile, rp_dump_profile, rp_create_region,
        rp_destroy_region, rp_resize_region, rp_allocate, rp_deallocate,
        rp_copy, rp_backup, rp_restore, rp_get_backup_info, rp_list_backup,
        rp_delete_backup, rp_get_local_pointer, rp_register_region_memory,
        rp_get_region_memory, rp_get_dataitem_memory, rp_acquire_CAS_lock,
        rp_release_CAS_lock, rp_get_atomic, rp_put_atomic,
        rp_scatter_strided_atomic, rp_gather_strided_atomic,
        rp_scatter_indexed_atomic, rp_gather_indexed_atomic,
        rp_update_memserver_addrlist, rp_open_region_with_registration,
        rp_open_region_without_registration, rp_close_region,
        rp_create_region_failure_cleanup;
    tl::endpoint server;
    void connect(const char *name, uint64_t port);
    char *get_fabric_addr(const char *name, uint64_t port);
    uint64_t memoryServerId;
};

} // namespace openfam
#endif /* end of FAM_MEMORY_SERVICE_THALLIUM_CLIENT_H_ */
