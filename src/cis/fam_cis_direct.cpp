/*
 * fam_cis_direct.cpp
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
#include "cis/fam_cis_direct.h"
#include "common/fam_memserver_profile.h"
#include <thread>

#include <boost/atomic.hpp>

#include <chrono>
#include <iomanip>
#include <string.h>
#include <unistd.h>

using namespace std;
using namespace chrono;
namespace openfam {
MEMSERVER_PROFILE_START(CIS_DIRECT)
#ifdef MEMSERVER_PROFILE
#define CIS_DIRECT_PROFILE_START_OPS()                                         \
    {                                                                          \
        Profile_Time start = CIS_DIRECT_get_time();

#define CIS_DIRECT_PROFILE_END_OPS(apiIdx)                                     \
    Profile_Time end = CIS_DIRECT_get_time();                                  \
    Profile_Time total = CIS_DIRECT_time_diff_nanoseconds(start, end);         \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(CIS_DIRECT, prof_##apiIdx, total)       \
    }
#define CIS_DIRECT_PROFILE_DUMP() cis_direct_profile_dump()
#else
#define CIS_DIRECT_PROFILE_START_OPS()
#define CIS_DIRECT_PROFILE_END_OPS(apiIdx)
#define CIS_DIRECT_PROFILE_DUMP()
#endif

void cis_direct_profile_dump() {
    MEMSERVER_PROFILE_END(CIS_DIRECT);
    MEMSERVER_DUMP_PROFILE_BANNER(CIS_DIRECT)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(CIS_DIRECT, name, prof_##name)
#include "cis/cis_direct_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name) MEMSERVER_PROFILE_TOTAL(CIS_DIRECT, prof_##name)
#include "cis/cis_direct_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(CIS_DIRECT)
}

Fam_CIS_Direct::Fam_CIS_Direct() {
    allocator = new Memserver_Allocator();
#ifdef METADATA_RPC
    metadataService =
        new Fam_Metadata_Service_Client(METADATA_SERVER, METADATA_RPC_PORT);
#else
    metadataService = new Fam_Metadata_Service_Direct();
#endif
}

Fam_CIS_Direct::~Fam_CIS_Direct() { delete allocator; }

void Fam_CIS_Direct::cis_direct_finalize() {
    allocator->memserver_allocator_finalize();
}

void Fam_CIS_Direct::reset_profile() {

    MEMSERVER_PROFILE_INIT(CIS_DIRECT)
    MEMSERVER_PROFILE_START_TIME(CIS_DIRECT)
    allocator->reset_profile();
    metadataService->reset_profile();
    return;
}

void Fam_CIS_Direct::dump_profile() {
    CIS_DIRECT_PROFILE_DUMP();
    allocator->dump_profile();
    metadataService->dump_profile();
}

Fam_Region_Item_Info
Fam_CIS_Direct::create_region(string name, size_t nbytes, mode_t permission,
                              Fam_Redundancy_Level redundancyLevel,
                              uint64_t memoryServerId, uint32_t uid,
                              uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Region_Metadata region;
    uint64_t regionId;
    ostringstream message;

    // Check if the name size is bigger than MAX_KEY_LEN supported
    if (name.size() > metadataService->metadata_maxkeylen()) {
        message << "Name too long";
        THROW_ERRNO_MSG(CIS_Exception, REGION_NAME_TOO_LONG,
                        message.str().c_str());
    }

    if (metadataService->metadata_find_region(name, region)) {
        message << "Region already exist";
        THROW_ERRNO_MSG(CIS_Exception, REGION_EXIST, message.str().c_str());
    }
    allocator->create_region(name, regionId, nbytes, permission, uid, gid);

    // Register the region into metadata service
    region.regionId = regionId;
    strncpy(region.name, name.c_str(), metadataService->metadata_maxkeylen());
    region.offset = INVALID_OFFSET;
    region.perm = permission;
    region.uid = uid;
    region.gid = gid;
    region.size = nbytes;
    metadataService->metadata_insert_region(regionId, name, &region);
    info.regionId = regionId;
    info.offset = INVALID_OFFSET;
    CIS_DIRECT_PROFILE_END_OPS(cis_create_region);
    return info;
}

void Fam_CIS_Direct::destroy_region(uint64_t regionId, uint64_t memoryServerId,
                                    uint32_t uid, uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    if (!(metadataService->metadata_find_region(regionId, region))) {
        message << "Region does not exist";
        THROW_ERRNO_MSG(CIS_Exception, REGION_NOT_FOUND, message.str().c_str());
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to destroy region, if not return error
    if (uid != region.uid) {
        bool isPermitted = metadataService->metadata_check_permissions(
            &region, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Destroying region is not permitted";
            THROW_ERRNO_MSG(CIS_Exception, DESTROY_REGION_NOT_PERMITTED,
                            message.str().c_str());
        }
    }

    // remove region from metadata service.
    // metadata_delete_region() is called before DestroyHeap() as
    // cached KVS is freed in metadata_delete_region and calling
    // metadata_delete_region after DestroyHeap will result in SIGSEGV.

    metadataService->metadata_delete_region(regionId);

    allocator->destroy_region(regionId, uid, gid);

    CIS_DIRECT_PROFILE_END_OPS(cis_destroy_region);
    return;
}

void Fam_CIS_Direct::resize_region(uint64_t regionId, size_t nbytes,
                                   uint64_t memoryServerId, uint32_t uid,
                                   uint32_t gid) {

    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    if (!(metadataService->metadata_find_region(regionId, region))) {
        message << "Region does not exist";
        THROW_ERRNO_MSG(CIS_Exception, REGION_NOT_FOUND, message.str().c_str());
    }

    bool isPermitted = metadataService->metadata_check_permissions(
        &region, META_REGION_ITEM_WRITE, uid, gid);
    if (!isPermitted) {
        message << "Region resize not permitted";
        THROW_ERRNO_MSG(CIS_Exception, REGION_RESIZE_NOT_PERMITTED,
                        message.str().c_str());
    }

    allocator->resize_region(regionId, uid, gid, nbytes);

    region.size = nbytes;
    // Update the size in the metadata service
    metadataService->metadata_modify_region(regionId, &region);

    CIS_DIRECT_PROFILE_END_OPS(cis_resize_region);

    return;
}

Fam_Region_Item_Info Fam_CIS_Direct::allocate(string name, size_t nbytes,
                                              mode_t permission,
                                              uint64_t regionId,
                                              uint64_t mememoryServerId,
                                              uint32_t uid, uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    void *localPointer;

    // Check if the name size is bigger than MAX_KEY_LEN supported
    if (name.size() > metadataService->metadata_maxkeylen()) {
        message << "Name too long";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NAME_TOO_LONG,
                        message.str().c_str());
    }

    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    if (!metadataService->metadata_find_region(regionId, region)) {
        message << "Region does not exist";
        THROW_ERRNO_MSG(CIS_Exception, REGION_NOT_FOUND, message.str().c_str());
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to create dataitem in that region, if not return error
    if (uid != region.uid) {
        bool isPermitted = metadataService->metadata_check_permissions(
            &region, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Allocation of dataitem is not permitted";
            THROW_ERRNO_MSG(CIS_Exception, DATAITEM_ALLOC_NOT_PERMITTED,
                            message.str().c_str());
        }
    }

    // Check with metadata service if data item with the requested name
    // is already exist, if exists return error
    Fam_DataItem_Metadata dataitem;
    uint64_t offset;
    if (name != "") {
        if (metadataService->metadata_find_dataitem(name, regionId, dataitem)) {
            message << "Dataitem with the name provided already exist";
            THROW_ERRNO_MSG(CIS_Exception, DATAITEM_EXIST,
                            message.str().c_str());
        }
    }

    allocator->allocate(name, regionId, nbytes, offset, permission, uid, gid,
                        localPointer);

    uint64_t dataitemId = offset / MIN_OBJ_SIZE;

    dataitem.regionId = regionId;
    strncpy(dataitem.name, name.c_str(), metadataService->metadata_maxkeylen());
    dataitem.offset = offset;
    dataitem.perm = permission;
    dataitem.gid = gid;
    dataitem.uid = uid;
    dataitem.size = nbytes;
    if (name == "")
        metadataService->metadata_insert_dataitem(dataitemId, regionId,
                                                  &dataitem);
    else
        metadataService->metadata_insert_dataitem(dataitemId, regionId,
                                                  &dataitem, name);

    uint64_t key;

    // This key is only applicable for shared memory model
    if (check_dataitem_permission(dataitem, 1, uid, gid)) {
        key = FAM_WRITE_KEY_SHM | FAM_READ_KEY_SHM;
    } else if (check_dataitem_permission(dataitem, 0, uid, gid)) {
        key = FAM_READ_KEY_SHM;
    } else {
        message << "Not permitted to use this dataitem";
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_NOPERM, message.str().c_str());
    }

    info.regionId = regionId;
    info.offset = offset;
    info.key = key;
    info.size = nbytes;
    info.base = get_local_pointer(regionId, offset);
    CIS_DIRECT_PROFILE_END_OPS(cis_allocate);
    return info;
}

void Fam_CIS_Direct::deallocate(uint64_t regionId, uint64_t offset,
                                uint64_t memoryServerId, uint32_t uid,
                                uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    // Check with metadata service if data item with the requested name
    // is already exist, if not return error
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    Fam_DataItem_Metadata dataitem;
    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to destroy region, if not return error
    if (uid != dataitem.uid) {
        bool isPermitted = metadataService->metadata_check_permissions(
            &dataitem, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Deallocation of dataitem is not permitted";
            THROW_ERRNO_MSG(CIS_Exception, DATAITEM_DEALLOC_NOT_PERMITTED,
                            message.str().c_str());
        }
    }

    // Remove data item from metadata service
    metadataService->metadata_delete_dataitem(dataitemId, regionId);

    allocator->deallocate(regionId, offset, uid, gid);

    CIS_DIRECT_PROFILE_END_OPS(cis_deallocate);

    return;
}

void Fam_CIS_Direct::change_region_permission(uint64_t regionId,
                                              mode_t permission,
                                              uint64_t mememoryServerId,
                                              uint32_t uid, uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()

    ostringstream message;
    message << "Error While changing region permission : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    Fam_Region_Metadata region;
    if (!metadataService->metadata_find_region(regionId, region)) {
        message << "Region does not exist";
        THROW_ERRNO_MSG(CIS_Exception, REGION_NOT_FOUND, message.str().c_str());
    }

    // Check with metadata service if the calling PE has the permission
    // to modify permissions of the region, if not return error
    if (uid != region.uid) {
        message << "Region permission modification not permitted";
        THROW_ERRNO_MSG(CIS_Exception, REGION_PERM_MODIFY_NOT_PERMITTED,
                        message.str().c_str());
    }

    // Update the permission of region with metadata service
    region.perm = permission;
    metadataService->metadata_modify_region(regionId, &region);

    CIS_DIRECT_PROFILE_END_OPS(cis_change_region_permission);

    return;
}

void Fam_CIS_Direct::change_dataitem_permission(uint64_t regionId,
                                                uint64_t offset,
                                                mode_t permission,
                                                uint64_t mememoryServerId,
                                                uint32_t uid, uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    message << "Error While changing dataitem permission : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    Fam_DataItem_Metadata dataitem;
    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    // Check with metadata service if the calling PE has the permission
    // to modify permissions of the region, if not return error
    if (uid != dataitem.uid) {
        message << "Dataitem permission modification not permitted";
        THROW_ERRNO_MSG(CIS_Exception, ITEM_PERM_MODIFY_NOT_PERMITTED,
                        message.str().c_str());
    }

    // Update the permission of region with metadata service
    dataitem.perm = permission;
    metadataService->metadata_modify_dataitem(dataitemId, regionId, &dataitem);

    CIS_DIRECT_PROFILE_END_OPS(cis_change_dataitem_permission);
    return;
}

/*
 * Check if the given uid/gid has read or rw permissions.
 */
