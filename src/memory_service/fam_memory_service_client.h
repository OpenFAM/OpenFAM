/*
 * fam_memory_service_client.h
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
#ifndef MEMORY_SERVICE_CLIENT_H_
#define MEMORY_SERVICE_CLIENT_H_

#include <sys/types.h>

#include <grpc/impl/codegen/log.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "memory_service/fam_memory_service.h"
#include "memory_service/fam_memory_service_rpc.grpc.pb.h"
using namespace std;

namespace openfam {

class Fam_Memory_Service_Client : public Fam_Memory_Service {
  public:
    Fam_Memory_Service_Client(const char *name, uint64_t port);

    ~Fam_Memory_Service_Client();

    void reset_profile();

    void dump_profile();

    void create_region(uint64_t regionId, size_t nbytes);

    void destroy_region(uint64_t regionId);

    void resize_region(uint64_t regionId, size_t nbytes);

    Fam_Region_Item_Info allocate(uint64_t regionId, size_t nbytes);

    void deallocate(uint64_t regionId, uint64_t offset);

    void copy(uint64_t srcRegionId, uint64_t srcOffset, uint64_t srcKey,
              uint64_t srcCopyStart, uint64_t srcBaseAddr, const char *srcAddr,
              uint32_t srcAddrLen, uint64_t destRegionId, uint64_t destOffset,
              uint64_t size, uint64_t srcMemserverId, uint64_t destMemserverId);

    void *get_local_pointer(uint64_t regionId, uint64_t offset);

    void acquire_CAS_lock(uint64_t offset);
    void release_CAS_lock(uint64_t offset);

    size_t get_addr_size();
    void *get_addr();

    uint64_t get_key(uint64_t regionId, uint64_t offset, uint64_t size,
                     bool rwFlag);

    void get_atomic(uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset,
                    uint64_t nbytes, uint64_t key, const char *nodeAddr,
                    uint32_t nodeAddrSize);

    void put_atomic(uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset,
                    uint64_t nbytes, uint64_t key, const char *nodeAddr,
                    uint32_t nodeAddrSize, const char *data);

    void scatter_strided_atomic(uint64_t regionId, uint64_t offset,
                                uint64_t nElements, uint64_t firstElement,
                                uint64_t stride, uint64_t elementSize,
                                uint64_t key, const char *nodeAddr,
                                uint32_t nodeAddrSize);

    void gather_strided_atomic(uint64_t regionId, uint64_t offset,
                               uint64_t nElements, uint64_t firstElement,
                               uint64_t stride, uint64_t elementSize,
                               uint64_t key, const char *nodeAddr,
                               uint32_t nodeAddrSize);

    void scatter_indexed_atomic(uint64_t regionId, uint64_t offset,
                                uint64_t nElements, const void *elementIndex,
                                uint64_t elementSize, uint64_t key,
                                const char *nodeAddr, uint32_t nodeAddrSize);

    void gather_indexed_atomic(uint64_t regionId, uint64_t offset,
                               uint64_t nElements, const void *elementIndex,
                               uint64_t elementSize, uint64_t key,
                               const char *nodeAddr, uint32_t nodeAddrSize);

  private:
    std::unique_ptr<Fam_Memory_Service_Rpc::Stub> stub;
    size_t memServerFabricAddrSize;
    char *memServerFabricAddr;
};

} // namespace openfam
#endif /* end of FAM_ALLOCATOR_H_ */
