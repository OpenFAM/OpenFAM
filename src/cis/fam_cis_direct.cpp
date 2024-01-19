/*
 * fam_cis_direct.cpp
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
#include "cis/fam_cis_direct.h"
#include "common/fam_config_info.h"
#include "common/fam_internal.h"
#include "common/fam_memserver_profile.h"
#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <chrono>
#include <cis/fam_cis_client.h>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory_service/fam_memory_service_client.h>
#include <metadata_service/fam_metadata_service_client.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>
#include <unistd.h>

#ifdef USE_THALLIUM
#include <common/fam_thallium_engine_helper.h>
#include <memory_service/fam_memory_service_thallium_client.h>
#include <metadata_service/fam_metadata_service_thallium_client.h>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#endif

#define MIN_REGION_SIZE (1UL << 20)
#define MIN_OBJ_SIZE 128

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

    // set rpc_framework and protocol options
    set_rpc_framework_protocol(config_options);

    if (cisName == NULL) {
        // Use localhost/127.0.0.1 as name;
        cisName = strdup("127.0.0.1");
    }

    if (useAsyncCopy) {
        asyncQHandler = new Fam_Async_QHandler(1);
    }

    this->isSharedMemory = isSharedMemory;

    memoryServerCount = 0;
    memServerInfoSize = 0;
    memServerInfoBuffer = NULL;
    memServerInfoV = new std::vector<std::tuple<uint64_t, size_t, void *> >();
    memoryServers = new memoryServerMap();
    metadataServers = new metadataServerMap();
    std::string delimiter1 = ",";
    std::string delimiter2 = ":";
    std::vector<uint64_t> memsrv_persistent_id_list;
    std::vector<uint64_t> memsrv_volatile_id_list;

    uint64_t memoryServerPersistentCount = 0;
    uint64_t memoryServerVolatileCount = 0;
    if (config_options.empty()) {
        // Raise an exception;
        message << "Fam config options not found.";
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }

    if (isSharedMemory) {
        Fam_Memory_Service *memoryService =
            new Fam_Memory_Service_Direct(0, isSharedMemory);
        memoryServers->insert({ 0, memoryService });
        Fam_Memory_Type memory_type = memoryService->get_memtype();
        if (memory_type == PERSISTENT) {
            memsrv_persistent_id_list.push_back(0);
            memoryServerPersistentCount++;
        } else {
            memsrv_volatile_id_list.push_back(0);
            memoryServerVolatileCount++;
        }
    } else if (strcmp(config_options["memsrv_interface_type"].c_str(),
                      FAM_OPTIONS_RPC_STR) == 0) {
        Server_Map memoryServerList = parse_server_list(
            config_options["memsrv_list"].c_str(), delimiter1, delimiter2);
        for (auto obj = memoryServerList.begin(); obj != memoryServerList.end();
             ++obj) {
            std::pair<std::string, uint64_t> service = obj->second;
            Fam_Memory_Service *memoryService;
            if (strcmp(config_options["rpc_framework_type"].c_str(),
                       FAM_OPTIONS_GRPC_STR) == 0) {
                memoryService = new Fam_Memory_Service_Client(
                    (service.first).c_str(), service.second);
            }
#ifdef USE_THALLIUM
            else if (strcmp(config_options["rpc_framework_type"].c_str(),
                            FAM_OPTIONS_THALLIUM_STR) == 0) {
                // initialize thallium engine
                string provider_protocol =
                    protocol_map(config_options["provider"]);
                Thallium_Engine *thal_engine_gen =
                    Thallium_Engine::get_instance(provider_protocol);
                tl::engine engine = thal_engine_gen->get_engine();
                memoryService = new Fam_Memory_Service_Thallium_Client(
                    engine, (service.first).c_str(), service.second);
            }
#endif
            else {
                // Raise an exception
                message << "Invalid value specified for Fam config "
                           "option:rpc_framework_type.";
                THROW_ERR_MSG(Fam_InvalidOption_Exception,
                              message.str().c_str());
            }
            memoryServers->insert({obj->first, memoryService});

            size_t addrSize = get_addr_size(obj->first);
            Fam_Memory_Type memory_type = memoryService->get_memtype();
            if (memory_type == PERSISTENT) {
                memsrv_persistent_id_list.push_back(obj->first);
                memoryServerPersistentCount++;
            } else {
                memsrv_volatile_id_list.push_back(obj->first);
                memoryServerVolatileCount++;
            }

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
        Fam_Memory_Service *memoryService = new Fam_Memory_Service_Direct(0);
        memoryServers->insert({ 0, memoryService });
        Fam_Memory_Type memory_type = memoryService->get_memtype();
        if (memory_type == PERSISTENT) {
            memsrv_persistent_id_list.push_back(0);
            memoryServerPersistentCount++;
        } else {
            memsrv_volatile_id_list.push_back(0);
            memoryServerVolatileCount++;
        }

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

    for (auto obj = memoryServers->begin(); obj != memoryServers->end();
         ++obj) {
        Fam_Memory_Service *memoryService = obj->second;
        memoryService->update_memserver_addrlist(
            memServerInfoBuffer, memServerInfoSize, memoryServers->size());
    }

    // TODO: In current implementation metadata server id is 0.
    // later it will be selected based on some strategy
    if (isSharedMemory) {
        Fam_Metadata_Service *metadataService =
            new Fam_Metadata_Service_Direct(true);
        metadataServers->insert({ 0, metadataService });
        memoryServerCount = memoryServers->size();
        // TODO: This code needs to be revisited. Currently memoryserverCount
        // will be updated to all metadata servers.
        metadataService->metadata_update_memoryserver(
            (int)memoryServerPersistentCount, memsrv_persistent_id_list,
            (int)memoryServerVolatileCount, memsrv_volatile_id_list);
    } else if (strcmp(config_options["metadata_interface_type"].c_str(),
                      FAM_OPTIONS_RPC_STR) == 0) {
        Server_Map metadataServerList = parse_server_list(
            config_options["metadata_list"].c_str(), delimiter1, delimiter2);
        for (auto obj = metadataServerList.begin();
             obj != metadataServerList.end(); ++obj) {
            std::pair<std::string, uint64_t> service = obj->second;
            Fam_Metadata_Service *metadataService;
            if (strcmp(config_options["rpc_framework_type"].c_str(),
                       FAM_OPTIONS_GRPC_STR) == 0) {
                metadataService = new Fam_Metadata_Service_Client(
                    (service.first).c_str(), service.second);
            }
#ifdef USE_THALLIUM
            else if (strcmp(config_options["rpc_framework_type"].c_str(),
                            FAM_OPTIONS_THALLIUM_STR) == 0) {
                // get thallium engine singleton instance and object
                string provider_protocol =
                    protocol_map(config_options["provider"]);
                Thallium_Engine *thal_engine_gen =
                    Thallium_Engine::get_instance(provider_protocol);
                tl::engine engine = thal_engine_gen->get_engine();
                metadataService = new Fam_Metadata_Service_Thallium_Client(
                    engine, (service.first).c_str(), service.second);
            }
#endif
            else {
                // Raise an exception
                message << "Invalid value specified for Fam config "
                           "option:rpc_interface_type.";
                THROW_ERR_MSG(Fam_InvalidOption_Exception,
                              message.str().c_str());
            }
            metadataServers->insert({obj->first, metadataService});
            memoryServerCount = memoryServers->size();
            metadataService->metadata_update_memoryserver(
                (int)memoryServerPersistentCount, memsrv_persistent_id_list,
                (int)memoryServerVolatileCount, memsrv_volatile_id_list);
            // TODO: This code needs to be revisited. Currently
            // memoryserverCount will be updated to all metadata servers.
        }
    } else if (strcmp(config_options["metadata_interface_type"].c_str(),
                      FAM_OPTIONS_DIRECT_STR) == 0) {
        Fam_Metadata_Service *metadataService;
        if (strcmp(config_options["memsrv_interface_type"].c_str(),
                   FAM_OPTIONS_DIRECT_STR) == 0) {
            metadataService = new Fam_Metadata_Service_Direct(true);
        } else {
            metadataService = new Fam_Metadata_Service_Direct(false);
        }
        metadataServers->insert({ 0, metadataService });
        memoryServerCount = memoryServers->size();
        // TODO: This code needs to be revisited. Currently memoryserverCount
        // will be updated to all metadata servers.
        metadataService->metadata_update_memoryserver(
            (int)memoryServerPersistentCount, memsrv_persistent_id_list,
            (int)memoryServerVolatileCount, memsrv_volatile_id_list);
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

    std::list<std::shared_future<void>> cleanupList;
    int cleanupFailed = 0;
    // Request all memory servers to cleanup asynchronously
    for (int n : create_region_success_list) {
        Fam_Memory_Service *memoryService = memoryServiceList[n];
        std::future<void> cleanupResult(std::async(
            std::launch::async,
            &openfam::Fam_Memory_Service::create_region_failure_cleanup,
            memoryService, regionId));
        cleanupList.push_back(cleanupResult.share());
    }
    // Wait for all memory server to complete asynchronous call
    for (auto result : cleanupList) {
        try {
            result.get();
        }
        catch (...) {
            cleanupFailed++;
        }
    }
    return cleanupFailed;
}

inline int Fam_CIS_Direct::allocate_failure_cleanup(
    std::vector<int> allocate_success_list,
    std::vector<Fam_Memory_Service *> memoryServiceList, uint64_t regionId,
    uint64_t *offsets) {

    std::list<std::shared_future<void>> deallocateList;
    int deallocate_failed = 0;
    int idx = 0;
    for (int n : allocate_success_list) {
        Fam_Memory_Service *memoryService = memoryServiceList[n];
        std::future<void> deallocate_result(std::async(
            std::launch::async, &openfam::Fam_Memory_Service::deallocate,
            memoryService, regionId, offsets[idx++]));
        deallocateList.push_back(deallocate_result.share());
    }
    for (auto result : deallocateList) {
        // Wait for deallocate in other memory servers to complete.
        try {
            result.get();
        } catch (...) {
            deallocate_failed++;
        }
    }
    return deallocate_failed;
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
    } catch (Fam_Exception &e) {
        throw e;
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
    std::string create_ErrMsg = "Unknown error";
    enum Fam_Error create_famErr = FAM_ERR_UNKNOWN;

    for (auto result : resultList) {
        try {
            result.get();
            create_region_success_list.push_back(id++);
        }
        catch (Fam_Exception &e) {
            create_region_failed_list.push_back(id++);
            create_ErrMsg = e.fam_error_msg();
            create_famErr = (Fam_Error)e.fam_error();
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
            THROW_ERRNO_MSG(CIS_Exception, create_famErr, create_ErrMsg);
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
    region.permissionLevel = regionAttributes->permissionLevel;
    region.used_memsrv_cnt = used_memsrv_cnt;
    memcpy(region.memServerIds, memServerIds,
           used_memsrv_cnt * sizeof(uint64_t));
    try {
        metadataService->metadata_insert_region(regionId, name, &region);
    } catch (Fam_Exception &e) {
        int ret = create_region_failure_cleanup(create_region_success_list,
                                                memoryServiceList, regionId);
        if (ret == 0) {
            metadataService->metadata_reset_bitmap(regionId);
        }
        throw e;
    }

    std::unordered_map<int, Fam_Exception> failureList;
    std::vector<int> successList;

    // Region memory intialization of region memory, registers the region memory
    // with libfabric in memory server. This operation is performed only if
    // region is set to REGION level permission in memory server model.
    if (!isSharedMemory && regionAttributes->permissionLevel == REGION) {
        register_region_memory(region, metadataService, memoryServiceList, uid,
                               gid);
    }
    info.regionId = regionId;
    info.offset = INVALID_OFFSET;

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
    std::list<std::shared_future<void>> resultList;
    uint64_t *resourceStatus = new uint64_t[memoryServiceList.size()];
    int idx = 0;
    for (auto memsrv : memoryServiceList) {
        Fam_Memory_Service *memoryService = memsrv;
        std::future<void> result(std::async(
            std::launch::async, &openfam::Fam_Memory_Service::destroy_region,
            memoryService, regionId, &resourceStatus[idx++]));
        resultList.push_back(result.share());
    }

    uint64_t statusAccumulate = (uint64_t)-1;
    int failCount = 0;
    idx = 0;
    Fam_Exception exception(FAM_ERR_UNKNOWN,
                            "Unknown error occured while closing the region");

    // Wait for all memory servers to complete asynchronous call
    for (auto result : resultList) {
        try {
            result.get();
            // Accumulate releaseRegionId flag from all memory server into index
            // 0 by AND operation
            statusAccumulate &= resourceStatus[idx];
        } catch (Fam_Exception &e) {
            failCount++;
            exception = e;
        } catch (...) {
            failCount++;
        }
        idx++;
    }

    switch (failCount) {
    // If all memory server successfully close the region
    case 0:
        // If the accumulated status from all the memory server is RELEASED,
        // release the region ID to free pool
        if (statusAccumulate == RELEASED) {
            metadataService->metadata_reset_bitmap(regionId);
        }
        return;
    // If only one memory server fails to close the region
    case 1:
        throw exception;
    // If multiple memory servers fail to close the region
    default:
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_RESOURCE,
                        "Multiple memory servers failed to destroy the region");
    }
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
        throw e;
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
    } catch (Fam_Exception &e) {
        throw e;
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

    uint64_t metadataServiceId = 0;

    uint64_t *memServerIds;
    int used_memsrv_cnt = 0;
    int user_policy = 0;
    size_t interleaveSize;
    Fam_Permission_Level permissionLevel;
    mode_t regionPermission;
    std::list<int> memory_server_list;
    std::vector<Fam_Memory_Service *> memoryServiceList;

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    // Check with metadata service if the given data item can be allocated.
    metadataService->metadata_validate_and_allocate_dataitem(
        name, regionId, uid, gid, nbytes, &memory_server_list, &interleaveSize,
        &permissionLevel, &regionPermission, user_policy);

// TODO:The following piece of code will be enabled in future.
#ifdef ENABLE_PERMISSION_COMPARE
    if (permissionLevel == REGION) {
        if (regionPermission != permission) {
            message << "Dataitem permission must be same as region permission, "
                       "if REGION level permission is set for a region";
            THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_NOPERM,
                            message.str().c_str());
        }
    }
#endif
    memServerIds =
        (uint64_t *)malloc(sizeof(uint64_t) * memory_server_list.size());
    for (auto it = memory_server_list.begin(); it != memory_server_list.end();
         ++it) {
        memoryServiceList.push_back(get_memory_service(*it));
        memServerIds[used_memsrv_cnt] = *it;
        used_memsrv_cnt++;
    }

    Fam_DataItem_Metadata dataitem;

    size_t blocks = 0, numBlocksPerServer = 0, extraBlocks = 0;
    if (interleaveSize) {
        blocks = nbytes / interleaveSize;
        if (nbytes % interleaveSize)
            blocks++;
        numBlocksPerServer = blocks / used_memsrv_cnt;
        extraBlocks = blocks % used_memsrv_cnt;
    }

    std::list<std::shared_future<Fam_Region_Item_Info>> resultList;
    for (auto memsrv : memoryServiceList) {
        Fam_Memory_Service *memoryService = memsrv;
        size_t size;
        if ((interleaveSize != 0) && (nbytes > interleaveSize)) {
            size = numBlocksPerServer * interleaveSize;
            if (extraBlocks) {
                size += interleaveSize;
                extraBlocks--;
            }
        } else {
            size = nbytes;
        }
        size_t aligned_size =
            align_to_address(size, 64); // align the size to a 64-bit boundary.
        size = (aligned_size > size ? aligned_size : size);
        if (size < MIN_OBJ_SIZE)
            size = MIN_OBJ_SIZE;

        std::future<Fam_Region_Item_Info> result(std::async(
            std::launch::async, &openfam::Fam_Memory_Service::allocate,
            memoryService, regionId, size));
        resultList.push_back(result.share());
    }

    std::vector<int> allocate_success_list;
    std::vector<int> allocate_failed_list;
    // Wait for region creation to complete.
    int id = 0;
    std::string create_ErrMsg = "Unknown error";
    enum Fam_Error create_famErr = FAM_ERR_UNKNOWN;
    bool regionFull = false;
    for (auto result : resultList) {
        try {
            Fam_Region_Item_Info itemInfo = result.get();
            dataitem.offsets[id] = itemInfo.offset;
            allocate_success_list.push_back(id++);
        } catch (Fam_Exception &e) {
            if (e.fam_error() == REGION_NO_SPACE) {
                regionFull = true;
            }
            allocate_failed_list.push_back(id++);
            create_ErrMsg = e.fam_error_msg();
            create_famErr = (Fam_Error)e.fam_error();
        } catch (...) {
            allocate_failed_list.push_back(id++);
        }
    }

    if (allocate_failed_list.size() > 0) {
        ostringstream message;
        allocate_failure_cleanup(allocate_success_list, memoryServiceList,
                                 regionId, dataitem.offsets);
        if (regionFull) {
            THROW_ERRNO_MSG(CIS_Exception, REGION_NO_SPACE,
                            "No space in region for data item allocation");
        } else if (allocate_failed_list.size() == 1) {
            THROW_ERRNO_MSG(CIS_Exception, create_famErr, create_ErrMsg);
        } else {
            message << "Multiple memory servers failed to allocate dataitem";
            THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_CREATED,
                            message.str().c_str());
        }
    }
    uint64_t dataitemId = get_dataitem_id(dataitem.offsets[0], memServerIds[0]);

    dataitem.regionId = regionId;
    strncpy(dataitem.name, name.c_str(), metadataMaxKeyLen);
    dataitem.permissionLevel = permissionLevel;
    if (permissionLevel == REGION)
        dataitem.perm = regionPermission;
    else
        dataitem.perm = permission;
    dataitem.gid = gid;
    dataitem.uid = uid;
    dataitem.size = nbytes;
    dataitem.used_memsrv_cnt = used_memsrv_cnt;
    dataitem.interleaveSize = interleaveSize;
    memcpy(dataitem.memoryServerIds, memServerIds,
           used_memsrv_cnt * sizeof(uint64_t));
    if (name == "") {
        metadataService->metadata_insert_dataitem(dataitemId, regionId,
                                                  &dataitem);
    } else {
        metadataService->metadata_insert_dataitem(dataitemId, regionId,
                                                  &dataitem, name);
    }

    // return other data item information
    if (!isSharedMemory && permissionLevel == REGION) {
        // In case of REGION level permission, offset from all the memory server
        // is required at the client to get actual offset and base address of
        // the data item
        memcpy(info.dataitemOffsets, dataitem.offsets,
               dataitem.used_memsrv_cnt * sizeof(uint64_t));
    } else {
        // In case of DATAITEM level permission, register the dataitem memory
        try {
            register_dataitem_memory(dataitem, metadataService,
                                     memoryServiceList, uid, gid, &info);
            info.itemRegistrationStatus = true;
        } catch (...) {
            info.itemRegistrationStatus = false;
        }
        // offset from only first memory server is required at the client
        info.dataitemOffsets[0] = dataitem.offsets[0];
    }
    info.used_memsrv_cnt = used_memsrv_cnt;
    info.interleaveSize = interleaveSize;
    info.permissionLevel = permissionLevel;
    info.perm = dataitem.perm;
    memcpy(info.memoryServerIds, memServerIds,
           used_memsrv_cnt * sizeof(uint64_t));
    info.size = nbytes;
    free(memServerIds);
    CIS_DIRECT_PROFILE_END_OPS(cis_allocate);
    return info;
}

/*
 * This function open the region and get the memory registration details (key,
 * base address etc.) from memory server. If not registered already, at first,
 * register the region memory and then returns the registration details.
 */
