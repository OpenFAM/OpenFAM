/*
 * fam_cis_direct.cpp
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
#include "cis/fam_cis_direct.h"
#include "common/fam_config_info.h"
#include "common/fam_memserver_profile.h"
#include <thread>

#include <boost/atomic.hpp>

#include <chrono>
#include <future>
#include <iomanip>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define MIN_REGION_SIZE (1UL << 20)
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

Fam_CIS_Direct::Fam_CIS_Direct(char *cisName, bool useAsyncCopy_,
                               bool isSharedMemory)
    : useAsyncCopy(useAsyncCopy_) {

    // Look for options information from config file.
    std::string config_file_path;
    configFileParams config_options;
    ostringstream message;
    // Check for config file in or in path mentioned
    // by OPENFAM_ROOT environment variable or in /opt/OpenFAM.
    try {
        config_file_path =
            find_config_file(strdup("fam_client_interface_config.yaml"));
    }
    catch (Fam_InvalidOption_Exception &e) {
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

    if (useAsyncCopy) {
        asyncQHandler = new Fam_Async_QHandler(1);
    }

    memoryServerCount = 0;
    memServerInfoSize = 0;
    memServerInfoBuffer = NULL;
    memServerInfoV = new std::vector<std::tuple<uint64_t, size_t, void *> >();
    memoryServers = new memoryServerMap();
    metadataServers = new metadataServerMap();
    std::string delimiter1 = ",";
    std::string delimiter2 = ":";
    std::vector<uint64_t> memsrv_id_list;

    if (config_options.empty()) {
        // Raise an exception;
        message << "Fam config options not found.";
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }

    if (isSharedMemory) {
        Fam_Memory_Service *memoryService = new Fam_Memory_Service_Direct(
            cisName, NULL, NULL, NULL, isSharedMemory);
        memoryServers->insert({ 0, memoryService });
        memsrv_id_list.push_back(0);
    } else if (strcmp(config_options["memsrv_interface_type"].c_str(),
                      FAM_OPTIONS_RPC_STR) == 0) {
        Server_Map memoryServerList = parse_server_list(
            config_options["memsrv_list"].c_str(), delimiter1, delimiter2);
        for (auto obj = memoryServerList.begin(); obj != memoryServerList.end();
             ++obj) {
            std::pair<std::string, uint64_t> service = obj->second;
            Fam_Memory_Service *memoryService = new Fam_Memory_Service_Client(
                (service.first).c_str(), service.second);
            memoryServers->insert({ obj->first, memoryService });
            memsrv_id_list.push_back(obj->first);

            size_t addrSize = get_addr_size(obj->first);
            void *addr = calloc(1, addrSize);
            get_addr(addr, obj->first);
            memServerInfoV->push_back(
                std::make_tuple(obj->first, addrSize, addr));
            memServerInfoSize += (sizeof(uint64_t) + sizeof(size_t) + addrSize);
        }
    } else if (strcmp(config_options["memsrv_interface_type"].c_str(),
                      FAM_OPTIONS_DIRECT_STR) == 0) {
        // Start memory service only with name(ipaddr) and let Memory service
        // direct reads libfabric port and provider from memroy server config
        // file.
        Fam_Memory_Service *memoryService =
            new Fam_Memory_Service_Direct(cisName, NULL, NULL, NULL);
        memoryServers->insert({ 0, memoryService });
        memsrv_id_list.push_back(0);

        // Note: Need to perform this only for memory server model.
        size_t addrSize = get_addr_size(0);
        void *addr = calloc(1, addrSize);
        get_addr(addr, 0);
        memServerInfoV->push_back(std::make_tuple(0, addrSize, addr));
        memServerInfoSize += (sizeof(uint64_t) + sizeof(size_t) + addrSize);
    } else {
        // Raise an exception
        message << "Invalid value specified for Fam config "
                   "option:memsrv_interface_type.";
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }

    // Allocate buffer for memServerInfo and populate the info from
    // memServerInfoV
    if (memServerInfoSize) {
        uint64_t offsetPtr = 0;
        memServerInfoBuffer = calloc(1, memServerInfoSize);
        for (size_t i = 0; i < memServerInfoV->size(); i++) {
            uint64_t nodeId = (uint64_t)get<0>((*memServerInfoV)[i]);
            size_t addrSize = (uint64_t)get<1>((*memServerInfoV)[i]);
            void *nodeAddr = (void *)get<2>((*memServerInfoV)[i]);
            memcpy(((char *)memServerInfoBuffer + offsetPtr), &nodeId,
                   sizeof(uint64_t));
            offsetPtr += sizeof(uint64_t);
            memcpy(((char *)memServerInfoBuffer + offsetPtr), &addrSize,
                   sizeof(size_t));
            offsetPtr += sizeof(size_t);
            memcpy(((char *)memServerInfoBuffer + offsetPtr), nodeAddr,
                   addrSize);
            offsetPtr += addrSize;
        }
    }

    // TODO: In current implementation metadata server id is 0.
    // later it will be selected based on some strategy
    if (isSharedMemory) {
        Fam_Metadata_Service *metadataService =
            new Fam_Metadata_Service_Direct();
        metadataServers->insert({ 0, metadataService });
        memoryServerCount = memoryServers->size();
        // TODO: This code needs to be revisited. Currently memoryserverCount
        // will be updated to all metadata servers.
        metadataService->metadata_update_memoryserver((int)memoryServerCount,
                                                      memsrv_id_list);
    } else if (strcmp(config_options["metadata_interface_type"].c_str(),
                      FAM_OPTIONS_RPC_STR) == 0) {
        Server_Map metadataServerList = parse_server_list(
            config_options["metadata_list"].c_str(), delimiter1, delimiter2);
        for (auto obj = metadataServerList.begin();
             obj != metadataServerList.end(); ++obj) {
            std::pair<std::string, uint64_t> service = obj->second;
            Fam_Metadata_Service *metadataService =
                new Fam_Metadata_Service_Client((service.first).c_str(),
                                                service.second);
            metadataServers->insert({ obj->first, metadataService });
            memoryServerCount = memoryServers->size();
            // TODO: This code needs to be revisited. Currently
            // memoryserverCount will be updated to all metadata servers.
            metadataService->metadata_update_memoryserver(
                (int)memoryServerCount, memsrv_id_list);
        }
    } else if (strcmp(config_options["metadata_interface_type"].c_str(),
                      FAM_OPTIONS_DIRECT_STR) == 0) {
        Fam_Metadata_Service *metadataService =
            new Fam_Metadata_Service_Direct();
        metadataServers->insert({ 0, metadataService });
        memoryServerCount = memoryServers->size();
        // TODO: This code needs to be revisited. Currently memoryserverCount
        // will be updated to all metadata servers.
        metadataService->metadata_update_memoryserver((int)memoryServerCount,
                                                      memsrv_id_list);
    } else {
        // Raise an exception
        message << "Invalid value specified for Fam config "
                   "option:metadata_interface_type.";
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }
    // TODO:Currently assuming the max key length is uniform accross the
    // multiple metadata server
    // we read metadataMaxKeyLen from only first metadata server. In future it
    // needs to be revised
    // if max key length is not uniform accross multiple metadata servers
    Fam_Metadata_Service *firstMetaServer = metadataServers->begin()->second;
    metadataMaxKeyLen = firstMetaServer->metadata_maxkeylen();
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

    for (size_t i = 0; i < memServerInfoV->size(); i++) {
        free(get<2>((*memServerInfoV)[i]));
    }

    if (memServerInfoBuffer)
        free(memServerInfoBuffer);

    delete memServerInfoV;
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
Fam_CIS_Direct::get_metadata_service(uint64_t metadataServiceId) {
    ostringstream message;
    auto obj = metadataServers->find(metadataServiceId);
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

void Fam_CIS_Direct::reset_profile() {

    MEMSERVER_PROFILE_INIT(CIS_DIRECT)
    MEMSERVER_PROFILE_START_TIME(CIS_DIRECT)
    uint64_t metadataServiceId = 0;

    for (auto obj = memoryServers->begin(); obj != memoryServers->end();
         ++obj) {
        obj->second->reset_profile();
    }
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    metadataService->reset_profile();
    return;
}

void Fam_CIS_Direct::dump_profile() {
    CIS_DIRECT_PROFILE_DUMP();
    uint64_t metadataServiceId = 0;

    for (auto obj = memoryServers->begin(); obj != memoryServers->end();
         ++obj) {
        obj->second->dump_profile();
    }

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    metadataService->dump_profile();
}

inline int Fam_CIS_Direct::create_region_failure_cleanup(
    std::vector<int> create_region_success_list,
    std::vector<Fam_Memory_Service *> memoryServiceList, uint64_t regionId) {

    std::list<std::shared_future<void> > destroyList;
    int destroy_failed = 0;
    for (int n : create_region_success_list) {
        Fam_Memory_Service *memoryService = memoryServiceList[n];
        std::future<void> destroy_result(std::async(
            std::launch::async, &openfam::Fam_Memory_Service::destroy_region,
            memoryService, regionId));
        destroyList.push_back(destroy_result.share());
    }
    for (auto result : destroyList) {
        // Wait for destroy region in other memory servers to complete.
        try {
            result.get();
        }
        catch (...) {
            destroy_failed++;
        }
    }
    return destroy_failed;
}
Fam_Region_Item_Info
Fam_CIS_Direct::create_region(string name, size_t nbytes, mode_t permission,
                              Fam_Region_Attributes *regionAttributes,
                              uint32_t uid, uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Region_Metadata region;
    ostringstream message;
    int user_policy = 0;
    uint64_t regionId = -1;
    std::list<int> memory_server_list;
    uint64_t *memServerIds;
    int used_memsrv_cnt = 0;
    uint64_t metadataServiceId = 0;
    // List of memory servers where a given region need to be spanned.
    std::vector<Fam_Memory_Service *> memoryServiceList;

    // TODO: For now we are using 0 as id for metadata server.
    // This piece of code may be revisted, if needed.
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);

    // Validate that region can be created and get regionId and memory servers
    // in which region needs to be spanned.
    try {
        metadataService->metadata_validate_and_create_region(
            name, nbytes, &regionId, regionAttributes, &memory_server_list,
            user_policy);
    }
    catch (...) {
        throw;
    }
    // Code for spanning region across multiple memory servers.
    // Asyncronously create regions in multiple memory servers for given region
    // id.
    memServerIds =
        (uint64_t *)malloc(sizeof(uint64_t) * memory_server_list.size());
    for (auto it = memory_server_list.begin(); it != memory_server_list.end();
         ++it) {
        memoryServiceList.push_back(get_memory_service(*it));
        memServerIds[used_memsrv_cnt++] = *it;
    }

    // Invoke each memory service asynchronously.
    std::list<std::shared_future<void> > resultList;
    for (auto memsrv : memoryServiceList) {
        Fam_Memory_Service *memoryService = memsrv;
        size_t size = nbytes / used_memsrv_cnt;
        size_t aligned_size =
            align_to_address(size, 64); // align the size to a 64-bit boundary.
        size = (aligned_size > size ? aligned_size : size);
        if (size < MIN_REGION_SIZE)
            size = MIN_REGION_SIZE;

        std::future<void> result(std::async(
            std::launch::async, &openfam::Fam_Memory_Service::create_region,
            memoryService, regionId, size));
        resultList.push_back(result.share());
    }

    std::vector<int> create_region_success_list;
    std::vector<int> create_region_failed_list;
    // Wait for region creation to complete.
    int id = 0;
    Fam_Exception ex;
    for (auto result : resultList) {
        try {
            result.get();
            create_region_success_list.push_back(id++);
        }
        catch (Fam_Exception &e) {
            create_region_failed_list.push_back(id++);
            ex = e;
        }
        catch (...) {
            create_region_failed_list.push_back(id++);
        }
    }

    if (create_region_failed_list.size() > 0) {
        ostringstream message;
        int ret = create_region_failure_cleanup(create_region_success_list,
                                                memoryServiceList, regionId);
        if (ret == 0) {
            metadataService->metadata_reset_bitmap(regionId);
        }
        if (create_region_failed_list.size() == 1) {
            THROW_ERRNO_MSG(CIS_Exception, (Fam_Error)ex.fam_error(),
                            ex.fam_error_msg());
        } else {
            message << "Multiple memory servers failed to create region";
            THROW_ERRNO_MSG(CIS_Exception, REGION_NOT_CREATED,
                            message.str().c_str());
        }
    }
    // Register the region into metadata service
    region.regionId = regionId;
    strncpy(region.name, name.c_str(), metadataMaxKeyLen);
    region.offset = INVALID_OFFSET;
    region.perm = permission;
    region.uid = uid;
    region.gid = gid;
    region.size = nbytes;
    region.redundancyLevel = regionAttributes->redundancyLevel;
    region.memoryType = regionAttributes->memoryType;
    region.interleaveEnable = regionAttributes->interleaveEnable;
    region.used_memsrv_cnt = used_memsrv_cnt;
    memcpy(region.memServerIds, memServerIds,
           used_memsrv_cnt * sizeof(uint64_t));
    try {
        metadataService->metadata_insert_region(regionId, name, &region);
    }
    catch (...) {
        int ret = create_region_failure_cleanup(create_region_success_list,
                                                memoryServiceList, regionId);
        if (ret == 0) {
            metadataService->metadata_reset_bitmap(regionId);
        }
        throw;
    }
    info.regionId = regionId;
    info.offset = INVALID_OFFSET;
    info.memoryServerId = 0; // TODO: This may or may not be required as regions
                             // span across multiple memory servers.
    free(memServerIds);
    CIS_DIRECT_PROFILE_END_OPS(cis_create_region);
    return info;
}

void Fam_CIS_Direct::destroy_region(uint64_t regionId, uint64_t memoryServerId,
                                    uint32_t uid, uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    std::list<int> memory_server_list;
    std::list<Fam_Memory_Service *> memoryServiceList;
    uint64_t metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    // Check with metadata service if the region exist, and can be destroyed.
    metadataService->metadata_validate_and_destroy_region(regionId, uid, gid,
                                                          &memory_server_list);
    for (auto it = memory_server_list.begin(); it != memory_server_list.end();
         ++it) {
        memoryServiceList.push_back(get_memory_service(*it));
    }
    std::list<std::shared_future<void> > resultList;
    for (auto memsrv : memoryServiceList) {
        Fam_Memory_Service *memoryService = memsrv;
        std::future<void> result(std::async(
            std::launch::async, &openfam::Fam_Memory_Service::destroy_region,
            memoryService, regionId));
        resultList.push_back(result.share());
    }

    // Wait for region destroy to complete.
    try {
        for (auto result : resultList) {
            result.get();
        }
    }
    catch (...) {
        throw;
    }

    metadataService->metadata_reset_bitmap(regionId);
    CIS_DIRECT_PROFILE_END_OPS(cis_destroy_region);
    return;
}

void Fam_CIS_Direct::resize_region(uint64_t regionId, size_t nbytes,
                                   uint64_t memoryServerId, uint32_t uid,
                                   uint32_t gid) {

    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    uint64_t used_memsrv_cnt = 0;
    uint64_t metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    try {
        metadataService->metadata_find_region_and_check_permissions(
            META_REGION_ITEM_WRITE, regionId, uid, gid, region);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Region resize not permitted";
            THROW_ERRNO_MSG(CIS_Exception, REGION_RESIZE_NOT_PERMITTED,
                            message.str().c_str());
        }
        throw;
    }
    used_memsrv_cnt = region.used_memsrv_cnt;
    std::list<std::shared_future<void> > resultList;
    size_t bytes_per_server = nbytes / used_memsrv_cnt;
    size_t aligned_size = align_to_address(
        bytes_per_server, 64); // align the size to a 64-bit boundary.
    bytes_per_server =
        (aligned_size > bytes_per_server ? aligned_size : bytes_per_server);

    for (uint32_t id = 0; id < used_memsrv_cnt; id++) {

        Fam_Memory_Service *memoryService =
            get_memory_service(region.memServerIds[(int)id]);
        std::future<void> result(std::async(
            std::launch::async, &openfam::Fam_Memory_Service::resize_region,
            memoryService, regionId, bytes_per_server));
        resultList.push_back(result.share());
    }

    // Wait for region creation to complete.
    try {
        for (auto result : resultList) {
            result.get();
        }
    }
    catch (...) {
        throw;
    }

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

    uint64_t id = 0;
    uint64_t metadataServiceId = 0;

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    // Check with metadata service if the given data item can be allocated.
    metadataService->metadata_validate_and_allocate_dataitem(name, regionId,
                                                             uid, gid, &id);
    Fam_Memory_Service *memoryService = get_memory_service((uint64_t)id);
    Fam_DataItem_Metadata dataitem;

    bool rwFlag, allocateSuccess = true;
    try {
        info = memoryService->allocate(regionId, nbytes);
    }
    catch (...) {
        std::list<int> memserverList =
            metadataService->get_memory_server_list(regionId);
        allocateSuccess = false;
        for (const auto &item : memserverList) {
            if ((uint64_t)item == id) {
                continue;
            }
            try {
                memoryService = get_memory_service((uint64_t)item);
                info = memoryService->allocate(regionId, nbytes);
            }
            catch (...) {
                continue;
            }
            allocateSuccess = true;
            id = (uint64_t)item;
            break;
        }
    }

    if (!allocateSuccess) {
        message << "Failed to allocate dataitem in any memory server";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_CREATED,
                        message.str().c_str());
    }

    uint64_t dataitemId = get_dataitem_id(info.offset, id);

    dataitem.regionId = regionId;
    strncpy(dataitem.name, name.c_str(), metadataMaxKeyLen);
    dataitem.offset = info.offset;
    dataitem.perm = permission;
    dataitem.gid = gid;
    dataitem.uid = uid;
    dataitem.size = nbytes;
    dataitem.memoryServerId = id;
    if (name == "") {
        metadataService->metadata_insert_dataitem(dataitemId, regionId,
                                                  &dataitem);
    } else {
        metadataService->metadata_insert_dataitem(dataitemId, regionId,
                                                  &dataitem, name);
    }
    if (check_dataitem_permission(dataitem, 1, metadataServiceId, uid, gid)) {
        rwFlag = 1;
    } else if (check_dataitem_permission(dataitem, 0, metadataServiceId, uid,
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
    info.memoryServerId = id;
    info.size = nbytes;
    CIS_DIRECT_PROFILE_END_OPS(cis_allocate);
    return info;
}

void Fam_CIS_Direct::deallocate(uint64_t regionId, uint64_t offset,
                                uint64_t memoryServerId, uint32_t uid,
                                uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    uint64_t metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    // Check with metadata service if data item with the requested name can be
    // deallocated.
    uint64_t dataitemId = get_dataitem_id(offset, memoryServerId);
    metadataService->metadata_validate_and_deallocate_dataitem(
        regionId, dataitemId, uid, gid);
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);

    memoryService->deallocate(regionId, offset);

    CIS_DIRECT_PROFILE_END_OPS(cis_deallocate);

    return;
}

void Fam_CIS_Direct::change_region_permission(uint64_t regionId,
                                              mode_t permission,
                                              uint64_t memoryServerId,
                                              uint32_t uid, uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    uint64_t metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
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
    uint64_t metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    message << "Error While changing dataitem permission : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    Fam_DataItem_Metadata dataitem;
    uint64_t dataitemId = get_dataitem_id(offset, memoryServerId);
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
                                             bool op,
                                             uint64_t metadataServiceId,
                                             uint32_t uid, uint32_t gid) {
    metadata_region_item_op_t opFlag;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
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
                                               bool op,
                                               uint64_t metadataServiceId,
                                               uint32_t uid, uint32_t gid) {

    metadata_region_item_op_t opFlag;

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    if (op)
        opFlag = META_REGION_ITEM_RW;
    else
        opFlag = META_REGION_ITEM_READ;

    return (metadataService->metadata_check_permissions(&dataitem, opFlag, uid,
                                                        gid));
}

Fam_Region_Item_Info Fam_CIS_Direct::lookup_region(string name, uint32_t uid,
                                                   uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    Fam_Region_Metadata region;
    uint64_t metadataServiceId = 0;

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    message << "Error While locating region : ";
    try {
        metadataService->metadata_find_region_and_check_permissions(
            META_REGION_ITEM_READ_ALLOW_OWNER, name, uid, gid, region);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }

    info.regionId = region.regionId;
    info.offset = region.offset;
    info.size = region.size;
    info.perm = region.perm;
    strncpy(info.name, region.name, metadataMaxKeyLen);
    info.maxNameLen = metadataMaxKeyLen;
    info.redundancyLevel = region.redundancyLevel;
    info.memoryType = region.memoryType;
    info.interleaveEnable = region.interleaveEnable;
    CIS_DIRECT_PROFILE_END_OPS(cis_lookup_region);
    return info;
}

Fam_Region_Item_Info Fam_CIS_Direct::lookup(string itemName, string regionName,
                                            uint32_t uid, uint32_t gid) {
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    uint64_t metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    Fam_DataItem_Metadata dataitem;
    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_READ_ALLOW_OWNER, itemName, regionName, uid, gid,
            dataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the dataitem";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }

    info.regionId = dataitem.regionId;
    info.offset = dataitem.offset;
    info.size = dataitem.size;
    info.perm = dataitem.perm;
    strncpy(info.name, dataitem.name, metadataMaxKeyLen);
    info.memoryServerId = dataitem.memoryServerId;
    info.maxNameLen = metadataMaxKeyLen;
    CIS_DIRECT_PROFILE_END_OPS(cis_lookup);
    return info;
}

Fam_Region_Item_Info Fam_CIS_Direct::check_permission_get_region_info(
    uint64_t regionId, uint64_t memoryServerId, uint32_t uid, uint32_t gid) {

    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Region_Metadata region;
    ostringstream message;
    uint64_t metadataServiceId = 0;

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    message << "Error While locating region : ";
    try {
        metadataService->metadata_find_region_and_check_permissions(
            META_REGION_ITEM_READ_ALLOW_OWNER, regionId, uid, gid, region);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }
    info.size = region.size;
    info.perm = region.perm;
    strncpy(info.name, region.name, metadataMaxKeyLen);
    info.maxNameLen = metadataMaxKeyLen;
    CIS_DIRECT_PROFILE_END_OPS(cis_check_permission_get_region_info);
    return info;
}

Fam_Region_Item_Info Fam_CIS_Direct::check_permission_get_item_info(
    uint64_t regionId, uint64_t offset, uint64_t memoryServerId, uint32_t uid,
    uint32_t gid) {

    ostringstream message;
    Fam_Region_Item_Info info;
    CIS_DIRECT_PROFILE_START_OPS()
    uint64_t metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_DataItem_Metadata dataitem;
    message << "Error While locating dataitem : ";
    uint64_t dataitemId = get_dataitem_id(offset, memoryServerId);

    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "could not find the dataitem";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    bool rwFlag;
    if (check_dataitem_permission(dataitem, 1, metadataServiceId, uid, gid)) {
        rwFlag = 1;
    } else if (check_dataitem_permission(dataitem, 0, metadataServiceId, uid,
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
    strncpy(info.name, dataitem.name, metadataMaxKeyLen);
    info.maxNameLen = metadataMaxKeyLen;
    info.key = key;
    info.base = get_local_pointer(regionId, offset, memoryServerId);
    info.memoryServerId = dataitem.memoryServerId;

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
    uint64_t metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    Fam_DataItem_Metadata dataitem;

    uint64_t dataitemId = get_dataitem_id(offset, memoryServerId);
    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_READ_ALLOW_OWNER, dataitemId, regionId, uid, gid,
            dataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }

    info.size = dataitem.size;
    info.perm = dataitem.perm;
    strncpy(info.name, dataitem.name, metadataMaxKeyLen);
    info.maxNameLen = metadataMaxKeyLen;
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
    uint64_t metadataServiceId = 0;

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    Fam_DataItem_Metadata dataitem;
    uint64_t dataitemId = get_dataitem_id(offset, memoryServerId);
    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "could not find the dataitem";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
    if (check_dataitem_permission(dataitem, 1, metadataServiceId, uid, gid) |
        check_dataitem_permission(dataitem, 0, metadataServiceId, uid, gid)) {
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
                           uint64_t srcCopyStart, uint64_t srcKey,
                           uint64_t srcBaseAddr, const char *srcAddr,
                           uint32_t srcAddrLen, uint64_t destRegionId,
                           uint64_t destOffset, uint64_t destCopyStart,
                           uint64_t nbytes, uint64_t srcMemoryServerId,
                           uint64_t destMemoryServerId, uint32_t uid,
                           uint32_t gid) {
    ostringstream message;
    message << "Error While copying from dataitem : ";
    Fam_DataItem_Metadata srcDataitem;
    Fam_DataItem_Metadata destDataitem;
    Fam_Copy_Wait_Object *waitObj = new Fam_Copy_Wait_Object();
    CIS_DIRECT_PROFILE_START_OPS()
    uint64_t metadataServiceId = 0;
    Fam_Memory_Service *memoryService = get_memory_service(destMemoryServerId);

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);

    // Permission check, data item out of bound check, already done on the
    // client side. This looks redundant, can be removed later.
    uint64_t srcDataitemId = get_dataitem_id(srcOffset, srcMemoryServerId);
    uint64_t destDataitemId = get_dataitem_id(destOffset, destMemoryServerId);

    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_READ, srcDataitemId, srcRegionId, uid, gid,
            srcDataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Read operation is not permitted on source dataitem";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }

    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_WRITE, destDataitemId, destRegionId, uid, gid,
            destDataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message
                << "Write operation is not permitted on destination dataitem";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }

    if (!((srcCopyStart + nbytes) < srcDataitem.size)) {
        message << "Source offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }

    if (!((destCopyStart + nbytes) < destDataitem.size)) {
        message << "Destination offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }

    if (useAsyncCopy) {
        Fam_Copy_Tag *tag = new Fam_Copy_Tag();
        tag->copyDone.store(false, boost::memory_order_seq_cst);
        tag->memoryService = memoryService;
        tag->srcRegionId = srcRegionId;
        tag->srcOffset = (srcOffset + srcCopyStart);
        tag->srcBaseAddr = srcBaseAddr;
        tag->destRegionId = destRegionId;
        tag->destOffset = (destOffset + destCopyStart);
        tag->size = nbytes;
        tag->srcKey = srcKey;
        tag->srcAddr = (char *)calloc(1, srcAddrLen);
        memcpy(tag->srcAddr, srcAddr, srcAddrLen);
        tag->srcAddrLen = srcAddrLen;
        tag->srcMemserverId = srcMemoryServerId;
        tag->destMemserverId = destMemoryServerId;
        Fam_Ops_Info opsInfo = { COPY, NULL, NULL, 0, 0, 0, 0, 0, tag };
        asyncQHandler->initiate_operation(opsInfo);
        waitObj->tag = tag;
    } else {
        memoryService->copy(srcRegionId, (srcOffset + srcCopyStart), srcKey,
                            srcCopyStart, srcBaseAddr, srcAddr, srcAddrLen,
                            destRegionId, (destOffset + destCopyStart), nbytes,
                            srcMemoryServerId, destMemoryServerId);
    }
    CIS_DIRECT_PROFILE_END_OPS(cis_copy);
    return (void *)waitObj;
}

void Fam_CIS_Direct::wait_for_copy(void *waitObj) {
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Copy_Wait_Object *obj = (Fam_Copy_Wait_Object *)waitObj;
    asyncQHandler->wait_for_copy((void *)(obj->tag));
    CIS_DIRECT_PROFILE_END_OPS(cis_wait_for_copy);
}

void Fam_CIS_Direct::backup(uint64_t srcRegionId, const char *srcAddr,
                            uint32_t srcAddrLen, uint64_t srcOffset,
                            uint64_t srcKey, uint64_t srcMemoryServerId,
                            string outputFile, uint32_t uid, uint32_t gid,
                            uint64_t size) {
    ostringstream message;
    message << "Error While backing from dataitem : ";
    Fam_DataItem_Metadata srcDataitem;
    CIS_DIRECT_PROFILE_START_OPS()
    uint64_t metadataServiceId = 0;
    Fam_Memory_Service *memoryService = get_memory_service(srcMemoryServerId);

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);

    // Permission check, data item out of bound check, already done on the
    // client side. This looks redundant, can be removed later.
    uint64_t srcDataitemId = get_dataitem_id(srcOffset, srcMemoryServerId);

    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_READ, srcDataitemId, srcRegionId, uid, gid,
            srcDataitem);
    } catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Read operation is not permitted on source dataitem";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }
    memoryService->backup(srcRegionId, srcAddr, srcAddrLen, srcOffset, srcKey,
                          srcMemoryServerId, outputFile, size);
}

void Fam_CIS_Direct::restore(uint64_t destRegionId, const char *destAddr,
                             uint32_t destAddrLen, uint64_t destOffset,
                             uint64_t destKey, uint64_t destMemoryServerId,
                             string inputFile, uint32_t uid, uint32_t gid,
                             uint64_t size) {
    ostringstream message;
    message << "Error While restoring dataitem : ";
    Fam_DataItem_Metadata destDataitem;
    // Fam_Copy_Wait_Object *waitObj = new Fam_Copy_Wait_Object();
    CIS_DIRECT_PROFILE_START_OPS()
    uint64_t metadataServiceId = 0;
    Fam_Memory_Service *memoryService = get_memory_service(destMemoryServerId);

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);

    // Permission check, data item out of bound check, already done on the
    // client side. This looks redundant, can be removed later.
    uint64_t destDataitemId = get_dataitem_id(destOffset, destMemoryServerId);

    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_READ, destDataitemId, destRegionId, uid, gid,
            destDataitem);
    } catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Read operation is not permitted on source dataitem";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }
    memoryService->restore(destRegionId, destAddr, destAddrLen, destOffset,
                           destKey, destMemoryServerId, inputFile, size);
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

uint64_t Fam_CIS_Direct::get_dataitem_id(uint64_t offset,
                                         uint64_t memoryServerId) {
    return ((memoryServerId << 32) + offset / MIN_OBJ_SIZE);
}

size_t Fam_CIS_Direct::get_addr_size(uint64_t memoryServerId) {
    size_t addrSize = 0;
    CIS_DIRECT_PROFILE_START_OPS()
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

size_t Fam_CIS_Direct::get_memserverinfo_size() {
    ostringstream message;
    if (!memoryServerCount) {
        message
            << "Memory service is not initialized, memory server list is empty";
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_MEMSERV_LIST_EMPTY,
                        message.str().c_str());
    }
    return memServerInfoSize;
}

void Fam_CIS_Direct::get_memserverinfo(void *memServerInfo) {
    memcpy(memServerInfo, memServerInfoBuffer, memServerInfoSize);
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
        }
        catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["memsrv_interface_type"] = (char *)strdup("rpc");
        }

        try {
            options["metadata_interface_type"] = (char *)strdup(
                (info->get_key_value("metadata_interface_type")).c_str());
        }
        catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["metadata_interface_type"] = (char *)strdup("rpc");
        }

        try {
            std::vector<std::string> temp = info->get_value_list("memsrv_list");
            ostringstream memsrvList;

            for (auto item : temp)
                memsrvList << item << ",";

            options["memsrv_list"] = (char *)strdup(memsrvList.str().c_str());
        }
        catch (Fam_InvalidOption_Exception e) {
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
        }
        catch (Fam_InvalidOption_Exception e) {
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
    uint64_t metadataServiceId = 0;

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    message << "Error While accessing dataitem : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    uint64_t dataitemId = get_dataitem_id(srcOffset, memoryServerId);
    Fam_DataItem_Metadata dataitem;
    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_READ, dataitemId, regionId, uid, gid, dataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }

    if (!((dstOffset + nbytes) <= dataitem.size)) {
        message << "Source offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }

    memoryService->get_atomic(regionId, srcOffset, dstOffset, nbytes, key,
                              nodeAddr, nodeAddrSize);
    CIS_DIRECT_PROFILE_END_OPS(cis_get_atomic);
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
    uint64_t metadataServiceId = 0;

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    message << "Error While accessing dataitem : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    uint64_t dataitemId = get_dataitem_id(srcOffset, memoryServerId);
    Fam_DataItem_Metadata dataitem;
    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_WRITE, dataitemId, regionId, uid, gid, dataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }

    if (!((dstOffset + nbytes) <= dataitem.size)) {
        message << "Source offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }
    memoryService->put_atomic(regionId, srcOffset, dstOffset, nbytes, key,
                              nodeAddr, nodeAddrSize, data);
    CIS_DIRECT_PROFILE_END_OPS(cis_put_atomic);
    return 0;
}

int Fam_CIS_Direct::scatter_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    const char *nodeAddr, uint32_t nodeAddrSize, uint64_t memoryServerId,
    uint32_t uid, uint32_t gid) {

    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    uint64_t metadataServiceId = 0;

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    message << "Error While accessing dataitem : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    uint64_t dataitemId = get_dataitem_id(offset, memoryServerId);
    Fam_DataItem_Metadata dataitem;
    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_WRITE, dataitemId, regionId, uid, gid, dataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }

    memoryService->scatter_strided_atomic(regionId, offset, nElements,
                                          firstElement, stride, elementSize,
                                          key, nodeAddr, nodeAddrSize);
    CIS_DIRECT_PROFILE_END_OPS(cis_scatter_strided_atomic);
    return 0;
}

int Fam_CIS_Direct::gather_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    const char *nodeAddr, uint32_t nodeAddrSize, uint64_t memoryServerId,
    uint32_t uid, uint32_t gid) {

    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    uint64_t metadataServiceId = 0;

    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);

    message << "Error While accessing dataitem : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    uint64_t dataitemId = get_dataitem_id(offset, memoryServerId);
    Fam_DataItem_Metadata dataitem;
    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_WRITE, dataitemId, regionId, uid, gid, dataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }

    memoryService->gather_strided_atomic(regionId, offset, nElements,
                                         firstElement, stride, elementSize, key,
                                         nodeAddr, nodeAddrSize);
    CIS_DIRECT_PROFILE_END_OPS(cis_gather_strided_atomic);
    return 0;
}

int Fam_CIS_Direct::scatter_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    const char *nodeAddr, uint32_t nodeAddrSize, uint64_t memoryServerId,
    uint32_t uid, uint32_t gid) {

    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    uint64_t metadataServiceId = 0;
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    message << "Error While accessing dataitem : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    uint64_t dataitemId = get_dataitem_id(offset, memoryServerId);
    Fam_DataItem_Metadata dataitem;
    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_WRITE, dataitemId, regionId, uid, gid, dataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }

    memoryService->scatter_indexed_atomic(regionId, offset, nElements,
                                          elementIndex, elementSize, key,
                                          nodeAddr, nodeAddrSize);
    CIS_DIRECT_PROFILE_END_OPS(cis_scatter_indexed_atomic);
    return 0;
}

int Fam_CIS_Direct::gather_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    const char *nodeAddr, uint32_t nodeAddrSize, uint64_t memoryServerId,
    uint32_t uid, uint32_t gid) {

    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    uint64_t metadataServiceId = 0;
    Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    message << "Error While accessing dataitem : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    uint64_t dataitemId = get_dataitem_id(offset, memoryServerId);
    Fam_DataItem_Metadata dataitem;
    try {
        metadataService->metadata_find_dataitem_and_check_permissions(
            META_REGION_ITEM_WRITE, dataitemId, regionId, uid, gid, dataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw;
    }

    memoryService->gather_indexed_atomic(regionId, offset, nElements,
                                         elementIndex, elementSize, key,
                                         nodeAddr, nodeAddrSize);
    CIS_DIRECT_PROFILE_END_OPS(cis_gather_indexed_atomic);
    return 0;
}

inline uint64_t Fam_CIS_Direct::align_to_address(uint64_t size, int multiple) {
    assert(multiple && ((multiple & (multiple - 1)) == 0));
    return (size + multiple - 1) & -multiple;
}

} // namespace openfam
