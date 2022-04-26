/*
 * fam_ops_shm.h
 * Copyright (c) 2019-2021 Hewlett Packard Enterprise Development, LP. All
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
#ifndef FAM_OPS_SHM_H
#define FAM_OPS_SHM_H

#include <iostream>
#include <map>
#include <string.h>
#include <sys/uio.h>
#include <thread>
#include <vector>

#include <nvmm/fam.h>

#include "allocator/fam_allocator_client.h"
#include "common/fam_async_qhandler.h"
#include "common/fam_context.h"
#include "common/fam_ops.h"
#include "fam/fam.h"

enum { FAM_MIN = 0, FAM_MAX, FAM_BOR, FAM_BAND, FAM_BXOR, FAM_SUM };

enum { INT32 = 0, UINT32, INT64, UINT64, FLOAT, DOUBLE };

using namespace std;
namespace openfam {
class Fam_Ops_SHM : public Fam_Ops {
  public:
    Fam_Ops_SHM(Fam_Thread_Model famTM, Fam_Context_Model famCM,
                Fam_Allocator_Client *famAlloc, uint64_t numConsumer);
    ~Fam_Ops_SHM();

    int initialize();

    void finalize();

    void reset_profile() {}

    void dump_profile() {}

    void abort(int status);

    void populate_address_vector(void *memServerInfoBuffer = NULL,
                                 size_t memServerInfoSize = 0,
                                 uint64_t numMemNodes = 0, uint64_t myId = 0) {
        return;
    }

    size_t get_fabric_iov_limit() { return 0; }

    Fam_Context *get_context(Fam_Descriptor *descriptor);
    int put_blocking(void *local, Fam_Descriptor *descriptor, uint64_t offset,
                     uint64_t nbytes);
    int get_blocking(void *local, Fam_Descriptor *descriptor, uint64_t offset,
                     uint64_t nbytes);
    int gather_blocking(void *local, Fam_Descriptor *descriptor,
                        uint64_t nElements, uint64_t firstElement,
                        uint64_t stride, uint64_t elementSize);

    int gather_blocking(void *local, Fam_Descriptor *descriptor,
                        uint64_t nElements, uint64_t *elementIndex,
                        uint64_t elementSize);

    int scatter_blocking(void *local, Fam_Descriptor *descriptor,
                         uint64_t nElements, uint64_t firstElement,
                         uint64_t stride, uint64_t elementSize);

    int scatter_blocking(void *local, Fam_Descriptor *descriptor,
                         uint64_t nElements, uint64_t *elementIndex,
                         uint64_t elementSize);
    void put_nonblocking(void *local, Fam_Descriptor *descriptor,
                         uint64_t offset, uint64_t nbytes);

    void get_nonblocking(void *local, Fam_Descriptor *descriptor,
                         uint64_t offset, uint64_t nbytes);

    void gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                            uint64_t nElements, uint64_t firstElement,
                            uint64_t stride, uint64_t elementSize);

    void gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                            uint64_t nElements, uint64_t *elementIndex,
                            uint64_t elementSize);

    void scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t nElements, uint64_t firstElement,
                             uint64_t stride, uint64_t elementSize);

    void scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t nElements, uint64_t *elementIndex,
                             uint64_t elementSize);

    void *copy(Fam_Descriptor *src, uint64_t srcOffset, Fam_Descriptor *dest,
               uint64_t destOffset, uint64_t nbytes);

    void wait_for_copy(void *waitObj);
    void *backup(Fam_Descriptor *desc, const char *BackupName);
    void *restore(const char *BackupName, Fam_Descriptor *dest);
    void wait_for_backup(void *waitObj);
    void wait_for_restore(void *waitObj);

    void fence(Fam_Region_Descriptor *descriptor = NULL);

    void quiet(Fam_Region_Descriptor *descriptor = NULL);
    uint64_t progress();
    void check_progress(Fam_Region_Descriptor *descriptor = NULL);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                    int128_t value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                    uint32_t value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                    uint64_t value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset, double value);

    void atomic_add(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void atomic_add(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                    uint32_t value);
    void atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                    uint64_t value);
    void atomic_add(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void atomic_add(Fam_Descriptor *descriptor, uint64_t offset, double value);

    void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                         int32_t value);
    void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                         int64_t value);
    void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                         uint32_t value);
    void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                         uint64_t value);
    void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                         float value);
    void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                         double value);

    void atomic_min(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void atomic_min(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                    uint32_t value);
    void atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                    uint64_t value);
    void atomic_min(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void atomic_min(Fam_Descriptor *descriptor, uint64_t offset, double value);

    void atomic_max(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void atomic_max(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                    uint32_t value);
    void atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                    uint64_t value);
    void atomic_max(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void atomic_max(Fam_Descriptor *descriptor, uint64_t offset, double value);

    void atomic_and(Fam_Descriptor *descriptor, uint64_t offset,
                    uint32_t value);
    void atomic_and(Fam_Descriptor *descriptor, uint64_t offset,
                    uint64_t value);

    void atomic_or(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void atomic_or(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);

    void atomic_xor(Fam_Descriptor *descriptor, uint64_t offset,
                    uint32_t value);
    void atomic_xor(Fam_Descriptor *descriptor, uint64_t offset,
                    uint64_t value);

    int32_t swap(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    int64_t swap(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    uint32_t swap(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    uint64_t swap(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);
    float swap(Fam_Descriptor *descriptor, uint64_t offset, float value);
    double swap(Fam_Descriptor *descriptor, uint64_t offset, double value);

    int32_t compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                         int32_t oldValue, int32_t newValue);
    int64_t compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                         int64_t oldValue, int64_t newValue);
    uint32_t compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                          uint32_t oldValue, uint32_t newValue);
    uint64_t compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                          uint64_t oldValue, uint64_t newValue);
    int128_t compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                          int128_t oldValue, int128_t newValue);

    int32_t atomic_fetch_int32(Fam_Descriptor *descriptor, uint64_t offset);
    int64_t atomic_fetch_int64(Fam_Descriptor *descriptor, uint64_t offset);
    int128_t atomic_fetch_int128(Fam_Descriptor *descriptor, uint64_t offset);
    uint32_t atomic_fetch_uint32(Fam_Descriptor *descriptor, uint64_t offset);
    uint64_t atomic_fetch_uint64(Fam_Descriptor *descriptor, uint64_t offset);
    float atomic_fetch_float(Fam_Descriptor *descriptor, uint64_t offset);
    double atomic_fetch_double(Fam_Descriptor *descriptor, uint64_t offset);

    int32_t atomic_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                             int32_t value);
    int64_t atomic_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                             int64_t value);
    uint32_t atomic_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                              uint32_t value);
    uint64_t atomic_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                              uint64_t value);
    float atomic_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                           float value);
    double atomic_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                            double value);

    int32_t atomic_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                  int32_t value);
    int64_t atomic_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                  int64_t value);
    uint32_t atomic_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value);
    uint64_t atomic_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value);
    float atomic_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                float value);
    double atomic_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                 double value);

    int32_t atomic_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                             int32_t value);
    int64_t atomic_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                             int64_t value);
    uint32_t atomic_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                              uint32_t value);
    uint64_t atomic_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                              uint64_t value);
    float atomic_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                           float value);
    double atomic_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                            double value);

    int32_t atomic_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                             int32_t value);
    int64_t atomic_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                             int64_t value);
    uint32_t atomic_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                              uint32_t value);
    uint64_t atomic_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                              uint64_t value);
    float atomic_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                           float value);
    double atomic_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                            double value);
    void *context_open();
    void context_close(void *);

    uint32_t atomic_fetch_and(Fam_Descriptor *descriptor, uint64_t offset,
                              uint32_t value);
    uint64_t atomic_fetch_and(Fam_Descriptor *descriptor, uint64_t offset,
                              uint64_t value);

    uint32_t atomic_fetch_or(Fam_Descriptor *descriptor, uint64_t offset,
                             uint32_t value);
    uint64_t atomic_fetch_or(Fam_Descriptor *descriptor, uint64_t offset,
                             uint64_t value);

    uint32_t atomic_fetch_xor(Fam_Descriptor *descriptor, uint64_t offset,
                              uint32_t value);
    uint64_t atomic_fetch_xor(Fam_Descriptor *descriptor, uint64_t offset,
                              uint64_t value);
    union int128store {
        struct {
            uint64_t low;
            uint64_t high;
        };
        int64_t i64[2];
        int128_t i128;
    };

    Fam_Context *get_defaultCtx() { return defaultCtx; };
    pthread_mutex_t *get_ctx_lock() { return &ctxLock; };

    void quiet_context(Fam_Context *context);
    uint64_t progress_context(Fam_Context *context);
    void context_open(uint64_t contextId);
    void context_close(uint64_t contextId);
    uint64_t get_context_id();

  protected:
    Fam_Async_QHandler *asyncQHandler;

    pthread_mutex_t ctxLock;

    Fam_Context *defaultCtx;
    std::map<uint64_t, Fam_Context *> *contexts;
    Fam_Thread_Model famThreadModel;
    Fam_Context_Model famContextModel;
    Fam_Allocator_Client *famAllocator;
};
} // end namespace openfam
#endif