void Fam_CIS_Direct::open_region_with_registration(
    uint64_t regionId, uint32_t uid, uint32_t gid,
    std::vector<uint64_t> *memserverIds,
    Fam_Region_Memory_Map *regionMemoryMap) {
    ostringstream message;
    // Look for region metadata and get the list of memory servers it is
    // spread across
    int metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    Fam_Region_Metadata region;
    metadataService->metadata_find_region(regionId, region);

    // Create a vector of memory service objects for all memory services in the
    // list
    std::vector<Fam_Memory_Service *> regionMemoryServiceList;
    for (uint64_t i = 0; i < region.used_memsrv_cnt; i++) {
        regionMemoryServiceList.push_back(
            get_memory_service(region.memServerIds[i]));
        memserverIds->push_back(region.memServerIds[i]);
    }

    bool rwFlag;
    if (check_region_permission(region, 1, metadataService, uid, gid)) {
        rwFlag = 1;
    } else if (check_region_permission(region, 0, metadataService, uid, gid)) {
        rwFlag = 0;
    } else {
        message << "Not permitted to use this dataitem";
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_NOPERM, message.str().c_str());
    }

    // Open region and also register memory on all memory servers asynchronously
    std::list<std::shared_future<Fam_Region_Memory>> resultList;
    int idx = 0;
    for (auto memsrv : regionMemoryServiceList) {
        Fam_Memory_Service *memoryService = memsrv;
        std::future<Fam_Region_Memory> result(std::async(
            std::launch::async,
            &openfam::Fam_Memory_Service::open_region_with_registration,
            memoryService, regionId, rwFlag));
        resultList.push_back(result.share());
        idx++;
    }
    idx = 0;
    int failCount = 0;
    Fam_Exception exception(FAM_ERR_UNKNOWN,
                            "Unknown error occured while opening the region");
    // wait for all memory servers to complete the task
    for (auto result : resultList) {
        try {
            Fam_Region_Memory regionMemory = result.get();
            regionMemoryMap->insert(
                {regionMemoryServiceList[idx]->get_memory_server_id(),
                 regionMemory});
        } catch (Fam_Exception &e) {
            failCount++;
            exception = e;
        } catch (...) {
            failCount++;
        }
        idx++;
    }

    switch (failCount) {
    // If all memory server successfully open the region and fetch region memory
    // registration information,
    case 0:
        return;
    // If only one memory server fails to open the region and fetch region
    // memory registration information,
    case 1:
        throw exception;
    // If multiple memory servers fail to open the region and fetch region
    // memory registration information,
    default:
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_RESOURCE,
                        "Multiple memory servers failed to open the region");
    }
}

