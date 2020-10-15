/*
 * memserver_allocator.cpp
 * Copyright (c) 2019-2020 Hewlett Packard Enterprise Development, LP. All
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
#include "allocator/memserver_allocator.h"
#include <boost/atomic.hpp>
#include <chrono>
#include <iomanip>
#include <string.h>
#include <unistd.h>

#include "common/atomic_queue.h"
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

Memserver_Allocator::Memserver_Allocator(const char *fam_path = "") {
    MEMSERVER_PROFILE_INIT(NVMM)
    MEMSERVER_PROFILE_START_TIME(NVMM)

    if (fam_path == NULL || (strcmp(fam_path, "") == 0))
        StartNVMM();
    else
        StartNVMM(fam_path);

    heapMap = new HeapMap();
    memoryManager = MemoryManager::GetInstance();
    (void)pthread_mutex_init(&heapMapLock, NULL);
}

Memserver_Allocator::~Memserver_Allocator() {
    delete heapMap;
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
}
void Memserver_Allocator::dump_profile() { NVMM_PROFILE_DUMP(); }
/*

 * Create a new region.
 * name - name of the region
 * regionId - return the regionId allocated by this routine
 * nbytes - size of region in bytes
 * permission - Permission for the region
 * uid/gid - user id and group id
 */
void Memserver_Allocator::create_region(uint64_t regionId, size_t nbytes) {
    ostringstream message;
    message << "Error While creating region : ";

    // Obtain free regionId from NVMM
    size_t tmpSize;

    // TODO: Obtain free regionId from NVMM
    if (regionId == (PoolId)BITMAP_NOTFOUND) {
        message << "No free pool ID";
        THROW_ERRNO_MSG(Memory_Service_Exception, NO_FREE_POOLID,
                        message.str().c_str());
    }

    // Else Create region using NVMM and set the fields in response to
    // appropriate value

    if (nbytes < MIN_REGION_SIZE)
        tmpSize = MIN_REGION_SIZE;
    else
        tmpSize = nbytes;
    int ret;
    NVMM_PROFILE_START_OPS()
    ret = memoryManager->CreateHeap((PoolId)regionId, tmpSize, MIN_OBJ_SIZE);
    NVMM_PROFILE_END_OPS(CreateHeap)

    if (ret != NO_ERROR) {
        message << "Heap not created";
        THROW_ERRNO_MSG(Memory_Service_Exception, HEAP_NOT_CREATED,
                        message.str().c_str());
    }
    Heap *heap = 0;
    NVMM_PROFILE_START_OPS()
    ret = memoryManager->FindHeap((PoolId)regionId, &heap);
    NVMM_PROFILE_END_OPS(FindHeap)
    if (ret != NO_ERROR) {
        message << "Heap not found";
        delete heap;
        THROW_ERRNO_MSG(Memory_Service_Exception, HEAP_NOT_FOUND,
                        message.str().c_str());
    }
    NVMM_PROFILE_START_OPS()
    ret = heap->Open(NVMM_NO_BG_THREAD);
    NVMM_PROFILE_END_OPS(Heap_Open)
    if (ret != NO_ERROR) {
        message << "Can not open heap";
        delete heap;
        THROW_ERRNO_MSG(Memory_Service_Exception, HEAP_NOT_OPENED,
                        message.str().c_str());
    }

    NVMM_PROFILE_START_OPS()
    pthread_mutex_lock(&heapMapLock);

    auto heapObj = heapMap->find(regionId);
    if (heapObj == heapMap->end()) {
        heapMap->insert({regionId, heap});
    } else {
        message << "Can not insert heap. regionId already found in map";
        pthread_mutex_unlock(&heapMapLock);
        delete heap;
        NVMM_PROFILE_START_OPS()
        ret = memoryManager->DestroyHeap((PoolId)regionId);
        NVMM_PROFILE_END_OPS(DestroyHeap)
        if (ret != NO_ERROR) {
            message << "Can not destroy heap";
        }
        THROW_ERRNO_MSG(Memory_Service_Exception, RBT_HEAP_NOT_INSERTED,
                        message.str().c_str());
    }

    pthread_mutex_unlock(&heapMapLock);
    NVMM_PROFILE_END_OPS(HeapMapInsertOp);
}

