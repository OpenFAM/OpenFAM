/*
 * fam_local_buf_reg_helper.cpp
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

/*
 * Local buffer registration helper
 *
 * Design overview:
 * This module implements a thread-safe, lock-free memory registration cache
 * that maintains registered buffer descriptors for local memory ranges. The
 * design uses a double-buffered global map with thread-local access patterns
 * to minimize contention and provide high-performance lookups.
 * 
 * Key design choices:
 * 
 * 1. Double-buffered global map:
 *    - Uses two global maps that are alternately updated
 *    - Allows atomic publishing of updates without blocking readers
 *    - Ensures consistency during concurrent modifications
 *
 * 2. Thread-local map pointers:
 *    - Each thread maintains a local pointer to the current global map
 *    - Provides lock-free read access in the common case
 *    - Uses sequence numbers to detect map updates
 *
 * 3. Shared ownership model:
 *    - RegisteredBuffer objects use shared_ptr for automatic cleanup
 *    - Memory registration descriptors remain valid across map updates
 *    - Reference counting ensures resources are freed when no longer needed
 * 
 * Thread safety:
 * - Map updates are protected by globalBufMapMutex
 * - Read operations are lock-free using thread-local cached pointers
 * - Sequence-based synchronization ensures consistency during updates
 */

#include <sched.h>
#include <sstream>

#include "common/fam_internal_exception.h"
#include "common/fam_libfabric.h"
#include "common/fam_local_buf_reg_helper.h"

namespace openfam {

std::mutex globalBufMapMutex;
size_t currentBufMapIndex = 0;
std::vector<MapPtr> globalBufMaps;
size_t numGlobalBufMaps = 2;

std::vector<SequencedMapPtrEntry> perThreadMapPtrList;

thread_local SequencedMapPtr *sequencedMapPtr = nullptr;

std::shared_ptr<RegisteredBuffer> create_new_buffer(void *base, size_t len,
                                                    struct fid_domain *domain,
                                                    size_t iov_limit) {
    std::ostringstream message;
    int ret;

    struct fid_mr *new_mr = NULL;

    void **new_mr_descs = (void **)calloc(iov_limit, sizeof(*new_mr_descs));
    if (!new_mr_descs) {
        message
            << "create_new_buffer failed to allocate memory for new mr_descs";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    ret = fi_mr_reg(domain, base, len, FI_READ | FI_WRITE, 0, 0, 0, &new_mr, 0);
    if (ret < 0) {
        free(new_mr_descs);
        message << "create_new_buffer fi_mr_reg failed: "
                << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    for (size_t i = 0; i < iov_limit; i++)
        new_mr_descs[i] = fi_mr_desc(new_mr);

    return std::make_shared<RegisteredBuffer>(new_mr_descs, new_mr,
                                              (size_t)base, (size_t)base + len);
}

MapPtr thread_acquire_local_mapptr() {
    if (!sequencedMapPtr)
        sequencedMapPtr = new SequencedMapPtr();
    return sequencedMapPtr->get();
}

void thread_release_local_mapptr() { sequencedMapPtr->put(); }

void publish_map(std::function<void(MapPtr)> op) {
    std::ostringstream message;

    // globalBufMapMutex must be locked.
    // Update the next map and publish it to all threads
    currentBufMapIndex = (currentBufMapIndex + 1) % numGlobalBufMaps;
    auto mapPtr = globalBufMaps[currentBufMapIndex];
    op(mapPtr);
    // Get the map size for a sanity check later.
    auto map_size = mapPtr->size();
    // Atomically update map in all threads and gather sequence numbers.
    SequencedMapPtrEntry *first = nullptr;
    auto prev = &first;
    for (auto &perThreadMapPtrEntry : perThreadMapPtrList) {
        auto perThreadMapPtr = perThreadMapPtrEntry.perThreadMapPtr;
        perThreadMapPtr->set_map(mapPtr);
        perThreadMapPtrEntry.sequence = perThreadMapPtr->get_sequence();
        if (perThreadMapPtrEntry.sequence & 1) {
            // Was in flight: need to make sure old map is not in use.
            *prev = &perThreadMapPtrEntry;
            prev = &perThreadMapPtrEntry.next;
        }
    }

    *prev = nullptr;
    // Now wait for all threads to finish using the old map.
    for (size_t i = 0, last_yield = 0; first;) {
        for (prev = &first; *prev; i++) {
            auto &perThreadMapPtrEntry = **prev;
            auto perThreadMapPtr = perThreadMapPtrEntry.perThreadMapPtr;
            assert(perThreadMapPtrEntry.sequence & 1);
            // Was in flight: need to make sure old map is not in use.
            if (perThreadMapPtr->get_sequence() !=
                perThreadMapPtrEntry.sequence) {
                // Remove from list, don't advance prev.
                *prev = perThreadMapPtrEntry.next;
                continue;
            }
            // Advance prev to the current next pointer.
            prev = &perThreadMapPtrEntry.next;
        }
        if (i - last_yield >= 1000) {
            // Yield to other threads
            sched_yield();
            last_yield = i;
        }
    }

    // Update the other maps
    auto mapIndex = currentBufMapIndex;
    for (size_t i = 1; i < numGlobalBufMaps; ++i) {
        mapIndex = (mapIndex + 1) % numGlobalBufMaps;
        mapPtr = globalBufMaps[mapIndex];
        op(mapPtr);
        if (mapPtr->size() != map_size) {
            message
                << "publish_map failed: Map sizes do not match after update.";
            THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
        }
    }
}

std::pair<int, RegisteredBuffer *> test_overlap(MapPtr mapPtr, size_t start,
                                                size_t end) {
    if (start >= end) {
        // Invalid memory range: start must be less than end
        return std::make_pair(-2, nullptr);
    }

    if (!mapPtr->size()) {
        // Map is empty: nothing overlaps or contains
        return std::make_pair(0, nullptr);
    }

    auto lower_start = mapPtr->lower_bound(start);
    if (lower_start == mapPtr->end()) {
        // Back up to last valid entry.
        lower_start--;
    } else if (start < lower_start->first) {
        if (end > lower_start->first) {
            // Something must overlap.
            return std::make_pair(-1, lower_start->second.get());
        }
        if (lower_start == mapPtr->begin()) {
            // end before first entry: nothing overlaps or contains
            return std::make_pair(0, nullptr);
        }
        lower_start--;
    }

    // Now start >= lower_start->first
    auto regBuf = lower_start->second.get();
    auto regEnd = regBuf->end_;
    if (end <= regEnd) {
        // Contains
        return std::make_pair(1, regBuf);
    }
    if (start < regEnd) {
        // Overlaps
        return std::make_pair(-1, regBuf);
    }

    return std::make_pair(0, nullptr);
}

} // namespace openfam