/*
 * This function open the region and does not register region memory
 */
void Fam_CIS_Direct::open_region_without_registration(
    uint64_t regionId, std::vector<uint64_t> *memserverIds) {
    ostringstream message;
    // Look for region metadata and get the list of memory servers it is
    // spread across
    int metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    Fam_Region_Metadata region;
    metadataService->metadata_find_region(regionId, region);

    // Create a vector of memory service objects for all memory services in the
    // list
    std::vector<Fam_Memory_Service *> regionMemoryServiceList;
    for (uint64_t i = 0; i < region.used_memsrv_cnt; i++) {
        regionMemoryServiceList.push_back(
            get_memory_service(region.memServerIds[i]));
        memserverIds->push_back(region.memServerIds[i]);
    }

    // Open the region all memory server without registering it
    std::list<std::shared_future<void>> resultList;
    for (auto memsrv : regionMemoryServiceList) {
        Fam_Memory_Service *memoryService = memsrv;
        std::future<void> result(std::async(
            std::launch::async,
            &openfam::Fam_Memory_Service::open_region_without_registration,
            memoryService, regionId));
        resultList.push_back(result.share());
    }
    int failCount = 0;
    Fam_Exception exception(FAM_ERR_UNKNOWN,
                            "Unknown error occured while opening the region");
    // Wait for all memory server to complete the asynchronous call
    for (auto result : resultList) {
        try {
            result.get();
        } catch (Fam_Exception &e) {
            failCount++;
            exception = e;
        } catch (...) {
            failCount++;
        }
    }

    switch (failCount) {
    // If all memory server successfully open the region
    case 0:
        return;
    // If only one memory server fails to open the region
    case 1:
        throw exception;
    // If multiple memory servers fail to open the region
    default:
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_RESOURCE,
                        "Multiple memory servers failed to open the region");
    }
}

/*
 * This function register the region memory and does not return any registration
 * details.
 */
