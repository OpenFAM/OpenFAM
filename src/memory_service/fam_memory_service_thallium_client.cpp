/*
 * fam_memory_service_thallium_client.cpp
 * Copyright (c) 2023 Hewlett Packard Enterprise Development, LP. All
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

#include "fam_memory_service_thallium_client.h"
#include "cis/fam_cis_direct.h"
#include "common/atomic_queue.h"
#include "common/fam_config_info.h"
#include "common/fam_memserver_profile.h"
#include "fam_memory_service_client.h"
#include "fam_memory_service_direct.h"

#include <boost/atomic.hpp>

#include <chrono>
#include <iomanip>
#include <string.h>
#include <unistd.h>

#include "fam_memory_service_thallium_rpc_structures.h"
#include <fstream>
#include <iostream>
#include <string>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>

namespace tl = thallium;
using namespace std;

namespace openfam {
MEMSERVER_PROFILE_START(MEMORY_SERVICE_THALLIUM_CLIENT)
#ifdef MEMSERVER_PROFILE
#define MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()                     \
    {                                                                          \
        Profile_Time start = MEMORY_SERVICE_THALLIUM_CLIENT_get_time();

#define MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(apiIdx)                 \
    Profile_Time end = MEMORY_SERVICE_THALLIUM_CLIENT_get_time();              \
    Profile_Time total =                                                       \
        MEMORY_SERVICE_THALLIUM_CLIENT_time_diff_nanoseconds(start, end);      \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(MEMORY_SERVICE_THALLIUM_CLIENT,         \
                                       prof_##apiIdx, total)                   \
    }
#define MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_DUMP()                          \
    memory_service_thallium_client_profile_dump()
#else
#define MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()
#define MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(apiIdx)
#define MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_DUMP()
#endif

void memory_service_thallium_client_profile_dump() {
    MEMSERVER_PROFILE_END(MEMORY_SERVICE_THALLIUM_CLIENT);
    MEMSERVER_DUMP_PROFILE_BANNER(MEMORY_SERVICE_THALLIUM_CLIENT)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(MEMORY_SERVICE_THALLIUM_CLIENT, name,          \
                                prof_##name)
#include "memory_service/memory_service_thallium_client_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(MEMORY_SERVICE_THALLIUM_CLIENT, prof_##name)
#include "memory_service/memory_service_thallium_client_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(MEMORY_SERVICE_THALLIUM_CLIENT)
}

// thallium engine initialization
Fam_Memory_Service_Thallium_Client::Fam_Memory_Service_Thallium_Client(
    tl::engine engine_, const char *name, uint64_t port) {
    myEngine = engine_;
    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_THALLIUM_CLIENT)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_THALLIUM_CLIENT)
    connect(name, port);
}

// grpc client code to get controlpath address for thallium
char *Fam_Memory_Service_Thallium_Client::get_fabric_addr(const char *name,
                                                          uint64_t port) {
    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_CLIENT)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_CLIENT)
    std::ostringstream message;
    std::string name_s(name);
    name_s += ":" + std::to_string(port);

    /** Creating a channel and stub **/
    this->stub = Fam_Memory_Service_Rpc::NewStub(
        grpc::CreateChannel(name_s, ::grpc::InsecureChannelCredentials()));
    if (!stub) {
        message << "Fam Grpc Initialialization failed: stub is null";
        throw Memory_Service_Exception(FAM_ERR_RPC, message.str().c_str());
    }

    Fam_Memory_Service_General_Request req;
    Fam_Memory_Service_Start_Response res;

    ::grpc::ClientContext ctx;
    /** sending a start signal to server **/
    ::grpc::Status status = stub->signal_start(&ctx, req, &res);
    if (!status.ok()) {
        message << "Fam Memory Service: " << status.error_message().c_str()
                << ":" << name_s;
        throw Memory_Service_Exception(FAM_ERR_RPC, message.str().c_str());
    }

    Fam_Memory_Type memory_type = (Fam_Memory_Type)res.memorytype();
    memoryServerId = res.memserver_id();

    // retrieve fabric address
    size_t FabricAddrSize = res.addrnamelen();
    char *fabricAddr = (char *)calloc(1, FabricAddrSize);

    uint32_t lastBytes = 0;
    int lastBytesCount = (int)(FabricAddrSize % sizeof(uint32_t));
    int readCount = res.addrname_size();

    if (lastBytesCount > 0)
        readCount -= 1;

    for (int ndx = 0; ndx < readCount; ndx++) {
        *((uint32_t *)fabricAddr + ndx) = res.addrname(ndx);
    }

    if (lastBytesCount > 0) {
        lastBytes = res.rpc_addrname(readCount);
        memcpy(((uint32_t *)fabricAddr + readCount), &lastBytes,
               lastBytesCount);
    }
    memServerFabricAddrSize = FabricAddrSize;
    memServerFabricAddr = fabricAddr;
    memServermemType = memory_type;

    // retrieve thallium controlpath address
    size_t rpc_FabricAddrSize = res.rpc_addrnamelen();
    char *rpc_fabricAddr = (char *)calloc(1, rpc_FabricAddrSize);

    uint32_t rpc_lastBytes = 0;
    int rpc_lastBytesCount = (int)(rpc_FabricAddrSize % sizeof(uint32_t));
    int rpc_readCount = res.rpc_addrname_size();

    if (rpc_lastBytesCount > 0)
        rpc_readCount -= 1;

    for (int ndx = 0; ndx < rpc_readCount; ndx++) {
        *((uint32_t *)rpc_fabricAddr + ndx) = res.rpc_addrname(ndx);
    }

    if (rpc_lastBytesCount > 0) {
        rpc_lastBytes = res.rpc_addrname(rpc_readCount);
        memcpy(((uint32_t *)rpc_fabricAddr + rpc_readCount), &rpc_lastBytes,
               rpc_lastBytesCount);
    }
    return rpc_fabricAddr;
}
// end of grpc code

