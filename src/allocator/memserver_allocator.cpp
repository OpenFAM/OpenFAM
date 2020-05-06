/*
 * memserver_allocator.cpp
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
#include "allocator/memserver_allocator.h"
#include <boost/atomic.hpp>
#include <chrono>
#include <iomanip>
#include <string.h>
#include <unistd.h>

#include "common/fam_memserver_profile.h"
using namespace std;
using namespace chrono;

namespace openfam {
MEMSERVER_PROFILE_START(NVMM)
#ifdef MEMSERVER_PROFILE
#define NVMM_PROFILE_START_OPS()                                               \
    {                                                                          \
        Profile_Time start = NVMM_get_time();

#define NVMM_PROFILE_END_OPS(apiIdx)                                           \
    Profile_Time end = NVMM_get_time();                                        \
    Profile_Time total = NVMM_time_diff_nanoseconds(start, end);               \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(NVMM, prof_##apiIdx, total)             \
    }
#define NVMM_PROFILE_DUMP() nvmm_profile_dump()
#else
#define NVMM_PROFILE_START_OPS()
#define NVMM_PROFILE_END_OPS(apiIdx)
#define NVMM_PROFILE_DUMP()
#endif

void nvmm_profile_dump(){MEMSERVER_PROFILE_END(NVMM)
                             MEMSERVER_DUMP_PROFILE_BANNER(NVMM)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(NVMM, name, prof_##name)
#include "allocator/NVMM_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name) MEMSERVER_PROFILE_TOTAL(NVMM, prof_##name)
#include "allocator/NVMM_counters.tbl"
                                 MEMSERVER_DUMP_PROFILE_SUMMARY(NVMM)}

Memserver_Allocator::Memserver_Allocator() {
    MEMSERVER_PROFILE_INIT(NVMM)
    MEMSERVER_PROFILE_START_TIME(NVMM)
    // NVMM_PROFILE_INIT();
    // NVMM_PROFILE_START_TIME();
    StartNVMM();
    heapMap = new HeapMap();
    memoryManager = MemoryManager::GetInstance();
    metadataManager = FAM_Metadata_Manager::GetInstance();
    (void)pthread_mutex_init(&heapMapLock, NULL);
    init_poolId_bmap();
}

Memserver_Allocator::~Memserver_Allocator() {
    delete heapMap;
    delete metadataManager;
    pthread_mutex_destroy(&heapMapLock);
}

void Memserver_Allocator::memserver_allocator_finalize() {
    HeapMap::iterator it = heapMap->begin();
    Heap *heap = 0;

    while (it != heapMap->end()) {
        heap = it->second;
        if (heap->IsOpen())
            heap->Close();
        it++;
    }
}

void Memserver_Allocator::reset_profile() {
    MEMSERVER_PROFILE_INIT(NVMM)
    MEMSERVER_PROFILE_START_TIME(NVMM)
    // NVMM_PROFILE_INIT();
    // NVMM_PROFILE_START_TIME();
    metadataManager->reset_profile();
}
void Memserver_Allocator::dump_profile() {
    NVMM_PROFILE_DUMP();
    metadataManager->dump_profile();
}
/*
 * Initalize the region Id bitmap address.
 * Size of bitmap is Max poolId's supported / 8 bytes.
 * Get the Bitmap address reserved from the root shelf.
 */
void Memserver_Allocator::init_poolId_bmap() {
    bmap = new bitmap();
    bmap->size = (ShelfId::kMaxPoolCount * BITSIZE) / sizeof(uint64_t);
    bmap->map = memoryManager->GetRegionIdBitmapAddr();
}

/*
 * Create a new region.
 * name - name of the region
 * regionId - return the regionId allocated by this routine
 * nbytes - size of region in bytes
 * permission - Permission for the region
 * uid/gid - user id and group id
 */
