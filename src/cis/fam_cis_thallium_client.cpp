/*
 * fam_cis_thallium_client.cpp
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

#include "cis/fam_cis_client.h"
#include "cis/fam_cis_direct.h"
#include "cis/fam_cis_thallium_client.h"
#include "cis/fam_cis_thallium_rpc_structures.h"
#include "common/atomic_queue.h"
#include "common/fam_config_info.h"
#include "common/fam_memserver_profile.h"

#include <boost/atomic.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string.h>
#include <string>
#include <thallium.hpp>
#include <thallium/async_response.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <unistd.h>

namespace tl = thallium;
using namespace std;
namespace openfam {

// thallium engine initialization
Fam_CIS_Thallium_Client::Fam_CIS_Thallium_Client(tl::engine engine_,
                                                 const char *name,
                                                 uint64_t port) {
    myEngine = engine_;
    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_THALLIUM_CLIENT)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_THALLIUM_CLIENT)
    connect(name, port);
}

// grpc client code to get controlpath address for thallium
char *Fam_CIS_Thallium_Client::get_fabric_addr(const char *name,
                                               uint64_t port) {
    std::ostringstream message;
    std::string name_s(name);
    name_s += ":" + std::to_string(port);

    /** Creating a channel and stub **/
    this->stub = Fam_CIS_Rpc::NewStub(
        grpc::CreateChannel(name_s, ::grpc::InsecureChannelCredentials()));
    if (!stub) {
        message << "Fam Grpc Initialialization failed: stub is null";
        throw CIS_Exception(FAM_ERR_RPC, message.str().c_str());
    }

    Fam_Request req;
    Fam_Response res;
    ::grpc::ClientContext ctx;

    /** sending a start signal to server **/
    ::grpc::Status status = stub->signal_start(&ctx, req, &res);
    if (!status.ok()) {
        message << "Fam CIS: " << status.error_message().c_str() << ":"
                << name_s;
        throw CIS_Exception(FAM_ERR_RPC, message.str().c_str());
    }

    // retrieve thallium controlpath address
    size_t thal_FabricAddrSize = res.rpc_addrnamelen();
    char *thal_fabricAddr = (char *)calloc(1, thal_FabricAddrSize);

    uint32_t thal_lastBytes = 0;
    int thal_lastBytesCount = (int)(thal_FabricAddrSize % sizeof(uint32_t));
    int thal_readCount = res.rpc_addrname_size();

    if (thal_lastBytesCount > 0)
        thal_readCount -= 1;

    for (int ndx = 0; ndx < thal_readCount; ndx++) {
        *((uint32_t *)thal_fabricAddr + ndx) = res.rpc_addrname(ndx);
    }

    if (thal_lastBytesCount > 0) {
        thal_lastBytes = res.rpc_addrname(thal_readCount);
        memcpy(((uint32_t *)thal_fabricAddr + thal_readCount), &thal_lastBytes,
               thal_lastBytesCount);
    }
    return thal_fabricAddr;
}
// end of grpc code

void Fam_CIS_Thallium_Client::connect(const char *name, uint64_t port) {
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
    rp_get_num_memory_servers = myEngine.define("get_num_memory_servers");
    rp_create_region = myEngine.define("create_region");
    rp_destroy_region = myEngine.define("destroy_region");
    rp_resize_region = myEngine.define("resize_region");
    rp_allocate = myEngine.define("allocate");
    rp_deallocate = myEngine.define("deallocate");
    rp_change_region_permission = myEngine.define("change_region_permission");
    rp_change_dataitem_permission =
        myEngine.define("change_dataitem_permission");
    rp_lookup_region = myEngine.define("lookup_region");
    rp_lookup = myEngine.define("lookup");
    rp_check_permission_get_region_info =
        myEngine.define("check_permission_get_region_info");
    rp_check_permission_get_item_info =
        myEngine.define("check_permission_get_item_info");
    rp_get_stat_info = myEngine.define("get_stat_info");
    rp_copy = myEngine.define("copy");
    rp_backup = myEngine.define("backup");
    rp_restore = myEngine.define("restore");
    rp_acquire_CAS_lock = myEngine.define("acquire_CAS_lock");
    rp_release_CAS_lock = myEngine.define("release_CAS_lock");
    rp_get_backup_info = myEngine.define("get_backup_info");
    rp_list_backup = myEngine.define("list_backup");
    rp_delete_backup = myEngine.define("delete_backup");
    rp_get_memserverinfo_size = myEngine.define("get_memserverinfo_size");
    rp_get_memserverinfo = myEngine.define("get_memserverinfo");
    rp_get_atomic = myEngine.define("get_atomic");
    rp_put_atomic = myEngine.define("put_atomic");
    rp_scatter_strided_atomic = myEngine.define("scatter_strided_atomic");
    rp_gather_strided_atomic = myEngine.define("gather_strided_atomic");
    rp_scatter_indexed_atomic = myEngine.define("scatter_indexed_atomic");
    rp_gather_indexed_atomic = myEngine.define("gather_indexed_atomic");
    rp_get_region_memory = myEngine.define("get_region_memory");
    rp_open_region_with_registration =
        myEngine.define("open_region_with_registration");
    rp_open_region_without_registration =
        myEngine.define("open_region_without_registration");
    rp_close_region = myEngine.define("close_region");
}