void Fam_Memory_Service_Thallium_Client::connect(const char *name,
                                                 uint64_t port) {
    // fetch fabric address and connect to server
    char *controlpath_addr = (char *)malloc(255);
    controlpath_addr = get_fabric_addr(name, port);
    server = myEngine.lookup(controlpath_addr);
    free(controlpath_addr);
    uint16_t provider_id = 10;
    tl::provider_handle ph_(server, provider_id);
    ph = ph_;

    // remote procedure definitions
    rp_reset_profile = myEngine.define("reset_profile");
    rp_dump_profile = myEngine.define("dump_profile");
    rp_create_region = myEngine.define("create_region");
    rp_destroy_region = myEngine.define("destroy_region");
    rp_resize_region = myEngine.define("resize_region");
    rp_allocate = myEngine.define("allocate");
    rp_deallocate = myEngine.define("deallocate");
    rp_copy = myEngine.define("copy");
    rp_backup = myEngine.define("backup");
    rp_restore = myEngine.define("restore");
    rp_get_backup_info = myEngine.define("get_backup_info");
    rp_list_backup = myEngine.define("list_backup");
    rp_delete_backup = myEngine.define("delete_backup");
    rp_get_local_pointer = myEngine.define("get_local_pointer");
    rp_register_region_memory = myEngine.define("register_region_memory");
    rp_get_region_memory = myEngine.define("get_region_memory");
    rp_get_dataitem_memory = myEngine.define("get_dataitem_memory");
    rp_acquire_CAS_lock = myEngine.define("acquire_CAS_lock");
    rp_release_CAS_lock = myEngine.define("release_CAS_lock");
    rp_get_atomic = myEngine.define("get_atomic");
    rp_put_atomic = myEngine.define("put_atomic");
    rp_scatter_strided_atomic = myEngine.define("scatter_strided_atomic");
    rp_gather_strided_atomic = myEngine.define("gather_strided_atomic");
    rp_scatter_indexed_atomic = myEngine.define("scatter_indexed_atomic");
    rp_gather_indexed_atomic = myEngine.define("gather_indexed_atomic");
    rp_update_memserver_addrlist = myEngine.define("update_memserver_addrlist");
    rp_open_region_with_registration =
        myEngine.define("open_region_with_registration");
    rp_open_region_without_registration =
        myEngine.define("open_region_without_registration");
    rp_close_region = myEngine.define("close_region");
    rp_create_region_failure_cleanup =
        myEngine.define("create_region_failure_cleanup");
}

Fam_Memory_Service_Thallium_Client::~Fam_Memory_Service_Thallium_Client() {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_DUMP();
}

void Fam_Memory_Service_Thallium_Client::reset_profile() {
    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_THALLIUM_CLIENT)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_THALLIUM_CLIENT)
    rp_reset_profile.on(ph)();
}

void Fam_Memory_Service_Thallium_Client::dump_profile() {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_DUMP();
    rp_dump_profile.on(ph)();
}

void Fam_Memory_Service_Thallium_Client::create_region(uint64_t regionId,
                                                       size_t nbytes) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);
    memRequest.set_size(nbytes);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_create_region.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_create_region);
}

void Fam_Memory_Service_Thallium_Client::destroy_region(
    uint64_t regionId, uint64_t *resourceStatus) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_destroy_region.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    *resourceStatus = memResponse.get_resource_status();
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_destroy_region);
}