int Memserver_Allocator::create_region(string name, uint64_t &regionId,
                                       size_t nbytes, mode_t permission,
                                       uint32_t uid, uint32_t gid) {
    ostringstream message;
    message << "Error While creating region : ";

    // Check if the name size is bigger than MAX_KEY_LEN supported
    if (name.size() > metadataManager->metadata_maxkeylen()) {
        message << "Name too long";
        throw Memserver_Exception(REGION_NAME_TOO_LONG, message.str().c_str());
    }

    // Obtain free regionId from NVMM
    size_t tmpSize;

    // TODO: Obtain free regionId from NVMM
    PoolId poolId = get_free_poolId();
    if (poolId == (PoolId)NO_FREE_POOLID) {
        message << "No free pool ID";
        throw Memserver_Exception(NO_FREE_POOLID, message.str().c_str());
    }
    // Checking if the region is already exist, if exists return error
    Fam_Region_Metadata region;
    int ret = metadataManager->metadata_find_region(name, region);
    if (ret == META_NO_ERROR) {
        message << "Region already exist";
        // Reset the poolId bit in the bitmap
        bitmap_reset(bmap, poolId);
        throw Memserver_Exception(REGION_EXIST, message.str().c_str());
    }
    // Else Create region using NVMM and set the fields in reponse to
    // appropriate value

    if (nbytes < MIN_REGION_SIZE)
        tmpSize = MIN_REGION_SIZE;
    else
        tmpSize = nbytes;
    {
        NVMM_PROFILE_START_OPS()
        ret = memoryManager->CreateHeap(poolId, tmpSize, MIN_OBJ_SIZE);
        NVMM_PROFILE_END_OPS(CreateHeap)
        // Profile_Time end = NVMM_PROFILE_GET_TIME();
        // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(CreateHeap, total);
    }

    if (ret != NO_ERROR) {
        // Reset the poolId bit in the bitmap
        bitmap_reset(bmap, poolId);
        message << "Heap not created";
        throw Memserver_Exception(HEAP_NOT_CREATED, message.str().c_str());
    }
    Heap *heap = 0;
    {
        NVMM_PROFILE_START_OPS()
        ret = memoryManager->FindHeap(poolId, &heap);
        NVMM_PROFILE_END_OPS(FindHeap)
        // Profile_Time end = NVMM_PROFILE_GET_TIME();
        // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(FindHeap, total);
    }
    if (ret != NO_ERROR) {
        message << "Heap not found";
        // Reset the poolId bit in the bitmap
        bitmap_reset(bmap, poolId);
        delete heap;
        throw Memserver_Exception(HEAP_NOT_FOUND, message.str().c_str());
    }
    {
        NVMM_PROFILE_START_OPS()
        ret = heap->Open();
        NVMM_PROFILE_END_OPS(Heap_Open)
        // Profile_Time end = NVMM_PROFILE_GET_TIME();
        // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_Open, total);
    }
    if (ret != NO_ERROR) {
        message << "Can not open heap";
        // Reset the poolId bit in the bitmap
        bitmap_reset(bmap, poolId);
        delete heap;
        throw Memserver_Exception(HEAP_NOT_OPENED, message.str().c_str());
    }
    regionId = (uint64_t)poolId;

    NVMM_PROFILE_START_OPS()
    pthread_mutex_lock(&heapMapLock);
    NVMM_PROFILE_START_OPS()

    auto heapObj = heapMap->find(regionId);
    if (heapObj == heapMap->end()) {
        heapMap->insert({regionId, heap});
    } else {
        message << "Can not insert heap. regionId already found in map";
        pthread_mutex_unlock(&heapMapLock);
        // Reset the poolId bit in the bitmap
        bitmap_reset(bmap, regionId);
        delete heap;
        {
            NVMM_PROFILE_START_OPS()
            ret = memoryManager->DestroyHeap((PoolId)regionId);
            NVMM_PROFILE_END_OPS(DestroyHeap)
            // Profile_Time end = NVMM_PROFILE_GET_TIME();
            // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
            // NVMM_PROFILE_ADD_TO_TOTAL_OPS(DestroyHeap, total);
        }
        if (ret != NO_ERROR) {
            message << "Can not destroy heap";
        }
        throw Memserver_Exception(RBT_HEAP_NOT_INSERTED, message.str().c_str());
    }
    NVMM_PROFILE_END_OPS(HeapMapInsertOp)
    // Profile_Time end1 = NVMM_PROFILE_GET_TIME();
    // Profile_Time total1 = NVMM_PROFILE_TIME_DIFF_NS(start2, end1);
    // NVMM_PROFILE_ADD_TO_TOTAL_OPS(HeapMapInsertOp, total1);

    pthread_mutex_unlock(&heapMapLock);
    NVMM_PROFILE_END_OPS(HeapMapInsertTotal);
    // Profile_Time end2 = NVMM_PROFILE_GET_TIME();
    // Profile_Time total2 = NVMM_PROFILE_TIME_DIFF_NS(start1, end2);
    // NVMM_PROFILE_ADD_TO_TOTAL_OPS(HeapMapInsertTotal, total2);

    // Register the region into metadata service
    region.regionId = regionId;
    strncpy(region.name, name.c_str(), metadataManager->metadata_maxkeylen());
    region.offset = INVALID_OFFSET;
    region.perm = permission;
    region.uid = uid;
    region.gid = gid;
    region.size = nbytes;
    ret = metadataManager->metadata_insert_region(regionId, name, &region);
    if (ret != META_NO_ERROR) {
        message << "Can not insert region into metadata service, ";
        ret = heap->Close();
        if (ret != NO_ERROR) {
            message << "Can not close heap, ";
        }

        HeapMap::iterator it = get_heap(regionId, heap);
        if (it != heapMap->end()) {
            pthread_mutex_lock(&heapMapLock);
            heapMap->erase(it);
            pthread_mutex_unlock(&heapMapLock);
        } else {
            message << "Can not remove heap from map";
        }

        // Reset the regionId bit in the bitmap
        bitmap_reset(bmap, regionId);
        delete heap;
        {
            NVMM_PROFILE_START_OPS()
            ret = memoryManager->DestroyHeap((PoolId)regionId);
            NVMM_PROFILE_END_OPS(DestroyHeap)
            // Profile_Time end = NVMM_PROFILE_GET_TIME();
            // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
            // NVMM_PROFILE_ADD_TO_TOTAL_OPS(DestroyHeap, total);
        }
        if (ret != NO_ERROR) {
            message << "Can not destroy heap";
        }
        throw Memserver_Exception(REGION_NOT_INSERTED, message.str().c_str());
    }

    return ALLOC_NO_ERROR;
}

