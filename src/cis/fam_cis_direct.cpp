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
#include "common/fam_config_info.h"
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

Fam_CIS_Direct::Fam_CIS_Direct(char *cisName) {

    // Look for options information from config file.
    std::string config_file_path;
    configFileParams config_options;
    ostringstream message;
    // Check for config file in or in path mentioned
    // by OPENFAM_ROOT environment variable or in /opt/OpenFAM.
    try {
        config_file_path =
            find_config_file(strdup("fam_client_interface_config.yaml"));
    } catch (Fam_InvalidOption_Exception &e) {
        // If the config_file is not present, then ignore the exception.
        // All the default parameters will be obtained from validate_cis_options
        // function.
    }
    // Get the configuration info from the configruation file.
    if (!config_file_path.empty()) {
        config_options = get_config_info(config_file_path);
    }

    if (cisName == NULL) {
        // Use localhost/127.0.0.1 as name;
        cisName = strdup("127.0.0.1");
    }
    memoryServers = new memoryServerMap();
    metadataServers = new metadataServerMap();
    std::string delimiter1 = ",";
    std::string delimiter2 = ":";

    if (config_options.empty()) {
        // Raise an exception;
        message << "Fam config options not found.";
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }

    if (strcmp(config_options["memsrv_interface_type"].c_str(),
               FAM_OPTIONS_RPC_STR) == 0) {
        Server_Map memoryServerList = parse_server_list(
            config_options["memsrv_list"].c_str(), delimiter1, delimiter2);
        for (auto obj = memoryServerList.begin(); obj != memoryServerList.end();
             ++obj) {
            std::pair<std::string, uint64_t> service = obj->second;
            Fam_Memory_Service *memoryService = new Fam_Memory_Service_Client(
                (service.first).c_str(), service.second);
            memoryServers->insert({obj->first, memoryService});
        }
    } else if (strcmp(config_options["memsrv_interface_type"].c_str(),
                      FAM_OPTIONS_DIRECT_STR) == 0) {
        // Start memory service only with name(ipaddr) and let Memory service
        // direct reads libfabric port and provider from memroy server config
        // file.
        Fam_Memory_Service *memoryService =
            new Fam_Memory_Service_Direct(cisName, NULL, NULL);
        memoryServers->insert({0, memoryService});
    } else {
        // Raise an exception
        message << "Invalid value specified for Fam config "
                   "option:memsrv_interface_type.";
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }

//TODO: In current implementation metadata server id is same as memory server id
//later it will be seleted based on some strategy
    if (strcmp(config_options["metadata_interface_type"].c_str(),
               FAM_OPTIONS_RPC_STR) == 0) {
        Server_Map metadataServerList = parse_server_list(
            config_options["metadata_list"].c_str(), delimiter1, delimiter2);
        for (auto obj = metadataServerList.begin();
             obj != metadataServerList.end(); ++obj) {
            std::pair<std::string, uint64_t> service = obj->second;
            Fam_Metadata_Service *metadataService =
                new Fam_Metadata_Service_Client((service.first).c_str(),
                                                service.second);
            metadataServers->insert({obj->first, metadataService});
        }
    } else if (strcmp(config_options["metadata_interface_type"].c_str(),
                      FAM_OPTIONS_DIRECT_STR) == 0) {
        Fam_Metadata_Service *metadataService =
            new Fam_Metadata_Service_Direct();
        metadataServers->insert({0, metadataService});
    } else {
        // Raise an exception
        message << "Invalid value specified for Fam config "
                   "option:metadata_interface_type.";
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }
    memoryServerCount = memoryServers->size();
}

Fam_CIS_Direct::~Fam_CIS_Direct() {
    for (auto obj = memoryServers->begin(); obj != memoryServers->end();
         ++obj) {
        delete obj->second;
    }

    for (auto obj = metadataServers->begin(); obj != metadataServers->end();
         ++obj) {
        delete obj->second;
    }
    delete memoryServers;
    delete metadataServers;
}

