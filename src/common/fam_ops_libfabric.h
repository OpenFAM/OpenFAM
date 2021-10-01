/*
 * fam_ops_libfabric.h
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
#ifndef FAM_OPS_LIBFABRIC_H
#define FAM_OPS_LIBFABRIC_H

#include <iostream>
#include <map>
#include <string.h>
#include <sys/uio.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <rdma/fabric.h>
#include <rdma/fi_atomic.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_rma.h>

#include "allocator/fam_allocator_client.h"
#include "common/fam_context.h"
#include "common/fam_internal.h"
#include "common/fam_ops.h"
#include "common/fam_options.h"
#include "fam/fam.h"

using namespace std;

namespace openfam {

class Fam_Allocator_Client;
struct Fam_Region_Map_t {
    uint64_t regionId;
    std::map<uint64_t, fid_mr *> *fiRegionMrs;
    pthread_rwlock_t fiRegionLock;
};

class Fam_Ops_Libfabric : public Fam_Ops {
  public:
    ~Fam_Ops_Libfabric();

    /**
     * Initialize the Fam_Ops_Libfabric object.
     * @param name - name of the memory server
     * @param service - port
     * @param source -  to indicate if it is called by a memory node
     * @param provider - libfabric provider
     * @param famTM - Fam Thread Model
     * @return - {true(0), false(1), errNo(<0)}
     */

    Fam_Ops_Libfabric(bool is_source, const char *provider,
                      Fam_Thread_Model famTM, Fam_Allocator_Client *famAlloc,
                      Fam_Context_Model famCM = FAM_CONTEXT_DEFAULT);
    Fam_Ops_Libfabric(bool source, const char *libfabricProvider,
                      Fam_Thread_Model famTM, Fam_Allocator_Client *famAlloc,
                      Fam_Context_Model famCM, const char *memServerName,
                      const char *libfabricPort);
    /**
     * Initialize the libfabric library. This method is required to be the first
     * method called when a process uses the OpenFAM library.
     * @return - {true(0), false(1), errNo(<0)}
     */
    int initialize();

    /**
     * Finalize the libfabric library. Once finalized, the process can continue
     * work, but it is disconnected from the OpenFAM library functions.
     */
    void finalize();

    void abort(int status);

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

    void fence(Fam_Region_Descriptor *descriptor = NULL);

    void quiet(Fam_Region_Descriptor *descriptor = NULL);
    uint64_t progress();
    void check_progress(Fam_Region_Descriptor *descriptor = NULL);

    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                    uint32_t value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                    uint64_t value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset, double value);
    void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                    int128_t value);

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
    uint32_t atomic_fetch_uint32(Fam_Descriptor *descriptor, uint64_t offset);
    uint64_t atomic_fetch_uint64(Fam_Descriptor *descriptor, uint64_t offset);
    float atomic_fetch_float(Fam_Descriptor *descriptor, uint64_t offset);
    double atomic_fetch_double(Fam_Descriptor *descriptor, uint64_t offset);

    int128_t atomic_fetch_int128(Fam_Descriptor *descriptor, uint64_t offset);

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
    /**
     * Routines to access protected members
     *
     * TODO: May not require in future.
     */
    struct fi_info *get_fi() {
        return fi;
    };
    struct fid_domain *get_domain() {
        return domain;
    };
    std::vector<fi_addr_t> *get_fiAddrs() { return fiAddrs; };
    std::map<uint64_t, Fam_Region_Map_t *> *get_fiMrs() { return fiMrs; };
    Fam_Context *get_defaultCtx(uint64_t nodeId) {
        auto obj = defContexts->find(nodeId);
        if (obj == defContexts->end())
            THROW_ERR_MSG(Fam_Datapath_Exception,
                          "Context for memserver not found");
        else
            return obj->second;
    }
    Fam_Context *get_defaultCtx(Fam_Region_Descriptor *descriptor) {
        auto obj = defContexts->find(descriptor->get_memserver_id());
        if (obj == defContexts->end())
            THROW_ERR_MSG(Fam_Datapath_Exception,
                          "Context for memserver not found");
        else
            return obj->second;
    };
    Fam_Context *get_defaultCtx(Fam_Descriptor *descriptor) {
        auto obj = defContexts->find(descriptor->get_memserver_id());
        if (obj == defContexts->end())
            THROW_ERR_MSG(Fam_Datapath_Exception,
                          "Context for memserver not found");
        else
            return obj->second;
    };
    pthread_rwlock_t *get_mr_lock() { return &fiMrLock; };

    pthread_rwlock_t *get_memsrvaddr_lock() { return &fiMemsrvAddrLock; };

    pthread_mutex_t *get_ctx_lock() { return &ctxLock; };

    Fam_Context *get_context(Fam_Descriptor *descriptor);

    void quiet_context(Fam_Context *context);

    uint64_t progress_context();

    size_t get_addr_size() { return serverAddrNameLen; };

    void *get_addr() { return serverAddrName; };

    char *get_provider() { return provider; };

    struct fid_av *get_av() {
        return av;
    }

    std::map<uint64_t, std::pair<void *, size_t>> *get_memServerAddrs() {
        return memServerAddrs;
    };

    std::map<uint64_t, fi_addr_t> *get_fiMemsrvMap() { return fiMemsrvMap; }

  protected:
    // Server_Map name;
    char *memoryServerName;
    char *service;
    char *provider;
    bool isSource;
    uint64_t numMemoryNodes;
    struct fi_info *fi;
    struct fid_fabric *fabric;
    struct fid_eq *eq;
    struct fid_domain *domain;
    struct fid_av *av;
    size_t fabric_iov_limit;
    size_t serverAddrNameLen;
    void *serverAddrName;
    std::map<uint64_t, std::pair<void *, size_t>> *memServerAddrs;
    std::map<uint64_t, fi_addr_t> *fiMemsrvMap;
    pthread_rwlock_t fiMemsrvAddrLock;

    pthread_rwlock_t fiMrLock;
    pthread_mutex_t ctxLock;

    std::vector<fi_addr_t> *fiAddrs;
    std::map<uint64_t, Fam_Region_Map_t *> *fiMrs;

    std::map<uint64_t, Fam_Context *> *contexts;
    std::map<uint64_t, Fam_Context *> *defContexts;
    Fam_Thread_Model famThreadModel;
    Fam_Context_Model famContextModel;
    Fam_Allocator_Client *famAllocator;
};
} // namespace openfam
#endif
