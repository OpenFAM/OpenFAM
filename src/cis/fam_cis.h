/*
 * fam_cis.h
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
#ifndef FAM_CIS_H_
#define FAM_CIS_H_

#include <string>
#include <sys/types.h>

#include <grpc/impl/codegen/log.h>
#include <grpcpp/grpcpp.h>

#include "cis/fam_cis_rpc.grpc.pb.h"
#include "common/fam_async_qhandler.h"
#include "common/fam_internal.h"
#include "common/fam_internal_exception.h"
#include "fam/fam.h"

using namespace std;

namespace openfam {

/**
 * structure for keeping state and data information of fam copy
 */
typedef struct {
    // Container for the data we expect from the server.
    Fam_Copy_Response res;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ::grpc::ClientContext ctx;

    // Storage for the status of the RPC upon completion.
    ::grpc::Status status;

    // falg for completion
    bool isCompleted;

    uint64_t memServerId;

    std::unique_ptr< ::grpc::ClientAsyncResponseReader<Fam_Copy_Response> >
    responseReader;

    Fam_Copy_Tag *tag;
} Fam_Copy_Wait_Object;

class Fam_CIS {
  public:
    virtual ~Fam_CIS() {};

    virtual void reset_profile() = 0;

    virtual void dump_profile() = 0;

    virtual uint64_t get_num_memory_servers() = 0;
    /**
     * Creates a Region in FAM
     * @param name-Name of the reion to be created
     * @param nbytes - size of the region
     * @param permissions - Permission with which the region needs to be created
     * @param redundancyLevel - Redundancy level of FAM
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     * @return - Fam_Region_Item_Info structure
     **/
    virtual Fam_Region_Item_Info
    create_region(string name, size_t nbytes, mode_t permission,
                  Fam_Region_Attributes *regionAttributes, uint32_t uid,
                  uint32_t gid) = 0;

    /**
     * Destroys a Region in FAM
     * @param regionId - region Id of a region
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     **/
    virtual void destroy_region(uint64_t regionId, uint64_t memoryServerId,
                                uint32_t uid, uint32_t gid) = 0;

    /**
     * Resize a Region existing in FAM
     * @param region - Fam region descriptor of a region which needs to be
     *resized
     * @param nbytes - new size of the region
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     **/
    virtual void resize_region(uint64_t regionId, size_t nbytes,
                               uint64_t memoryServerId, uint32_t uid,
                               uint32_t gid) = 0;

    /**
     * Allocate data item within the specified region
     * @param name - name of the data item
     * @param namelen - no. charecter in name
     * @param nbytes - size of the data item
     * @param regionId - region Id of a region within which data
     * item needs to be created
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     * @return -  Fam_Region_Item_Info
     **/
    virtual Fam_Region_Item_Info allocate(string name, size_t nbytes,
                                          mode_t permission, uint64_t regionId,
                                          uint64_t memoryServerId, uint32_t uid,
                                          uint32_t gid) = 0;

    /**
     * deallocates the data item
     * @param regionId - region Id of a region within which data
     * item needs to be created
     * @param offset - offset of a dataitem within region
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     **/
    virtual void deallocate(uint64_t regionId, uint64_t offset,
                            uint64_t memoryServerId, uint32_t uid,
                            uint32_t gid) = 0;

    /**
     * changes the permission of a region
     * @param regionId - region Id of a region whose permissions needs to be
     *changed
     * @param permission - new permission
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     **/
    virtual void change_region_permission(uint64_t regionId, mode_t permission,
                                          uint64_t memoryServerId, uint32_t uid,
                                          uint32_t gid) = 0;

    /**
     * changes the permission of a dataitem
     * @param regionId - region Id of a region within which dataitem recides
     * @param offset - offset of a dataitem whose permissions needs to be
     * changed
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     * @param permission - new permission
     **/
    virtual void change_dataitem_permission(uint64_t regionId, uint64_t offset,
                                            mode_t permission,
                                            uint64_t memoryServerId,
                                            uint32_t uid, uint32_t gid) = 0;

    /**
     * look for a region with given name
     * @param name - name of the region
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     * @return - Fam_Region_Item_Info
     **/
    virtual Fam_Region_Item_Info lookup_region(string name, uint32_t uid,
                                               uint32_t gid) = 0;

    /**
     * look for a dataiem with given name
     * @param itemName - name of the dataitem
     * @param regionName - name of the region
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     * @return - Fam_Region_Item_Info
     **/
    virtual Fam_Region_Item_Info lookup(string itemName, string regionName,
                                        uint32_t uid, uint32_t gid) = 0;

    /**
     * check permission for region access and returns region information
     * @param regionId - region Id of region
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     * @return - Fam_Region_Item_Info
     **/
    virtual Fam_Region_Item_Info
    check_permission_get_region_info(uint64_t regionId, uint64_t memoryServerId,
                                     uint32_t uid, uint32_t gid) = 0;

    /**
     * check permission for dataitem access and returns region information
     * @param regionId - region Id of region
     * @param offset - offset of the dataitem
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     * @return - Fam_Region_Item_Info
     **/
    virtual Fam_Region_Item_Info
    check_permission_get_item_info(uint64_t regionId, uint64_t offset,
                                   uint64_t memoryServerId, uint32_t uid,
                                   uint32_t gid) = 0;