Fam_CIS_Thallium_Client::~Fam_CIS_Thallium_Client() {}

void Fam_CIS_Thallium_Client::reset_profile() { rp_reset_profile.on(ph)(); }

void Fam_CIS_Thallium_Client::dump_profile() { rp_dump_profile.on(ph)(); }

uint64_t Fam_CIS_Thallium_Client::get_num_memory_servers() {

    Fam_CIS_Thallium_Request cisRequest;

    Fam_CIS_Thallium_Response cisResponse =
        rp_get_num_memory_servers.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
    return cisResponse.get_num_memory_server();
}

Fam_Region_Item_Info Fam_CIS_Thallium_Client::create_region(
    string name, size_t nbytes, mode_t permission,
    Fam_Region_Attributes *regionAttributes, uint32_t uid, uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_name(name);
    cisRequest.set_size(nbytes);
    cisRequest.set_perm(permission);
    cisRequest.set_redundancylevel(regionAttributes->redundancyLevel);
    cisRequest.set_memorytype(regionAttributes->memoryType);
    cisRequest.set_interleaveenable(regionAttributes->interleaveEnable);
    cisRequest.set_permissionlevel(regionAttributes->permissionLevel);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);

    Fam_CIS_Thallium_Response cisResponse = rp_create_region.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
    Fam_Region_Item_Info info;
    info.regionId = cisResponse.get_regionid();
    info.offset = cisResponse.get_offset();
    return info;
}

void Fam_CIS_Thallium_Client::destroy_region(uint64_t regionId,
                                             uint64_t memoryServerId,
                                             uint32_t uid, uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    cisRequest.set_memserver_id(memoryServerId);

    Fam_CIS_Thallium_Response cisResponse =
        rp_destroy_region.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}

void Fam_CIS_Thallium_Client::resize_region(uint64_t regionId, size_t nbytes,
                                            uint64_t memoryServerId,
                                            uint32_t uid, uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    cisRequest.set_size(nbytes);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    cisRequest.set_memserver_id(memoryServerId);

    Fam_CIS_Thallium_Response cisResponse = rp_resize_region.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}

Fam_Region_Item_Info
Fam_CIS_Thallium_Client::allocate(string name, size_t nbytes, mode_t permission,
                                  uint64_t regionId, uint64_t memoryServerId,
                                  uint32_t uid, uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_name(name);
    cisRequest.set_regionid(regionId);
    cisRequest.set_size(nbytes);
    cisRequest.set_perm(permission);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    cisRequest.set_memserver_id(memoryServerId);

    Fam_CIS_Thallium_Response cisResponse = rp_allocate.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
    Fam_Region_Item_Info info;
    info.regionId = cisResponse.get_regionid();
    info.used_memsrv_cnt = cisResponse.get_memsrv_list().size();
    info.interleaveSize = cisResponse.get_interleave_size();
    info.permissionLevel =
        (Fam_Permission_Level)cisResponse.get_permission_level();
    for (uint64_t i = 0; i < info.used_memsrv_cnt; i++) {
        info.memoryServerIds[i] = cisResponse.get_memsrv_list()[(int)i];
    }

    if (info.permissionLevel == REGION) {
        for (uint64_t i = 0; i < info.used_memsrv_cnt; i++) {
            info.dataitemOffsets[i] = cisResponse.get_offsets()[(int)i];
        }
    } else {
        if (cisResponse.get_item_registration_status()) {
            for (uint64_t i = 0; i < info.used_memsrv_cnt; i++) {
                info.dataitemKeys[i] = cisResponse.get_keys()[(int)i];
                info.baseAddressList[i] =
                    cisResponse.get_base_addr_list()[(int)i];
            }
        }
        info.itemRegistrationStatus =
            cisResponse.get_item_registration_status();
        int j = 0;
        info.dataitemOffsets[0] = cisResponse.get_offsets()[j];
    }
    info.perm = (mode_t)cisResponse.get_perm();
    return info;
}