void Fam_Memory_Service_Thallium_Client::resize_region(uint64_t regionId,
                                                       size_t nbytes) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);
    memRequest.set_size(nbytes);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_resize_region.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_resize_region);
}

Fam_Region_Item_Info
Fam_Memory_Service_Thallium_Client::allocate(uint64_t regionId, size_t nbytes) {
    Fam_Region_Item_Info info;
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);
    memRequest.set_size(nbytes);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_allocate.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)
    info.offset = memResponse.get_offset();

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_allocate);
    return info;
}

void Fam_Memory_Service_Thallium_Client::deallocate(uint64_t regionId,
                                                    uint64_t offset) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);
    memRequest.set_offset(offset);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_deallocate.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_deallocate);
}

void Fam_Memory_Service_Thallium_Client::copy(
    uint64_t srcRegionId, uint64_t *srcOffsets, uint64_t srcUsedMemsrvCnt,
    uint64_t srcCopyStart, uint64_t srcCopyEnd, uint64_t *srcKeys,
    uint64_t *srcBaseAddrList, uint64_t destRegionId, uint64_t destOffset,
    uint64_t destUsedMemsrvCnt, uint64_t *srcMemserverIds,
    uint64_t srcInterleaveSize, uint64_t destInterleaveSize, uint64_t size) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_src_offsets(srcOffsets, (int)srcUsedMemsrvCnt);
    memRequest.set_src_keys(srcKeys, (int)srcUsedMemsrvCnt);
    memRequest.set_src_base_addr_list(srcBaseAddrList, (int)srcUsedMemsrvCnt);
    memRequest.set_src_ids(srcMemserverIds, (int)srcUsedMemsrvCnt);
    memRequest.set_src_used_memsrv_cnt(srcUsedMemsrvCnt);
    memRequest.set_dest_used_memsrv_cnt(destUsedMemsrvCnt);
    memRequest.set_src_region_id(srcRegionId);
    memRequest.set_dest_region_id(destRegionId);
    memRequest.set_src_copy_start(srcCopyStart);
    memRequest.set_src_copy_end(srcCopyEnd);
    memRequest.set_dest_offset(destOffset);
    memRequest.set_size(size);
    memRequest.set_src_interleave_size(srcInterleaveSize);
    memRequest.set_dest_interleave_size(destInterleaveSize);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_copy.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(thallium_mem_client_copy);
}

void Fam_Memory_Service_Thallium_Client::backup(
    uint64_t srcRegionId, uint64_t srcOffset, uint64_t size, uint64_t chunkSize,
    uint64_t usedMemserverCnt, uint64_t fileStartPos, const string BackupName,
    uint32_t uid, uint32_t gid, mode_t mode, const string dataitemName,
    uint64_t itemSize, bool writeMetadata) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(srcRegionId);
    memRequest.set_offset(srcOffset);
    memRequest.set_bname(BackupName);
    memRequest.set_size(size);
    memRequest.set_chunk_size(chunkSize);
    memRequest.set_used_memserver_cnt(usedMemserverCnt);
    memRequest.set_file_start_pos(fileStartPos);
    memRequest.set_uid(uid);
    memRequest.set_gid(gid);
    memRequest.set_mode(mode);
    memRequest.set_diname(dataitemName);
    memRequest.set_item_size(itemSize);
    memRequest.set_write_metadata(writeMetadata);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_backup.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(thallium_mem_client_backup);
}

void Fam_Memory_Service_Thallium_Client::restore(
    uint64_t destRegionId, uint64_t destOffset, uint64_t size,
    uint64_t chunkSize, uint64_t usedMemserverCnt, uint64_t fileStartPos,
    string BackupName) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(destRegionId);
    memRequest.set_offset(destOffset);
    memRequest.set_bname(BackupName);
    memRequest.set_size(size);
    memRequest.set_chunk_size(chunkSize);
    memRequest.set_used_memserver_cnt(usedMemserverCnt);
    memRequest.set_file_start_pos(fileStartPos);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_restore.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(thallium_mem_client_restore);
}

Fam_Backup_Info Fam_Memory_Service_Thallium_Client::get_backup_info(
    std::string BackupName, uint32_t uid, uint32_t gid, uint32_t mode) {

    Fam_Backup_Info info;
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_bname(BackupName);
    memRequest.set_uid(uid);
    memRequest.set_gid(gid);
    memRequest.set_mode(mode);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_get_backup_info.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)
    info.size = memResponse.get_size();
    info.bname = (char *)(memResponse.get_name().c_str());
    info.uid = memResponse.get_uid();
    info.gid = memResponse.get_gid();
    info.mode = memResponse.get_mode();

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_get_backup_info);

    return info;
}