void Fam_CIS_Direct::register_region_memory(
    Fam_Region_Metadata region, Fam_Metadata_Service *metadataService,
    std::vector<Fam_Memory_Service *> regionMemoryServiceList, uint32_t uid,
    uint32_t gid) {
    ostringstream message;
    bool rwFlag;
    if (check_region_permission(region, 1, metadataService, uid, gid)) {
        rwFlag = 1;
    } else if (check_region_permission(region, 0, metadataService, uid, gid)) {
        rwFlag = 0;
    } else {
        message << "Not permitted to use this dataitem";
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_NOPERM, message.str().c_str());
    }

    std::list<std::shared_future<void>> resultList;
    for (auto memsrv : regionMemoryServiceList) {
        Fam_Memory_Service *memoryService = memsrv;
        std::future<void> result(
            std::async(std::launch::async,
                       &openfam::Fam_Memory_Service::register_region_memory,
                       memoryService, region.regionId, rwFlag));
        resultList.push_back(result.share());
    }

    for (auto result : resultList) {
        try {
            result.get();
        } catch (...) {
        }
    }
}

/*
 * This function get the memory registration details (key, base address etc.) of
 * already opened region
 */
void Fam_CIS_Direct::get_region_memory(uint64_t regionId, uint32_t uid,
                                       uint32_t gid,
                                       Fam_Region_Memory_Map *regionMemoryMap) {
    ostringstream message;
    // Look for region metadata and get the list of memory servers it is
    // spread across
    int metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    Fam_Region_Metadata region;
    metadataService->metadata_find_region(regionId, region);

    // Create a vector of memory service objects for all memory services in the
    // list
    std::vector<Fam_Memory_Service *> regionMemoryServiceList;
    for (uint64_t i = 0; i < region.used_memsrv_cnt; i++) {
        regionMemoryServiceList.push_back(
            get_memory_service(region.memServerIds[i]));
    }

    bool rwFlag;
    if (check_region_permission(region, 1, metadataService, uid, gid)) {
        rwFlag = 1;
    } else if (check_region_permission(region, 0, metadataService, uid, gid)) {
        rwFlag = 0;
    } else {
        message << "Not permitted to use this dataitem";
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_NOPERM, message.str().c_str());
    }

    // Register region memory or get the key if already registered
    std::list<std::shared_future<Fam_Region_Memory>> resultList;
    int idx = 0;
    // Request all memory servers for memory registration details of already
    // opened region
    for (auto memsrv : regionMemoryServiceList) {
        Fam_Memory_Service *memoryService = memsrv;
        std::future<Fam_Region_Memory> result(std::async(
            std::launch::async, &openfam::Fam_Memory_Service::get_region_memory,
            memoryService, regionId, rwFlag));
        resultList.push_back(result.share());
        idx++;
    }
    idx = 0;
    int failCount = 0;
    Fam_Exception exception(FAM_ERR_UNKNOWN,
                            "Unknown error occured while fetching region "
                            "memory registration information");
    // Wait for all emmory servers to complete asynchronous call
    for (auto result : resultList) {
        try {
            Fam_Region_Memory regionMemory = result.get();
            regionMemoryMap->insert(
                {regionMemoryServiceList[idx]->get_memory_server_id(),
                 regionMemory});
        } catch (Fam_Exception &e) {
            failCount++;
            exception = e;
        } catch (...) {
            failCount++;
        }
        idx++;
    }

    switch (failCount) {
    // If all memory server successfully fetch region memory registration
    // information,
    case 0:
        return;
    // If only one memory server fails to fetch region memory registration
    // information,
    case 1:
        throw exception;
    // If multiple memory servers fail to fetch region memory registration
    // information,
    default:
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_RESOURCE,
                        "Multiple memory servers failed to get region memory "
                        "registration information");
    }
}

/*
 * This function get dataitem registration details (key, base address etc.). If
 * not registered already, Register the dataitem and then return back the
 * registration details.
 */
void Fam_CIS_Direct::register_dataitem_memory(
    Fam_DataItem_Metadata dataitem, Fam_Metadata_Service *metadataService,
    std::vector<Fam_Memory_Service *> memoryServiceList, uint32_t uid,
    uint32_t gid, Fam_Region_Item_Info *info) {
    ostringstream message;
    bool rwFlag;
    if (check_dataitem_permission(dataitem, 1, metadataService, uid, gid)) {
        rwFlag = 1;
    } else if (check_dataitem_permission(dataitem, 0, metadataService, uid,
                                         gid)) {
        rwFlag = 0;
    } else {
        message << "Not permitted to use this dataitem";
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_NOPERM, message.str().c_str());
    }

    std::list<std::shared_future<Fam_Dataitem_Memory>> resultList;
    int idx = 0;
    size_t blocks = 0, numBlocksPerServer = 0, extraBlocks = 0;
    if (dataitem.interleaveSize) {
        blocks = dataitem.size / dataitem.interleaveSize;
        if (dataitem.size % dataitem.interleaveSize)
            blocks++;
        numBlocksPerServer = blocks / dataitem.used_memsrv_cnt;
        extraBlocks = blocks % dataitem.used_memsrv_cnt;
    }
    for (auto memsrv : memoryServiceList) {
        Fam_Memory_Service *memoryService = memsrv;
        size_t size;
        if ((dataitem.interleaveSize != 0) &&
            (dataitem.size > dataitem.interleaveSize)) {
            size = numBlocksPerServer * dataitem.interleaveSize;
            if (extraBlocks) {
                size += dataitem.interleaveSize;
                extraBlocks--;
            }
        } else {
            size = dataitem.size;
        }
        size_t aligned_size =
            align_to_address(size, 64); // align the size to a 64-bit boundary.
        size = (aligned_size > size ? aligned_size : size);
        if (size < MIN_OBJ_SIZE)
            size = MIN_OBJ_SIZE;
        std::future<Fam_Dataitem_Memory> result(std::async(
            std::launch::async,
            &openfam::Fam_Memory_Service::get_dataitem_memory, memoryService,
            dataitem.regionId, dataitem.offsets[idx], size, rwFlag));
        resultList.push_back(result.share());
        idx++;
    }

    int failCount = 0;
    idx = 0;
    Fam_Exception exception(FAM_ERR_UNKNOWN,
                            "Unknown error occured while fetching dataitem "
                            "memory registration information");
    for (auto result : resultList) {
        try {
            Fam_Dataitem_Memory dataitemMemory = result.get();
            info->dataitemKeys[idx] = dataitemMemory.key;
            info->baseAddressList[idx] = dataitemMemory.base;
        } catch (Fam_Exception &e) {
            failCount++;
            exception = e;
        } catch (...) {
            failCount++;
        }
        idx++;
    }

    switch (failCount) {
    // If all memory server successfully fetch data item memory registration
    // information,
    case 0:
        return;
    // If only one memory server fails to fetch data item memory registration
    // information,
    case 1:
        throw exception;
    // If multiple memory servers fail to fetch data item memory registration
    // information,
    default:
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_RESOURCE,
                        "Multiple memory servers failed to get data item "
                        "memory registration information");
    }
}

/*
 * This function close the region
 */
void Fam_CIS_Direct::close_region(uint64_t regionId,
                                  std::vector<uint64_t> memserverIds) {
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Region_Metadata region;
    ostringstream message;

    // Create a vector of memory service objects for all memory services in the
    // list
    std::vector<Fam_Memory_Service *> regionMemoryServiceList;
    for (auto id : memserverIds) {
        regionMemoryServiceList.push_back(get_memory_service(id));
    }

    std::list<std::shared_future<uint64_t>> resultList;

    // Send request to all memory servers to close the region
    for (auto memsrv : regionMemoryServiceList) {
        Fam_Memory_Service *memoryService = memsrv;
        std::future<uint64_t> result(std::async(
            std::launch::async, &openfam::Fam_Memory_Service::close_region,
            memoryService, regionId));
        resultList.push_back(result.share());
    }

    int failCount = 0;
    Fam_Exception exception(FAM_ERR_UNKNOWN,
                            "Unknown error occured while closing the region");

    // Wait for all memory servers to complete asynchronous call
    for (auto result : resultList) {
        try {
            result.get();
        } catch (Fam_Exception &e) {
            failCount++;
            exception = e;
        } catch (...) {
            failCount++;
        }
    }

    switch (failCount) {
    // If all memory server successfully close the region
    case 0:
        return;
    // If only one memory server fails to close the region
    case 1:
        throw exception;
    // If multiple memory servers fail to close the region
    default:
        THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_RESOURCE,
                        "Multiple memory servers failed to close the region");
    }
}

