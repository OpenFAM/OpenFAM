/*
 * memserver_allocator.h
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
#ifndef MEMSERVER_ALLOCATOR_H_
#define MEMSERVER_ALLOCATOR_H_

#include <iostream>
#include <pthread.h>
#include <sys/types.h> // needed for mode_t

#include <boost/atomic.hpp>
#include <nvmm/error_code.h>
#include <nvmm/global_ptr.h>
#include <nvmm/heap.h>
#include <nvmm/log.h>
#include <nvmm/memory_manager.h>
#include <nvmm/shelf_id.h>

#include "bitmap-manager/bitmap.h"
#include "common/fam_internal.h"
#include "common/memserver_exception.h"
#include "fam/fam.h"
#include "metadata/fam_metadata_manager.h"

#define MIN_OBJ_SIZE 128
#define MIN_REGION_SIZE (1UL << 20)

using namespace std;
using namespace nvmm;
using namespace metadata;

namespace openfam {

using HeapMap = std::map<uint64_t, Heap *>;

class Memserver_Allocator {
  public:
    Memserver_Allocator();
    ~Memserver_Allocator();
    void memserver_allocator_finalize();
    void reset_profile();
    void dump_profile();
    int create_region(string name, uint64_t &regionId, size_t nbytes,
                      mode_t permission, uint32_t uid, uint32_t gid);
    int destroy_region(uint64_t regionId, uint32_t uid, uint32_t gid);
    int resize_region(uint64_t regionId, uint32_t uid, uint32_t gid,
                      size_t nbytes);
    int allocate(string name, uint64_t regionId, size_t nbytes,
                 uint64_t &offset, mode_t permission, uint32_t uid,
                 uint32_t gid, Fam_DataItem_Metadata &dataitem,
                 void *&localPointer);
    int deallocate(uint64_t regionId, uint64_t offset, uint32_t uid,
                   uint32_t gid);
    int change_region_permission(uint64_t regionId, mode_t permission,
                                 uint32_t uid, uint32_t gid);
    int change_dataitem_permission(uint64_t regionId, uint64_t offset,
                                   mode_t permission, uint32_t uid,
                                   uint32_t gid);
    int get_region(string name, uint32_t uid, uint32_t gid,
                   Fam_Region_Metadata &region);
    int get_region(uint64_t regionId, uint32_t uid, uint32_t gid,
                   Fam_Region_Metadata &region);
    bool check_region_permission(Fam_Region_Metadata region, bool op,
                                 uint32_t uid, uint32_t gid);
    int get_dataitem(string itemName, string regionName, uint32_t uid,
                     uint32_t gid, Fam_DataItem_Metadata &dataitem);
    int get_dataitem(uint64_t regionId, uint64_t offset, uint32_t uid,
                     uint32_t gid, Fam_DataItem_Metadata &dataitem);
    bool check_dataitem_permission(Fam_DataItem_Metadata dataitem, bool op,
                                   uint32_t uid, uint32_t gid);
    void *get_local_pointer(uint64_t regionId, uint64_t offset);
    int open_heap(uint64_t regionId);
    int copy(uint64_t srcRegionId, uint64_t srcOffset, uint64_t srcCopyStart,
             uint64_t destRegionId, uint64_t destOffset, uint64_t destCopyStart,
             uint32_t uid, uint32_t gid, size_t nbytes);

  private:
    MemoryManager *memoryManager;
    FAM_Metadata_Manager *metadataManager;
    HeapMap *heapMap;
    pthread_mutex_t heapMapLock;
    HeapMap::iterator get_heap(uint64_t regionId, Heap *&heap);
    PoolId get_free_poolId();
    bitmap *bmap;
    void init_poolId_bmap();
};

} // namespace openfam

#endif /* end of MEMSERVER_ALLOCATOR_H_ */