void Fam_CIS_Thallium_Client::deallocate(uint64_t regionId, uint64_t offset,
                                         uint64_t memoryServerId, uint32_t uid,
                                         uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    cisRequest.set_offset(offset);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    cisRequest.set_memserver_id(memoryServerId);

    Fam_CIS_Thallium_Response cisResponse = rp_deallocate.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}

void Fam_CIS_Thallium_Client::change_region_permission(uint64_t regionId,
                                                       mode_t permission,
                                                       uint64_t memoryServerId,
                                                       uint32_t uid,
                                                       uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    cisRequest.set_perm((uint64_t)permission);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    cisRequest.set_memserver_id(memoryServerId);

    Fam_CIS_Thallium_Response cisResponse =
        rp_change_region_permission.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}

void Fam_CIS_Thallium_Client::change_dataitem_permission(
    uint64_t regionId, uint64_t offset, mode_t permission,
    uint64_t memoryServerId, uint32_t uid, uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    cisRequest.set_offset(offset);
    cisRequest.set_perm((uint64_t)permission);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    cisRequest.set_memserver_id(memoryServerId);

    Fam_CIS_Thallium_Response cisResponse =
        rp_change_dataitem_permission.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}

Fam_Region_Item_Info Fam_CIS_Thallium_Client::lookup_region(string name,
                                                            uint32_t uid,
                                                            uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_name(name);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);

    Fam_CIS_Thallium_Response cisResponse = rp_lookup_region.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)

    Fam_Region_Item_Info info;
    info.regionId = cisResponse.get_regionid();
    info.offset = cisResponse.get_offset();
    info.size = cisResponse.get_size();
    info.perm = (mode_t)cisResponse.get_perm();
    strncpy(info.name, (cisResponse.get_name()).c_str(),
            cisResponse.get_maxnamelen());
    info.redundancyLevel =
        (Fam_Redundancy_Level)cisResponse.get_redundancylevel();
    info.memoryType = (Fam_Memory_Type)cisResponse.get_memorytype();
    info.interleaveEnable =
        (Fam_Interleave_Enable)cisResponse.get_interleaveenable();
    info.permissionLevel =
        (Fam_Permission_Level)cisResponse.get_permissionlevel();
    return info;
}

Fam_Region_Item_Info Fam_CIS_Thallium_Client::lookup(string itemName,
                                                     string regionName,
                                                     uint32_t uid,
                                                     uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_name(itemName);
    cisRequest.set_regionname(regionName);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);

    Fam_CIS_Thallium_Response cisResponse = rp_lookup.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)

    Fam_Region_Item_Info info;
    info.regionId = cisResponse.get_regionid();
    info.size = cisResponse.get_size();
    info.perm = (mode_t)cisResponse.get_perm();
    info.used_memsrv_cnt = cisResponse.get_memsrv_list().size();
    strncpy(info.name, (cisResponse.get_name()).c_str(),
            cisResponse.get_maxnamelen());
    info.offset = cisResponse.get_offset();
    info.uid = cisResponse.get_uid();
    info.gid = cisResponse.get_gid();
    info.interleaveSize = cisResponse.get_interleave_size();
    info.permissionLevel =
        (Fam_Permission_Level)cisResponse.get_permissionlevel();
    for (uint64_t i = 0; i < info.used_memsrv_cnt; i++) {
        info.memoryServerIds[i] = cisResponse.get_memsrv_list()[(int)i];
    }
    return info;
}

Fam_Region_Item_Info Fam_CIS_Thallium_Client::check_permission_get_region_info(
    uint64_t regionId, uint64_t memoryServerId, uint32_t uid, uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    cisRequest.set_gid(gid);
    cisRequest.set_uid(uid);
    cisRequest.set_memserver_id(memoryServerId);

    Fam_CIS_Thallium_Response cisResponse =
        rp_check_permission_get_region_info.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)

    Fam_Region_Item_Info info;
    info.size = cisResponse.get_size();
    info.perm = (mode_t)cisResponse.get_perm();
    info.uid = cisResponse.get_uid();
    info.gid = cisResponse.get_gid();
    info.redundancyLevel =
        (Fam_Redundancy_Level)cisResponse.get_redundancylevel();
    info.memoryType = (Fam_Memory_Type)cisResponse.get_memorytype();
    info.interleaveEnable =
        (Fam_Interleave_Enable)cisResponse.get_interleaveenable();
    info.interleaveSize = cisResponse.get_interleavesize();
    info.permissionLevel =
        (Fam_Permission_Level)cisResponse.get_permissionlevel();
    info.used_memsrv_cnt = cisResponse.get_memsrv_list().size();
    for (uint64_t i = 0; i < info.used_memsrv_cnt; i++) {
        info.memoryServerIds[i] = cisResponse.get_memsrv_list()[(int)i];
    }
    strncpy(info.name, (cisResponse.get_name()).c_str(),
            cisResponse.get_maxnamelen());
    return info;
}

