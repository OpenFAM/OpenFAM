/*
 * fam_cis_client.h
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
#ifndef FAM_CIS_CLIENT_H_
#define FAM_CIS_CLIENT_H_

#include <sys/types.h>

#include <grpc/impl/codegen/log.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "cis/fam_cis.h"
#include "cis/fam_cis_rpc.grpc.pb.h"

using namespace std;

namespace openfam {

using service = std::unique_ptr<Fam_CIS_Rpc::Stub>;
typedef struct {
    service stub;
    size_t memServerFabricAddrSize;
    char *memServerFabricAddr;
    ::grpc::CompletionQueue *copycq;
    ::grpc::CompletionQueue *backupcq;
    ::grpc::CompletionQueue *restorecq;
} Fam_CIS_Server_Info;

using Server_List = std::map<uint64_t, Fam_CIS_Server_Info *>;

class Fam_CIS_Client : public Fam_CIS {
  public:
    Fam_CIS_Client(const char *, uint64_t port);

    ~Fam_CIS_Client();

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

    size_t get_addr_size(uint64_t memoryServerId);
    void get_addr(void *memServerFabricAddr, uint64_t memoryServerId);
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

    void open_region_with_registration(uint64_t regionId, uint32_t uid,
                                       uint32_t gid,
                                       std::vector<uint64_t> *memserverIds,
                                       Fam_Region_Memory_Map *regionMemoryMap);

    void open_region_without_registration(uint64_t regionId,
                                          std::vector<uint64_t> *memserverIds);

    void get_region_memory(uint64_t regionId, uint32_t uid, uint32_t gid,
                           Fam_Region_Memory_Map *regionMemoryMap);

    // set and get controlpath address definitions
    void set_controlpath_addr(string addr) {}

    string get_controlpath_addr() { return std::string(); }

    void close_region(uint64_t regionId, std::vector<uint64_t> memserverIds);

  private:
    std::unique_ptr<Fam_CIS_Rpc::Stub> stub;
    ::grpc::CompletionQueue *copycq;
    ::grpc::CompletionQueue *backupcq;
    ::grpc::CompletionQueue *restorecq;
    ::grpc::CompletionQueue *delbackupcq;
};

} // namespace openfam
#endif /* end of FAM_ALLOCATOR_H_ */