    /**
     * returns dataitem name, size and permission
     * @param regionId - region Id of region
     * @param offset - offset of the dataitem
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     * @return - Fam_Region_Item_Info
     **/
    virtual Fam_Region_Item_Info get_stat_info(uint64_t regionId,
                                               uint64_t offset,
                                               uint64_t memoryServerId,
                                               uint32_t uid, uint32_t gid) = 0;

    /**
     * copy data from one dataitem to another
     * @param srcRegionId - region Id of source region
     * @param srcOffset - offset of source dataitem
     * @param srcCopyStart - start position in source dataitem
     * @param srcKey - source dataitem key
     * @param srcBaseAddr - source Base Address
     * @param srcAddr - source memory server address
     * @param srcAddrLen - source memory server address size
     * @param destRegionId - region Id of destination dataitem
     * @param destOffset - offset of destination dataitem
     * @param destCopyStar - start position in destination dataitem
     * @param nbytes - number of bytes to be copied
     * @param srcMemoryServerId - Memory server Id
     * @param destMemoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     **/
    virtual void *
    copy(uint64_t srcRegionId, uint64_t srcOffset, uint64_t srcCopyStart,
         uint64_t srcKey, uint64_t srcBaseAddr, const char *srcAddr,
         uint32_t srcAddrLen, uint64_t destRegionId, uint64_t destOffset,
         uint64_t destCopyStar, uint64_t nbytes, uint64_t srcMemoryServerId,
         uint64_t destMemoryServerId, uint32_t uid, uint32_t gid) = 0;

    /**
     * wait for particular copy issued earlier corresponding to wait object
     * @param waitObj - wait object
     **/
    virtual void wait_for_copy(void *waitObj) = 0;

    /**
     * Map a data item in FAM to the local virtual address space, and return its
     * pointer.
     * @param regionId - region Id of region
     * @param offset - offset of the dataitem
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     * @return - Fam_Region_Item_Info
     **/
    virtual void *fam_map(uint64_t regionId, uint64_t offset,
                          uint64_t memoryServerId, uint32_t uid,
                          uint32_t gid) = 0;

    /**
     * Unmap a data item in FAM from the local virtual address space.
     * @param local - pointer within the process virtual address space to be
     * unmapped
     * @param regionId - region Id of region
     * @param offset - offset of the dataitem
     * @param memoryServerId - Memory server Id
     * @param uid - uid of user
     * @param gid - gid of user
     * @return - Fam_Region_Item_Info
     **/
    virtual void fam_unmap(void *local, uint64_t regionId, uint64_t offset,
                           uint64_t memoryServerId, uint32_t uid,
                           uint32_t gid) = 0;

    virtual void acquire_CAS_lock(uint64_t offset, uint64_t memoryServerId) = 0;
    virtual void release_CAS_lock(uint64_t offset, uint64_t memoryServerId) = 0;

    virtual size_t get_addr_size(uint64_t memoryServerId) = 0;
    virtual void get_addr(void *memServerFabricAddr,
                          uint64_t memoryServerId) = 0;

    virtual size_t get_memserverinfo_size() = 0;
    virtual void get_memserverinfo(void *memServerInfo) = 0;

    virtual int get_atomic(uint64_t regionId, uint64_t srcOffset,
                           uint64_t dstOffset, uint64_t nbytes, uint64_t key,
                           const char *nodeAddr, uint32_t nodeAddrSize,
                           uint64_t memoryServerId, uint32_t uid,
                           uint32_t gid) = 0;

    virtual int put_atomic(uint64_t regionId, uint64_t srcOffset,
                           uint64_t dstOffset, uint64_t nbytes, uint64_t key,
                           const char *nodeAddr, uint32_t nodeAddrSize,
                           const char *data, uint64_t memoryServerId,
                           uint32_t uid, uint32_t gid) = 0;

    virtual int scatter_strided_atomic(
        uint64_t regionId, uint64_t offset, uint64_t nElements,
        uint64_t firstElement, uint64_t stride, uint64_t elementSize,
        uint64_t key, const char *nodeAddr, uint32_t nodeAddrSize,
        uint64_t memoryServerId, uint32_t uid, uint32_t gid) = 0;

    virtual int gather_strided_atomic(uint64_t regionId, uint64_t offset,
                                      uint64_t nElements, uint64_t firstElement,
                                      uint64_t stride, uint64_t elementSize,
                                      uint64_t key, const char *nodeAddr,
                                      uint32_t nodeAddrSize,
                                      uint64_t memoryServerId, uint32_t uid,
                                      uint32_t gid) = 0;

    virtual int scatter_indexed_atomic(
        uint64_t regionId, uint64_t offset, uint64_t nElements,
        const void *elementIndex, uint64_t elementSize, uint64_t key,
        const char *nodeAddr, uint32_t nodeAddrSize, uint64_t memoryServerId,
        uint32_t uid, uint32_t gid) = 0;

    virtual int gather_indexed_atomic(
        uint64_t regionId, uint64_t offset, uint64_t nElements,
        const void *elementIndex, uint64_t elementSize, uint64_t key,
        const char *nodeAddr, uint32_t nodeAddrSize, uint64_t memoryServerId,
        uint32_t uid, uint32_t gid) = 0;
};

} // namespace openfam
#endif /* end of FAM_ALLOCATOR_H_ */
