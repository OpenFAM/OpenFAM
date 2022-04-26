/*
 * fam_allocator_client.cpp
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

#include <iostream>
#include <stdint.h>   // needed
#include <sys/stat.h> // needed for mode_t

#include "allocator/fam_allocator_client.h"

using namespace std;

namespace openfam {
Fam_Allocator_Client::Fam_Allocator_Client(const char *name, uint64_t port) {
    try {
        famCIS = new Fam_CIS_Client(name, port);
    } catch (Fam_Exception &e) {
        throw Fam_Allocator_Exception((enum Fam_Error)e.fam_error(),
                                      e.fam_error_msg());
    }
    uid = (uint32_t)getuid();
    gid = (uint32_t)getgid();
}

Fam_Allocator_Client::Fam_Allocator_Client(bool isSharedMemory) {
    famCIS = new Fam_CIS_Direct(NULL, true, isSharedMemory);
    uid = (uint32_t)getuid();
    gid = (uint32_t)getgid();
}

Fam_Allocator_Client::~Fam_Allocator_Client() { delete famCIS; }

void Fam_Allocator_Client::allocator_initialize() {}

void Fam_Allocator_Client::allocator_finalize() {}

uint64_t Fam_Allocator_Client::get_num_memory_servers() {
    return famCIS->get_num_memory_servers();
}

Fam_Region_Descriptor *
Fam_Allocator_Client::create_region(const char *name, uint64_t nbytes,
                                    mode_t permissions,
                                    Fam_Region_Attributes *regionAttributes) {
    Fam_Region_Item_Info info;
    Fam_Global_Descriptor globalDescriptor;
    Fam_Region_Descriptor *region;

        info = famCIS->create_region(name, nbytes, permissions,
                                     regionAttributes, uid, gid);
        globalDescriptor.regionId = info.regionId;
        globalDescriptor.offset = info.offset;
        region = new Fam_Region_Descriptor(globalDescriptor, nbytes);
        region->set_redundancyLevel(regionAttributes->redundancyLevel);
        region->set_memoryType(regionAttributes->memoryType);
        region->set_interleaveEnable(regionAttributes->interleaveEnable);

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
        famCIS->destroy_region(regionId, memoryServerId, uid, gid);
}

void Fam_Allocator_Client::resize_region(Fam_Region_Descriptor *descriptor,
                                         uint64_t nbytes) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t memoryServerId = descriptor->get_memserver_id();
        famCIS->resize_region(regionId, nbytes, memoryServerId, uid, gid);
}

Fam_Descriptor *Fam_Allocator_Client::allocate(const char *name,
                                               uint64_t nbytes,
                                               mode_t accessPermissions,
                                               Fam_Region_Descriptor *region) {
    Fam_Global_Descriptor globalDescriptor = region->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t memoryServerId = region->get_memserver_id();
    Fam_Region_Item_Info info;
    info = famCIS->allocate(name, nbytes, accessPermissions, regionId,
                            memoryServerId, uid, gid);
    // Note : This global descriptor can not be used to create
    // Fam_Region_Descriptor because along with region id, first memory server
    // id is stored in regionId field of Fam_Global_Descriptor
    globalDescriptor.regionId =
        info.regionId | (info.memoryServerIds[0] << MEMSERVERID_SHIFT);
    globalDescriptor.offset = info.offset;
    Fam_Descriptor *dataItem = new Fam_Descriptor(globalDescriptor, nbytes);
    dataItem->set_used_memsrv_cnt(info.used_memsrv_cnt);
    dataItem->set_memserver_ids(info.memoryServerIds);
    dataItem->bind_keys(info.keys, info.used_memsrv_cnt);
    dataItem->set_base_address_list(info.baseAddressList, info.used_memsrv_cnt);
    dataItem->set_name((char *)name);
    dataItem->set_perm(accessPermissions);
    dataItem->set_desc_status(DESC_INIT_DONE);
    dataItem->set_interleave_size(info.interleaveSize);
    dataItem->set_uid(uid);
    dataItem->set_gid(gid);
    return dataItem;
}

void Fam_Allocator_Client::deallocate(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t offset = globalDescriptor.offset;
    uint64_t firstMemserverId = descriptor->get_first_memserver_id();
    descriptor->set_desc_status(DESC_INVALID);
    famCIS->deallocate(regionId, offset, firstMemserverId, uid, gid);
}

void Fam_Allocator_Client::change_permission(Fam_Region_Descriptor *descriptor,
                                             mode_t accessPermissions) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t memoryServerId = descriptor->get_memserver_id();
        famCIS->change_region_permission(regionId, accessPermissions,
                                         memoryServerId, uid, gid);
}

void Fam_Allocator_Client::change_permission(Fam_Descriptor *descriptor,
                                             mode_t accessPermissions) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t offset = globalDescriptor.offset;
    uint64_t firstMemserverId = descriptor->get_first_memserver_id();
    famCIS->change_dataitem_permission(regionId, offset, accessPermissions,
                                       firstMemserverId, uid, gid);
}

Fam_Region_Descriptor *Fam_Allocator_Client::lookup_region(const char *name) {
    Fam_Region_Item_Info info;
    info = famCIS->lookup_region(name, uid, gid);
    Fam_Global_Descriptor globalDescriptor;
    globalDescriptor.regionId = info.regionId;
    globalDescriptor.offset = info.offset;

    Fam_Region_Descriptor *region = new Fam_Region_Descriptor(globalDescriptor);
    region->set_size(info.size);
    region->set_perm(info.perm);
    region->set_name(info.name);
    region->set_desc_status(DESC_INIT_DONE);
    region->set_redundancyLevel(info.redundancyLevel);
    region->set_memoryType(info.memoryType);
    region->set_interleaveEnable(info.interleaveEnable);
    return region;
}

Fam_Descriptor *Fam_Allocator_Client::lookup(const char *itemName,
                                             const char *regionName) {
    Fam_Region_Item_Info info;
    info = famCIS->lookup(itemName, regionName, uid, gid);
    Fam_Global_Descriptor globalDescriptor;
    globalDescriptor.regionId =
        info.regionId | (info.memoryServerIds[0] << MEMSERVERID_SHIFT);
    globalDescriptor.offset = info.offset;
    Fam_Descriptor *dataItem = new Fam_Descriptor(globalDescriptor);
    dataItem->set_used_memsrv_cnt(info.used_memsrv_cnt);
    dataItem->set_memserver_ids(info.memoryServerIds);
    dataItem->set_size(info.size);
    dataItem->set_perm(info.perm);
    dataItem->set_name(info.name);
    dataItem->set_desc_status(DESC_INIT_DONE_BUT_KEY_NOT_VALID);
    dataItem->set_interleave_size(info.interleaveSize);
    dataItem->set_uid(uid);
    dataItem->set_gid(gid);
    return dataItem;
}

Fam_Region_Item_Info Fam_Allocator_Client::check_permission_get_info(
    Fam_Region_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t memoryServerId = descriptor->get_memserver_id();
        Fam_Region_Item_Info info = famCIS->check_permission_get_region_info(
            regionId, memoryServerId, uid, gid);
        descriptor->set_desc_status(DESC_INIT_DONE);
        return info;
}

Fam_Region_Item_Info
Fam_Allocator_Client::check_permission_get_info(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t offset = globalDescriptor.offset;
    uint64_t firstMemserverId = descriptor->get_first_memserver_id();
    Fam_Region_Item_Info info = famCIS->check_permission_get_item_info(
        regionId, offset, firstMemserverId, uid, gid);
    descriptor->set_used_memsrv_cnt(info.used_memsrv_cnt);
    descriptor->set_memserver_ids(info.memoryServerIds);
    descriptor->set_desc_status(DESC_INIT_DONE);
    descriptor->bind_keys(info.keys, info.used_memsrv_cnt);
    descriptor->set_name(info.name);
    descriptor->set_perm(info.perm);
    descriptor->set_size(info.size);
    descriptor->set_base_address_list(info.baseAddressList,
                                      info.used_memsrv_cnt);
    descriptor->set_interleave_size(info.interleaveSize);
    return info;
}

Fam_Region_Item_Info
Fam_Allocator_Client::get_stat_info(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t offset = globalDescriptor.offset;
    uint64_t firstMemserverId = descriptor->get_first_memserver_id();
    Fam_Region_Item_Info info =
        famCIS->get_stat_info(regionId, offset, firstMemserverId, uid, gid);
    descriptor->set_desc_status(DESC_INIT_DONE_BUT_KEY_NOT_VALID);
    descriptor->set_name(info.name);
    descriptor->set_perm(info.perm);
    descriptor->set_size(info.size);
    descriptor->set_uid(info.uid);
    descriptor->set_gid(info.gid);
    descriptor->set_interleave_size(info.interleaveSize);
    descriptor->set_used_memsrv_cnt(info.used_memsrv_cnt);
    descriptor->set_memserver_ids(info.memoryServerIds);
    return info;
}

void *Fam_Allocator_Client::copy(Fam_Descriptor *src, uint64_t srcCopyStart,
                                 Fam_Descriptor *dest, uint64_t destCopyStart,
                                 uint64_t nbytes) {
    Fam_Global_Descriptor globalDescriptor = src->get_global_descriptor();
    uint64_t srcRegionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t srcOffset = globalDescriptor.offset;
    globalDescriptor = dest->get_global_descriptor();
    uint64_t destRegionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t destOffset = globalDescriptor.offset;
    uint64_t firstSrcMemserverId = src->get_first_memserver_id();
    uint64_t firstdestMemserverId = dest->get_first_memserver_id();
    uint64_t srcItemSize = src->get_size();
    uint64_t destItemSize = dest->get_size();
    void **srcBaseAddrList = src->get_base_address_list();
    uint64_t *srcKeys = src->get_keys();
    uint64_t srcUsedMemsrvCnt = src->get_used_memsrv_cnt();

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

    return famCIS->copy(srcRegionId, srcOffset, srcUsedMemsrvCnt, srcCopyStart,
                        srcKeys, (uint64_t *)srcBaseAddrList, destRegionId,
                        destOffset, destCopyStart, nbytes, firstSrcMemserverId,
                        firstdestMemserverId, uid, gid);
}

void Fam_Allocator_Client::wait_for_copy(void *waitObj) {
    return famCIS->wait_for_copy(waitObj);
}

void *Fam_Allocator_Client::backup(Fam_Descriptor *src,
                                   const char *BackupName) {
    Fam_Global_Descriptor globalDescriptor = src->get_global_descriptor();
    uint64_t srcRegionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t srcFirstMemserverId = src->get_first_memserver_id();
    uint64_t srcOffset = globalDescriptor.offset;

    void *waitObj;
    waitObj = famCIS->backup(srcRegionId, srcOffset, srcFirstMemserverId,
                             BackupName, uid, gid);
    return waitObj;
}

void *Fam_Allocator_Client::restore(Fam_Descriptor *dest,
                                    const char *BackupName) {
    Fam_Global_Descriptor globalDescriptor = dest->get_global_descriptor();
    uint64_t destRegionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t destFirstMemserverId = dest->get_first_memserver_id();
    uint64_t destOffset = globalDescriptor.offset;
    return famCIS->restore(destRegionId, destOffset, destFirstMemserverId,
                           BackupName, uid, gid);
}

void *Fam_Allocator_Client::delete_backup(const char *BackupName) {
    uint64_t num_mservers = famCIS->get_num_memory_servers();
    uint64_t memoryserverIdx =
        ((num_mservers == 1) ? 0 : (rand() % num_mservers));
    return famCIS->delete_backup(BackupName, memoryserverIdx, uid, gid);
}

char *Fam_Allocator_Client::list_backup(const char *BackupName) {
    uint64_t num_mservers = famCIS->get_num_memory_servers();
    uint64_t memoryserverIdx =
        ((num_mservers == 1) ? 0 : (rand() % num_mservers));

    return ((char *)strdup(
        famCIS->list_backup(BackupName, memoryserverIdx, uid, gid).c_str()));
}

Fam_Backup_Info
Fam_Allocator_Client::get_backup_info(string BackupName,
                                      Fam_Region_Descriptor *destRegion) {
    return famCIS->get_backup_info(BackupName, destRegion->get_memserver_id(),
                                   uid, gid);
}

void Fam_Allocator_Client::wait_for_backup(void *waitObj) {
    return famCIS->wait_for_backup(waitObj);
}
void Fam_Allocator_Client::wait_for_restore(void *waitObj) {
    return famCIS->wait_for_restore(waitObj);
}

void Fam_Allocator_Client::wait_for_delete_backup(void *waitObj) {
    return famCIS->wait_for_delete_backup(waitObj);
}

void *Fam_Allocator_Client::fam_map(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t offset = globalDescriptor.offset;
    uint64_t firstMemserverId = descriptor->get_first_memserver_id();
    return famCIS->fam_map(regionId, offset, firstMemserverId, uid, gid);
}

void Fam_Allocator_Client::fam_unmap(void *local, Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t offset = globalDescriptor.offset;
    uint64_t firstMemserverId = descriptor->get_first_memserver_id();
    return famCIS->fam_unmap(local, regionId, offset, firstMemserverId, uid,
                             gid);
}

void Fam_Allocator_Client::acquire_CAS_lock(Fam_Descriptor *descriptor,
                                            uint64_t memserverId) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t offset = globalDescriptor.offset;
    return famCIS->acquire_CAS_lock(offset, memserverId);
}

void Fam_Allocator_Client::release_CAS_lock(Fam_Descriptor *descriptor,
                                            uint64_t memserverId) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t offset = globalDescriptor.offset;
    return famCIS->release_CAS_lock(offset, memserverId);
}

int Fam_Allocator_Client::get_addr_size(size_t *addrSize,
                                        uint64_t memoryServerId = 0) {
    if ((*addrSize = famCIS->get_addr_size(memoryServerId)) <= 0)
        return -1;
    return 0;
}

int Fam_Allocator_Client::get_addr(void *addr, size_t addrSize,
                                   uint64_t memoryServerId = 0) {
    if (addrSize > famCIS->get_addr_size(memoryServerId))
        return -1;

    famCIS->get_addr(addr, memoryServerId);
    return 0;
}

int Fam_Allocator_Client::get_memserverinfo_size(size_t *memServerInfoSize) {
    if ((*memServerInfoSize = famCIS->get_memserverinfo_size()) <= 0)
        return -1;
    return 0;
}

int Fam_Allocator_Client::get_memserverinfo(void *memServerInfoBuffer) {

    famCIS->get_memserverinfo(memServerInfoBuffer);
    return 0;
}
} // namespace openfam
