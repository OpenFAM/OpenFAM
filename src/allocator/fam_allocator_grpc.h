/*
 * fam_allocator_grpc.h
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
#ifndef FAM_ALLOCATOR_GRPC_H_
#define FAM_ALLOCATOR_GRPC_H_

#include "allocator/fam_allocator.h"
#include "rpc/fam_rpc_client.h"

namespace openfam {

using RpcClientMap = std::map<uint64_t, Fam_Rpc_Client *>;
using MemServerMap = std::map<uint64_t, std::string>;

class Fam_Allocator_Grpc : public Fam_Allocator {
  public:
    Fam_Allocator_Grpc(MemServerMap name, uint64_t port);

    ~Fam_Allocator_Grpc();

    void allocator_initialize();

    void allocator_finalize();

    Fam_Rpc_Client *get_rpc_client(uint64_t channel);

    Fam_Region_Descriptor *create_region(const char *name, uint64_t nbytes,
                                         mode_t permissions,
                                         Fam_Redundancy_Level redundancyLevel,
                                         uint64_t memoryServerId);
    void destroy_region(Fam_Region_Descriptor *descriptor);
    int resize_region(Fam_Region_Descriptor *descriptor, uint64_t nbytes);

    Fam_Descriptor *allocate(const char *name, uint64_t nbytes,
                             mode_t accessPermissions,
                             Fam_Region_Descriptor *region);
    void deallocate(Fam_Descriptor *descriptor);

    int change_permission(Fam_Region_Descriptor *descriptor,
                          mode_t accessPermissions);
    int change_permission(Fam_Descriptor *descriptor, mode_t accessPermissions);
    Fam_Region_Descriptor *lookup_region(const char *name,
                                         uint64_t memoryServerId);
    Fam_Descriptor *lookup(const char *itemName, const char *regionName,
                           uint64_t memoryServerId);
    Fam_Region_Item_Info
    check_permission_get_info(Fam_Region_Descriptor *descriptor);
    Fam_Region_Item_Info check_permission_get_info(Fam_Descriptor *descriptor);

    void *copy(Fam_Descriptor *src, uint64_t srcOffset, Fam_Descriptor **dest,
               uint64_t destOffset, uint64_t nbytes);

    void wait_for_copy(void *waitObj);

    /**
     * fam_map - Map a data item in FAM to the process virtual address space.
     * @param descriptor - Descriptor associated with the data item in FAM.
     * @return - A pointer in the PE’s virtual address space that can be used
     * to directly manipulate contents of FAM.
     */
    virtual void *fam_map(Fam_Descriptor *descriptor);

    /**
     * fam_unmap - Unmap a data item in FAM from the process virtual
     * address space.
     * @param local - Local pointer to be unmapped.
     * @param descriptor - Descriptor associated with the data item in FAM
     * to be unmapped.
     */
    virtual void fam_unmap(void *local, Fam_Descriptor *descriptor);

    /**
     * acquire_CAS_lock - Acquire the mutex lock to perform
     * 128-bit CAS operation.
     * @param descriptor - Descriptor associated with the data item in FAM
     */
    virtual void acquire_CAS_lock(Fam_Descriptor *descriptor);
    /**
     * release_CAS_lock - Release the mutex lock acquired to perform
     * 128-bit CAS operation.
     * @param descriptor - Descriptor associated with the data item in FAM
     */
    virtual void release_CAS_lock(Fam_Descriptor *descriptor);

    virtual int get_addr_size(size_t *addrSize, uint64_t nodeId);

    virtual int get_addr(void *addr, size_t addrSize, uint64_t nodeId);

  private:
    RpcClientMap *rpcClients;
};

} // namespace openfam
#endif /* end of FAM_ALLOCATOR_GRPC_H_ */