Fam_Region_Item_Info Fam_CIS_Thallium_Client::check_permission_get_item_info(
    uint64_t regionId, uint64_t offset, uint64_t memoryServerId, uint32_t uid,
    uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    cisRequest.set_offset(offset);
    cisRequest.set_gid(gid);
    cisRequest.set_uid(uid);
    cisRequest.set_memserver_id(memoryServerId);

    Fam_CIS_Thallium_Response cisResponse =
        rp_check_permission_get_item_info.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)

    Fam_Region_Item_Info info;
    info.base = cisResponse.get_base();
    info.size = cisResponse.get_size();
    info.perm = (mode_t)cisResponse.get_perm();
    info.used_memsrv_cnt = cisResponse.get_memsrv_list().size();
    strncpy(info.name, (cisResponse.get_name()).c_str(),
            cisResponse.get_maxnamelen());
    info.offset = cisResponse.get_offset();
    info.interleaveSize = cisResponse.get_interleave_size();
    info.permissionLevel =
        (Fam_Permission_Level)cisResponse.get_permission_level();
    for (uint64_t i = 0; i < info.used_memsrv_cnt; i++) {
        info.memoryServerIds[i] = cisResponse.get_memsrv_list()[(int)i];
    }

    if (info.permissionLevel == REGION) {
        for (uint64_t i = 0; i < info.used_memsrv_cnt; i++) {
            info.dataitemOffsets[i] = cisResponse.get_offsets()[(int)i];
        }
    } else {
        for (uint64_t i = 0; i < info.used_memsrv_cnt; i++) {
            info.dataitemKeys[i] = cisResponse.get_keys()[(int)i];
            info.baseAddressList[i] = cisResponse.get_base_addr_list()[(int)i];
        }

        info.itemRegistrationStatus =
            cisResponse.get_item_registration_status();
        info.dataitemOffsets[0] = cisResponse.get_offset();
    }
    return info;
}

Fam_Region_Item_Info
Fam_CIS_Thallium_Client::get_stat_info(uint64_t regionId, uint64_t offset,
                                       uint64_t memoryServerId, uint32_t uid,
                                       uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    cisRequest.set_offset(offset);
    cisRequest.set_gid(gid);
    cisRequest.set_uid(uid);
    cisRequest.set_memserver_id(memoryServerId);

    Fam_CIS_Thallium_Response cisResponse = rp_get_stat_info.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)

    Fam_Region_Item_Info info;
    info.size = cisResponse.get_size();
    info.perm = (mode_t)cisResponse.get_perm();
    info.uid = cisResponse.get_uid();
    info.gid = cisResponse.get_gid();
    info.used_memsrv_cnt = cisResponse.get_memsrv_list().size();
    info.interleaveSize = cisResponse.get_interleave_size();
    info.permissionLevel =
        (Fam_Permission_Level)cisResponse.get_permission_level();
    for (uint64_t i = 0; i < info.used_memsrv_cnt; i++) {
        info.memoryServerIds[i] = cisResponse.get_memsrv_list()[(int)i];
    }
    strncpy(info.name, (cisResponse.get_name()).c_str(),
            cisResponse.get_maxnamelen());
    return info;
}

