/*
 * fam_cis.h
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
#ifndef FAM_MEMORY_SERVICE_H_
#define FAM_MEMORY_SERVICE_H_

#include <string>
#include <sys/types.h>

#include "common/fam_internal.h"
#include "common/fam_internal_exception.h"
#include "fam/fam.h"

using namespace std;

namespace openfam {

class Fam_Memory_Service {
  public:
    virtual ~Fam_Memory_Service() {}

    virtual void reset_profile() = 0;

    virtual void dump_profile() = 0;

    virtual uint64_t create_region(size_t nbytes) = 0;

    virtual void destroy_region(uint64_t regionId) = 0;

    virtual void resize_region(uint64_t regionId, size_t nbytes) = 0;

    virtual Fam_Region_Item_Info allocate(uint64_t regionId, size_t nbytes) = 0;

    virtual void deallocate(uint64_t regionId, uint64_t offset) = 0;

    virtual void copy(uint64_t srcRegionId, uint64_t srcOffset,
                      uint64_t destRegionId, uint64_t destOffset,
                      uint64_t nbytes) = 0;

    virtual void *get_local_pointer(uint64_t regionId, uint64_t offset) = 0;

    virtual void acquire_CAS_lock(uint64_t offset) = 0;
    virtual void release_CAS_lock(uint64_t offset) = 0;

    virtual size_t get_addr_size() = 0;
    virtual void *get_addr() = 0;
    virtual uint64_t get_key(uint64_t regionId, uint64_t offset, uint64_t size,
                             bool rwFlag) = 0;

    virtual void get_atomic(uint64_t regionId, uint64_t srcOffset,
                            uint64_t dstOffset, uint64_t nbytes, uint64_t key,
                            const char *nodeAddr, uint32_t nodeAddrSize) = 0;

    virtual void put_atomic(uint64_t regionId, uint64_t srcOffset,
                            uint64_t dstOffset, uint64_t nbytes, uint64_t key,
                            const char *nodeAddr, uint32_t nodeAddrSize,
                            const char *data) = 0;
};

} // namespace openfam
#endif /* end of FAM_ALLOCATOR_H_ */
