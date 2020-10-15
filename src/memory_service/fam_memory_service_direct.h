/*
 * fam_memory_service_direct.h
 * Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All rights
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
#ifndef FAM_MEMORY_SERVICE_DIRECT_H_
#define FAM_MEMORY_SERVICE_DIRECT_H_

#include <sys/types.h>

#include "allocator/memserver_allocator.h"
#include "memory_service/fam_memory_service.h"
#include "memory_service/fam_memory_registration.h"
#include "memory_service/fam_memory_registration_libfabric.h"
#include "memory_service/fam_memory_registration_shm.h"

namespace openfam {

#define CAS_LOCK_CNT 128
#define LOCKHASH(offset) (offset >> 7) % CAS_LOCK_CNT

class Fam_Memory_Service_Direct : public Fam_Memory_Service {
  public:
    Fam_Memory_Service_Direct(const char *name, const char *libfabricPort,
                              const char *libfabricProvider,
                              const char *fam_path);

    ~Fam_Memory_Service_Direct();

    void reset_profile();

    void dump_profile();

    void create_region(uint64_t regionId, size_t nbytes);

    void destroy_region(uint64_t regionId);

    void resize_region(uint64_t regionId, size_t nbytes);

    Fam_Region_Item_Info allocate(uint64_t regionId, size_t nbytes);

    void deallocate(uint64_t regionId, uint64_t offset);

    void copy(uint64_t srcRegionId, uint64_t srcOffset, uint64_t destRegionId,
              uint64_t destOffset, uint64_t size);

    void *get_local_pointer(uint64_t regionId, uint64_t offset);

    void acquire_CAS_lock(uint64_t offset);
    void release_CAS_lock(uint64_t offset);

    size_t get_addr_size();
    void *get_addr();

    uint64_t get_key(uint64_t regionId, uint64_t offset, uint64_t size,
                     bool rwFlag);
    configFileParams get_config_info(std::string filename);

    void init_atomic_queue();
    void get_atomic(uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset,
                    uint64_t nbytes, uint64_t key, const char *nodeAddr,
                    uint32_t nodeAddrSize);

    void put_atomic(uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset,
                    uint64_t nbytes, uint64_t key, const char *nodeAddr,
                    uint32_t nodeAddrSize, const char *data);

  private:
    Memserver_Allocator *allocator;
    pthread_mutex_t casLock[CAS_LOCK_CNT];
    Fam_Memory_Registration *memoryRegistration;
};

} // namespace openfam
#endif /* end of FAM_MEMORY_SERVICE_DIRECT_H_ */