void *Fam_CIS_Thallium_Client::copy(
    uint64_t srcRegionId, uint64_t srcOffset, uint64_t srcUsedMemsrvCnt,
    uint64_t srcCopyStart, uint64_t *srcKeys, uint64_t *srcBaseAddrList,
    uint64_t destRegionId, uint64_t destOffset, uint64_t destCopyStart,
    uint64_t size, uint64_t firstSrcMemserverId, uint64_t firstDestMemserverId,
    uint32_t uid, uint32_t gid) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_src_region_id(srcRegionId);
    cisRequest.set_dest_region_id(destRegionId);
    cisRequest.set_src_offset(srcOffset);
    cisRequest.set_dest_offset(destOffset);
    cisRequest.set_src_copy_start(srcCopyStart);
    cisRequest.set_dest_copy_start(destCopyStart);
    cisRequest.set_gid(gid);
    cisRequest.set_uid(uid);
    cisRequest.set_copy_size(size);
    cisRequest.set_first_src_memsrv_id(firstSrcMemserverId);
    cisRequest.set_first_dest_memsrv_id(firstDestMemserverId);
    cisRequest.set_src_used_memsrv_cnt(srcUsedMemsrvCnt);
    cisRequest.set_src_keys(srcKeys, (int)srcUsedMemsrvCnt);
    cisRequest.set_src_base_addr(srcBaseAddrList, (int)srcUsedMemsrvCnt);

    auto my_async_response_ptr =
        new tl::async_response(rp_copy.on(ph).async(cisRequest));
    void *waitObj = static_cast<void *>(my_async_response_ptr);
    return waitObj;
}

void Fam_CIS_Thallium_Client::wait_for_copy(void *waitObj) {

    tl::async_response *waitObjIn = static_cast<tl::async_response *>(waitObj);

    if (!waitObjIn) {
        throw CIS_Exception(FAM_ERR_INVALID, "Copy waitObj is null");
    }

    Fam_CIS_Thallium_Response cisResponse = waitObjIn->wait();
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}

void *Fam_CIS_Thallium_Client::backup(uint64_t srcRegionId, uint64_t srcOffset,
                                      uint64_t srcMemoryServerId,
                                      string BackupName, uint32_t uid,
                                      uint32_t gid) {

    Fam_Backup_Restore_Request req;
    Fam_Backup_Restore_Response res;
    Fam_CIS_Thallium_Request cisRequest;
    cisRequest.set_regionid(srcRegionId);
    cisRequest.set_offset(srcOffset);
    cisRequest.set_memserver_id(srcMemoryServerId);
    cisRequest.set_bname(BackupName);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);

    auto my_async_response_ptr =
        new tl::async_response(rp_backup.on(ph).async(cisRequest));
    void *waitObj = static_cast<void *>(my_async_response_ptr);
    return waitObj;
}

void Fam_CIS_Thallium_Client::wait_for_backup(void *waitObj) {

    tl::async_response *waitObjIn = static_cast<tl::async_response *>(waitObj);

    if (!waitObjIn) {
        throw CIS_Exception(FAM_ERR_INVALID, "Copy waitObj is null");
    }

    Fam_CIS_Thallium_Response cisResponse = waitObjIn->wait();
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}

void *Fam_CIS_Thallium_Client::restore(uint64_t destRegionId,
                                       uint64_t destOffset,
                                       uint64_t destMemoryServerId,
                                       string BackupName, uint32_t uid,
                                       uint32_t gid) {
    Fam_Backup_Restore_Request req;
    Fam_Backup_Restore_Response res;
    Fam_CIS_Thallium_Request cisRequest;
    cisRequest.set_regionid(destRegionId);
    cisRequest.set_memserver_id(destMemoryServerId);
    cisRequest.set_offset(destOffset);
    cisRequest.set_bname(BackupName);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);

    auto my_async_response_ptr =
        new tl::async_response(rp_restore.on(ph).async(cisRequest));
    void *waitObj = static_cast<void *>(my_async_response_ptr);
    return waitObj;
}

void Fam_CIS_Thallium_Client::wait_for_restore(void *waitObj) {

    tl::async_response *waitObjIn = static_cast<tl::async_response *>(waitObj);

    if (!waitObjIn) {
        throw CIS_Exception(FAM_ERR_INVALID, "Copy waitObj is null");
    }

    Fam_CIS_Thallium_Response cisResponse = waitObjIn->wait();
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}

string Fam_CIS_Thallium_Client::list_backup(string BackupName,
                                            uint64_t memoryServerId,
                                            uint32_t uid, uint32_t gid) {
    Fam_Backup_List_Request req;
    Fam_Backup_List_Response res;
    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_bname(BackupName);
    cisRequest.set_memserver_id(memoryServerId);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);

    Fam_CIS_Thallium_Response cisResponse = rp_list_backup.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
    string info = cisResponse.get_contents();
    return info;
}