bool Fam_CIS_Direct::check_region_permission(Fam_Region_Metadata region,
                                             bool op, uint32_t uid,
                                             uint32_t gid) {
    metadata_region_item_op_t opFlag;
    if (op)
        opFlag = META_REGION_ITEM_RW;
    else
        opFlag = META_REGION_ITEM_READ;

    return (
        metadataService->metadata_check_permissions(&region, opFlag, uid, gid));
}

/*
 * Check if the given uid/gid has read or rw permissions for
 * a given dataitem.
 */
bool Fam_CIS_Direct::check_dataitem_permission(Fam_DataItem_Metadata dataitem,
                                               bool op, uint32_t uid,
                                               uint32_t gid) {

    metadata_region_item_op_t opFlag;
    if (op)
        opFlag = META_REGION_ITEM_RW;
    else
        opFlag = META_REGION_ITEM_READ;

    return (metadataService->metadata_check_permissions(&dataitem, opFlag, uid,
                                                        gid));
}

Fam_Region_Item_Info Fam_CIS_Direct::lookup_region(string name,
                                                   uint64_t memoryServerId,
                                                   uint32_t uid, uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    Fam_Region_Metadata region;

    message << "Error While locating region : ";
    if (!metadataService->metadata_find_region(name, region)) {
        message << "could not find the region";
        THROW_ERRNO_MSG(CIS_Exception, REGION_NOT_FOUND, message.str().c_str());
    }

    if (uid != region.uid) {
        if (!check_region_permission(region, 0, uid, gid)) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
    }
    info.regionId = region.regionId;
    info.offset = region.offset;
    info.size = region.size;
    info.perm = region.perm;
    strncpy(info.name, region.name, metadataService->metadata_maxkeylen());
    info.maxNameLen = metadataService->metadata_maxkeylen();
    CIS_DIRECT_PROFILE_END_OPS(cis_lookup_region);
    return info;
}

