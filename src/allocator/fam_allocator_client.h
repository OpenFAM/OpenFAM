/*
 * fam_allocator_client.h
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
#ifndef FAM_ALLOCATOR_CLIENT_H_
#define FAM_ALLOCATOR_CLIENT_H_

#include "allocator/fam_client_resource_manager.h"
#include "cis/fam_cis_client.h"
#include "cis/fam_cis_direct.h"

namespace openfam {

class Fam_Allocator_Client {
  public:
    Fam_Allocator_Client(const char *name, uint64_t port,
                         string rpc_framework_type, string rpc_protocol_type,
                         bool enableResourceRelease);

    Fam_Allocator_Client(bool isSharedMemory, bool enableResourceRelease);

    ~Fam_Allocator_Client();

    void allocator_initialize();

    void allocator_finalize();

    uint64_t get_num_memory_servers();
    Fam_Region_Descriptor *
    create_region(const char *name, uint64_t nbytes, mode_t permissions,
                  Fam_Region_Attributes *regionAttributes);
    void destroy_region(Fam_Region_Descriptor *descriptor);
    void resize_region(Fam_Region_Descriptor *descriptor, uint64_t nbytes);

    Fam_Descriptor *allocate(const char *name, uint64_t nbytes,
                             mode_t accessPermissions,
                             Fam_Region_Descriptor *region);
    void deallocate(Fam_Descriptor *descriptor);

    void close(Fam_Descriptor *descriptor);

    void change_permission(Fam_Region_Descriptor *descriptor,
                           mode_t accessPermissions);
    void change_permission(Fam_Descriptor *descriptor,
                           mode_t accessPermissions);
    Fam_Region_Descriptor *lookup_region(const char *name);
    Fam_Descriptor *lookup(const char *itemName, const char *regionName);
    Fam_Region_Item_Info
    check_permission_get_info(Fam_Region_Descriptor *descriptor);

    Fam_Region_Item_Info check_permission_get_info(Fam_Descriptor *descriptor);

    Fam_Region_Item_Info get_stat_info(Fam_Descriptor *descriptor);

    void *copy(Fam_Descriptor *src, uint64_t srcOffset, Fam_Descriptor *dest,
               uint64_t destOffset, uint64_t nbytes);

    void wait_for_copy(void *waitObj);
    void *backup(Fam_Descriptor *descriptor, const char *BackupName);
    void *restore(Fam_Descriptor *dest, const char *BackupName);
    void wait_for_backup(void *waitObj);
    void wait_for_restore(void *waitObj);
    void *delete_backup(const char *BackupName);
    void wait_for_delete_backup(void *waitObj);
    char *list_backup(const char *BackupName);
    /**
     * fam_map - Map a data item in FAM to the process address space.
     * @param descriptor - Descriptor associated with the data item in FAM.
     * @return - A pointer in the PE’s address space that can be used
     * to directly manipulate contents of FAM.
     */
    void *fam_map(Fam_Descriptor *descriptor);

    /**
     * fam_unmap - Unmap a data item in FAM from the process virtual
     * address space.
     * @param local - Local pointer to be unmapped.
     * @param descriptor - Descriptor associated with the data item in FAM
     * to be unmapped.
     */
    void fam_unmap(void *local, Fam_Descriptor *descriptor);

    /**
     * acquire_CAS_lock - Acquire the mutex lock to perform
     * 128-bit CAS operation.
     * @param descriptor - Descriptor associated with the data item in FAM
     */
    void acquire_CAS_lock(Fam_Descriptor *descriptor, uint64_t memserverId);
    /**
     * release_CAS_lock - Release the mutex lock acquired to perform
     * 128-bit CAS operation.
     * @param descriptor - Descriptor associated with the data item in FAM
     */
    void release_CAS_lock(Fam_Descriptor *descriptor, uint64_t memserverId);

    int get_addr_size(size_t *addrSize, uint64_t nodeId);

    int get_addr(void *addr, size_t addrSize, uint64_t nodeId);

    Fam_Backup_Info get_backup_info(string BackupName,
                                    Fam_Region_Descriptor *destRegion);

    int get_memserverinfo_size(size_t *memServerInfoSize);

    int get_memserverinfo(void *memServerInfoBuffer);

    void open_region_without_registration(uint64_t regionId);

    void
    open_region_with_registration(uint64_t regionId,
                                  Fam_Region_Memory_Map *famRegionMemoryMap);

    void close_region(uint64_t regionId);

    void close_all_regions();

  private:
    void lookup_region_memory_map(Fam_Region_Memory_Map famRegionMemoryMap,
                                  uint64_t memserverId,
                                  Fam_Region_Memory *regionMemory);
    Fam_Client_Resource_Manager *famResourceManager;
    Fam_CIS *famCIS;
    bool isSharedMemory;
    bool enableResourceRelease;
    uint32_t uid;
    uint32_t gid;
};

} // namespace openfam
#endif /* end of FAM_ALLOCATOR_CLIENT_H_ */