void *Fam_CIS_Thallium_Client::delete_backup(string BackupName,
                                             uint64_t memoryServerId,
                                             uint32_t uid, uint32_t gid) {
    Fam_Backup_List_Request req;
    Fam_Backup_List_Response res;
    Fam_CIS_Thallium_Request cisRequest;
    cisRequest.set_bname(BackupName);
    cisRequest.set_memserver_id(memoryServerId);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);

    auto my_async_response_ptr =
        new tl::async_response(rp_delete_backup.on(ph).async(cisRequest));
    void *waitObj = static_cast<void *>(my_async_response_ptr);
    return waitObj;
}

void Fam_CIS_Thallium_Client::wait_for_delete_backup(void *waitObj) {

    tl::async_response *waitObjIn = static_cast<tl::async_response *>(waitObj);

    if (!waitObjIn) {
        throw CIS_Exception(FAM_ERR_INVALID, "Copy waitObj is null");
    }

    Fam_CIS_Thallium_Response cisResponse = waitObjIn->wait();
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}

Fam_Backup_Info
Fam_CIS_Thallium_Client::get_backup_info(std::string BackupName,
                                         uint64_t memoryServerId, uint32_t uid,
                                         uint32_t gid) {
    Fam_Backup_Info_Request req;
    Fam_Backup_Info_Response res;
    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_memserver_id(memoryServerId);
    cisRequest.set_bname(BackupName);
    Fam_CIS_Thallium_Response cisResponse =
        rp_get_backup_info.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
    Fam_Backup_Info info;
    info.bname = (char *)(cisResponse.get_name().c_str());
    info.size = cisResponse.get_size();
    info.uid = cisResponse.get_uid();
    info.gid = cisResponse.get_gid();
    info.mode = cisResponse.get_mode();

    return info;
}

void *Fam_CIS_Thallium_Client::fam_map(uint64_t regionId, uint64_t offset,
                                       uint64_t memoryServerId, uint32_t uid,
                                       uint32_t gid) {
    FAM_UNIMPLEMENTED_MEMSRVMODEL();
    return NULL;
}

void Fam_CIS_Thallium_Client::fam_unmap(void *local, uint64_t regionId,
                                        uint64_t offset,
                                        uint64_t memoryServerId, uint32_t uid,
                                        uint32_t gid) {
    FAM_UNIMPLEMENTED_MEMSRVMODEL();
}

void Fam_CIS_Thallium_Client::acquire_CAS_lock(uint64_t offset,
                                               uint64_t memoryServerId) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_offset(offset);
    cisRequest.set_memserver_id(memoryServerId);

    Fam_CIS_Thallium_Response cisResponse =
        rp_acquire_CAS_lock.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}

void Fam_CIS_Thallium_Client::release_CAS_lock(uint64_t offset,
                                               uint64_t memoryServerId) {

    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_offset(offset);
    cisRequest.set_memserver_id(memoryServerId);
    Fam_CIS_Thallium_Response cisResponse =
        rp_release_CAS_lock.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}

size_t Fam_CIS_Thallium_Client::get_memserverinfo_size() {
    Fam_CIS_Thallium_Request cisRequest;

    Fam_CIS_Thallium_Response cisResponse =
        rp_get_memserverinfo_size.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)

    return (size_t)cisResponse.get_memserverinfo_size();
}

void Fam_CIS_Thallium_Client::get_memserverinfo(void *memServerInfoBuffer) {
    Fam_Request req;
    Fam_Memserverinfo_Response res;
    Fam_CIS_Thallium_Request cisRequest;

    Fam_CIS_Thallium_Response cisResponse =
        rp_get_memserverinfo.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)

    // copy memserverinfo into the buffer
    memcpy(memServerInfoBuffer, (void *)cisResponse.get_memserverinfo().c_str(),
           cisResponse.get_memserverinfo_size());
}

int Fam_CIS_Thallium_Client::get_atomic(uint64_t regionId, uint64_t srcOffset,
                                        uint64_t dstOffset, uint64_t nbytes,
                                        uint64_t key, uint64_t srcBaseAddr,
                                        const char *nodeAddr,
                                        uint32_t nodeAddrSize,
                                        uint64_t memoryServerId, uint32_t uid,
                                        uint32_t gid) {
    Fam_Atomic_Get_Request req;
    Fam_Atomic_Response res;
    Fam_CIS_Thallium_Request cisRequest;
    cisRequest.set_regionid(regionId & REGIONID_MASK);
    cisRequest.set_srcoffset(srcOffset);
    cisRequest.set_dstoffset(dstOffset);
    cisRequest.set_nbytes(nbytes);
    cisRequest.set_key(key);
    cisRequest.set_srcbaseaddr(srcBaseAddr);
    cisRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    cisRequest.set_nodeaddrsize(nodeAddrSize);
    cisRequest.set_memserver_id(memoryServerId);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    Fam_CIS_Thallium_Response cisResponse = rp_get_atomic.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
    return 0;
}