std::string Fam_Memory_Service_Thallium_Client::list_backup(
    std::string BackupName, uint32_t uid, uint32_t gid, mode_t mode) {

    std::string info;
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_bname(BackupName);
    memRequest.set_uid(uid);
    memRequest.set_gid(gid);
    memRequest.set_mode(mode);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_list_backup.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)
    info = memResponse.get_contents();

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_list_backup);

    return info;
}

void Fam_Memory_Service_Thallium_Client::delete_backup(std::string BackupName) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_bname(BackupName);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_delete_backup.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_delete_backup_info);
}

void Fam_Memory_Service_Thallium_Client::acquire_CAS_lock(uint64_t offset) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_offset(offset);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_acquire_CAS_lock.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_acquire_CAS_lock);
}

void Fam_Memory_Service_Thallium_Client::release_CAS_lock(uint64_t offset) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_offset(offset);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_release_CAS_lock.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_release_CAS_lock);
}

size_t Fam_Memory_Service_Thallium_Client::get_addr_size() {
    return memServerFabricAddrSize;
}

void *Fam_Memory_Service_Thallium_Client::get_addr() {
    return memServerFabricAddr;
}

Fam_Memory_Type Fam_Memory_Service_Thallium_Client::get_memtype() {
    return memServermemType;
}

void *Fam_Memory_Service_Thallium_Client::get_local_pointer(uint64_t regionId,
                                                            uint64_t offset) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);
    memRequest.set_offset(offset);
    memResponse = rp_get_local_pointer.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_get_local_pointer);

    return (void *)memResponse.get_base();
}

Fam_Region_Memory
Fam_Memory_Service_Thallium_Client::open_region_with_registration(
    uint64_t regionId, bool accessType) {
    Fam_Region_Memory regionMemory;
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);
    memRequest.set_rw_flag(accessType);

    Fam_Memory_Service_Thallium_Response memResponse =
        rp_open_region_with_registration.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    int numExtents = (int)memResponse.get_region_keys().size();
    for (int i = 0; i < numExtents; i++) {
        regionMemory.keys.push_back(memResponse.get_region_keys()[i]);
        regionMemory.base.push_back(memResponse.get_region_base()[i]);
    }

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        mem_open_region_with_registration);
    return regionMemory;
}

void Fam_Memory_Service_Thallium_Client::open_region_without_registration(
    uint64_t regionId) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);

    Fam_Memory_Service_Thallium_Response memResponse =
        rp_open_region_without_registration.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_open_region_without_registration);
}

uint64_t Fam_Memory_Service_Thallium_Client::close_region(uint64_t regionId) {
    uint64_t resourceStatus;
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);

    Fam_Memory_Service_Thallium_Response memResponse =
        rp_close_region.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    resourceStatus = memResponse.get_resource_status();

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_close_region);
    return resourceStatus;
}

void Fam_Memory_Service_Thallium_Client::register_region_memory(
    uint64_t regionId, bool accessType) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);
    memRequest.set_rw_flag(accessType);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_register_region_memory.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_register_region_memory);
}

Fam_Region_Memory
Fam_Memory_Service_Thallium_Client::get_region_memory(uint64_t regionId,
                                                      bool accessType) {
    Fam_Region_Memory regionMemory;
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);
    memRequest.set_rw_flag(accessType);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_get_region_memory.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    int numExtents = (int)memResponse.get_region_keys().size();
    for (int i = 0; i < numExtents; i++) {
        regionMemory.keys.push_back(memResponse.get_region_keys()[i]);
        regionMemory.base.push_back(memResponse.get_region_base()[i]);
    }

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_get_region_memory);
    return regionMemory;
}

Fam_Dataitem_Memory Fam_Memory_Service_Thallium_Client::get_dataitem_memory(
    uint64_t regionId, uint64_t offset, uint64_t size, bool accessType) {
    Fam_Dataitem_Memory dataitemMemory;
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId);
    memRequest.set_offset(offset);
    memRequest.set_size(size);
    memRequest.set_rw_flag(accessType);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_get_dataitem_memory.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    dataitemMemory.key = memResponse.get_key();
    dataitemMemory.base = memResponse.get_base();

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_get_dataitem_memory);

    return dataitemMemory;
}

