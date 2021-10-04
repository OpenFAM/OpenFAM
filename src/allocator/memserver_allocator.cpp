/*
 * memserver_allocator.cpp
 * Copyright (c) 2020-2021 Hewlett Packard Enterprise Development, LP. All
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
#include <libgen.h>
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

Memserver_Allocator::Memserver_Allocator(uint64_t delayed_free_threads,
                                         const char *fam_path = "") {
    MEMSERVER_PROFILE_INIT(NVMM)
    MEMSERVER_PROFILE_START_TIME(NVMM)

    if (fam_path == NULL || (strcmp(fam_path, "") == 0))
        StartNVMM();
    else
        StartNVMM(fam_path);

    num_delayed_free_threads = delayed_free_threads;
    heapMap = new HeapMap();
    memoryManager = MemoryManager::GetInstance();
    em = EpochManager::GetInstance();
    (void)pthread_mutex_init(&heapMapLock, NULL);
    for (uint64_t i = 0; i < num_delayed_free_threads; i++) {
        delayed_free_thread_array.push_back(gc_th_struct_t());
        delayed_free_thread_array[i].pthread_running = true;
        delayed_free_thread_array[i].heap_list = new HeapInfo();
        pthread_rwlock_init(&delayed_free_thread_array[i].rwLock, NULL);
    }

    uint64_t i = 0;
    for (auto itr = delayed_free_thread_array.begin();
         itr != delayed_free_thread_array.end(); i++, itr++) {
        itr->delayed_free_thread =
            std::thread(&Memserver_Allocator::delayed_free_th, this, i);
    }
}

Memserver_Allocator::~Memserver_Allocator() {
    delete heapMap;
    pthread_mutex_destroy(&heapMapLock);
    for (uint64_t i = 0; i < num_delayed_free_threads; i++) {
        delayed_free_thread_array[i].pthread_running = false;
        if (delayed_free_thread_array[i].delayed_free_thread.joinable()) {
            delayed_free_thread_array[i].delayed_free_thread.join();
        }
    }
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

void Memserver_Allocator::delayed_free_th(uint64_t thread_index) {
    gc_th_struct_t *gc_th_obj = &delayed_free_thread_array[thread_index];
    uint64_t nextId;
    while (gc_th_obj->pthread_running == true) {
        nextId = 0;
        HeapInfo::iterator itr;
        pthread_rwlock_rdlock(&gc_th_obj->rwLock);
        while ((itr = gc_th_obj->heap_list->upper_bound(nextId)) !=
               gc_th_obj->heap_list->end()) {
            nextId = itr->first;
            Fam_Heap_Info_t *heapInfo = itr->second;
            pthread_rwlock_rdlock(&heapInfo->rwLock);
            pthread_rwlock_unlock(&gc_th_obj->rwLock);
            if (heapInfo->isValid && heapInfo->heap->IsOpen()) {
                heapInfo->heap->delayed_free_fn();
            }
            pthread_rwlock_unlock(&heapInfo->rwLock);
            pthread_rwlock_rdlock(&gc_th_obj->rwLock);
        }
        pthread_rwlock_unlock(&gc_th_obj->rwLock);
        usleep(delayed_free_th_sleep_MicroSeconds);
    }
}

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

    uint64_t flags = 0;
    if (num_delayed_free_threads != 0)
        flags = NVMM_FAST_ALLOC;

    // Else Create region using NVMM and set the fields in response to
    // appropriate value

    if (nbytes < MIN_REGION_SIZE)
        tmpSize = MIN_REGION_SIZE;
    else
        tmpSize = nbytes;
    int ret;
    NVMM_PROFILE_START_OPS()
    ret = memoryManager->CreateHeap((PoolId)regionId, tmpSize, MIN_OBJ_SIZE,
                                    flags);
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
    if (num_delayed_free_threads != 0) {
        uint64_t idx = regionId % num_delayed_free_threads;
        pthread_rwlock_wrlock(&delayed_free_thread_array[idx].rwLock);
        Fam_Heap_Info_t *heapInfo = new Fam_Heap_Info_t();
        heapInfo->heap = heap;
        heapInfo->isValid = true;
        pthread_rwlock_init(&heapInfo->rwLock, NULL);
        delayed_free_thread_array[idx].heap_list->insert({regionId, heapInfo});
        pthread_rwlock_unlock(&delayed_free_thread_array[idx].rwLock);
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
        THROW_ERRNO_MSG(Memory_Service_Exception, HEAPMAP_INSERT_FAILED,
                        message.str().c_str());
    }

    pthread_mutex_unlock(&heapMapLock);
    NVMM_PROFILE_END_OPS(HeapMapInsertOp);
}
Fam_Heap_Info_t *Memserver_Allocator::remove_heap_from_list(uint64_t regionId) {
    if (num_delayed_free_threads != 0) {
        uint64_t idx = regionId % num_delayed_free_threads;
        Fam_Heap_Info_t *heapInfo = NULL;
        pthread_rwlock_wrlock(&delayed_free_thread_array[idx].rwLock);
        auto obj = delayed_free_thread_array[idx].heap_list->find(regionId);
        if (obj != delayed_free_thread_array[idx].heap_list->end()) {
            heapInfo = obj->second;
            heapInfo->isValid = false;
            delayed_free_thread_array[idx].heap_list->erase(regionId);
        }
        pthread_rwlock_unlock(&delayed_free_thread_array[idx].rwLock);
        return heapInfo;
    } else {
        return NULL;
    }
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
        Fam_Heap_Info_t *heapInfo;
        NVMM_PROFILE_START_OPS()
        pthread_mutex_lock(&heapMapLock);
        heapMap->erase(it);
        pthread_mutex_unlock(&heapMapLock);
        heapInfo = remove_heap_from_list(regionId);
        NVMM_PROFILE_END_OPS(HeapMapEraseOp)
        if (heapInfo) {
            pthread_rwlock_wrlock(&heapInfo->rwLock);
            NVMM_PROFILE_START_OPS()
            ret = heap->Close();
            NVMM_PROFILE_END_OPS(Heap_Close)
            pthread_rwlock_unlock(&heapInfo->rwLock);
            delete heapInfo;
        } else {
            NVMM_PROFILE_START_OPS()
            ret = heap->Close();
            NVMM_PROFILE_END_OPS(Heap_Close)
        }
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
            THROW_ERRNO_MSG(Memory_Service_Exception, HEAPMAP_HEAP_NOT_FOUND,
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
            THROW_ERRNO_MSG(Memory_Service_Exception, HEAPMAP_HEAP_NOT_FOUND,
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
        if (num_delayed_free_threads > 0) {
            EpochOp op(em);
            heap->Free(op, offset);
        } else {
            heap->Free(offset);
        }
        NVMM_PROFILE_END_OPS(Heap_Free)
    } else {
        // Heap not found in map. Get the heap from NVMM
        open_heap(regionId);
        it = get_heap(regionId, heap);
        if (it == heapMap->end()) {
            message << "Can not find heap in map";
            THROW_ERRNO_MSG(Memory_Service_Exception, HEAPMAP_HEAP_NOT_FOUND,
                            message.str().c_str());
        }
        NVMM_PROFILE_START_OPS()
        if (num_delayed_free_threads > 0) {
            EpochOp op(em);
            heap->Free(op, offset);
        } else {
            heap->Free(offset);
        }
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

void Memserver_Allocator::backup(uint64_t srcRegionId, uint64_t srcOffset,
                                 string outputFile, uint64_t size) {

    void *src = get_local_pointer(srcRegionId, srcOffset);
    int fileid = open((char *)outputFile.c_str(), O_RDWR | O_CREAT, 0777);
    if (fileid == -1) {
        THROW_ERRNO_MSG(Memory_Service_Exception, FAM_ERR_OUTOFRANGE,
                        "backup file creation failed.");
    }
    long pgsz = sysconf(_SC_PAGESIZE);
    unsigned long page_size = (((size + 64) + (pgsz - 1)) / pgsz) * pgsz;
    lseek(fileid, page_size - 1, SEEK_SET);
    write(fileid, "", 1);
    char *destaddr;
    destaddr = (char *)mmap(NULL, page_size, PROT_WRITE | PROT_READ, MAP_SHARED,
                            fileid, 0);

    if (destaddr == MAP_FAILED) {
        THROW_ERRNO_MSG(Memory_Service_Exception, FAM_ERR_OUTOFRANGE,
                        "mmap of file failed.");
    }
    memcpy(destaddr, src, page_size);
    msync(destaddr, page_size, MS_SYNC);
    munmap(destaddr, page_size);
    close(fileid);
}

void Memserver_Allocator::restore(uint64_t destRegionId, uint64_t destOffset,
                                  string inputFile, uint64_t size) {
    void *dest = get_local_pointer(destRegionId, destOffset);
    struct stat info;
    int exist = stat(inputFile.c_str(), &info);
    if (exist == -1) {
        THROW_ERRNO_MSG(Memory_Service_Exception, FAM_ERR_OUTOFRANGE,
                        "InputFile doesnt exist.");
    }
    //    size = info.st_size;
    int fileid = open((char *)inputFile.c_str(), O_RDONLY, 0777);
    if (fileid == -1) {
        THROW_ERRNO_MSG(Memory_Service_Exception, FAM_ERR_OUTOFRANGE,
                        "Opening of input file failed.");
    }
    long pgsz = sysconf(_SC_PAGESIZE);
    unsigned long page_size = ((size + (pgsz - 1)) / pgsz) * pgsz;
    lseek(fileid, page_size - 1, SEEK_SET);
    char *srcaddr;
    srcaddr = (char *)mmap(NULL, page_size, PROT_READ, MAP_SHARED, fileid, 0);

    if (srcaddr == MAP_FAILED) {
        THROW_ERRNO_MSG(Memory_Service_Exception, FAM_ERR_OUTOFRANGE,
                        "mmap of input file failed.");
    }

    memcpy(dest, srcaddr, page_size);
    // msync(dest, page_size, MS_SYNC | MS_INVALIDATE);
    msync(dest, page_size, MS_SYNC);
    msync(srcaddr, page_size, MS_SYNC);
    openfam_persist(dest, page_size);
    munmap(srcaddr, page_size);
    close(fileid);
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

        if (num_delayed_free_threads != 0) {
            uint64_t idx = regionId % num_delayed_free_threads;
            pthread_rwlock_wrlock(&delayed_free_thread_array[idx].rwLock);
            Fam_Heap_Info_t *heapInfo = new Fam_Heap_Info_t();
            heapInfo->heap = heap;
            heapInfo->isValid = true;
            pthread_rwlock_init(&heapInfo->rwLock, NULL);
            delayed_free_thread_array[idx].heap_list->insert(
                {regionId, heapInfo});
            pthread_rwlock_unlock(&delayed_free_thread_array[idx].rwLock);
        }
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
        THROW_ERRNO_MSG(Memory_Service_Exception, HEAPMAP_INSERT_FAILED,
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