int Fam_CIS_Thallium_Client::put_atomic(uint64_t regionId, uint64_t srcOffset,
                                        uint64_t dstOffset, uint64_t nbytes,
                                        uint64_t key, uint64_t srcBaseAddr,
                                        const char *nodeAddr,
                                        uint32_t nodeAddrSize, const char *data,
                                        uint64_t memoryServerId, uint32_t uid,
                                        uint32_t gid) {
    Fam_Atomic_Put_Request req;
    Fam_Atomic_Response res;
    Fam_CIS_Thallium_Request cisRequest;
    cisRequest.set_regionid(regionId & REGIONID_MASK);
    cisRequest.set_srcoffset(srcOffset);
    cisRequest.set_dstoffset(dstOffset);
    cisRequest.set_nbytes(nbytes);
    cisRequest.set_key(key);
    cisRequest.set_srcbaseaddr(srcBaseAddr);
    cisRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    if (nbytes <= MAX_DATA_IN_MSG)
        cisRequest.set_data(data, (int)nbytes);
    cisRequest.set_nodeaddrsize(nodeAddrSize);
    cisRequest.set_memserver_id(memoryServerId);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    Fam_CIS_Thallium_Response cisResponse = rp_put_atomic.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
    return 0;
}

int Fam_CIS_Thallium_Client::scatter_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize,
    uint64_t memoryServerId, uint32_t uid, uint32_t gid) {
    Fam_Atomic_SG_Strided_Request req;
    Fam_Atomic_Response res;
    Fam_CIS_Thallium_Request cisRequest;
    cisRequest.set_regionid(regionId & REGIONID_MASK);
    cisRequest.set_offset(offset);
    cisRequest.set_nelements(nElements);
    cisRequest.set_firstelement(firstElement);
    cisRequest.set_stride(stride);
    cisRequest.set_elementsize(elementSize);
    cisRequest.set_key(key);
    cisRequest.set_srcbaseaddr(srcBaseAddr);
    cisRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    cisRequest.set_nodeaddrsize(nodeAddrSize);
    cisRequest.set_memserver_id(memoryServerId);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    Fam_CIS_Thallium_Response cisResponse =
        rp_scatter_strided_atomic.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
    return 0;
}

int Fam_CIS_Thallium_Client::gather_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize,
    uint64_t memoryServerId, uint32_t uid, uint32_t gid) {
    Fam_Atomic_SG_Strided_Request req;
    Fam_Atomic_Response res;
    Fam_CIS_Thallium_Request cisRequest;
    cisRequest.set_regionid(regionId & REGIONID_MASK);
    cisRequest.set_offset(offset);
    cisRequest.set_nelements(nElements);
    cisRequest.set_firstelement(firstElement);
    cisRequest.set_stride(stride);
    cisRequest.set_elementsize(elementSize);
    cisRequest.set_key(key);
    cisRequest.set_srcbaseaddr(srcBaseAddr);
    cisRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    cisRequest.set_nodeaddrsize(nodeAddrSize);
    cisRequest.set_memserver_id(memoryServerId);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    Fam_CIS_Thallium_Response cisResponse =
        rp_gather_strided_atomic.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
    return 0;
}

int Fam_CIS_Thallium_Client::scatter_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize,
    uint64_t memoryServerId, uint32_t uid, uint32_t gid) {
    Fam_Atomic_SG_Indexed_Request req;
    Fam_Atomic_Response res;
    Fam_CIS_Thallium_Request cisRequest;
    cisRequest.set_regionid(regionId & REGIONID_MASK);
    cisRequest.set_offset(offset);
    cisRequest.set_nelements(nElements);
    cisRequest.set_elementindex((const char *)elementIndex,
                                (int)(strlen((char *)elementIndex)));
    cisRequest.set_elementsize(elementSize);
    cisRequest.set_key(key);
    cisRequest.set_srcbaseaddr(srcBaseAddr);
    cisRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    cisRequest.set_nodeaddrsize(nodeAddrSize);
    cisRequest.set_memserver_id(memoryServerId);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    Fam_CIS_Thallium_Response cisResponse =
        rp_scatter_indexed_atomic.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
    return 0;
}