/*
 * destroy a region
 * regionId - region Id of the region to be destroyed
 * uid - user id of the process
 * gid - group id of the process
 */
void Memserver_Allocator::destroy_region(uint64_t regionId) {
    ostringstream message;
    message << "Error While destroy region : ";
    int ret;
    // destroy region using NVMM
    // Even if heap is not found in map, continue with DestroyHeap
    Heap *heap = 0;

    HeapMap::iterator it = get_heap(regionId, heap);

    if (it != heapMap->end()) {
        NVMM_PROFILE_START_OPS()
        pthread_mutex_lock(&heapMapLock);
        heapMap->erase(it);
        pthread_mutex_unlock(&heapMapLock);
        NVMM_PROFILE_END_OPS(HeapMapEraseOp)
        NVMM_PROFILE_START_OPS()
        ret = heap->Close();
        NVMM_PROFILE_END_OPS(Heap_Close)
        if (ret != NO_ERROR) {
            message << "Can not close heap";
            THROW_ERRNO_MSG(Memory_Service_Exception, HEAP_NOT_CLOSED,
                            message.str().c_str());
        }
        delete heap;
    }

    NVMM_PROFILE_START_OPS()
    ret = memoryManager->DestroyHeap((PoolId)regionId);
    NVMM_PROFILE_END_OPS(DestroyHeap)
    if (ret != NO_ERROR) {
        message << "Can not destroy heap";
        THROW_ERRNO_MSG(Memory_Service_Exception, HEAP_NOT_DESTROYED,
                        message.str().c_str());
    }

}

/*
 * Resize the region
 * regionId - region Id of the region to be resized.
 * nbytes - new size of the region
 */