/*
 * destroy a region
 * regionId - region Id of the region to be destroyed
 * uid - user id of the process
 * gid - group id of the process
 */
int Memserver_Allocator::destroy_region(uint64_t regionId, uint32_t uid,
                                        uint32_t gid) {
    ostringstream message;
    message << "Error While destroy region : ";
    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    int ret = metadataManager->metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        message << "Region does not exist";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to destroy region, if not return error
    if (uid != region.uid) {
        bool isPermitted = metadataManager->metadata_check_permissions(
            &region, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Destroying region is not permitted";
            throw Memserver_Exception(DESTROY_REGION_NOT_PERMITTED,
                                      message.str().c_str());
        }
    }

    // destroy region using NVMM
    // Even if heap is not found in map, continue with DestroyHeap
    Heap *heap = 0;

    HeapMap::iterator it = get_heap(regionId, heap);

    if (it != heapMap->end()) {
        NVMM_PROFILE_START_OPS()
        pthread_mutex_lock(&heapMapLock);
        NVMM_PROFILE_START_OPS()
        heapMap->erase(it);
        NVMM_PROFILE_END_OPS(HeapMapEraseOp)
        // Profile_Time end1 = NVMM_PROFILE_GET_TIME();
        // Profile_Time total1 = NVMM_PROFILE_TIME_DIFF_NS(start2, end1);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(HeapMapEraseOp, total1);
        pthread_mutex_unlock(&heapMapLock);
        NVMM_PROFILE_END_OPS(HeapMapEraseTotal)
        // Profile_Time end2 = NVMM_PROFILE_GET_TIME();
        // Profile_Time total2 = NVMM_PROFILE_TIME_DIFF_NS(start1, end2);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(HeapMapEraseTotal, total2);
        {
            NVMM_PROFILE_START_OPS()
            ret = heap->Close();
            NVMM_PROFILE_END_OPS(Heap_Close)
            // Profile_Time end = NVMM_PROFILE_GET_TIME();
            // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
            // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_Close, total);
        }
        if (ret != NO_ERROR) {
            message << "Can not close heap";
            throw Memserver_Exception(HEAP_NOT_CLOSED, message.str().c_str());
        }
        delete heap;
    }

    // remove region from metadata service.
    // metadata_delete_region() is called before DestroyHeap() as
    // cached KVS is freed in metadata_delete_region and calling
    // metadata_delete_region after DestroyHeap will result in SIGSEGV.
    ret = metadataManager->metadata_delete_region(regionId);
    if (ret != META_NO_ERROR) {
        message << "Can not remove region from metadata service";
        throw Memserver_Exception(REGION_NOT_REMOVED, message.str().c_str());
    }
    {
        NVMM_PROFILE_START_OPS()
        ret = memoryManager->DestroyHeap((PoolId)regionId);
        NVMM_PROFILE_END_OPS(DestroyHeap)
        // Profile_Time end = NVMM_PROFILE_GET_TIME();
        // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(DestroyHeap, total);
    }
    if (ret != NO_ERROR) {
        message << "Can not destroy heap";
        throw Memserver_Exception(HEAP_NOT_DESTROYED, message.str().c_str());
    }

    // Reset the regionId bit in the bitmap
    bitmap_reset(bmap, regionId);

    return ALLOC_NO_ERROR;
}