Fam_Memory_Service *
Fam_CIS_Direct::get_memory_service(uint64_t memoryServerId) {
    ostringstream message;
    auto obj = memoryServers->find(memoryServerId);
    if (obj == memoryServers->end()) {
        message << "Memory service RPC client not found";
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_RPC_CLIENT_NOTFOUND,
                        message.str().c_str());
    }
    return obj->second;
}

Fam_Metadata_Service *
Fam_CIS_Direct::get_metadata_service(uint64_t metadataServerId) {
    ostringstream message;
    auto obj = metadataServers->find(metadataServerId);
    if (obj == metadataServers->end()) {
        message << "Metadata service RPC client not found";
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_RPC_CLIENT_NOTFOUND,
                        message.str().c_str());
    }
    return obj->second;
}

uint64_t Fam_CIS_Direct::get_num_memory_servers() {
    ostringstream message;
    if (!memoryServerCount) {
        message
            << "Memory service is not initialized, memory server list is empty";
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_MEMSERV_LIST_EMPTY,
                        message.str().c_str());
    }
    return memoryServerCount;
}

void Fam_CIS_Direct::reset_profile(uint64_t memoryServerId) {

    MEMSERVER_PROFILE_INIT(CIS_DIRECT)
    MEMSERVER_PROFILE_START_TIME(CIS_DIRECT)
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
    memoryService->reset_profile();
    metadataService->reset_profile();
    return;
}

void Fam_CIS_Direct::dump_profile(uint64_t memoryServerId) {
    CIS_DIRECT_PROFILE_DUMP();
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
    memoryService->dump_profile();
    metadataService->dump_profile();
}

Fam_Region_Item_Info
Fam_CIS_Direct::create_region(string name, size_t nbytes, mode_t permission,
                              Fam_Redundancy_Level redundancyLevel,
                              uint32_t uid, uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Region_Metadata region;
    ostringstream message;

    uint64_t memoryServerId = generate_memory_server_id(name.c_str());

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
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

    uint64_t regionId = memoryService->create_region(nbytes);

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
    info.memoryServerId = memoryServerId;
    CIS_DIRECT_PROFILE_END_OPS(cis_create_region);
    return info;
}

void Fam_CIS_Direct::destroy_region(uint64_t regionId, uint64_t memoryServerId,
                                    uint32_t uid, uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
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

    memoryService->destroy_region(regionId);

    CIS_DIRECT_PROFILE_END_OPS(cis_destroy_region);
    return;
}

void Fam_CIS_Direct::resize_region(uint64_t regionId, size_t nbytes,
                                   uint64_t memoryServerId, uint32_t uid,
                                   uint32_t gid) {

    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
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

    memoryService->resize_region(regionId, nbytes);

    region.size = nbytes;
    // Update the size in the metadata service
    metadataService->metadata_modify_region(regionId, &region);

    CIS_DIRECT_PROFILE_END_OPS(cis_resize_region);

    return;
}

Fam_Region_Item_Info Fam_CIS_Direct::allocate(string name, size_t nbytes,
                                              mode_t permission,
                                              uint64_t regionId,
                                              uint64_t memoryServerId,
                                              uint32_t uid, uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
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
    if (name != "") {
        if (metadataService->metadata_find_dataitem(name, regionId, dataitem)) {
            message << "Dataitem with the name provided already exist";
            THROW_ERRNO_MSG(CIS_Exception, DATAITEM_EXIST,
                            message.str().c_str());
        }
    }

    bool rwFlag;
    info = memoryService->allocate(regionId, nbytes);

    uint64_t dataitemId = info.offset / MIN_OBJ_SIZE;

    dataitem.regionId = regionId;
    strncpy(dataitem.name, name.c_str(), metadataService->metadata_maxkeylen());
    dataitem.offset = info.offset;
    dataitem.perm = permission;
    dataitem.gid = gid;
    dataitem.uid = uid;
    dataitem.size = nbytes;
    if (name == "") {
        metadataService->metadata_insert_dataitem(dataitemId, regionId,
                                                  &dataitem);
    } else {
        metadataService->metadata_insert_dataitem(dataitemId, regionId,
                                                  &dataitem, name);
    }
    if (check_dataitem_permission(dataitem, 1, memoryServerId, uid, gid)) {
        rwFlag = 1;
    } else if (check_dataitem_permission(dataitem, 0, memoryServerId, uid,
                                         gid)) {
        rwFlag = 0;
    } else {
        message << "Not permitted to use this dataitem";
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_NOPERM, message.str().c_str());
    }
    uint64_t key =
        memoryService->get_key(regionId, info.offset, nbytes, rwFlag);
    info.key = key;
    info.regionId = regionId;
    info.size = nbytes;
    CIS_DIRECT_PROFILE_END_OPS(cis_allocate);
    return info;
}