void Memserver_Allocator::resize_region(uint64_t regionId, size_t nbytes) {
    ostringstream message;
    message << "Error while resizing the region";
    int ret;
    // Get the heap and open it if not open already
    Heap *heap = 0;
    HeapMap::iterator it = get_heap(regionId, heap);

    if (it == heapMap->end()) {
        open_heap(regionId);
        it = get_heap(regionId, heap);
        if (it == heapMap->end()) {
            message << "Can not find heap in map";
            THROW_ERRNO_MSG(Memory_Service_Exception, RBT_HEAP_NOT_FOUND,
                            message.str().c_str());
        }
    }

    // Call NVMM to resize the heap
    NVMM_PROFILE_START_OPS()
    ret = heap->Resize(nbytes);
    NVMM_PROFILE_END_OPS(Heap_Resize)
    if (ret != NO_ERROR) {
        message << "heap resize failed";
        THROW_ERRNO_MSG(Memory_Service_Exception, RESIZE_FAILED,
                        message.str().c_str());
    }
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
uint64_t Memserver_Allocator::allocate(uint64_t regionId, size_t nbytes) {
    ostringstream message;
    message << "Error While allocating dataitem : ";
    size_t tmpSize;
    uint64_t offset;
    // Call NVMM to create a new data item
    Heap *heap = 0;

    HeapMap::iterator it = get_heap(regionId, heap);
    if (it == heapMap->end()) {
        open_heap(regionId);
        it = get_heap(regionId, heap);
        if (it == heapMap->end()) {
            message << "Can not find heap in map";
            THROW_ERRNO_MSG(Memory_Service_Exception, RBT_HEAP_NOT_FOUND,
                            message.str().c_str());
        }
    }

    // If the requested siz is lessar than MIN_OBJ_SIZE,
    // allocate data item of size MIN_OBJ_SIZE
    if (nbytes < MIN_OBJ_SIZE)
        tmpSize = MIN_OBJ_SIZE;
    else
        tmpSize = nbytes;
    NVMM_PROFILE_START_OPS()
    offset = heap->AllocOffset(tmpSize);
    NVMM_PROFILE_END_OPS(Heap_AllocOffset)
    if (!offset) {
        try {
            {
                NVMM_PROFILE_START_OPS()
                heap->Merge();
                NVMM_PROFILE_END_OPS(Heap_Merge)
            }
        } catch (...) {
            message << "Heap Merge() failed";
            THROW_ERRNO_MSG(Memory_Service_Exception, HEAP_MERGE_FAILED,
                            message.str().c_str());
        }
        {
            NVMM_PROFILE_START_OPS()
            offset = heap->AllocOffset(tmpSize);
            NVMM_PROFILE_END_OPS(Heap_AllocOffset)
        }
        if (!offset) {
            message << "alloc() failed";
            THROW_ERRNO_MSG(Memory_Service_Exception, HEAP_ALLOCATE_FAILED,
                            message.str().c_str());
        }
    }

    return offset;
}

/*
 *
 * Deallocates a dataitem in a region at the given offset.
 */
void Memserver_Allocator::deallocate(uint64_t regionId, uint64_t offset) {
    ostringstream message;
    message << "Error While deallocating dataitem : ";
    // call NVMM to destroy the data item
    Heap *heap = 0;

    HeapMap::iterator it = get_heap(regionId, heap);
    if (it != heapMap->end()) {
        NVMM_PROFILE_START_OPS()
        heap->Free(offset);
        NVMM_PROFILE_END_OPS(Heap_Free)
    } else {
        // Heap not found in map. Get the heap from NVMM
        open_heap(regionId);
        it = get_heap(regionId, heap);
        if (it == heapMap->end()) {
            message << "Can not find heap in map";
            THROW_ERRNO_MSG(Memory_Service_Exception, RBT_HEAP_NOT_FOUND,
                            message.str().c_str());
        }
        NVMM_PROFILE_START_OPS()
        heap->Free(offset);
        NVMM_PROFILE_END_OPS(Heap_Free)
    }
}

void Memserver_Allocator::copy(uint64_t srcRegionId, uint64_t srcOffset,
                               uint64_t destRegionId, uint64_t destOffset,
                               uint64_t size) {
    void *src = get_local_pointer(srcRegionId, srcOffset);
    void *dest = get_local_pointer(destRegionId, destOffset);

    fam_memcpy(dest, src, size);
}

void *Memserver_Allocator::get_local_pointer(uint64_t regionId,
                                             uint64_t offset) {
    ostringstream message;
    message << "Error While getting localpointer to dataitem : ";
    Heap *heap = 0;

    HeapMap::iterator it = get_heap(regionId, heap);
    if (it == heapMap->end()) {
        open_heap(regionId);
        it = get_heap(regionId, heap);
        if (it == heapMap->end()) {
            message << "Can not find heap in map";
            THROW_ERRNO_MSG(Memory_Service_Exception, NO_LOCAL_POINTER,
                            message.str().c_str());
        }
    }
    void *localPtr;
    NVMM_PROFILE_START_OPS()
    localPtr = heap->OffsetToLocal(offset);
    NVMM_PROFILE_END_OPS(Heap_OffsetToLocal)
    return localPtr;
}

void Memserver_Allocator::open_heap(uint64_t regionId) {
    ostringstream message;
    message << "Error While opening heap : ";
    Heap *heap = 0;

    // Check if the heap is already open

    get_heap(regionId, heap);
    if (heap == NULL) {

        // Heap is not open, open it now
        int ret;
        NVMM_PROFILE_START_OPS()
        ret = memoryManager->FindHeap((PoolId)regionId, &heap);
        NVMM_PROFILE_END_OPS(FindHeap)
        if (ret != NO_ERROR) {
            message << "heap not found";
            delete heap;
            THROW_ERRNO_MSG(Memory_Service_Exception, HEAP_NOT_OPENED,
                            message.str().c_str());
        }
        NVMM_PROFILE_START_OPS()
        heap->Open(NVMM_NO_BG_THREAD);
        NVMM_PROFILE_END_OPS(Heap_Open)
        NVMM_PROFILE_START_OPS()
        pthread_mutex_lock(&heapMapLock);

        // Heap opened now, Add this into map for future references.
        auto heapObj = heapMap->find(regionId);
        if (heapObj == heapMap->end()) {
            heapMap->insert({regionId, heap});
            pthread_mutex_unlock(&heapMapLock);
        } else {
            pthread_mutex_unlock(&heapMapLock);
            message << "Can not insert heap. regionId already found in map";
            NVMM_PROFILE_START_OPS()
            heap->Close();
            NVMM_PROFILE_END_OPS(Heap_Close)
            delete heap;
        }
        NVMM_PROFILE_END_OPS(HeapMapFindOp)
    }
}

HeapMap::iterator Memserver_Allocator::get_heap(uint64_t regionId,
                                                Heap *&heap) {
    HeapMap::iterator heapObj;
    NVMM_PROFILE_START_OPS()
    pthread_mutex_lock(&heapMapLock);

    heapObj = heapMap->find(regionId);
    if (heapObj != heapMap->end()) {
        heap = heapObj->second;
    }

    pthread_mutex_unlock(&heapMapLock);
    NVMM_PROFILE_END_OPS(HeapMapFindOp)
    return heapObj;
}

/*
 create the fixed region for ATL with ID ATOMIC_REGION_ID
*/
void Memserver_Allocator::create_ATL_root(size_t nbytes) {
    ostringstream message;
    message << "Error While creating region : ";

    size_t tmpSize;
    bool regionATLexists = true;
    int ret;
    GlobalPtr ATLroot;
    PoolId poolId = ATOMIC_REGION_ID;
    ATLroot = memoryManager->GetATLRegionRootPtr(ATL_REGION_DATA);
    if (ATLroot == 0) { // region doesn't exists..create it
        regionATLexists = false;

        if (nbytes < MIN_REGION_SIZE)
            tmpSize = MIN_REGION_SIZE;
        else
            tmpSize = nbytes;
        int ret;
        NVMM_PROFILE_START_OPS()
        ret = memoryManager->CreateHeap(poolId, tmpSize, MIN_OBJ_SIZE);
        NVMM_PROFILE_END_OPS(CreateHeap)
        if (ret == ID_FOUND) {
            message << "Heap already exists";
        }
        if ((ret != NO_ERROR) && (ret != ID_FOUND)) {
            message << "Heap not created";
            THROW_ERRNO_MSG(Memory_Service_Exception, HEAP_NOT_CREATED,
                            message.str().c_str());
        }
    }
    Heap *heap = 0;
    NVMM_PROFILE_START_OPS()
    ret = memoryManager->FindHeap(poolId, &heap);
    NVMM_PROFILE_END_OPS(FindHeap)
    if (ret != NO_ERROR) {
        message << "Heap not found";
        delete heap;
        THROW_ERRNO_MSG(Memory_Service_Exception, HEAP_NOT_FOUND,
                        message.str().c_str());
    }
    NVMM_PROFILE_START_OPS()
    ret = heap->Open(NVMM_NO_BG_THREAD);
    NVMM_PROFILE_END_OPS(Heap_Open)
    if (ret != NO_ERROR) {
        message << "Can not open heap";
        delete heap;
        THROW_ERRNO_MSG(Memory_Service_Exception, HEAP_NOT_OPENED,
                        message.str().c_str());
    }

    uint64_t regionId = (uint64_t)poolId;
    NVMM_PROFILE_START_OPS()
    pthread_mutex_lock(&heapMapLock);

    auto heapObj = heapMap->find(regionId);
    if (heapObj == heapMap->end()) {
        heapMap->insert({regionId, heap});
    } else {
        message << "Can not insert heap. regionId already found in map";
        pthread_mutex_unlock(&heapMapLock);
        delete heap;
        NVMM_PROFILE_START_OPS()
        ret = memoryManager->DestroyHeap((PoolId)regionId);
        NVMM_PROFILE_END_OPS(DestroyHeap)
        if (ret != NO_ERROR) {
            message << "Can not destroy heap";
        }
        THROW_ERRNO_MSG(Memory_Service_Exception, RBT_HEAP_NOT_INSERTED,
                        message.str().c_str());
    }

    pthread_mutex_unlock(&heapMapLock);
    NVMM_PROFILE_END_OPS(HeapMapInsertOp);

    if (!regionATLexists) {
        ATLroot = heap->Alloc(sizeof(uint64_t) * MAX_ATOMIC_THREADS);
        assert(ATLroot.IsValid() == true);
        ATLroot = memoryManager->SetATLRegionRootPtr(ATL_REGION_DATA, ATLroot);
    }

    atomicRegionIdRoot = memoryManager->GlobalToLocal(ATLroot);
    if (!regionATLexists) {
        memset(atomicRegionIdRoot, 0, sizeof(uint64_t) * MAX_ATOMIC_THREADS);
        openfam_persist(atomicRegionIdRoot,
                        sizeof(uint64_t) * MAX_ATOMIC_THREADS);
    }
}

} // namespace openfam
