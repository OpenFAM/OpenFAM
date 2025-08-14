/*
 * fam_local_buf_reg_helper.h
 * Copyright (c) 2025 Hewlett Packard Enterprise Development, LP. All
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
#ifndef FAM_LOCAL_BUF_REG_HELPER_H
#define FAM_LOCAL_BUF_REG_HELPER_H

#include <atomic>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>

namespace openfam {

class RegisteredBuffer {
  public:
    RegisteredBuffer(void **mr_descs, struct fid_mr *mr, size_t start,
                     size_t end)
        : mr_descs_(mr_descs), mr_(mr), start_(start), end_(end){};

    ~RegisteredBuffer() {
        free(mr_descs_);
        fi_close(&mr_->fid);
    };

    void **mr_descs_;
    struct fid_mr *mr_;
    size_t start_, end_;
};

using MapType = std::map<size_t, std::shared_ptr<RegisteredBuffer>>;
using MapPtr = MapType *;

extern std::mutex globalBufMapMutex;
extern size_t currentBufMapIndex;
extern std::vector<MapPtr> globalBufMaps;
extern size_t numGlobalBufMaps;

class SequencedMapPtr;
struct SequencedMapPtrEntry {
    SequencedMapPtrEntry(SequencedMapPtr *perThreadMapPtr)
        : perThreadMapPtr(perThreadMapPtr), sequence(0), next(0) {}
    SequencedMapPtr *perThreadMapPtr;
    uint64_t sequence;
    SequencedMapPtrEntry *next;
};

extern std::vector<SequencedMapPtrEntry> perThreadMapPtrList;

#if __cplusplus >= 201703L
struct alignas(64) AlignedSequenceMapPtrData {
#else
struct AlignedSequenceMapPtrData {
#endif
    std::atomic<uint64_t> sequence;
    std::atomic<MapPtr> mapPtr;
};

static void thread_inc_sequence(std::atomic<uint64_t> &sequence) {
    // Bump sequence. Only updated in thread, so no need for
    // fetch_add() locking, but we do care about atomic store ordering and
    // tearing with the thread updating the map ptr.
    auto new_seq = sequence.load(std::memory_order_relaxed) + 1;
    sequence.store(new_seq, std::memory_order_seq_cst);
}

class SequencedMapPtr {
  public:
    SequencedMapPtr() {
        data_.sequence.store(0, std::memory_order_seq_cst);
        data_.mapPtr.store(globalBufMaps[currentBufMapIndex],
                           std::memory_order_seq_cst);
        // Add this structure to the global list of structures.
        std::unique_lock<std::mutex> lock(globalBufMapMutex);
        perThreadMapPtrList.push_back(SequencedMapPtrEntry(this));
    }

    ~SequencedMapPtr() {
        std::unique_lock<std::mutex> lock(globalBufMapMutex);
        for (auto it = perThreadMapPtrList.begin();
             it != perThreadMapPtrList.end(); ++it) {
            if (it->perThreadMapPtr == this) {
                perThreadMapPtrList.erase(it);
                break;
            }
        }
    }

    MapPtr get() {
        thread_inc_sequence(data_.sequence);
        return data_.mapPtr.load(std::memory_order_seq_cst);
    }

    void put() { thread_inc_sequence(data_.sequence); }

    uint64_t get_sequence() {
        return data_.sequence.load(std::memory_order_seq_cst);
    }

    void set_map(MapPtr mapPtr) {
        data_.mapPtr.store(mapPtr, std::memory_order_seq_cst);
    }

  private:
    AlignedSequenceMapPtrData data_;
};

extern thread_local SequencedMapPtr *sequencedMapPtr;

std::shared_ptr<RegisteredBuffer> create_new_buffer(void *base, size_t len,
                                                    struct fid_domain *domain,
                                                    size_t iov_limit);
MapPtr thread_acquire_local_mapptr();
void thread_release_local_mapptr();
void publish_map(std::function<void(MapPtr)> op);
std::pair<int, RegisteredBuffer *> test_overlap(MapPtr mapPtr, size_t start,
                                                size_t end);

} // namespace openfam
#endif