/*
 * Resize the region
 * regionId - region Id of the region to be resized.
 * nbytes - new size of the region
 */
int Memserver_Allocator::resize_region(uint64_t regionId, uint32_t uid,
                                       uint32_t gid, size_t nbytes) {
    ostringstream message;
    message << "Error while resizing the region";
    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    int ret = metadataManager->metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        message << "Region does not exist";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    bool isPermitted = metadataManager->metadata_check_permissions(
        &region, META_REGION_ITEM_WRITE, uid, gid);
    if (!isPermitted) {
        message << "Region resize not permitted";
        throw Memserver_Exception(REGION_RESIZE_NOT_PERMITTED,
                                  message.str().c_str());
    }

    // Get the heap and open it if not open already
    Heap *heap = 0;
    HeapMap::iterator it = get_heap(regionId, heap);

    if (it == heapMap->end()) {
        ret = open_heap(regionId);
        if (ret != ALLOC_NO_ERROR) {
            message << "Opening of heap failed";
            throw Memserver_Exception(HEAP_NOT_OPENED, message.str().c_str());
        }
        it = get_heap(regionId, heap);
        if (it == heapMap->end()) {
            message << "Can not find heap in map";
            throw Memserver_Exception(RBT_HEAP_NOT_FOUND,
                                      message.str().c_str());
        }
    }

    // Call NVMM to resize the heap
    {
        NVMM_PROFILE_START_OPS()
        ret = heap->Resize(nbytes);
        NVMM_PROFILE_END_OPS(Heap_Resize)
        // Profile_Time end = NVMM_PROFILE_GET_TIME();
        // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_Resize, total);
    }
    if (ret != NO_ERROR) {
        message << "heap resize failed";
        throw Memserver_Exception(RESIZE_FAILED, message.str().c_str());
    }

    region.size = nbytes;
    // Update the size in the metadata service
    ret = metadataManager->metadata_modify_region(regionId, &region);
    if (ret != META_NO_ERROR) {
        message << "Can not modify metadata service, ";
        throw Memserver_Exception(REGION_NOT_MODIFIED, message.str().c_str());
    }

    return ALLOC_NO_ERROR;
}

/*
 * Allocate a data item in a region
 * name - name of the data item
 * regionId - Region id of the region in which data item to be allocated
 * nbytes - size of the data item
 * offset - offset of data item in the region
 * dataitem - dataitem descriptor
 * localpointer - local pointer for dataitem offset
 */
