/*
 * memserver_allocator.h
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
#ifndef MEMSERVER_ALLOCATOR_H_
#define MEMSERVER_ALLOCATOR_H_

#include <iostream>
#include <pthread.h>
#include <sys/types.h> // needed for mode_t
#include <thread>

#include <boost/atomic.hpp>
#include <nvmm/error_code.h>
#include <nvmm/global_ptr.h>
#include <nvmm/heap.h>
#include <nvmm/log.h>
#include <nvmm/memory_manager.h>
#include <nvmm/shelf_id.h>

#include "bitmap-manager/bitmap.h"
#include "common/fam_internal.h"
#include "common/fam_internal_exception.h"
#include "fam/fam.h"

#define MIN_OBJ_SIZE 128
#define MIN_REGION_SIZE (1UL << 20)

using namespace std;
using namespace nvmm;

namespace openfam {
typedef struct Fam_Heap_Info {
    Heap *heap;
    bool isValid;
    pthread_rwlock_t rwLock;
} Fam_Heap_Info_t;
using HeapInfo = std::map<uint64_t, Fam_Heap_Info_t *>;
using HeapMap = std::map<uint64_t, Heap *>;
typedef struct gc_th_struct {
    std::thread delayed_free_thread;
    // int num_heaps;
    bool pthread_running;
    HeapInfo *heap_list;
    pthread_rwlock_t rwLock;
} gc_th_struct_t;

class Memserver_Allocator {
  public:
    Memserver_Allocator(uint64_t num_delayed_free_threads,
                        const char *fam_path);
    ~Memserver_Allocator();
    void memserver_allocator_finalize();
    void reset_profile();
    void dump_profile();
    void create_region(uint64_t regionId, size_t nbytes);
    void destroy_region(uint64_t regionId);
    void resize_region(uint64_t regionId, size_t nbytes);
    uint64_t allocate(uint64_t regionId, size_t nbytes);
    void deallocate(uint64_t regionId, uint64_t offset);
    void copy(uint64_t srcRegionId, uint64_t srcOffset, uint64_t destRegionId,
              uint64_t destOffset, uint64_t size);
    void *get_local_pointer(uint64_t regionId, uint64_t offset);
    void open_heap(uint64_t regionId);
    void create_ATL_root(size_t nbytes);
    Fam_Heap_Info_t *remove_heap_from_list(uint64_t regionId);
    void delayed_free_th(uint64_t thread_index);

  private:
    MemoryManager *memoryManager;
    EpochManager *em;
    HeapMap *heapMap;
    pthread_mutex_t heapMapLock;
    HeapMap::iterator get_heap(uint64_t regionId, Heap *&heap);
    PoolId get_free_poolId();
    bitmap *bmap;
    void init_poolId_bmap();
    uint64_t num_delayed_free_threads;
    static uint64_t const delayed_free_th_sleep_MicroSeconds = 1000;
    std::vector<gc_th_struct_t> delayed_free_thread_array;
};

} // namespace openfam

#endif /* end of MEMSERVER_ALLOCATOR_H_ */