int Fam_CIS_Thallium_Client::gather_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize,
    uint64_t memoryServerId, uint32_t uid, uint32_t gid) {
    Fam_Atomic_SG_Indexed_Request req;
    Fam_Atomic_Response res;
    Fam_CIS_Thallium_Request cisRequest;
    cisRequest.set_regionid(regionId & REGIONID_MASK);
    cisRequest.set_offset(offset);
    cisRequest.set_nelements(nElements);
    cisRequest.set_elementindex((const char *)elementIndex,
                                (int)(strlen((char *)elementIndex)));
    cisRequest.set_elementsize(elementSize);
    cisRequest.set_key(key);
    cisRequest.set_srcbaseaddr(srcBaseAddr);
    cisRequest.set_nodeaddr(nodeAddr, (int)nodeAddrSize);
    cisRequest.set_nodeaddrsize(nodeAddrSize);
    cisRequest.set_memserver_id(memoryServerId);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);
    Fam_CIS_Thallium_Response cisResponse =
        rp_gather_indexed_atomic.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
    return 0;
}

void Fam_CIS_Thallium_Client::get_region_memory(
    uint64_t regionId, uint32_t uid, uint32_t gid,
    Fam_Region_Memory_Map *regionMemoryMap) {
    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);

    Fam_CIS_Thallium_Response cisResponse =
        rp_get_region_memory.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)

    for (int i = 0; i < (int)cisResponse.get_region_key_map().size(); i++) {
        ::Fam_CIS_Thallium_Response::Region_Key_Map regionKeyMap =
            cisResponse.get_region_key_map()[i];
        std::vector<uint64_t> keyVector;
        std::vector<uint64_t> baseVector;
        for (int j = 0; j < (int)regionKeyMap.get_region_keys().size(); j++) {
            keyVector.push_back(regionKeyMap.get_region_keys()[j]);
            baseVector.push_back(regionKeyMap.get_region_base()[j]);
        }
        Fam_Region_Memory regionMemory;
        regionMemory.keys = keyVector;
        regionMemory.base = baseVector;
        regionMemoryMap->insert(
            {regionKeyMap.get_memserver_id(), regionMemory});
    }
}

void Fam_CIS_Thallium_Client::open_region_with_registration(
    uint64_t regionId, uint32_t uid, uint32_t gid,
    std::vector<uint64_t> *memserverIds,
    Fam_Region_Memory_Map *regionMemoryMap) {
    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    cisRequest.set_uid(uid);
    cisRequest.set_gid(gid);

    Fam_CIS_Thallium_Response cisResponse =
        rp_open_region_with_registration.on(ph)(cisRequest);
    RPC_STATUS_CHECK(CIS_Exception, cisResponse)

    for (int i = 0; i < (int)cisResponse.get_region_key_map().size(); i++) {
        ::Fam_CIS_Thallium_Response::Region_Key_Map regionKeyMap =
            cisResponse.get_region_key_map()[i];
        std::vector<uint64_t> keyVector;
        std::vector<uint64_t> baseVector;
        for (int j = 0; j < (int)regionKeyMap.get_region_keys().size(); j++) {
            keyVector.push_back(regionKeyMap.get_region_keys()[j]);
            baseVector.push_back(regionKeyMap.get_region_base()[j]);
        }
        Fam_Region_Memory regionMemory;
        regionMemory.keys = keyVector;
        regionMemory.base = baseVector;
        regionMemoryMap->insert(
            {regionKeyMap.get_memserver_id(), regionMemory});
        memserverIds->push_back(cisResponse.get_memsrv_list()[(int)i]);
    }
}

void Fam_CIS_Thallium_Client::open_region_without_registration(
    uint64_t regionId, std::vector<uint64_t> *memserverIds) {
    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    Fam_CIS_Thallium_Response cisResponse =
        rp_open_region_without_registration.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)

    for (int i = 0; i < (int)cisResponse.get_region_key_map().size(); i++) {
        memserverIds->push_back(cisResponse.get_memsrv_list()[(int)i]);
    }
}

void Fam_CIS_Thallium_Client::close_region(uint64_t regionId,
                                           std::vector<uint64_t> memserverIds) {
    Fam_CIS_Thallium_Request cisRequest;

    cisRequest.set_regionid(regionId);
    cisRequest.set_memsrv_list(memserverIds);

    Fam_CIS_Thallium_Response cisResponse = rp_close_region.on(ph)(cisRequest);

    RPC_STATUS_CHECK(CIS_Exception, cisResponse)
}
} // namespace openfam