int Memserver_Allocator::allocate(string name, uint64_t regionId, size_t nbytes,
                                  uint64_t &offset, mode_t permission,
                                  uint32_t uid, uint32_t gid,
                                  Fam_DataItem_Metadata &dataitem,
                                  void *&localPointer) {
    ostringstream message;
    message << "Error While allocating dataitem : ";

    // Check if the name size is bigger than MAX_KEY_LEN supported
    if (name.size() > metadataManager->metadata_maxkeylen()) {
        message << "Name too long";
        throw Memserver_Exception(DATAITEM_NAME_TOO_LONG,
                                  message.str().c_str());
    }

    size_t tmpSize;
    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    int ret = metadataManager->metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        message << "Region does not exist";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to create dataitem in that region, if not return error
    if (uid != region.uid) {
        bool isPermitted = metadataManager->metadata_check_permissions(
            &region, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Allocation of dataitem is not permitted";
            throw Memserver_Exception(DATAITEM_ALLOC_NOT_PERMITTED,
                                      message.str().c_str());
        }
    }

    // Check with metadata service if data item with the requested name
    // is already exist, if exists return error
    if (name != "") {
        ret = metadataManager->metadata_find_dataitem(name, regionId, dataitem);
        if (ret == META_NO_ERROR) {
            message << "Dataitem with the name provided already exist";
            throw Memserver_Exception(DATAITEM_EXIST, message.str().c_str());
        }
    }

    // Call NVMM to create a new data item
    Heap *heap = 0;

    HeapMap::iterator it = get_heap(regionId, heap);
    if (it == heapMap->end()) {
        ret = open_heap(regionId);
        if (ret != ALLOC_NO_ERROR) {
            message << "Opening of heap failed";
            throw Memserver_Exception(HEAP_NOT_OPENED, message.str().c_str());
        }
        it = get_heap(regionId, heap);
        if (it == heapMap->end()) {
            message << "Can not find heap in map";
            throw Memserver_Exception(RBT_HEAP_NOT_FOUND,
                                      message.str().c_str());
        }
    }

    // If the requested siz is lessar than MIN_OBJ_SIZE,
    // allocate data item of size MIN_OBJ_SIZE
    if (nbytes < MIN_OBJ_SIZE)
        tmpSize = MIN_OBJ_SIZE;
    else
        tmpSize = nbytes;
    {
        NVMM_PROFILE_START_OPS()
        offset = heap->AllocOffset(tmpSize);
        NVMM_PROFILE_END_OPS(Heap_AllocOffset)
        // Profile_Time end = NVMM_PROFILE_GET_TIME();
        // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_AllocOffset, total);
    }
    if (!offset) {
        try {
            {
                NVMM_PROFILE_START_OPS()
                heap->Merge();
                NVMM_PROFILE_END_OPS(Heap_Merge)
                // Profile_Time end = NVMM_PROFILE_GET_TIME();
                // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
                // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_Merge, total);
            }
        } catch (...) {
            message << "Heap Merge() failed";
            throw Memserver_Exception(HEAP_MERGE_FAILED, message.str().c_str());
        }
        {
            NVMM_PROFILE_START_OPS()
            offset = heap->AllocOffset(tmpSize);
            NVMM_PROFILE_END_OPS(Heap_AllocOffset)
            // Profile_Time end = NVMM_PROFILE_GET_TIME();
            // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
            // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_AllocOffset, total);
        }
        if (!offset) {
            message << "alloc() failed";
            throw Memserver_Exception(HEAP_ALLOCATE_FAILED,
                                      message.str().c_str());
        }
    }

    // Register the data item with metadata service
    {
        NVMM_PROFILE_START_OPS()
        localPointer = heap->OffsetToLocal(offset);
        NVMM_PROFILE_END_OPS(Heap_OffsetToLocal)
        // Profile_Time end = NVMM_PROFILE_GET_TIME();
        // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_OffsetToLocal, total);
    }
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    dataitem.regionId = regionId;
    strncpy(dataitem.name, name.c_str(), metadataManager->metadata_maxkeylen());
    dataitem.offset = offset;
    dataitem.perm = permission;
    dataitem.gid = gid;
    dataitem.uid = uid;
    dataitem.size = nbytes;
    if (name == "")
        ret = metadataManager->metadata_insert_dataitem(dataitemId, regionId,
                                                        &dataitem);
    else
        ret = metadataManager->metadata_insert_dataitem(dataitemId, regionId,
                                                        &dataitem, name);
    if (ret != META_NO_ERROR) {
        message << "Can not insert dataitem into metadata service";
        {
            NVMM_PROFILE_START_OPS()
            heap->Free(offset);
            NVMM_PROFILE_END_OPS(Heap_Free)
            // Profile_Time end = NVMM_PROFILE_GET_TIME();
            // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
            // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_Free, total);
        }
        throw Memserver_Exception(DATAITEM_NOT_INSERTED, message.str().c_str());
    }

    return ALLOC_NO_ERROR;
}

/*
 *
 * Deallocates a dataitem in a region at the given offset.
 */
