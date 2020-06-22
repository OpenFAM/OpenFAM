/*
 * fam_allocator_client.cpp
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

#include "allocator/fam_allocator_client.h"

using namespace std;

namespace openfam {
Fam_Allocator_Client::Fam_Allocator_Client(MemServerMap servers,
                                           uint64_t port) {
    if (servers.size() == 0) {
        throw Fam_Allocator_Exception(FAM_ERR_MEMSERVER_LIST_EMPTY,
                                      "server name not found");
    }
    famCIS = new Fam_CIS_Client(servers, port);
    uid = (uint32_t)getuid();
    gid = (uint32_t)getgid();
}

Fam_Allocator_Client::Fam_Allocator_Client() {
    famCIS = new Fam_CIS_Direct();
    uid = (uint32_t)getuid();
    gid = (uint32_t)getgid();
}

Fam_Allocator_Client::~Fam_Allocator_Client() { delete famCIS; }

void Fam_Allocator_Client::allocator_initialize() {}

void Fam_Allocator_Client::allocator_finalize() {}

Fam_Region_Descriptor *Fam_Allocator_Client::create_region(
    const char *name, uint64_t nbytes, mode_t permissions,
    Fam_Redundancy_Level redundancyLevel, uint64_t memoryServerId = 0) {
    Fam_Region_Item_Info info;
    try {
        info = famCIS->create_region(name, nbytes, permissions, redundancyLevel,
                                     memoryServerId, uid, gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
    Fam_Global_Descriptor globalDescriptor;
    globalDescriptor.regionId = info.regionId;
    globalDescriptor.offset = info.offset;

    Fam_Region_Descriptor *region =
        new Fam_Region_Descriptor(globalDescriptor, nbytes);
    region->set_name((char *)name);
    region->set_perm(permissions);
    region->set_desc_status(DESC_INIT_DONE);
    return region;
}

void Fam_Allocator_Client::destroy_region(Fam_Region_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    descriptor->set_desc_status(DESC_INVALID);
    try {
        famCIS->destroy_region(regionId, memoryServerId, uid, gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
}

void Fam_Allocator_Client::resize_region(Fam_Region_Descriptor *descriptor,
                                         uint64_t nbytes) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    try {
        famCIS->resize_region(regionId, nbytes, memoryServerId, uid, gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
}

Fam_Descriptor *Fam_Allocator_Client::allocate(const char *name,
                                               uint64_t nbytes,
                                               mode_t accessPermissions,
                                               Fam_Region_Descriptor *region) {
    Fam_Global_Descriptor globalDescriptor = region->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t memoryServerId = region->get_memserver_id();
    Fam_Region_Item_Info info;
    try {
        info = famCIS->allocate(name, nbytes, accessPermissions, regionId,
                                memoryServerId, uid, gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
    globalDescriptor.regionId = info.regionId;
    globalDescriptor.offset = info.offset;
    Fam_Descriptor *dataItem = new Fam_Descriptor(globalDescriptor, nbytes);
    dataItem->bind_key(info.key);
    dataItem->set_base_address(info.base);
    dataItem->set_name((char *)name);
    dataItem->set_perm(accessPermissions);
    dataItem->set_desc_status(DESC_INIT_DONE);
    return dataItem;
}

void Fam_Allocator_Client::deallocate(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t offset = globalDescriptor.offset;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    descriptor->set_desc_status(DESC_INVALID);
    try {
        famCIS->deallocate(regionId, offset, memoryServerId, uid, gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
}

void Fam_Allocator_Client::change_permission(Fam_Region_Descriptor *descriptor,
                                             mode_t accessPermissions) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    try {
        famCIS->change_region_permission(regionId, accessPermissions,
                                         memoryServerId, uid, gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
}

void Fam_Allocator_Client::change_permission(Fam_Descriptor *descriptor,
                                             mode_t accessPermissions) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t offset = globalDescriptor.offset;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    try {
        famCIS->change_dataitem_permission(regionId, offset, accessPermissions,
                                           memoryServerId, uid, gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
}

Fam_Region_Descriptor *
Fam_Allocator_Client::lookup_region(const char *name,
                                    uint64_t memoryServerId = 0) {
    Fam_Region_Item_Info info;
    try {
        info = famCIS->lookup_region(name, memoryServerId, uid, gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
    Fam_Global_Descriptor globalDescriptor;
    globalDescriptor.regionId = info.regionId;
    globalDescriptor.offset = info.offset;

    Fam_Region_Descriptor *region = new Fam_Region_Descriptor(globalDescriptor);
    region->set_size(info.size);
    region->set_perm(info.perm);
    region->set_name(info.name);
    region->set_desc_status(DESC_INIT_DONE);
    return region;
}

Fam_Descriptor *Fam_Allocator_Client::lookup(const char *itemName,
                                             const char *regionName,
                                             uint64_t memoryServerId = 0) {
    Fam_Region_Item_Info info;
    try {
        info = famCIS->lookup(itemName, regionName, memoryServerId, uid, gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
    Fam_Global_Descriptor globalDescriptor;
    globalDescriptor.regionId = info.regionId;
    globalDescriptor.offset = info.offset;
    Fam_Descriptor *dataItem = new Fam_Descriptor(globalDescriptor);
    dataItem->bind_key(FAM_KEY_UNINITIALIZED);
    dataItem->set_size(info.size);
    dataItem->set_perm(info.perm);
    dataItem->set_name(info.name);
    dataItem->set_desc_status(DESC_INIT_DONE_BUT_KEY_NOT_VALID);
    return dataItem;
}

Fam_Region_Item_Info Fam_Allocator_Client::check_permission_get_info(
    Fam_Region_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    try {
        Fam_Region_Item_Info info = famCIS->check_permission_get_region_info(
            regionId, memoryServerId, uid, gid);
        descriptor->set_desc_status(DESC_INIT_DONE);
        return info;
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
}

Fam_Region_Item_Info
Fam_Allocator_Client::check_permission_get_info(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t offset = globalDescriptor.offset;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    try {
        Fam_Region_Item_Info info = famCIS->check_permission_get_item_info(
            regionId, offset, memoryServerId, uid, gid);
        descriptor->set_desc_status(DESC_INIT_DONE);
        return info;
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
}

Fam_Region_Item_Info
Fam_Allocator_Client::get_stat_info(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t offset = globalDescriptor.offset;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    try {
        Fam_Region_Item_Info info =
            famCIS->get_stat_info(regionId, offset, memoryServerId, uid, gid);
        descriptor->set_desc_status(DESC_INIT_DONE_BUT_KEY_NOT_VALID);
        return info;
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
}

void *Fam_Allocator_Client::copy(Fam_Descriptor *src, uint64_t srcCopyStart,
                                 Fam_Descriptor *dest, uint64_t destCopyStart,
                                 uint64_t nbytes) {
    Fam_Global_Descriptor globalDescriptor = src->get_global_descriptor();
    uint64_t srcRegionId = globalDescriptor.regionId;
    uint64_t srcOffset = globalDescriptor.offset;
    globalDescriptor = dest->get_global_descriptor();
    uint64_t destRegionId = globalDescriptor.regionId;
    uint64_t destOffset = globalDescriptor.offset;
    uint64_t memoryServerId = src->get_memserver_id();
    uint64_t srcItemSize = src->get_size();
    uint64_t destItemSize = dest->get_size();

    if ((srcCopyStart + nbytes) > srcItemSize) {
        throw Fam_Allocator_Exception(
            FAM_ERR_OUTOFRANGE,
            "Source offset or size is beyond dataitem boundary");
    }

    if ((destCopyStart + nbytes) > destItemSize) {
        throw Fam_Allocator_Exception(
            FAM_ERR_OUTOFRANGE,
            "Destination offset or size is beyond dataitem boundary");
    }

    try {
        return famCIS->copy(srcRegionId, srcOffset, srcCopyStart, destRegionId,
                            destOffset, destCopyStart, nbytes, memoryServerId,
                            uid, gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
}

void Fam_Allocator_Client::wait_for_copy(void *waitObj) {
    return famCIS->wait_for_copy(waitObj);
}

void *Fam_Allocator_Client::fam_map(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t offset = globalDescriptor.offset;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    try {
        return famCIS->fam_map(regionId, offset, memoryServerId, uid, gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
} // namespace openfam

void Fam_Allocator_Client::fam_unmap(void *local, Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t offset = globalDescriptor.offset;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    try {
        return famCIS->fam_unmap(local, regionId, offset, memoryServerId, uid,
                                 gid);
    } catch (Memserver_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
}

void Fam_Allocator_Client::acquire_CAS_lock(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t offset = globalDescriptor.offset;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    return famCIS->acquire_CAS_lock(offset, memoryServerId);
}

void Fam_Allocator_Client::release_CAS_lock(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t offset = globalDescriptor.offset;
    uint64_t memoryServerId = descriptor->get_memserver_id();
    return famCIS->release_CAS_lock(offset, memoryServerId);
}

int Fam_Allocator_Client::get_addr_size(size_t *addrSize,
                                        uint64_t memoryServerId = 0) {
    return famCIS->get_addr_size(addrSize, memoryServerId);
}

int Fam_Allocator_Client::get_addr(void *addr, size_t addrSize,
                                   uint64_t memoryServerId = 0) {
    return famCIS->get_addr(addr, addrSize, memoryServerId);
}

} // namespace openfam