void Fam_CIS_Direct::deallocate(uint64_t regionId, uint64_t offset,
                                uint64_t memoryServerId, uint32_t uid,
                                uint32_t gid) {
    CIS_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    uint64_t metadataServiceId = 0;
    std::vector<Fam_Memory_Service *> memoryServiceList;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    // Check with metadata service if data item with the requested name can be
    // deallocated.
    uint64_t dataitemId = get_dataitem_id(offset, memoryServerId);
    Fam_DataItem_Metadata dataitem;
    metadataService->metadata_validate_and_deallocate_dataitem(
        regionId, dataitemId, uid, gid, dataitem);
    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        memoryServiceList.push_back(
            get_memory_service(dataitem.memoryServerIds[i]));
    }

    std::list<std::shared_future<void>> resultList;
    int idx = 0;

        for (auto memsrv : memoryServiceList) {
            Fam_Memory_Service *memoryService = memsrv;
            std::future<void> result(std::async(
                std::launch::async, &openfam::Fam_Memory_Service::deallocate,
                memoryService, regionId, dataitem.offsets[idx++]));
            resultList.push_back(result.share());
        }

    // Wait for region destroy to complete.
    try {
        for (auto result : resultList) {
            result.get();
        }
    } catch (Fam_Exception &e) {
        throw e;
    }

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

    if (dataitem.permissionLevel == REGION) {
        message << "Dataitem permission modification not permitted if the "
                   "region is created with region level permission";
        THROW_ERRNO_MSG(CIS_Exception, ITEM_PERM_MODIFY_NOT_PERMITTED,
                        message.str().c_str());
    }
    // Check with metadata service if the calling PE has the permission
    // to modify perm
    // issions of the region, if not return error
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
bool Fam_CIS_Direct::check_region_permission(
    Fam_Region_Metadata region, bool op, Fam_Metadata_Service *metadataService,
    uint32_t uid, uint32_t gid) {
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
bool Fam_CIS_Direct::check_dataitem_permission(
    Fam_DataItem_Metadata dataitem, bool op,
    Fam_Metadata_Service *metadataService, uint32_t uid, uint32_t gid) {

    metadata_region_item_op_t opFlag;

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
        throw e;
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
    info.permissionLevel = region.permissionLevel;
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
        throw e;
    }

    info.regionId = dataitem.regionId;
    info.offset = dataitem.offsets[0];
    info.size = dataitem.size;
    info.perm = dataitem.perm;
    info.uid = dataitem.uid;
    info.gid = dataitem.gid;
    strncpy(info.name, dataitem.name, metadataMaxKeyLen);
    info.used_memsrv_cnt = dataitem.used_memsrv_cnt;
    memcpy(info.memoryServerIds, dataitem.memoryServerIds,
           dataitem.used_memsrv_cnt * sizeof(uint64_t));
    info.maxNameLen = metadataMaxKeyLen;
    info.interleaveSize = dataitem.interleaveSize;
    info.permissionLevel = dataitem.permissionLevel;
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
        throw e;
    }
    info.size = region.size;
    info.perm = region.perm;
    strncpy(info.name, region.name, metadataMaxKeyLen);
    info.maxNameLen = metadataMaxKeyLen;
    info.redundancyLevel = region.redundancyLevel;
    info.memoryType = region.memoryType;
    info.interleaveEnable = region.interleaveEnable;
    info.interleaveSize = region.interleaveSize;
    info.used_memsrv_cnt = region.used_memsrv_cnt;
    info.uid = region.uid;
    info.gid = region.gid;
    info.permissionLevel = region.permissionLevel;
    memcpy(info.memoryServerIds, region.memServerIds,
           region.used_memsrv_cnt * sizeof(uint64_t));
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
    std::vector<Fam_Memory_Service *> memoryServiceList;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);
    Fam_DataItem_Metadata dataitem;
    message << "Error While locating dataitem : ";

    uint64_t dataitemId = get_dataitem_id(offset, memoryServerId);

    if (!metadataService->metadata_find_dataitem(dataitemId, regionId,
                                                 dataitem)) {
        message << "could not find the dataitem";
        THROW_ERRNO_MSG(CIS_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        memoryServiceList.push_back(
            get_memory_service(dataitem.memoryServerIds[i]));
    }

    if (!isSharedMemory && dataitem.permissionLevel == REGION) {
        memcpy(info.dataitemOffsets, dataitem.offsets,
               dataitem.used_memsrv_cnt * sizeof(uint64_t));
    } else {
        // Register the data item memory
        register_dataitem_memory(dataitem, metadataService, memoryServiceList,
                                 uid, gid, &info);
        info.dataitemOffsets[0] = dataitem.offsets[0];
    }

    // return all other data item information
    info.regionId = dataitem.regionId;
    info.used_memsrv_cnt = dataitem.used_memsrv_cnt;
    info.size = dataitem.size;
    info.perm = dataitem.perm;
    strncpy(info.name, dataitem.name, metadataMaxKeyLen);
    info.maxNameLen = metadataMaxKeyLen;
    info.interleaveSize = dataitem.interleaveSize;
    info.permissionLevel = dataitem.permissionLevel;
    memcpy(info.memoryServerIds, dataitem.memoryServerIds,
           dataitem.used_memsrv_cnt * sizeof(uint64_t));

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
        throw e;
    }

    info.size = dataitem.size;
    info.perm = dataitem.perm;
    strncpy(info.name, dataitem.name, metadataMaxKeyLen);
    info.maxNameLen = metadataMaxKeyLen;
    info.uid = dataitem.uid;
    info.gid = dataitem.gid;
    info.interleaveSize = dataitem.interleaveSize;
    info.used_memsrv_cnt = dataitem.used_memsrv_cnt;
    info.permissionLevel = dataitem.permissionLevel;
    memcpy(info.memoryServerIds, dataitem.memoryServerIds,
           dataitem.used_memsrv_cnt * sizeof(uint64_t));
    CIS_DIRECT_PROFILE_END_OPS(cis_get_stat_info);
    return info;
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
    if (check_dataitem_permission(dataitem, 1, metadataService, uid, gid) |
        check_dataitem_permission(dataitem, 0, metadataService, uid, gid)) {
        Fam_Memory_Service *memoryService = get_memory_service(memoryServerId);
        localPointer = memoryService->get_local_pointer(regionId, offset);
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
                           uint64_t srcUsedMemsrvCnt, uint64_t srcCopyStart,
                           uint64_t *srcKeys, uint64_t *srcBaseAddrList,
                           uint64_t destRegionId, uint64_t destOffset,
                           uint64_t destCopyStart, uint64_t size,
                           uint64_t firstSrcMemserverId,
                           uint64_t firstDestMemserverId, uint32_t uid,
                           uint32_t gid) {
    ostringstream message;
    message << "Error While copying from dataitem : ";
    Fam_DataItem_Metadata srcDataitem;
    Fam_DataItem_Metadata destDataitem;
    Fam_Copy_Wait_Object *waitObj = new Fam_Copy_Wait_Object();
    CIS_DIRECT_PROFILE_START_OPS()
    uint64_t metadataServiceId = 0;

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);

    // Permission check, data item out of bound check, already done on the
    // client side. This looks redundant, can be removed later.
    uint64_t srcDataitemId = get_dataitem_id(srcOffset, firstSrcMemserverId);
    uint64_t destDataitemId = get_dataitem_id(destOffset, firstDestMemserverId);

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
        throw e;
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
        throw e;
    }

    if (!((srcCopyStart + size) <= srcDataitem.size)) {
        message << "Source offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }

    if (!((destCopyStart + size) <= destDataitem.size)) {
        message << "Destination offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }

    if (useAsyncCopy) {
        Fam_Copy_Tag *tag = new Fam_Copy_Tag();
        tag->copyDone.store(false, boost::memory_order_seq_cst);
        tag->memoryServiceMap = get_memory_service_map();
        tag->srcRegionId = srcRegionId;
        memcpy(tag->srcOffsets, srcDataitem.offsets,
               srcDataitem.used_memsrv_cnt * sizeof(uint64_t));
        tag->srcCopyStart = srcCopyStart;
        tag->destCopyStart = destCopyStart;
        memcpy(tag->srcBaseAddrList, srcBaseAddrList,
               srcDataitem.used_memsrv_cnt * sizeof(uint64_t));
        tag->destRegionId = destRegionId;
        memcpy(tag->destOffsets, destDataitem.offsets,
               destDataitem.used_memsrv_cnt * sizeof(uint64_t));
        tag->size = size;
        memcpy(tag->srcKeys, srcKeys,
               srcDataitem.used_memsrv_cnt * sizeof(uint64_t));
        tag->srcInterleaveSize = srcDataitem.interleaveSize;
        tag->destInterleaveSize = destDataitem.interleaveSize;
        tag->srcUsedMemsrvCnt = srcDataitem.used_memsrv_cnt;
        tag->destUsedMemsrvCnt = destDataitem.used_memsrv_cnt;
        memcpy(tag->srcMemserverIds, srcDataitem.memoryServerIds,
               srcDataitem.used_memsrv_cnt * sizeof(uint64_t));
        memcpy(tag->destMemserverIds, destDataitem.memoryServerIds,
               destDataitem.used_memsrv_cnt * sizeof(uint64_t));
        Fam_Ops_Info opsInfo = { COPY, NULL, NULL, 0, 0, 0, 0, 0, tag };
        asyncQHandler->initiate_operation(opsInfo);
        waitObj->tag = tag;
    } else {
        uint64_t srcCopyEnd = srcCopyStart + size;
        uint64_t destStartServerIdx =
            (destDataitem.used_memsrv_cnt == 1)
                ? 0
                : (destCopyStart / destDataitem.interleaveSize) %
                      destDataitem.used_memsrv_cnt;
        uint64_t destFamPtr =
            (destDataitem.used_memsrv_cnt == 1)
                ? destCopyStart
                : (((destCopyStart / destDataitem.interleaveSize) -
                    destStartServerIdx) /
                   destDataitem.used_memsrv_cnt) *
                      destDataitem.interleaveSize;
        uint64_t destDisplacement =
            (destDataitem.used_memsrv_cnt == 1)
                ? 0
                : destCopyStart % destDataitem.interleaveSize;
        std::list<std::shared_future<void>> resultList;
        for (int i = 0; i < (int)destDataitem.used_memsrv_cnt; i++) {
            int index = (i + (int)destStartServerIdx) %
                        (int)destDataitem.used_memsrv_cnt;
            uint64_t additionalOffset =
                (index == (int)destStartServerIdx) ? destDisplacement : 0;
            Fam_Memory_Service *memoryService =
                get_memory_service(destDataitem.memoryServerIds[index]);
            std::future<void> result(std::async(
                std::launch::async, &openfam::Fam_Memory_Service::copy,
                memoryService, srcRegionId, srcDataitem.offsets,
                srcDataitem.used_memsrv_cnt, srcCopyStart, srcCopyEnd, srcKeys,
                srcBaseAddrList, destRegionId,
                destDataitem.offsets[index] + destFamPtr + additionalOffset,
                destDataitem.used_memsrv_cnt, srcDataitem.memoryServerIds,
                srcDataitem.interleaveSize, destDataitem.interleaveSize, size));
            resultList.push_back(result.share());
            if (index == (int)(destDataitem.used_memsrv_cnt - 1))
                destFamPtr += destDataitem.interleaveSize;
            srcCopyStart += (destDataitem.interleaveSize - additionalOffset);
        }

        // Wait for region destroy to complete.
        try {
            for (auto result : resultList) {
                result.get();
            }
        } catch (Fam_Exception &e) {
            throw e;
        }
    }
    CIS_DIRECT_PROFILE_END_OPS(cis_copy);
    return (void *)waitObj;
}

void Fam_CIS_Direct::wait_for_copy(void *waitObj) {
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Copy_Wait_Object *obj = (Fam_Copy_Wait_Object *)waitObj;
    asyncQHandler->wait_for_copy((void *)(obj->tag));
    if (obj)
        delete obj;
    CIS_DIRECT_PROFILE_END_OPS(cis_wait_for_copy);
}

void *Fam_CIS_Direct::backup(uint64_t srcRegionId, uint64_t srcOffset,
                             uint64_t srcFirstMemoryServerId, string BackupName,
                             uint32_t uid, uint32_t gid) {
    ostringstream message;
    Fam_DataItem_Metadata srcDataitem;
    Fam_Backup_Wait_Object *waitObj = new Fam_Backup_Wait_Object();
    CIS_DIRECT_PROFILE_START_OPS()
    uint64_t metadataServiceId = 0;
    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);

    // Permission check, data item out of bound check, already done on the
    // client side. This looks redundant, can be removed later.
    uint64_t srcDataitemId = get_dataitem_id(srcOffset, srcFirstMemoryServerId);
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
        throw e;
    }

    // Get data item name and permission bits from Fam_DataItem_Metadata
    // and set that as permission bits for backup as well.
    string dataitemName = std::string(srcDataitem.name);
    mode_t backup_mode = srcDataitem.perm;

    // Now check if backup file already exists from first memory server.
    Fam_Memory_Service *memoryService =
        get_memory_service(srcFirstMemoryServerId);
    Fam_Backup_Info info;
    try {
        info =
            memoryService->get_backup_info(BackupName, uid, gid, BACKUP_READ);
    } catch (Fam_Exception &e) {
        // Ignore these exceptions as they are not essential for backup
        // creation.
    }
    if (info.size == (int)-1) {
        THROW_ERRNO_MSG(CIS_Exception, BACKUP_FILE_EXIST,
                        "Backup already exists.");
    }

    size_t blocks = 1, numBlocksPerServer = 1, extraBlocks = 0;

    uint64_t sizePerServer, chunkSize;

    if ((srcDataitem.interleaveSize != 0) &&
        (srcDataitem.size > srcDataitem.interleaveSize) &&
        (srcDataitem.used_memsrv_cnt > 1)) {
        blocks = srcDataitem.size / srcDataitem.interleaveSize;
        if (srcDataitem.size % srcDataitem.interleaveSize)
            blocks++;
        numBlocksPerServer = (blocks > srcDataitem.used_memsrv_cnt)
                                 ? blocks / srcDataitem.used_memsrv_cnt
                                 : 1;
        sizePerServer = numBlocksPerServer * srcDataitem.interleaveSize;
        chunkSize = srcDataitem.interleaveSize;
        extraBlocks = blocks % srcDataitem.used_memsrv_cnt;
    } else {
        sizePerServer = srcDataitem.size;
        chunkSize = srcDataitem.size;
    }
    if (useAsyncCopy) {
        Fam_Backup_Tag *tag = new Fam_Backup_Tag();
        tag->backupDone.store(false, boost::memory_order_seq_cst);
        tag->memoryServiceMap = get_memory_service_map();
        tag->srcRegionId = srcRegionId;
        memcpy(tag->srcOffsets, srcDataitem.offsets,
               srcDataitem.used_memsrv_cnt * sizeof(uint64_t));
        memcpy(tag->srcMemserverIds, srcDataitem.memoryServerIds,
               srcDataitem.used_memsrv_cnt * sizeof(uint64_t));
        tag->usedMemserverCnt = srcDataitem.used_memsrv_cnt;
        tag->srcInterleaveSize = srcDataitem.interleaveSize;
        tag->srcItemSize = srcDataitem.size;
        tag->sizePerServer = sizePerServer;
        tag->chunkSize = chunkSize;
        tag->extraBlocks = extraBlocks;
        tag->uid = uid;
        tag->gid = gid;
        tag->mode = backup_mode;
        tag->BackupName = BackupName;
        tag->dataitemName = dataitemName;
        Fam_Ops_Info opsInfo = { BACKUP, NULL, NULL, 0, 0, 0, 0, 0, tag };
        asyncQHandler->initiate_operation(opsInfo);
        waitObj->tag = tag;
    } else {
        bool writeMetadata = true;
        std::list<std::shared_future<void>> resultList;
        for (int i = 0; i < (int)srcDataitem.used_memsrv_cnt; i++) {
            Fam_Memory_Service *memoryService =
                get_memory_service(srcDataitem.memoryServerIds[i]);
            uint64_t size = sizePerServer;
            if (extraBlocks) {
                size += srcDataitem.interleaveSize;
                extraBlocks--;
            }

            std::future<void> result(std::async(
                std::launch::async, &openfam::Fam_Memory_Service::backup,
                memoryService, srcRegionId, srcDataitem.offsets[i], size,
                chunkSize, srcDataitem.used_memsrv_cnt, i, BackupName, uid, gid,
                backup_mode, dataitemName, srcDataitem.size, writeMetadata));
            resultList.push_back(result.share());
            writeMetadata = false;
        }

        // Wait for region destroy to complete.
        try {
            for (auto result : resultList) {
                result.get();
            }
        } catch (Fam_Exception &e) {
            throw e;
        }
    }
    CIS_DIRECT_PROFILE_END_OPS(cis_backup);
    return (void *)waitObj;
}