void Fam_CIS_Direct::deallocate(uint64_t regionId, uint64_t offset,
                                uint64_t memoryServerId, uint32_t uid,
                                uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
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

    memoryService->deallocate(regionId, offset);

    CIS_DIRECT_PROFILE_END_OPS(cis_deallocate);

    return;
}

void Fam_CIS_Direct::change_region_permission(uint64_t regionId,
                                              mode_t permission,
                                              uint64_t memoryServerId,
                                              uint32_t uid, uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
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
                                                uint64_t memoryServerId,
                                                uint32_t uid, uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
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
                                             bool op, uint64_t memoryServerId,
                                             uint32_t uid, uint32_t gid) {
    metadata_region_item_op_t opFlag;

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
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
                                               bool op, uint64_t memoryServerId,
                                               uint32_t uid, uint32_t gid) {

    metadata_region_item_op_t opFlag;

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
    if (op)
        opFlag = META_REGION_ITEM_RW;
    else
        opFlag = META_REGION_ITEM_READ;

    return (metadataService->metadata_check_permissions(&dataitem, opFlag, uid,
                                                        gid));
}

Fam_Region_Item_Info Fam_CIS_Direct::lookup_region(string name,
                                                   uint32_t uid, uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    Fam_Region_Metadata region;

    uint64_t memoryServerId = generate_memory_server_id(name.c_str());

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
    message << "Error While locating region : ";
    if (!metadataService->metadata_find_region(name, region)) {
        message << "could not find the region";
        THROW_ERRNO_MSG(CIS_Exception, REGION_NOT_FOUND, message.str().c_str());
    }

    if (uid != region.uid) {
        if (!check_region_permission(region, 0, memoryServerId, uid, gid)) {
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
    info.memoryServerId = memoryServerId;
    CIS_DIRECT_PROFILE_END_OPS(cis_lookup_region);
    return info;
}

Fam_Region_Item_Info Fam_CIS_Direct::lookup(string itemName, string regionName,
                                            uint32_t uid, uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    uint64_t memoryServerId = generate_memory_server_id(regionName.c_str());

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
    Fam_DataItem_Metadata dataitem;
    message << "Error While locating dataitem : ";
    if (!metadataService->metadata_find_dataitem(itemName, regionName,
                                                 dataitem)) {
        message << "could not find the dataitem ";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    if (uid != dataitem.uid) {
        if (!check_dataitem_permission(dataitem, 0, memoryServerId, uid, gid)) {
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
    info.memoryServerId = memoryServerId;
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

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
    message << "Error While locating region : ";
    if (!metadataService->metadata_find_region(regionId, region)) {
        message << "could not find the region";
        THROW_ERRNO_MSG(CIS_Exception, REGION_NOT_FOUND, message.str().c_str());
    }

    if (uid != region.uid) {
        if (!check_region_permission(region, 0, memoryServerId, uid, gid)) {
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

    ostringstream message;
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_DataItem_Metadata dataitem;
    message << "Error While locating dataitem : ";
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "could not find the dataitem";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    bool rwFlag;
    if (check_dataitem_permission(dataitem, 1, memoryServerId, uid, gid)) {
        rwFlag = 1;
    } else if (check_dataitem_permission(dataitem, 0, memoryServerId, uid,
                                         gid)) {
        rwFlag = 0;
    } else {
        message << "Not permitted to use this dataitem";
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_NOPERM, message.str().c_str());
    }

    uint64_t key =
        memoryService->get_key(regionId, offset, dataitem.size, rwFlag);

    info.regionId = dataitem.regionId;
    info.offset = dataitem.offset;
    info.size = dataitem.size;
    info.perm = dataitem.perm;
    strncpy(info.name, dataitem.name, metadataService->metadata_maxkeylen());
    info.maxNameLen = metadataService->metadata_maxkeylen();
    info.key = key;
    info.base = get_local_pointer(regionId, offset, memoryServerId);

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

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
    Fam_DataItem_Metadata dataitem;
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "could not find the dataitem";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    if (uid != dataitem.uid) {
        if (!check_dataitem_permission(dataitem, 0, memoryServerId, uid, gid)) {
            message << "Not permitted to access the dataitem";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
    }

    info.size = dataitem.size;
    info.perm = dataitem.perm;
    strncpy(info.name, dataitem.name, metadataService->metadata_maxkeylen());
    info.maxNameLen = metadataService->metadata_maxkeylen();
    CIS_DIRECT_PROFILE_END_OPS(cis_get_stat_info);
    return info;
}

void *Fam_CIS_Direct::get_local_pointer(uint64_t regionId, uint64_t offset,
                                        uint64_t memoryServerId) {
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    return memoryService->get_local_pointer(regionId, offset);
}

void *Fam_CIS_Direct::fam_map(uint64_t regionId, uint64_t offset,
                              uint64_t memoryServerId, uint32_t uid,
                              uint32_t gid) {
    void *localPointer;
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
    Fam_DataItem_Metadata dataitem;
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "could not find the dataitem";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
    if (check_dataitem_permission(dataitem, 1, memoryServerId, uid, gid) |
        check_dataitem_permission(dataitem, 0, memoryServerId, uid, gid)) {
        localPointer = get_local_pointer(regionId, offset, memoryServerId);
    } else {
        THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                        "Not permitted to use this dataitem");
    }
    CIS_DIRECT_PROFILE_END_OPS(cis_fam_map);

    return localPointer;
}
void Fam_CIS_Direct::fam_unmap(void *local, uint64_t regionId, uint64_t offset,
                               uint64_t memoryServerId, uint32_t uid,
                               uint32_t gid) {
    return;
}

void *Fam_CIS_Direct::copy(uint64_t srcRegionId, uint64_t srcOffset,
                           uint64_t srcCopyStart, uint64_t destRegionId,
                           uint64_t destOffset, uint64_t destCopyStart,
                           uint64_t nbytes, uint64_t memoryServerId,
                           uint32_t uid, uint32_t gid) {
    ostringstream message;
    message << "Error While copying from dataitem : ";
    Fam_DataItem_Metadata srcDataitem;
    Fam_DataItem_Metadata destDataitem;
    CIS_DIRECT_PROFILE_START_OPS()

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);

    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);

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

    if (!((srcCopyStart + nbytes) < srcDataitem.size)) {
        message << "Source offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }

    if (!((destCopyStart + nbytes) < destDataitem.size)) {
        message << "Destination offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }

    memoryService->copy(srcRegionId, (srcOffset + srcCopyStart), destRegionId,
                        (destOffset + destCopyStart), nbytes);

    CIS_DIRECT_PROFILE_END_OPS(cis_copy);

    Fam_Copy_Tag *tag = new Fam_Copy_Tag();

    return (void *)tag;
}

void Fam_CIS_Direct::acquire_CAS_lock(uint64_t offset,
                                      uint64_t memoryServerId) {
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    memoryService->acquire_CAS_lock(offset);
    CIS_DIRECT_PROFILE_END_OPS(cis_acquire_CAS_lock);
}

void Fam_CIS_Direct::release_CAS_lock(uint64_t offset,
                                      uint64_t memoryServerId) {
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    memoryService->release_CAS_lock(offset);
    CIS_DIRECT_PROFILE_END_OPS(cis_release_CAS_lock);
}

size_t Fam_CIS_Direct::get_addr_size(uint64_t memoryServerId) {
    CIS_DIRECT_PROFILE_START_OPS()
    size_t addrSize = 0;
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    addrSize = memoryService->get_addr_size();
    CIS_DIRECT_PROFILE_END_OPS(cis_get_addr_size);
    return addrSize;
}

void Fam_CIS_Direct::get_addr(void *memServerFabricAddr,
                              uint64_t memoryServerId) {
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    memcpy(memServerFabricAddr, (void *)memoryService->get_addr(),
           memoryService->get_addr_size());
    CIS_DIRECT_PROFILE_END_OPS(cis_get_addr);
}

/*
 * get_config_info - Obtain the required information from
 * fam_pe_config file. On Success, returns a map that has options updated from
 * from configuration file. Set default values if not found in config file.
 */
configFileParams Fam_CIS_Direct::get_config_info(std::string filename) {
    configFileParams options;
    config_info *info = NULL;
    if (filename.find("fam_client_interface_config.yaml") !=
        std::string::npos) {
        info = new yaml_config_info(filename);
        try {
            options["memsrv_interface_type"] = (char *)strdup(
                (info->get_key_value("memsrv_interface_type")).c_str());
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["memsrv_interface_type"] = (char *)strdup("rpc");
        }

        try {
            options["metadata_interface_type"] = (char *)strdup(
                (info->get_key_value("metadata_interface_type")).c_str());
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["metadata_interface_type"] = (char *)strdup("rpc");
        }

        try {
            std::vector<std::string> temp = info->get_value_list("memsrv_list");
            ostringstream memsrvList;

            for (auto item : temp)
                memsrvList << item << ",";

            options["memsrv_list"] = (char *)strdup(memsrvList.str().c_str());
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["memsrv_list"] = (char *)strdup("0:127.0.0.1:8787");
        }

        try {
            std::vector<std::string> temp =
                info->get_value_list("metadata_list");
            ostringstream metasrvList;

            for (auto item : temp)
                metasrvList << item << ",";

            options["metadata_list"] =
                (char *)strdup(metasrvList.str().c_str());
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["metadata_list"] = (char *)strdup("0:127.0.0.1:8787");
        }
    }
    return options;
}
int Fam_CIS_Direct::get_atomic(uint64_t regionId, uint64_t srcOffset,
                               uint64_t dstOffset, uint64_t nbytes,
                               uint64_t key, const char *nodeAddr,
                               uint32_t nodeAddrSize, uint64_t memoryServerId,
                               uint32_t uid, uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
    message << "Error While changing dataitem permission : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    uint64_t dataitemId = srcOffset / MIN_OBJ_SIZE;
    Fam_DataItem_Metadata dataitem;
    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
    if (!(metadataService->metadata_check_permissions(
            &dataitem, META_REGION_ITEM_READ, uid, gid))) {
        message << "Write operation is not permitted on destination dataitem";
        THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION, message.str().c_str());
    }

    if (!((dstOffset + nbytes) <= dataitem.size)) {
        message << "Source offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }

    memoryService->get_atomic(regionId, srcOffset, dstOffset, nbytes, key,
                              nodeAddr, nodeAddrSize);
    //    CIS_DIRECT_PROFILE_END_OPS(cis_get_atomic);
    return 0;
}

int Fam_CIS_Direct::put_atomic(uint64_t regionId, uint64_t srcOffset,
                               uint64_t dstOffset, uint64_t nbytes,
                               uint64_t key, const char *nodeAddr,
                               uint32_t nodeAddrSize, const char *data,
                               uint64_t memoryServerId, uint32_t uid,
                               uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_Metadata_Service *metadataService =
        get_metadata_service(memoryServerId);
    message << "Error While changing dataitem permission : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    uint64_t dataitemId = srcOffset / MIN_OBJ_SIZE;
    Fam_DataItem_Metadata dataitem;
    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
    if (!(metadataService->metadata_check_permissions(
            &dataitem, META_REGION_ITEM_WRITE, uid, gid))) {
        message << "Write operation is not permitted on destination dataitem";
        THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION, message.str().c_str());
    }

    if (!((dstOffset + nbytes) <= dataitem.size)) {
        message << "Source offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }
    memoryService->put_atomic(regionId, srcOffset, dstOffset, nbytes, key,
                              nodeAddr, nodeAddrSize, data);
    //    CIS_DIRECT_PROFILE_END_OPS(cis_get_atomic);
    return 0;
}

} // namespace openfam
