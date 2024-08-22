/*
 * fam_allocator_client.cpp
 * Copyright (c) 2020-2023 Hewlett Packard Enterprise Development, LP. All
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
#ifdef USE_THALLIUM
#include <cis/fam_cis_thallium_client.h>
#include <common/fam_thallium_engine_helper.h>
#include <thallium.hpp>
#endif

using namespace std;

namespace openfam {

Fam_Allocator_Client::Fam_Allocator_Client(const char *name, uint64_t port,
                                           string rpc_framework_type,
                                           string rpc_protocol_type,
                                           bool enableResourceRelease) {
    ostringstream message;

    if (strcmp(rpc_framework_type.c_str(), FAM_OPTIONS_GRPC_STR) == 0) {
        famCIS = new Fam_CIS_Client(name, port);
    }
#ifdef USE_THALLIUM
    else if (strcmp(rpc_framework_type.c_str(), FAM_OPTIONS_THALLIUM_STR) ==
             0) {
        // Using Thallium_Engine singleton class to obtain the engine
        string provider_protocol = protocol_map(rpc_protocol_type);
        Thallium_Engine *thal_engine_gen =
            Thallium_Engine::get_instance(provider_protocol);
        tl::engine engine = thal_engine_gen->get_engine();
        famCIS = new Fam_CIS_Thallium_Client(engine, name, port);
    }
#endif
    else {
        // Raise an exception
        message << "Invalid value specified for Fam config "
                   "option:rpc_framework_type.";
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }
    this->isSharedMemory = false;
    this->enableResourceRelease = enableResourceRelease;
    uid = (uint32_t)getuid();
    gid = (uint32_t)getgid();
    // resource manager to keep reference count of resources within PE
    famResourceManager = new Fam_Client_Resource_Manager(famCIS);
}

Fam_Allocator_Client::Fam_Allocator_Client(bool isSharedMemory,
                                           bool enableResourceRelease) {
    famCIS = new Fam_CIS_Direct(NULL, true, isSharedMemory);
    this->isSharedMemory = isSharedMemory;
    this->enableResourceRelease = enableResourceRelease;
    uid = (uint32_t)getuid();
    gid = (uint32_t)getgid();
    // resource manager to keep reference count of resources within PE
    famResourceManager = new Fam_Client_Resource_Manager(famCIS);
}

Fam_Allocator_Client::~Fam_Allocator_Client() {
    delete famCIS;
    if (famResourceManager)
        delete famResourceManager;
}

void Fam_Allocator_Client::allocator_initialize() {}

void Fam_Allocator_Client::allocator_finalize() {}

uint64_t Fam_Allocator_Client::get_num_memory_servers() {
    return famCIS->get_num_memory_servers();
}

/*
 * Region in fam can span across memory servers, region memory
 * map(Fam_Region_Memory_Map) is a mapping between memory server ID and
 * corresponding memory registration details(keys, base address etc.)
 * lookup_region_memory_map function read memory registration details from
 * Fam_Region_Memory_Map for a given memory server ID.
 */
void Fam_Allocator_Client::lookup_region_memory_map(
    Fam_Region_Memory_Map famRegionMemoryMap, uint64_t memserverId,
    Fam_Region_Memory *regionMemory) {
    std::ostringstream message;
    auto obj = famRegionMemoryMap.find(memserverId);
    if (obj == famRegionMemoryMap.end()) {
        message << "Memory registration information not found for requested "
                   "memory server in the memory map";
        THROW_ERRNO_MSG(Fam_Allocator_Exception, FAM_ERR_RESOURCE,
                        message.str().c_str());
    }
    *regionMemory = obj->second;
}

/*
 * Open region with REGION level permission
 */
void Fam_Allocator_Client::open_region_with_registration(
    uint64_t regionId, Fam_Region_Memory_Map *famRegionMemoryMap) {
    if (enableResourceRelease) {
        Fam_Client_Resource *famResource = famResourceManager->open_resource(
            regionId, REGION, uid, gid, FAM_REGISTER_MEMORY);
        *famRegionMemoryMap = famResource->memoryMap;
    } else {
        famCIS->get_region_memory(regionId, uid, gid, famRegionMemoryMap);
    }
}