void *Fam_CIS_Direct::restore(uint64_t destRegionId, uint64_t destOffset,
                              uint64_t destFirstMemoryServerId,
                              string BackupName, uint32_t uid, uint32_t gid) {
    ostringstream message;
    Fam_DataItem_Metadata destDataitem;
    Fam_Restore_Wait_Object *waitObj = new Fam_Restore_Wait_Object();
    CIS_DIRECT_PROFILE_START_OPS()
    uint64_t metadataServiceId = 0;
    Fam_Memory_Service *memoryService =
        get_memory_service(destFirstMemoryServerId);

    Fam_Metadata_Service *metadataService =
        get_metadata_service(metadataServiceId);

    // Permission check, data item out of bound check, already done on the
    // client side. This looks redundant, can be removed later.
    uint64_t destDataitemId =
        get_dataitem_id(destOffset, destFirstMemoryServerId);

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
        throw e;
    }
    Fam_Backup_Info info;
    try {
        info =
            memoryService->get_backup_info(BackupName, uid, gid, BACKUP_READ);

    } catch (Fam_Exception &e) {
        throw e;
    }
    if (destDataitem.size < (uint64_t)info.size) {
        message << "data item size is smaller than backup ";
        THROW_ERRNO_MSG(CIS_Exception, BACKUP_SIZE_TOO_LARGE,
                        message.str().c_str());
    }

    size_t blocks = 1, numBlocksPerServer = 1, extraBlocks = 0;
    uint64_t sizePerServer, chunkSize;
    if ((destDataitem.interleaveSize != 0) &&
        ((uint64_t)info.size > destDataitem.interleaveSize) &&
        (destDataitem.used_memsrv_cnt > 1)) {
        blocks = info.size / destDataitem.interleaveSize;
        if (info.size % destDataitem.interleaveSize)
            blocks++;
        numBlocksPerServer = (blocks > destDataitem.used_memsrv_cnt)
                                 ? blocks / destDataitem.used_memsrv_cnt
                                 : 1;
        sizePerServer = numBlocksPerServer * destDataitem.interleaveSize;
        chunkSize = destDataitem.interleaveSize;
        extraBlocks = blocks % destDataitem.used_memsrv_cnt;
    } else {
        sizePerServer = info.size;
        chunkSize = info.size;
    }

    uint64_t iterations = (blocks > destDataitem.used_memsrv_cnt)
                              ? destDataitem.used_memsrv_cnt
                              : blocks;
    if (useAsyncCopy) {
        Fam_Restore_Tag *tag = new Fam_Restore_Tag();
        tag->restoreDone.store(false, boost::memory_order_seq_cst);
        tag->memoryServiceMap = get_memory_service_map();
        tag->destRegionId = destRegionId;
        memcpy(tag->destOffsets, destDataitem.offsets,
               destDataitem.used_memsrv_cnt * sizeof(uint64_t));
        memcpy(tag->destMemserverIds, destDataitem.memoryServerIds,
               destDataitem.used_memsrv_cnt * sizeof(uint64_t));
        tag->usedMemserverCnt = destDataitem.used_memsrv_cnt;
        tag->destInterleaveSize = destDataitem.interleaveSize;
        tag->destItemSize = destDataitem.size;
        tag->sizePerServer = sizePerServer;
        tag->chunkSize = chunkSize;
        tag->extraBlocks = extraBlocks;
        tag->iterations = iterations;
        tag->BackupName = BackupName;
        Fam_Ops_Info opsInfo = { RESTORE, NULL, NULL, 0, 0, 0, 0, 0, tag };
        asyncQHandler->initiate_operation(opsInfo);
        waitObj->tag = tag;
    } else {
        std::list<std::shared_future<void>> resultList;
        for (int i = 0; i < (int)iterations; i++) {
            Fam_Memory_Service *memoryService =
                get_memory_service(destDataitem.memoryServerIds[i]);
            uint64_t size = sizePerServer;
            if (extraBlocks) {
                size += destDataitem.interleaveSize;
                extraBlocks--;
            }
            std::future<void> result(std::async(
                std::launch::async, &openfam::Fam_Memory_Service::restore,
                memoryService, destRegionId, destDataitem.offsets[i], size,
                chunkSize, destDataitem.used_memsrv_cnt, i, BackupName));
            resultList.push_back(result.share());
        }

        // Wait for region destroy to complete.
        try {
            for (auto result : resultList) {
                result.get();
            }
        } catch (Fam_Exception &e) {
            throw e;
        }
    }
    CIS_DIRECT_PROFILE_END_OPS(cis_restore);
    return (void *)waitObj;
}