void Fam_Memory_Service_Thallium_Client::get_atomic(
    uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset, uint64_t nbytes,
    uint64_t key, uint64_t srcBaseAddr, const char *nodeAddr,
    uint32_t nodeAddrSize) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId & REGIONID_MASK);
    memRequest.set_srcoffset(srcOffset);
    memRequest.set_dstoffset(dstOffset);
    memRequest.set_nbytes(nbytes);
    memRequest.set_key(key);
    memRequest.set_src_base_addr(srcBaseAddr);
    memRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    memRequest.set_nodeaddrsize(nodeAddrSize);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_get_atomic.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_get_atomic);
}

void Fam_Memory_Service_Thallium_Client::put_atomic(
    uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset, uint64_t nbytes,
    uint64_t key, uint64_t srcBaseAddr, const char *nodeAddr,
    uint32_t nodeAddrSize, const char *data) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId & REGIONID_MASK);
    memRequest.set_srcoffset(srcOffset);
    memRequest.set_dstoffset(dstOffset);
    memRequest.set_nbytes(nbytes);
    memRequest.set_key(key);
    memRequest.set_src_base_addr(srcBaseAddr);
    memRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    memRequest.set_nodeaddrsize(nodeAddrSize);
    if (nbytes <= MAX_DATA_IN_MSG)
        memRequest.set_data(data, (int)nbytes);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_put_atomic.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_put_atomic);
}

void Fam_Memory_Service_Thallium_Client::scatter_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId & REGIONID_MASK);
    memRequest.set_offset(offset);
    memRequest.set_nelements(nElements);
    memRequest.set_firstelement(firstElement);
    memRequest.set_stride(stride);
    memRequest.set_elementsize(elementSize);
    memRequest.set_key(key);
    memRequest.set_src_base_addr(srcBaseAddr);
    memRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    memRequest.set_nodeaddrsize(nodeAddrSize);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_scatter_strided_atomic.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_scatter_strided_atomic);
}

void Fam_Memory_Service_Thallium_Client::gather_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId & REGIONID_MASK);
    memRequest.set_offset(offset);
    memRequest.set_nelements(nElements);
    memRequest.set_firstelement(firstElement);
    memRequest.set_stride(stride);
    memRequest.set_elementsize(elementSize);
    memRequest.set_key(key);
    memRequest.set_src_base_addr(srcBaseAddr);
    memRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    memRequest.set_nodeaddrsize(nodeAddrSize);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_gather_strided_atomic.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_gather_strided_atomic);
}

void Fam_Memory_Service_Thallium_Client::scatter_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId & REGIONID_MASK);
    memRequest.set_offset(offset);
    memRequest.set_nelements(nElements);
    memRequest.set_elementindex((const char *)elementIndex,
                                (int)(strlen((char *)elementIndex)));
    memRequest.set_elementsize(elementSize);
    memRequest.set_key(key);
    memRequest.set_src_base_addr(srcBaseAddr);
    memRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    memRequest.set_nodeaddrsize(nodeAddrSize);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_scatter_indexed_atomic.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_scatter_indexed_atomic);
}

void Fam_Memory_Service_Thallium_Client::gather_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_region_id(regionId & REGIONID_MASK);
    memRequest.set_offset(offset);
    memRequest.set_nelements(nElements);
    memRequest.set_elementindex((const char *)elementIndex,
                                (int)(strlen((char *)elementIndex)));
    memRequest.set_elementsize(elementSize);
    memRequest.set_key(key);
    memRequest.set_src_base_addr(srcBaseAddr);
    memRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    memRequest.set_nodeaddrsize(nodeAddrSize);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_gather_indexed_atomic.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_gather_indexed_atomic);
}

void Fam_Memory_Service_Thallium_Client::update_memserver_addrlist(
    void *memServerInfoBuffer, size_t memServerInfoSize,
    uint64_t memoryServerCount) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;
    memRequest.set_memserverinfo_size(memServerInfoSize);
    memRequest.set_memserverinfo((char *)memServerInfoBuffer,
                                 (int)memServerInfoSize);
    memRequest.set_num_memservers(memoryServerCount);
    Fam_Memory_Service_Thallium_Response memResponse =
        rp_update_memserver_addrlist.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_client_update_memserver_addrlist);
}

void Fam_Memory_Service_Thallium_Client::create_region_failure_cleanup(
    uint64_t regionId) {
    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_START_OPS()

    Fam_Memory_Service_Thallium_Request memRequest;

    memRequest.set_region_id(regionId);

    Fam_Memory_Service_Thallium_Response memResponse =
        rp_create_region_failure_cleanup.on(ph)(memRequest);
    RPC_STATUS_CHECK(Memory_Service_Exception, memResponse)

    MEMORY_SERVICE_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_mem_create_region_failure_cleanup);
}

} // namespace openfam