/*
 * Open region with DATAITEM level permission
 */
void Fam_Allocator_Client::open_region_without_registration(uint64_t regionId) {
    if (enableResourceRelease) {
        famResourceManager->open_resource(regionId, DATAITEM, uid, gid,
                                          FAM_RESOURCE_DEFAULT);
    }
}
/*
 * close_region function closes the local resources associated with the region
 */
void Fam_Allocator_Client::close_region(uint64_t regionId) {
    /*
     * Close the local resources of the region(This operation reduces the
     * reference count)
     */
    if (enableResourceRelease)
        famResourceManager->close_resource(regionId);
}

/*
 * Close all the regions referenced locally
 */
void Fam_Allocator_Client::close_all_regions() {
    /*
     * Close resources of all regions that are open
     */
    if (enableResourceRelease)
        famResourceManager->close_all_resources();
}

Fam_Region_Descriptor *
Fam_Allocator_Client::create_region(const char *name, uint64_t nbytes,
                                    mode_t permissions,
                                    Fam_Region_Attributes *regionAttributes) {
    Fam_Region_Item_Info info;
    Fam_Global_Descriptor globalDescriptor;
    Fam_Region_Descriptor *region;

    info = famCIS->create_region(name, nbytes, permissions, regionAttributes,
                                 uid, gid);
    globalDescriptor.regionId = info.regionId;
    globalDescriptor.offset = info.offset;
    region = new Fam_Region_Descriptor(globalDescriptor, nbytes);
    region->set_redundancyLevel(regionAttributes->redundancyLevel);
    region->set_memoryType(regionAttributes->memoryType);
    region->set_interleaveEnable(regionAttributes->interleaveEnable);
    region->set_permissionLevel(regionAttributes->permissionLevel);
    region->set_allocationPolicy(regionAttributes->allocationPolicy);
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
    std::ostringstream message;
    Fam_Global_Descriptor globalDescriptor = region->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId;
    uint64_t memoryServerId = region->get_memserver_id();
    Fam_Permission_Level permissionLevel = region->get_permissionLevel();
    Fam_Region_Item_Info info = famCIS->allocate(
        name, nbytes, accessPermissions, regionId, memoryServerId, uid, gid);
    // Note : This global descriptor can not be used to create
    // Fam_Region_Descriptor because along with region id, first memory
    // server id is stored in regionId field of Fam_Global_Descriptor
    globalDescriptor.regionId =
        regionId | (info.memoryServerIds[0] << MEMSERVERID_SHIFT);
    globalDescriptor.offset = info.dataitemOffsets[0];

    // descriptor is completely valid only if all the fields are set. If memory
    // registration details such as key and base address are missing due to
    // memory registration failure, descriptor is in unintialized state. This
    // descriptor is again validated during datapath/atomic operations and tried
    // to fill missing fields

    // Create a new descriptor
    Fam_Descriptor *dataItem = new Fam_Descriptor(globalDescriptor, nbytes);
    // Fill the fields of the descritor
    dataItem->set_used_memsrv_cnt(info.used_memsrv_cnt);
    dataItem->set_memserver_ids(info.memoryServerIds);
    dataItem->set_name((char *)name);
    dataItem->set_perm(info.perm);
    dataItem->set_interleave_size(info.interleaveSize);
    dataItem->set_permissionLevel(permissionLevel);
    dataItem->set_uid(uid);
    dataItem->set_gid(gid);

    // Only for memory server model region keys are used.
    if (isSharedMemory || (permissionLevel == DATAITEM)) {
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
        try {
            open_region_without_registration(regionId);
        } catch (...) {
            // If failed to open, set descriptor to
            // DESC_INIT_DONE_BUT_KEY_NOT_VALID and return
            dataItem->set_desc_status(DESC_INIT_DONE_BUT_KEY_NOT_VALID);
            return dataItem;
        }
#else
        if (isSharedMemory) {
            try {
                open_region_without_registration(regionId);
            } catch (...) {
                // If failed to open, set descriptor to
                // DESC_INIT_DONE_BUT_KEY_NOT_VALID and return
                dataItem->set_desc_status(DESC_INIT_DONE_BUT_KEY_NOT_VALID);
                return dataItem;
            }
        }
#endif
        // In shared memory model, there is no memory registration.
        // But, there is a fixed key for different access types(READ or
        // READWRITE).

        // If permission level is set to DATAITEM, each dataitem memory is
        // registered in the memory server during allocate process.
        // "itemRegistrationStatus" field indicates the status of the memory
        // registration, if it succeeds(TRUE value), set key and base address
        // fields in the descriptor and set descriptor status to DESC_INIT_DONE
        // otherwise if memory registration fails set descriptor status to
        // DESC_INIT_DONE_BUT_KEY_NOT_VALID.
        if (info.itemRegistrationStatus) {
            dataItem->bind_keys(info.dataitemKeys, info.used_memsrv_cnt);
            dataItem->set_base_address_list(info.baseAddressList,
                                            info.used_memsrv_cnt);
            dataItem->set_desc_status(DESC_INIT_DONE);
        } else {
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
            close_region(regionId);
#else
            if (isSharedMemory)
                close_region(regionId);
#endif
            dataItem->set_desc_status(DESC_INIT_DONE_BUT_KEY_NOT_VALID);
        }
    } else {
        Fam_Region_Memory_Map famRegionMemoryMap;
        try {
            open_region_with_registration(regionId, &famRegionMemoryMap);
        } catch (...) {
            // If failed to open, set descriptor to
            // DESC_INIT_DONE_BUT_KEY_NOT_VALID and return
            dataItem->set_desc_status(DESC_INIT_DONE_BUT_KEY_NOT_VALID);
            return dataItem;
        }

        // Obtain key and base address corresponding to data item based on
        // the extent index. Iterate through all memory servers across which
        // the data item is interleaved.
        for (int i = 0; i < (int)info.used_memsrv_cnt; i++) {
            // Get the region memory structure Fam_Region_Memory
            // corresponding to given memory sevrer
            int extentIdx;
            uint64_t startPos;
            Fam_Region_Memory regionMemory;
            try {
                lookup_region_memory_map(
                    famRegionMemoryMap, info.memoryServerIds[i], &regionMemory);
            } catch (...) {
                // Close the region
                close_region(regionId);
                dataItem->set_desc_status(DESC_INIT_DONE_BUT_KEY_NOT_VALID);
                return dataItem;
            }
            // Decode the obtained data item offset and get index of the
            // region extent to which it belongs and offset from region base
            // address
            decode_offset(info.dataitemOffsets[i], &extentIdx, &startPos);
            int numExtents = (int)regionMemory.keys.size();
            // If the memory information corresponding to extentIdx not
            // present locally update the region memory map
            if (extentIdx >= numExtents) {
                Fam_Region_Memory_Map updatedRegionMemoryMap;
                if (enableResourceRelease) {
                    Fam_Client_Resource *famResource =
                        famResourceManager->find_resource(regionId);
                    // Lock the resource to synchronize update operation with
                    // other threads
                    if (LOCK_REGION_RESOURCE(&famResource->statusAndRefcount)) {
                        try {
                            famCIS->get_region_memory(regionId, uid, gid,
                                                      &updatedRegionMemoryMap);
                        } catch (...) {
                            // Unlock the resource so that other threads can
                            // continue
                            UNLOCK_REGION_RESOURCE(
                                &famResource->statusAndRefcount);
                            // Close the region
                            close_region(regionId);
                            dataItem->set_desc_status(
                                DESC_INIT_DONE_BUT_KEY_NOT_VALID);
                            return dataItem;
                        }
                        // update region memory map
                        famResource->memoryMap = updatedRegionMemoryMap;
                        UNLOCK_REGION_RESOURCE(&famResource->statusAndRefcount);
                    }
                } else {
                    try {
                        famCIS->get_region_memory(regionId, uid, gid,
                                                  &updatedRegionMemoryMap);
                    } catch (...) {
                        dataItem->set_desc_status(
                            DESC_INIT_DONE_BUT_KEY_NOT_VALID);
                        return dataItem;
                    }
                }

                // lookup the updated region memory map
                lookup_region_memory_map(updatedRegionMemoryMap,
                                         info.memoryServerIds[i],
                                         &regionMemory);
                // obtain the number of extents from updated region
                // memory information
                numExtents = (int)regionMemory.keys.size();
                if (extentIdx >= numExtents) {
                    // Close the region
                    close_region(regionId);
                    dataItem->set_desc_status(DESC_INIT_DONE_BUT_KEY_NOT_VALID);
                    return dataItem;
                }
            }
            // Add the offset to region base address to get dataitem base
            // address
            info.baseAddressList[i] = regionMemory.base[extentIdx] + startPos;
            info.dataitemKeys[i] = regionMemory.keys[extentIdx];
        }
        // Set key and base address fields in descriptor if registration is
        // valid else set descriptor status to
        // DESC_INIT_DONE_BUT_KEY_NOT_VALID.
        dataItem->bind_keys(info.dataitemKeys, info.used_memsrv_cnt);
        dataItem->set_base_address_list(info.baseAddressList,
                                        info.used_memsrv_cnt);
        dataItem->set_desc_status(DESC_INIT_DONE);
    }
    return dataItem;
}