Fam_Backup_Info Fam_CIS_Direct::get_backup_info(std::string BackupName,
                                                uint64_t memoryServerId,
                                                uint32_t uid, uint32_t gid) {
    Fam_Backup_Info info;
    ostringstream message;
    CIS_DIRECT_PROFILE_START_OPS()
    std::uint64_t hashVal = std::hash<std::string>{}(BackupName);
    uint64_t memserverRandomIndex = hashVal % memoryServers->size();

    auto it = memoryServers->begin();
    std::advance(it, memserverRandomIndex);

    Fam_Memory_Service *memoryService = get_memory_service((uint64_t)it->first);
    info = memoryService->get_backup_info(BackupName, uid, gid, BACKUP_READ);
    CIS_DIRECT_PROFILE_END_OPS(cis_get_backup_info);
    return info;
}

std::string Fam_CIS_Direct::list_backup(std::string BackupName,
                                        uint64_t memoryServerIdx, uint32_t uid,
                                        uint32_t gid) {
    ostringstream message;
    string info;
    std::uint64_t hashVal = std::hash<std::string>{}(BackupName);
    uint64_t memserverRandomIndex = hashVal % memoryServers->size();

    auto it = memoryServers->begin();
    std::advance(it, memserverRandomIndex);

    Fam_Memory_Service *memoryService = get_memory_service((uint64_t)it->first);
    info = memoryService->list_backup(BackupName, uid, gid, BACKUP_READ);
    return info;
}

void *Fam_CIS_Direct::delete_backup(string BackupName, uint64_t memoryServerIdx,
                                    uint32_t uid, uint32_t gid) {
    ostringstream message;
    std::uint64_t hashVal = std::hash<std::string>{}(BackupName);
    uint64_t memserverRandomIndex = hashVal % memoryServers->size();

    auto it = memoryServers->begin();
    std::advance(it, memserverRandomIndex);

    Fam_Memory_Service *memoryService = get_memory_service((uint64_t)it->first);
    Fam_Backup_Info info;
    try {
        info =
            memoryService->get_backup_info(BackupName, uid, gid, BACKUP_WRITE);
    } catch (Fam_Exception &e) {
        throw e;
    }

    Fam_Delete_Backup_Wait_Object *waitObj =
        new Fam_Delete_Backup_Wait_Object();
    CIS_DIRECT_PROFILE_START_OPS()
    if (useAsyncCopy) {
        Fam_Delete_Backup_Tag *tag = new Fam_Delete_Backup_Tag();
        tag->delbackupDone.store(false, boost::memory_order_seq_cst);
        tag->memoryService = memoryService;
        tag->BackupName = BackupName;
        Fam_Ops_Info opsInfo = {DELETE_BACKUP, NULL, NULL, 0, 0, 0, 0, 0, tag};
        asyncQHandler->initiate_operation(opsInfo);
        waitObj->tag = tag;
    } else {
        memoryService->delete_backup(BackupName);
    }
    CIS_DIRECT_PROFILE_END_OPS(cis_delete_backup);
    return (void *)waitObj;
}

