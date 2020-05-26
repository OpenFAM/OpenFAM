/*
 * fam_allocator.h
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
#ifndef FAM_ALLOCATOR_H_
#define FAM_ALLOCATOR_H_

#include <sys/types.h>

#include "common/fam_internal.h"
#include "fam/fam.h"

namespace openfam {

class Fam_Allocator {
  public:
    virtual ~Fam_Allocator(){};

    virtual void allocator_initialize() = 0;

    virtual void allocator_finalize() = 0;

    virtual Fam_Region_Descriptor *
    create_region(const char *name, uint64_t nbytes, mode_t permissions,
                  Fam_Redundancy_Level redundancyLevel,
                  uint64_t memoryServerId) = 0;
    virtual void destroy_region(Fam_Region_Descriptor *descriptor) = 0;
    virtual int resize_region(Fam_Region_Descriptor *descriptor,
                              uint64_t nbytes) = 0;

    virtual Fam_Descriptor *allocate(const char *name, uint64_t nbytes,
                                     mode_t accessPermissions,
                                     Fam_Region_Descriptor *region) = 0;
    virtual void deallocate(Fam_Descriptor *descriptor) = 0;

    virtual int change_permission(Fam_Region_Descriptor *descriptor,
                                  mode_t accessPermissions) = 0;
    virtual int change_permission(Fam_Descriptor *descriptor,
                                  mode_t accessPermissions) = 0;
    virtual Fam_Region_Descriptor *lookup_region(const char *name,
                                                 uint64_t memoryServerId) = 0;
    virtual Fam_Descriptor *lookup(const char *itemName, const char *regionName,
                                   uint64_t memoryServerId) = 0;
    virtual Fam_Region_Item_Info
    check_permission_get_info(Fam_Region_Descriptor *descriptor) = 0;
    virtual Fam_Region_Item_Info
    check_permission_get_info(Fam_Descriptor *descriptor) = 0;

    virtual void *copy(Fam_Descriptor *src, uint64_t srcOffset,
                       Fam_Descriptor *dest, uint64_t destOffset,
                       uint64_t nbytes) = 0;

    virtual void wait_for_copy(void *waitObj) = 0;

    virtual void *fam_map(Fam_Descriptor *descriptor) = 0;
    virtual void fam_unmap(void *local, Fam_Descriptor *descriptor) = 0;

    virtual void acquire_CAS_lock(Fam_Descriptor *descriptor) = 0;
    virtual void release_CAS_lock(Fam_Descriptor *descriptor) = 0;

    virtual int get_addr_size(size_t *addrSize, uint64_t nodeId) = 0;
    virtual int get_addr(void *addr, size_t addrSize, uint64_t nodeId) = 0;
};

} // namespace openfam
#endif /* end of FAM_ALLOCATOR_H_ */