Fam_Region_Item_Info Fam_CIS_Direct::lookup(string itemName, string regionName,
                                            uint64_t memoryServerId,
                                            uint32_t uid, uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    Fam_DataItem_Metadata dataitem;
    message << "Error While locating dataitem : ";
    if (!metadataService->metadata_find_dataitem(itemName, regionName,
                                                 dataitem)) {
        message << "could not find the dataitem ";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    if (uid != dataitem.uid) {
        if (!check_dataitem_permission(dataitem, 0, uid, gid)) {
            message << "Not permitted to access the dataitem";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
    }

    info.regionId = dataitem.regionId;
    info.offset = dataitem.offset;
    info.size = dataitem.size;
    info.perm = dataitem.perm;
    strncpy(info.name, dataitem.name, metadataService->metadata_maxkeylen());
    info.maxNameLen = metadataService->metadata_maxkeylen();
    CIS_DIRECT_PROFILE_END_OPS(cis_lookup);
    return info;
}

Fam_Region_Item_Info Fam_CIS_Direct::check_permission_get_region_info(
    uint64_t regionId, uint64_t memoryServerId, uint32_t uid, uint32_t gid) {

    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Region_Metadata region;
    ostringstream message;
    message << "Error While locating region : ";
    if (!metadataService->metadata_find_region(regionId, region)) {
        message << "could not find the region";
        THROW_ERRNO_MSG(CIS_Exception, REGION_NOT_FOUND, message.str().c_str());
    }

    if (uid != region.uid) {
        if (!check_region_permission(region, 0, uid, gid)) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
    }
    info.size = region.size;
    info.perm = region.perm;
    strncpy(info.name, region.name, metadataService->metadata_maxkeylen());
    info.maxNameLen = metadataService->metadata_maxkeylen();
    CIS_DIRECT_PROFILE_END_OPS(cis_check_permission_get_region_info);
    return info;
}

Fam_Region_Item_Info Fam_CIS_Direct::check_permission_get_item_info(
    uint64_t regionId, uint64_t offset, uint64_t memoryServerId, uint32_t uid,
    uint32_t gid) {

    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()

    ostringstream message;
    Fam_DataItem_Metadata dataitem;
    uint64_t key;
    message << "Error While locating dataitem : ";
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "could not find the dataitem";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    // This key is only applicable for shared memory model
    if (check_dataitem_permission(dataitem, 1, uid, gid)) {
        key = FAM_WRITE_KEY_SHM | FAM_READ_KEY_SHM;
    } else if (check_dataitem_permission(dataitem, 0, uid, gid)) {
        key = FAM_READ_KEY_SHM;
    } else if (uid == dataitem.uid) {
        key = FAM_KEY_INVALID;
    } else {
        message << "Dataitem access is not permitted ";
        THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION, message.str().c_str());
    }

    info.regionId = dataitem.regionId;
    info.offset = dataitem.offset;
    info.size = dataitem.size;
    info.perm = dataitem.perm;
    strncpy(info.name, dataitem.name, metadataService->metadata_maxkeylen());
    info.maxNameLen = metadataService->metadata_maxkeylen();
    info.key = key;
    info.base = get_local_pointer(regionId, offset);

    CIS_DIRECT_PROFILE_END_OPS(cis_check_permission_get_item_info);
    return info;
}

Fam_Region_Item_Info Fam_CIS_Direct::get_stat_info(uint64_t regionId,
                                                   uint64_t offset,
                                                   uint64_t memoryServerId,
                                                   uint32_t uid, uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    message << "Error While locating dataitem : ";

    Fam_DataItem_Metadata dataitem;
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "could not find the dataitem";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    if (uid != dataitem.uid) {
        if (!check_dataitem_permission(dataitem, 0, uid, gid)) {
            message << "Not permitted to access the dataitem";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
    }

    info.size = dataitem.size;
    info.perm = dataitem.perm;
    strncpy(info.name, dataitem.name, metadataService->metadata_maxkeylen());
    info.maxNameLen = metadataService->metadata_maxkeylen();
    CIS_DIRECT_PROFILE_END_OPS(get_stat_info);
    return info;
}

void *Fam_CIS_Direct::get_local_pointer(uint64_t regionId, uint64_t offset) {
    return allocator->get_local_pointer(regionId, offset);
}

void *Fam_CIS_Direct::fam_map(uint64_t regionId, uint64_t offset,
                              uint64_t mememoryServerId, uint32_t uid,
                              uint32_t gid) {
    void *localPointer;
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    Fam_DataItem_Metadata dataitem;
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "could not find the dataitem";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
    if (check_dataitem_permission(dataitem, 1, uid, gid) |
        check_dataitem_permission(dataitem, 0, uid, gid)) {
        localPointer = get_local_pointer(regionId, offset);
    } else {
        THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                        "Not permitted to use this dataitem");
    }
    CIS_DIRECT_PROFILE_END_OPS(cis_fam_map);

    return localPointer;
}
void Fam_CIS_Direct::fam_unmap(void *local, uint64_t regionId, uint64_t offset,
                               uint64_t mememoryServerId, uint32_t uid,
                               uint32_t gid) {
    return;
}

void *Fam_CIS_Direct::copy(uint64_t srcRegionId, uint64_t srcOffset,
                           uint64_t srcCopyStart, uint64_t destRegionId,
                           uint64_t destOffset, uint64_t destCopyStart,
                           uint64_t nbytes, uint64_t mememoryServerId,
                           uint32_t uid, uint32_t gid) {
    ostringstream message;
    message << "Error While copying from dataitem : ";
    Fam_DataItem_Metadata srcDataitem;
    Fam_DataItem_Metadata destDataitem;
    void *srcStart;
    void *destStart;
    CIS_DIRECT_PROFILE_START_OPS()

    uint64_t srcDataitemId = srcOffset / MIN_OBJ_SIZE;
    uint64_t destDataitemId = destOffset / MIN_OBJ_SIZE;
    if (!metadataService->metadata_find_dataitem(srcDataitemId, srcRegionId,
                                                 srcDataitem)) {
        message << "could not find the source dataitem";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    if (!(metadataService->metadata_check_permissions(
            &srcDataitem, META_REGION_ITEM_READ, uid, gid))) {
        message << "Read operation is not permitted on source dataitem";
        THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION, message.str().c_str());
    }

    if (!metadataService->metadata_find_dataitem(destDataitemId, destRegionId,
                                                 destDataitem)) {
        message << "could not find the destination dataitem";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    if (!(metadataService->metadata_check_permissions(
            &destDataitem, META_REGION_ITEM_WRITE, uid, gid))) {
        message << "Write operation is not permitted on destination dataitem";
        THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION, message.str().c_str());
    }

    if ((srcCopyStart + nbytes) < srcDataitem.size)
        srcStart = get_local_pointer(srcRegionId, srcOffset + srcCopyStart);
    else {
        message << "Source offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }

    if ((destCopyStart + nbytes) < destDataitem.size)
        destStart = get_local_pointer(destRegionId, destOffset + destCopyStart);
    else {
        message << "Destination offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }

    if ((srcStart == NULL) || (destStart == NULL)) {
        message
            << "Failed to get local pointer to source or destination dataitem";
        THROW_ERRNO_MSG(CIS_Exception, NULL_POINTER_ACCESS,
                        message.str().c_str());
    } else {
        allocator->copy(destStart, srcStart, nbytes);
    }
    CIS_DIRECT_PROFILE_END_OPS(cis_copy);
    return destStart;
}
} // namespace openfam