int Memserver_Allocator::deallocate(uint64_t regionId, uint64_t offset,
                                    uint32_t uid, uint32_t gid) {
    ostringstream message;
    message << "Error While deallocating dataitem : ";
    // Check with metadata service if data item with the requested name
    // is already exist, if not return error
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    Fam_DataItem_Metadata dataitem;
    int ret =
        metadataManager->metadata_find_dataitem(dataitemId, regionId, dataitem);
    if (ret != META_NO_ERROR) {
        message << "Dataitem does not exist";
        throw Memserver_Exception(DATAITEM_NOT_FOUND, message.str().c_str());
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to destroy region, if not return error
    if (uid != dataitem.uid) {
        bool isPermitted = metadataManager->metadata_check_permissions(
            &dataitem, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Deallocation of dataitem is not permitted";
            throw Memserver_Exception(DATAITEM_DEALLOC_NOT_PERMITTED,
                                      message.str().c_str());
        }
    }

    // Remove data item from metadata service
    ret = metadataManager->metadata_delete_dataitem(dataitemId, regionId);
    if (ret != META_NO_ERROR) {
        message << "Can not remove dataitem from metadata service";
        throw Memserver_Exception(DATAITEM_NOT_REMOVED, message.str().c_str());
    }
    // call NVMM to destroy the data item
    Heap *heap = 0;

    HeapMap::iterator it = get_heap(regionId, heap);
    if (it != heapMap->end()) {
        {
            NVMM_PROFILE_START_OPS()
            heap->Free(offset);
            NVMM_PROFILE_END_OPS(Heap_Free)
            // Profile_Time end = NVMM_PROFILE_GET_TIME();
            // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
            // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_Free, total);
        }
    } else {
        // Heap not found in map. Get the heap from NVMM
        ret = open_heap(regionId);
        if (ret != ALLOC_NO_ERROR) {
            message << "Opening of heap failed";
            throw Memserver_Exception(HEAP_NOT_OPENED, message.str().c_str());
        }
        it = get_heap(regionId, heap);
        if (it == heapMap->end()) {
            message << "Can not find heap in map";
            throw Memserver_Exception(RBT_HEAP_NOT_FOUND,
                                      message.str().c_str());
        }
        {
            NVMM_PROFILE_START_OPS()
            heap->Free(offset);
            NVMM_PROFILE_END_OPS(Heap_Free)
            // Profile_Time end = NVMM_PROFILE_GET_TIME();
            // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
            // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_Free, total);
        }
    }
    return ALLOC_NO_ERROR;
}

/*
 * Change the permission for a given region
 */
int Memserver_Allocator::change_region_permission(uint64_t regionId,
                                                  mode_t permission,
                                                  uint32_t uid, uint32_t gid) {
    ostringstream message;
    message << "Error While changing region permission : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    Fam_Region_Metadata region;
    int ret = metadataManager->metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        message << "Region does not exist";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    // Check with metadata service if the calling PE has the permission
    // to modify permissions of the region, if not return error
    if (uid != region.uid) {
        message << "Region permission modify not permitted";
        throw Memserver_Exception(REGION_PERM_MODIFY_NOT_PERMITTED,
                                  message.str().c_str());
    }

    // Update the permission of region with metadata service
    region.perm = permission;
    ret = metadataManager->metadata_modify_region(regionId, &region);
    return ALLOC_NO_ERROR;
}

/*
 * Change the permission od a dataitem in a given region and offset.
 */
int Memserver_Allocator::change_dataitem_permission(uint64_t regionId,
                                                    uint64_t offset,
                                                    mode_t permission,
                                                    uint32_t uid,
                                                    uint32_t gid) {
    ostringstream message;
    message << "Error While changing dataitem permission : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    Fam_DataItem_Metadata dataitem;
    int ret =
        metadataManager->metadata_find_dataitem(dataitemId, regionId, dataitem);
    if (ret != META_NO_ERROR) {
        message << "Dataitem does not exist";
        throw Memserver_Exception(DATAITEM_NOT_FOUND, message.str().c_str());
    }

    // Check with metadata service if the calling PE has the permission
    // to modify permissions of the region, if not return error
    if (uid != dataitem.uid) {
        message << "Dataitem permission modify not permitted";
        throw Memserver_Exception(ITEM_PERM_MODIFY_NOT_PERMITTED,
                                  message.str().c_str());
    }

    // Update the permission of region with metadata service
    dataitem.perm = permission;
    ret = metadataManager->metadata_modify_dataitem(dataitemId, regionId,
                                                    &dataitem);
    return ALLOC_NO_ERROR;
}

/*
 * Region name lookup given the region name.
 * Returns the Fam_Region_Metadata for the given name
 */
int Memserver_Allocator::get_region(string name, uint32_t uid, uint32_t gid,
                                    Fam_Region_Metadata &region) {
    ostringstream message;
    message << "Error While locating region : ";
    int ret = metadataManager->metadata_find_region(name, region);
    if (ret != META_NO_ERROR) {
        message << "could not find the region";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    return ALLOC_NO_ERROR;
}

/*
 * Region name lookup given the region id.
 * Returns the Fam_Region_Metadata for the given name
 */
int Memserver_Allocator::get_region(uint64_t regionId, uint32_t uid,
                                    uint32_t gid, Fam_Region_Metadata &region) {
    ostringstream message;
    message << "Error While locating region : ";
    int ret = metadataManager->metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        message << "could not find the region";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    return ALLOC_NO_ERROR;
}

/*
 * Check if the given uid/gid has read or rw permissions.
 */
bool Memserver_Allocator::check_region_permission(Fam_Region_Metadata region,
                                                  bool op, uint32_t uid,
                                                  uint32_t gid) {
    metadata_region_item_op_t opFlag;
    if (op)
        opFlag = META_REGION_ITEM_RW;
    else
        opFlag = META_REGION_ITEM_READ;

    return (
        metadataManager->metadata_check_permissions(&region, opFlag, uid, gid));
}

/*
 * dataitem lookup for the given region and dataitem name.
 * Returns the Fam_Dataitem_Metadata for the given name
 */
int Memserver_Allocator::get_dataitem(string itemName, string regionName,
                                      uint32_t uid, uint32_t gid,
                                      Fam_DataItem_Metadata &dataitem) {
    ostringstream message;
    message << "Error While locating dataitem : ";
    int ret =
        metadataManager->metadata_find_dataitem(itemName, regionName, dataitem);
    if (ret != META_NO_ERROR) {
        message << "could not find the dataitem" << itemName << " "
                << regionName << " " << ret;
        throw Memserver_Exception(DATAITEM_NOT_FOUND, message.str().c_str());
    }

    return ALLOC_NO_ERROR;
}

/*
 * dataitem lookup for the given region name and offset.
 * Returns the Fam_Dataitem_Metadata for the given name
 */
int Memserver_Allocator::get_dataitem(uint64_t regionId, uint64_t offset,
                                      uint32_t uid, uint32_t gid,
                                      Fam_DataItem_Metadata &dataitem) {
    ostringstream message;
    message << "Error While locating dataitem : ";
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    int ret =
        metadataManager->metadata_find_dataitem(dataitemId, regionId, dataitem);
    if (ret != META_NO_ERROR) {
        message << "could not find the dataitem";
        throw Memserver_Exception(DATAITEM_NOT_FOUND, message.str().c_str());
    }

    return ALLOC_NO_ERROR;
}

/*
 * Check if the given uid/gid has read or rw permissions for
 * a given dataitem.
 */
bool Memserver_Allocator::check_dataitem_permission(
    Fam_DataItem_Metadata dataitem, bool op, uint32_t uid, uint32_t gid) {

    metadata_region_item_op_t opFlag;
    if (op)
        opFlag = META_REGION_ITEM_RW;
    else
        opFlag = META_REGION_ITEM_READ;

    return (metadataManager->metadata_check_permissions(&dataitem, opFlag, uid,
                                                        gid));
}

void *Memserver_Allocator::get_local_pointer(uint64_t regionId,
                                             uint64_t offset) {
    ostringstream message;
    message << "Error While getting localpointer to dataitem : ";
    Heap *heap = 0;
    int ret;

    HeapMap::iterator it = get_heap(regionId, heap);
    if (it == heapMap->end()) {
        ret = open_heap(regionId);
        if (ret != ALLOC_NO_ERROR) {
            message << "Opening of heap failed";
            throw Memserver_Exception(HEAP_NOT_OPENED, message.str().c_str());
        }
        it = get_heap(regionId, heap);
        if (it == heapMap->end()) {
            message << "Can not find heap in map";
            throw Memserver_Exception(NO_LOCAL_POINTER, message.str().c_str());
        }
    }
    void *localPtr;
    {
        NVMM_PROFILE_START_OPS()
        localPtr = heap->OffsetToLocal(offset);
        NVMM_PROFILE_END_OPS(Heap_OffsetToLocal)
        // Profile_Time end = NVMM_PROFILE_GET_TIME();
        // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_OffsetToLocal, total);
    }
    return localPtr;
}

int Memserver_Allocator::open_heap(uint64_t regionId) {
    ostringstream message;
    message << "Error While opening heap : ";
    Heap *heap = 0;

    // Check if the heap is already open

    get_heap(regionId, heap);
    if (heap == NULL) {

        // Heap is not open, open it now
        int ret;
        {
            NVMM_PROFILE_START_OPS()
            ret = memoryManager->FindHeap((PoolId)regionId, &heap);
            NVMM_PROFILE_END_OPS(FindHeap)
            // Profile_Time end = NVMM_PROFILE_GET_TIME();
            // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
            // NVMM_PROFILE_ADD_TO_TOTAL_OPS(FindHeap, total);
        }
        if (ret != NO_ERROR) {
            message << "heap not found";
            delete heap;
            throw Memserver_Exception(HEAP_NOT_OPENED, message.str().c_str());
        }
        {
            NVMM_PROFILE_START_OPS()
            heap->Open();
            NVMM_PROFILE_END_OPS(Heap_Open)
            // Profile_Time end = NVMM_PROFILE_GET_TIME();
            // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
            // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_Open, total);
        }
        NVMM_PROFILE_START_OPS()
        pthread_mutex_lock(&heapMapLock);
        NVMM_PROFILE_START_OPS()

        // Heap opened now, Add this into map for future references.
        auto heapObj = heapMap->find(regionId);
        if (heapObj == heapMap->end()) {
            heapMap->insert({regionId, heap});
        } else {
            message << "Can not insert heap. regionId already found in map";
            {
                NVMM_PROFILE_START_OPS()
                heap->Close();
                NVMM_PROFILE_END_OPS(Heap_Close)
                // Profile_Time end = NVMM_PROFILE_GET_TIME();
                // Profile_Time total = NVMM_PROFILE_TIME_DIFF_NS(start, end);
                // NVMM_PROFILE_ADD_TO_TOTAL_OPS(Heap_Close, total);
            }
            delete heap;
        }
        NVMM_PROFILE_END_OPS(HeapMapFindOp)
        // Profile_Time end1 = NVMM_PROFILE_GET_TIME();
        // Profile_Time total1 = NVMM_PROFILE_TIME_DIFF_NS(start2, end1);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(HeapMapFindOp, total1);
        pthread_mutex_unlock(&heapMapLock);
        NVMM_PROFILE_END_OPS(HeapMapFindTotal)
        // Profile_Time end2 = NVMM_PROFILE_GET_TIME();
        // Profile_Time total2 = NVMM_PROFILE_TIME_DIFF_NS(start1, end2);
        // NVMM_PROFILE_ADD_TO_TOTAL_OPS(HeapMapFindTotal, total2);

        return ALLOC_NO_ERROR;
    }
    return ALLOC_NO_ERROR;
}

int Memserver_Allocator::copy(uint64_t regionId, uint64_t srcOffset,
                              uint64_t srcCopyStart, uint64_t destOffset,
                              uint64_t destCopyStart, uint32_t uid,
                              uint32_t gid, size_t nbytes) {
    ostringstream message;
    message << "Error While copying from dataitem : ";
    Fam_DataItem_Metadata srcDataitem;
    Fam_DataItem_Metadata destDataitem;
    void *srcStart;
    void *destStart;

    get_dataitem(regionId, srcOffset, uid, gid, srcDataitem);

    get_dataitem(regionId, destOffset, uid, gid, destDataitem);

    if ((srcCopyStart + nbytes) < srcDataitem.size)
        srcStart = get_local_pointer(regionId, srcOffset + srcCopyStart);
    else {
        message << "Source offset or size is beyond dataitem boundary";
        throw Memserver_Exception(OUT_OF_RANGE, message.str().c_str());
    }

    if ((destCopyStart + nbytes) < destDataitem.size)
        destStart = get_local_pointer(regionId, destOffset + destCopyStart);
    else {
        message << "Destination offset or size is beyond dataitem boundary";
        throw Memserver_Exception(OUT_OF_RANGE, message.str().c_str());
    }

    if ((srcStart == NULL) || (destStart == NULL)) {
        message
            << "Failed to get local pointer to source or destination dataitem";
        throw Memserver_Exception(NULL_POINTER_ACCESS, message.str().c_str());
    } else {
        fam_memcpy(destStart, srcStart, nbytes);
        return ALLOC_NO_ERROR;
    }
}

HeapMap::iterator Memserver_Allocator::get_heap(uint64_t regionId,
                                                Heap *&heap) {
    HeapMap::iterator heapObj;
    NVMM_PROFILE_START_OPS()
    pthread_mutex_lock(&heapMapLock);
    NVMM_PROFILE_START_OPS()

    heapObj = heapMap->find(regionId);
    if (heapObj != heapMap->end()) {
        heap = heapObj->second;
    }
    NVMM_PROFILE_END_OPS(HeapMapFindOp)
    // Profile_Time end1 = NVMM_PROFILE_GET_TIME();
    // Profile_Time total1 = NVMM_PROFILE_TIME_DIFF_NS(start2, end1);
    // NVMM_PROFILE_ADD_TO_TOTAL_OPS(HeapMapFindOp, total1);
    pthread_mutex_unlock(&heapMapLock);
    NVMM_PROFILE_END_OPS(HeapMapFindTotal)
    // Profile_Time end2 = NVMM_PROFILE_GET_TIME();
    // Profile_Time total2 = NVMM_PROFILE_TIME_DIFF_NS(start1, end2);
    // NVMM_PROFILE_ADD_TO_TOTAL_OPS(HeapMapFindTotal, total2);
    return heapObj;
}

/*
 * Allocate the first free region id to be allocated.
 */
PoolId Memserver_Allocator::get_free_poolId() {
    int64_t freePoolId;

    // Find the first free bit after 10th bit in poolId bitmap.
    // First 10 nvmm ID will be reserved.
    freePoolId = bitmap_find_and_reserve(bmap, 0, MEMSERVER_REGIONID_START);
    if (freePoolId == BITMAP_NOTFOUND)
        return NO_FREE_POOLID;

    return (PoolId)freePoolId;
}

} // namespace openfam