void Fam_CIS_Direct::wait_for_backup(void *waitObj) {
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Backup_Wait_Object *obj = (Fam_Backup_Wait_Object *)waitObj;
    asyncQHandler->wait_for_backup((void *)(obj->tag));
    if (obj)
        delete obj;
    CIS_DIRECT_PROFILE_END_OPS(cis_wait_for_backup);
}

void Fam_CIS_Direct::wait_for_restore(void *waitObj) {
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Restore_Wait_Object *obj = (Fam_Restore_Wait_Object *)waitObj;
    asyncQHandler->wait_for_restore((void *)(obj->tag));
    if (obj)
        delete obj;
    CIS_DIRECT_PROFILE_END_OPS(cis_wait_for_restore);
}

void Fam_CIS_Direct::wait_for_delete_backup(void *waitObj) {
    CIS_DIRECT_PROFILE_START_OPS()
    Fam_Delete_Backup_Wait_Object *obj =
        (Fam_Delete_Backup_Wait_Object *)waitObj;
    asyncQHandler->wait_for_delete_backup((void *)(obj->tag));
    if (obj)
        delete obj;
    CIS_DIRECT_PROFILE_END_OPS(cis_wait_for_delete_backup);
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
    uint64_t dataitemId = memoryServerId << 48;
    dataitemId |= offset / MIN_OBJ_SIZE;
    return dataitemId;
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
            options["provider"] =
                (char *)strdup((info->get_key_value("provider")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["provider"] = (char *)strdup("sockets");
        }

        try {
            options["memsrv_interface_type"] = (char *)strdup(
                (info->get_key_value("memsrv_interface_type")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["memsrv_interface_type"] = (char *)strdup("rpc");
        }

        try {
            options["metadata_interface_type"] = (char *)strdup(
                (info->get_key_value("metadata_interface_type")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["metadata_interface_type"] = (char *)strdup("rpc");
        }

        // fetch rpc_framework type
        try {
            options["rpc_framework_type"] = (char *)strdup(
                (info->get_key_value("rpc_framework_type")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["rpc_framework_type"] = (char *)strdup("grpc");
        }

        try {
            std::vector<std::string> temp = info->get_value_list("memsrv_list");
            ostringstream memsrvList;
            std::vector<std::string> memsrvId;
            for (auto item : temp) {
                std::string delim = ":";

                auto start = 0U;
                auto end = item.find(delim);
                std::string currMemsrvId = item.substr(start, end - start);
                if (std::find(memsrvId.begin(), memsrvId.end(), currMemsrvId) ==
                    memsrvId.end()) {
                    /* memsrvId does not contain currMemsrvId */
                    memsrvId.push_back(currMemsrvId);
                    memsrvList << item << ",";

                } else {
                    /* memsrvId contains currMemsrvId  ie; duplicate memory
                     * server id*/
                    ostringstream message;
                    message << "Duplicate memory server id specified in fam config option memsrv_list: "<<currMemsrvId;
                    THROW_ERR_MSG(Fam_InvalidOption_Exception,
                                  message.str().c_str());
                }
            }
            options["memsrv_list"] = (char *)strdup(memsrvList.str().c_str());
        } catch (Fam_InvalidOption_Exception &e) {
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
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["metadata_list"] = (char *)strdup("0:127.0.0.1:8787");
        }
    }
    return options;
}
int Fam_CIS_Direct::get_atomic(uint64_t regionId, uint64_t srcOffset,
                               uint64_t dstOffset, uint64_t nbytes,
                               uint64_t key, uint64_t srcBaseAddr,
                               const char *nodeAddr, uint32_t nodeAddrSize,
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
            META_REGION_ITEM_READ, dataitemId, regionId, uid, gid, dataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw e;
    }

    if (!((dstOffset + nbytes) <= dataitem.size)) {
        message << "Source offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }

    memoryService->get_atomic(regionId, srcOffset, dstOffset, nbytes, key,
                              srcBaseAddr, nodeAddr, nodeAddrSize);
    CIS_DIRECT_PROFILE_END_OPS(cis_get_atomic);
    return 0;
}

int Fam_CIS_Direct::put_atomic(uint64_t regionId, uint64_t srcOffset,
                               uint64_t dstOffset, uint64_t nbytes,
                               uint64_t key, uint64_t srcBaseAddr,
                               const char *nodeAddr, uint32_t nodeAddrSize,
                               const char *data, uint64_t memoryServerId,
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
            META_REGION_ITEM_WRITE, dataitemId, regionId, uid, gid, dataitem);
    }
    catch (Fam_Exception &e) {
        if (e.fam_error() == NO_PERMISSION) {
            message << "Not permitted to access the region";
            THROW_ERRNO_MSG(CIS_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
        throw e;
    }

    if (!((dstOffset + nbytes) <= dataitem.size)) {
        message << "Source offset or size is beyond dataitem boundary";
        THROW_ERRNO_MSG(CIS_Exception, OUT_OF_RANGE, message.str().c_str());
    }
    memoryService->put_atomic(regionId, srcOffset, dstOffset, nbytes, key,
                              srcBaseAddr, nodeAddr, nodeAddrSize, data);
    CIS_DIRECT_PROFILE_END_OPS(cis_put_atomic);
    return 0;
}

int Fam_CIS_Direct::scatter_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize,
    uint64_t memoryServerId, uint32_t uid, uint32_t gid) {

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
        throw e;
    }

    memoryService->scatter_strided_atomic(
        regionId, offset, nElements, firstElement, stride, elementSize, key,
        srcBaseAddr, nodeAddr, nodeAddrSize);
    CIS_DIRECT_PROFILE_END_OPS(cis_scatter_strided_atomic);
    return 0;
}

int Fam_CIS_Direct::gather_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize,
    uint64_t memoryServerId, uint32_t uid, uint32_t gid) {

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
        throw e;
    }

    memoryService->gather_strided_atomic(regionId, offset, nElements,
                                         firstElement, stride, elementSize, key,
                                         srcBaseAddr, nodeAddr, nodeAddrSize);
    CIS_DIRECT_PROFILE_END_OPS(cis_gather_strided_atomic);
    return 0;
}

int Fam_CIS_Direct::scatter_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize,
    uint64_t memoryServerId, uint32_t uid, uint32_t gid) {

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
        throw e;
    }

    memoryService->scatter_indexed_atomic(regionId, offset, nElements,
                                          elementIndex, elementSize, key,
                                          srcBaseAddr, nodeAddr, nodeAddrSize);
    CIS_DIRECT_PROFILE_END_OPS(cis_scatter_indexed_atomic);
    return 0;
}

int Fam_CIS_Direct::gather_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize,
    uint64_t memoryServerId, uint32_t uid, uint32_t gid) {

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
        throw e;
    }

    memoryService->gather_indexed_atomic(regionId, offset, nElements,
                                         elementIndex, elementSize, key,
                                         srcBaseAddr, nodeAddr, nodeAddrSize);
    CIS_DIRECT_PROFILE_END_OPS(cis_gather_indexed_atomic);
    return 0;
}

inline uint64_t Fam_CIS_Direct::align_to_address(uint64_t size, int multiple) {
    assert(multiple && ((multiple & (multiple - 1)) == 0));
    return (size + multiple - 1) & -multiple;
}

// get and set controlpath address functions
string Fam_CIS_Direct::get_controlpath_addr() { return controlpath_addr; }

void Fam_CIS_Direct::set_controlpath_addr(string addr) {
    controlpath_addr = addr;
}

// get and set rpc_framework and protocol type
void Fam_CIS_Direct::set_rpc_framework_protocol(configFileParams file_options) {
    rpc_framework_type = file_options["rpc_framework_type"];
    rpc_protocol_type = protocol_map(file_options["provider"]);
}

string Fam_CIS_Direct::get_rpc_framework_type() { return rpc_framework_type; }

string Fam_CIS_Direct::get_rpc_protocol_type() { return rpc_protocol_type; }

} // namespace openfam
