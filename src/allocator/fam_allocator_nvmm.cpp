/*
 * fam_allocator_nvmm.cpp
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
#include <iostream>
#include <stdint.h>   // needed
#include <sys/stat.h> // needed for mode_t

#include "allocator/fam_allocator_nvmm.h"
#include "fam/fam.h"

using namespace std;

namespace openfam {
Fam_Allocator_NVMM::Fam_Allocator_NVMM() {
    allocator = new Memserver_Allocator();
    uid = (uint32_t)getuid();
    gid = (uint32_t)getgid();
}

void Fam_Allocator_NVMM::allocator_initialize() {}

void Fam_Allocator_NVMM::allocator_finalize() {
    allocator->memserver_allocator_finalize();
}

Fam_Region_Descriptor *Fam_Allocator_NVMM::create_region(
    const char *name, uint64_t nbytes, mode_t permissions,
    Fam_Redundancy_Level redundancyLevel, uint64_t memoryServerId) {
    uint64_t regionId;
    try {
        allocator->create_region(name, regionId, (size_t)nbytes, permissions,
                                 uid, gid);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }

    Fam_Global_Descriptor globalDescriptor;
    globalDescriptor.regionId = regionId;
    globalDescriptor.offset = INVALID_OFFSET;

    Fam_Region_Descriptor *region =
        new Fam_Region_Descriptor(globalDescriptor, nbytes);
    region->set_size(nbytes);
    region->set_perm(permissions);
    region->set_name((char *)name);
    region->set_desc_status(DESC_INIT_DONE);
    return region;
}

void Fam_Allocator_NVMM::destroy_region(Fam_Region_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();

    descriptor->set_desc_status(DESC_INVALID);
    try {
        allocator->destroy_region(globalDescriptor.regionId, uid, gid);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
    return;
}

int Fam_Allocator_NVMM::resize_region(Fam_Region_Descriptor *descriptor,
                                      uint64_t nbytes) {

    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();

    int ret = 0;
    try {
        ret = allocator->resize_region(globalDescriptor.regionId, uid, gid,
                                       nbytes);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }

    return ret;
}

Fam_Descriptor *Fam_Allocator_NVMM::allocate(const char *name, uint64_t nbytes,
                                             mode_t accessPermissions,
                                             Fam_Region_Descriptor *region) {

    Fam_Global_Descriptor globalDescriptor = region->get_global_descriptor();
    Fam_DataItem_Metadata dataitem;
    uint64_t offset;
    uint64_t key;
    void *localPointer;
    try {
        allocator->allocate(name, globalDescriptor.regionId, nbytes, offset,
                            accessPermissions, uid, gid, dataitem,
                            localPointer);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }

    globalDescriptor.offset = offset;
    Fam_Descriptor *dataItemDesc = new Fam_Descriptor(globalDescriptor, nbytes);
    if (allocator->check_dataitem_permission(dataitem, 1, uid, gid)) {
        key = FAM_WRITE_KEY_SHM | FAM_READ_KEY_SHM;
    } else if (allocator->check_dataitem_permission(dataitem, 0, uid, gid)) {
        key = FAM_READ_KEY_SHM;
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_NOPERM,
                                      "Not permitted to use this dataitem");
    }
    dataItemDesc->set_base_address(localPointer);
    dataItemDesc->bind_key(key);
    dataItemDesc->set_size(nbytes);
    dataItemDesc->set_perm(accessPermissions);
    dataItemDesc->set_name((char *)name);
    dataItemDesc->set_desc_status(DESC_INIT_DONE);

    return dataItemDesc;
}

void Fam_Allocator_NVMM::deallocate(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    descriptor->set_desc_status(DESC_INVALID);

    try {
        allocator->deallocate(globalDescriptor.regionId,
                              globalDescriptor.offset, uid, gid);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
    return;
}

int Fam_Allocator_NVMM::change_permission(Fam_Region_Descriptor *descriptor,
                                          mode_t accessPermissions) {

    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();

    try {
        allocator->change_region_permission(globalDescriptor.regionId,
                                            accessPermissions, uid, gid);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
    return FAM_SUCCESS;
}

int Fam_Allocator_NVMM::change_permission(Fam_Descriptor *descriptor,
                                          mode_t accessPermissions) {

    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();

    try {
        allocator->change_dataitem_permission(globalDescriptor.regionId,
                                              globalDescriptor.offset,
                                              accessPermissions, uid, gid);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
    return FAM_SUCCESS;
}

Fam_Region_Descriptor *
Fam_Allocator_NVMM::lookup_region(const char *name, uint64_t memoryServerId) {
    Fam_Region_Metadata region;

    try {
        allocator->get_region(name, uid, gid, region);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }

    if (allocator->check_region_permission(region, 0, uid, gid)) {
        Fam_Global_Descriptor globalDescriptor;
        globalDescriptor.regionId = region.regionId;
        globalDescriptor.offset = region.offset;
        Fam_Region_Descriptor *regionDesc =
            new Fam_Region_Descriptor(globalDescriptor, region.size);
        regionDesc->set_size(region.size);
        regionDesc->set_perm(region.perm);
        regionDesc->set_name(region.name);
        regionDesc->set_desc_status(DESC_INIT_DONE);
        return regionDesc;
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_NOPERM,
                                      "Region access not permitted");
    }
}

Fam_Descriptor *Fam_Allocator_NVMM::lookup(const char *itemName,
                                           const char *regionName,
                                           uint64_t memoryServerId) {
    Fam_DataItem_Metadata dataitem;
    uint64_t key;
    try {
        allocator->get_dataitem(itemName, regionName, uid, gid, dataitem);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }

    if (allocator->check_dataitem_permission(dataitem, 0, uid, gid)) {
        Fam_Global_Descriptor globalDescriptor;
        globalDescriptor.regionId = dataitem.regionId;
        globalDescriptor.offset = dataitem.offset;
        Fam_Descriptor *dataItemDesc =
            new Fam_Descriptor(globalDescriptor, dataitem.size);
        if (allocator->check_dataitem_permission(dataitem, 1, uid, gid)) {
            key = FAM_WRITE_KEY_SHM | FAM_READ_KEY_SHM;
        } else if (allocator->check_dataitem_permission(dataitem, 0, uid,
                                                        gid)) {
            key = FAM_READ_KEY_SHM;
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_NOPERM,
                                          "Not permitted to use this dataitem");
        }

        void *localPointer;
        try {
            localPointer = allocator->get_local_pointer(dataitem.regionId,
                                                        dataitem.offset);
        }
        catch (Memserver_Exception &e) {
            throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                          e.fam_error_msg());
        }

        dataItemDesc->set_base_address(localPointer);
        dataItemDesc->bind_key(key);
        dataItemDesc->set_size(dataitem.size);
        dataItemDesc->set_perm(dataitem.perm);
        dataItemDesc->set_name(dataitem.name);
        dataItemDesc->set_desc_status(DESC_INIT_DONE);
        return dataItemDesc;
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_NOPERM,
                                      "Not permitted to use this dataitem");
    }
}

Fam_Region_Item_Info Fam_Allocator_NVMM::check_permission_get_info(
    Fam_Region_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    Fam_Region_Metadata region;

    // find if dataitem present
    try {
        allocator->get_region(globalDescriptor.regionId, uid, gid, region);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }

    Fam_Region_Item_Info regionInfo;
    regionInfo.size = region.size;
    regionInfo.perm = region.perm;
    regionInfo.name = strdup(region.name);
    descriptor->set_desc_status(DESC_INIT_DONE);
    return regionInfo;
}

Fam_Region_Item_Info
Fam_Allocator_NVMM::check_permission_get_info(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    Fam_DataItem_Metadata dataitem;
    uint64_t key;

    // find if dataitem present
    try {
        allocator->get_dataitem(globalDescriptor.regionId,
                                globalDescriptor.offset, uid, gid, dataitem);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }

    if (allocator->check_dataitem_permission(dataitem, 1, uid, gid)) {
        key = FAM_WRITE_KEY_SHM | FAM_READ_KEY_SHM;
    } else if (allocator->check_dataitem_permission(dataitem, 0, uid, gid)) {
        key = FAM_READ_KEY_SHM;
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_NOPERM,
                                      "Not permitted to use this dataitem");
    }

    try {
        allocator->open_heap(globalDescriptor.regionId);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }

    void *base;
    try {
        base = allocator->get_local_pointer(globalDescriptor.regionId,
                                            globalDescriptor.offset);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
    Fam_Region_Item_Info itemInfo;
    itemInfo.key = key;
    itemInfo.size = dataitem.size;
    itemInfo.base = base;
    itemInfo.perm = dataitem.perm;
    itemInfo.name = strdup(dataitem.name);
    descriptor->set_desc_status(DESC_INIT_DONE);

    return itemInfo;
}

Fam_Region_Item_Info
Fam_Allocator_NVMM::get_stat_info(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    Fam_DataItem_Metadata dataitem;

    // find if dataitem present
    try {
        allocator->get_dataitem(globalDescriptor.regionId,
                                globalDescriptor.offset, uid, gid, dataitem);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }

    Fam_Region_Item_Info itemInfo;
    itemInfo.size = dataitem.size;
    itemInfo.perm = dataitem.perm;
    itemInfo.name = strdup(dataitem.name);
    descriptor->set_desc_status(DESC_INIT_DONE);

    return itemInfo;
}

void *Fam_Allocator_NVMM::fam_map(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    Fam_DataItem_Metadata dataitem;
    void *localPtr = NULL;

    // find if dataitem present
    try {
        allocator->get_dataitem(globalDescriptor.regionId,
                                globalDescriptor.offset, uid, gid, dataitem);
    }
    catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }

    if (allocator->check_dataitem_permission(dataitem, 1, uid, gid)) {
        try {
            localPtr = allocator->get_local_pointer(globalDescriptor.regionId,
                                                    globalDescriptor.offset);
        }
        catch (Memserver_Exception &e) {
            throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                          e.fam_error_msg());
        }
    } else if (allocator->check_dataitem_permission(dataitem, 0, uid, gid)) {
        try {
            localPtr = allocator->get_local_pointer(globalDescriptor.regionId,
                                                    globalDescriptor.offset);
        }
        catch (Memserver_Exception &e) {
            throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                          e.fam_error_msg());
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_NOPERM,
                                      "Not permitted to use this dataitem");
    }

    return localPtr;
}

void Fam_Allocator_NVMM::fam_unmap(void *local, Fam_Descriptor *descriptor) {
    return;
}

} // namespace openfam