void Fam_Allocator_Client::deallocate(Fam_Descriptor *descriptor) {
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t offset = globalDescriptor.offset;
    uint64_t firstMemserverId = descriptor->get_first_memserver_id();
    Fam_Permission_Level permissionLevel = descriptor->get_permissionLevel();
    // Close the descriptor
    if (permissionLevel == REGION) {
        close(descriptor);
    } else {
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
        close(descriptor);
#else
        descriptor->set_desc_status(DESC_INVALID);
#endif
    }
    famCIS->deallocate(regionId, offset, firstMemserverId, uid, gid);
}

// This function close the descriptor and mark it as invalid. This descriptor
// can not be used further. This also reduces the reference count of the region.
void Fam_Allocator_Client::close(Fam_Descriptor *descriptor) {
    // Mark the descriptor as invalid
    descriptor->set_desc_status(DESC_INVALID);
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId & REGIONID_MASK;
    Fam_Permission_Level permissionLevel = descriptor->get_permissionLevel();
    if (permissionLevel == REGION) {
        close_region(regionId);
    } else {
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
        close_region(regionId);
#endif
    }

    // TODO: Descriptor will be destroyed rather than setting invalid.
    // This may require to cache the descriptor of all the data item of the
    // region
    // delete descriptor;
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
    region->set_permissionLevel(info.permissionLevel);
    region->set_allocationPolicy(info.allocationPolicy);
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
    dataItem->set_permissionLevel(info.permissionLevel);
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
    std::ostringstream message;
    Fam_Global_Descriptor globalDescriptor =
        descriptor->get_global_descriptor();
    uint64_t regionId = globalDescriptor.regionId & REGIONID_MASK;
    uint64_t offset = globalDescriptor.offset;
    uint64_t firstMemserverId = descriptor->get_first_memserver_id();
    Fam_Region_Item_Info info = famCIS->check_permission_get_item_info(
        regionId, offset, firstMemserverId, uid, gid);

    // Only for memory server model region keys are used.
    if (isSharedMemory || (info.permissionLevel == DATAITEM)) {
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
        open_region_without_registration(regionId);
#else
        if (isSharedMemory) {
            open_region_without_registration(regionId);
        }
#endif
    } else {
        Fam_Region_Memory_Map famRegionMemoryMap;
        open_region_with_registration(regionId, &famRegionMemoryMap);

        // Obtain key and base address corresponding to data item based on the
        // extent index
        for (int i = 0; i < (int)info.used_memsrv_cnt; i++) {
            int extentIdx;
            uint64_t startPos;
            Fam_Region_Memory regionMemory;
            lookup_region_memory_map(famRegionMemoryMap,
                                     info.memoryServerIds[i], &regionMemory);
            // Decode the obtained data item offset and get index of the region
            // extent to which it belongs and offset from region base address
            decode_offset(info.dataitemOffsets[i], &extentIdx, &startPos);
            int numExtents = (int)regionMemory.keys.size();
            // If the memory information corresponding to extentIdx not present
            // locally get it from memory server and update the local entry
            if (extentIdx >= numExtents) {
                Fam_Region_Memory_Map updatedRegionMemoryMap;
                if (enableResourceRelease) {
                    Fam_Client_Resource *famResource =
                        famResourceManager->find_resource(regionId);
                    // Lock the resource to synchronize update operation with
                    // other threads
                    if (LOCK_REGION_RESOURCE(&famResource->statusAndRefcount)) {
                        try {
                            famCIS->get_region_memory(regionId, uid, gid,
                                                      &updatedRegionMemoryMap);
                        } catch (Fam_Exception &e) {
                            // Unlock the resource so that other threads can
                            // continue
                            UNLOCK_REGION_RESOURCE(
                                &famResource->statusAndRefcount);
                            // Close the region
                            close_region(regionId);
                            throw e;
                        } catch (...) {
                            // Unlock the resource so that other threads can
                            // continue
                            UNLOCK_REGION_RESOURCE(
                                &famResource->statusAndRefcount);
                            // Close the region
                            close_region(regionId);
                            message << "Failed to get the region memory "
                                       "information";
                            THROW_ERRNO_MSG(Fam_Allocator_Exception,
                                            FAM_ERR_RESOURCE,
                                            message.str().c_str());
                        }
                        // update region memory map
                        famResource->memoryMap = updatedRegionMemoryMap;
                        UNLOCK_REGION_RESOURCE(&famResource->statusAndRefcount);
                    }
                } else {
                    famCIS->get_region_memory(regionId, uid, gid,
                                              &updatedRegionMemoryMap);
                }
                // lookup the updated region memory map
                lookup_region_memory_map(updatedRegionMemoryMap,
                                         info.memoryServerIds[i],
                                         &regionMemory);

                // obtain the number of extents from updated region memory
                // information
                numExtents = (int)regionMemory.keys.size();
                if (extentIdx > numExtents) {
                    // Close the region
                    close_region(regionId);
                    message << "Failed to get information of region memory "
                               "registration, requested extent index not found";
                    THROW_ERRNO_MSG(Fam_Allocator_Exception, FAM_ERR_RESOURCE,
                                    message.str().c_str());
                }
            }
            // Add the offset to region base address to get dataitem base
            // address
            info.baseAddressList[i] = regionMemory.base[extentIdx] + startPos;
            info.dataitemKeys[i] = regionMemory.keys[extentIdx];
        }
    }
    descriptor->bind_keys(info.dataitemKeys, info.used_memsrv_cnt);
    descriptor->set_used_memsrv_cnt(info.used_memsrv_cnt);
    descriptor->set_memserver_ids(info.memoryServerIds);
    descriptor->set_desc_status(DESC_INIT_DONE);
    descriptor->set_name(info.name);
    descriptor->set_perm(info.perm);
    descriptor->set_size(info.size);
    descriptor->set_base_address_list(info.baseAddressList,
                                      info.used_memsrv_cnt);
    descriptor->set_interleave_size(info.interleaveSize);
    descriptor->set_permissionLevel(info.permissionLevel);
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
    descriptor->set_permissionLevel(info.permissionLevel);
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
    uint64_t *srcBaseAddrList = src->get_base_address_list();
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
