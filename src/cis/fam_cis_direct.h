/*
 * fam_cis_direct.h
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
#ifndef FAM_CIS_DIRECT_H_
#define FAM_CIS_DIRECT_H_

#include <sys/types.h>

#include "allocator/memserver_allocator.h"
#include "cis/fam_cis.h"
#include "metadata/fam_metadata_manager.h"

using namespace metadata;

namespace openfam {

class Fam_CIS_Direct : public Fam_CIS {
  public:
    // Fam_CIS_Direct(Memserver_Allocator *memAlloc);

    Fam_CIS_Direct();

    ~Fam_CIS_Direct();

    void cis_direct_finalize();

    void reset_profile();

    void dump_profile();

    Fam_Region_Item_Info create_region(string name, size_t nbytes,
                                       mode_t permission,
                                       Fam_Redundancy_Level redundancyLevel,
                                       uint64_t memoryServerId, uint32_t uid,
                                       uint32_t gid);
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
                                 uint32_t uid, uint32_t gid);
    bool check_dataitem_permission(Fam_DataItem_Metadata dataitem, bool op,
                                   uint32_t uid, uint32_t gid);
    Fam_Region_Item_Info lookup_region(string name, uint64_t memoryServerId,
                                       uint32_t uid, uint32_t gid);
    Fam_Region_Item_Info lookup(string itemName, string regionName,
                                uint64_t memoryServerId, uint32_t uid,
                                uint32_t gid);
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

    void *copy(uint64_t srcRegionId, uint64_t srcOffset, uint64_t srcCopyStart,
               uint64_t destRegionId, uint64_t destOffset,
               uint64_t destCopyStar, uint64_t nbytes, uint64_t memoryServerId,
               uint32_t uid, uint32_t gid);

    void wait_for_copy(void *waitObj) {}

    void *fam_map(uint64_t regionId, uint64_t offset, uint64_t memoryServerId,
                  uint32_t uid, uint32_t gid);
    void fam_unmap(void *local, uint64_t regionId, uint64_t offset,
                   uint64_t memoryServerId, uint32_t uid, uint32_t gid);

    void *get_local_pointer(uint64_t regionId, uint64_t offset);

    void acquire_CAS_lock(uint64_t offset, uint64_t memoryServerId) {}
    void release_CAS_lock(uint64_t offset, uint64_t memoryServerId) {}

    int get_addr_size(size_t *addrSize, uint64_t memoryServerId) { return 0; }
    int get_addr(void *addr, size_t addrSize, uint64_t memoryServerId) {
        return 0;
    }

  private:
    Memserver_Allocator *allocator;
    FAM_Metadata_Manager *metadataManager;
};

} // namespace openfam
#endif /* end of FAM_ALLOCATOR_H_ */
