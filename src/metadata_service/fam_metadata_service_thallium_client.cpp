/*
 *   fam_metadata_service_thallium_client.cpp
 *   Copyright (c) 2023 Hewlett Packard Enterprise Development, LP. All rights
 *   reserved. Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following conditions
 *   are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the name of the copyright holder nor the names of its
 *      contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 *      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 *      IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *      BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *      FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 *      SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *      INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *      DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *      OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *      INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *      CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *      OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *      IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * See https://spdx.org/licenses/BSD-3-Clause
 *
 */
#include "fam_metadata_service_thallium_client.h"
#include "cis/fam_cis_direct.h"
#include "common/fam_config_info.h"
#include "common/fam_memserver_profile.h"
#include "fam_metadata_service_client.h"
#include "fam_metadata_service_direct.h"
#include "fam_metadata_service_thallium_rpc_structures.h"
#include <fstream>
#include <iostream>
#include <string>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>

// To-do: Convert #define to enum
#define HAS_KEY_DATAITEM_ID_REGION_ID 1
#define HAS_KEY_DATAITEM_ID_REGION_NAME 2
#define HAS_KEY_DATAITEM_NAME_REGION_ID 3
#define HAS_KEY_DATAITEM_NAME_REGION_NAME 4

namespace tl = thallium;
using namespace std;
using namespace openfam;

namespace metadata {
MEMSERVER_PROFILE_START(METADATA_THALLIUM_CLIENT)
#ifdef MEMSERVER_PROFILE
#define METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()                           \
    {                                                                          \
        Profile_Time start = METADATA_THALLIUM_CLIENT_get_time();

#define METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(apiIdx)                       \
    Profile_Time end = METADATA_THALLIUM_CLIENT_get_time();                    \
    Profile_Time total =                                                       \
        METADATA_THALLIUM_CLIENT_time_diff_nanoseconds(start, end);            \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(METADATA_THALLIUM_CLIENT,               \
                                       prof_##apiIdx, total)                   \
    }
#define METADATA_THALLIUM_CLIENT_PROFILE_DUMP()                                \
    metadata_thallium_client_profile_dump()
#else
#define METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
#define METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(apiIdx)
#define METADATA_THALLIUM_CLIENT_PROFILE_DUMP()
#endif

void metadata_thallium_client_profile_dump() {
    MEMSERVER_PROFILE_END(METADATA_THALLIUM_CLIENT);
    MEMSERVER_DUMP_PROFILE_BANNER(METADATA_THALLIUM_CLIENT)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(METADATA_THALLIUM_CLIENT, name, prof_##name)
#include "metadata_service/metadata_client_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(METADATA_THALLIUM_CLIENT, prof_##name)
#include "metadata_service/metadata_client_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(METADATA_THALLIUM_CLIENT)
}

// thallium engine initialization
Fam_Metadata_Service_Thallium_Client::Fam_Metadata_Service_Thallium_Client(
    tl::engine engine_, const char *name, uint64_t port) {
    myEngine = engine_;
    MEMSERVER_PROFILE_INIT(METADATA_THALLIUM_THALLIUM_CLIENT)
    MEMSERVER_PROFILE_START_TIME(METADATA_THALLIUM_THALLIUM_CLIENT)
    connect(name, port);
}

// grpc client code to connect
char *Fam_Metadata_Service_Thallium_Client::get_fabric_addr(const char *name,
                                                            uint64_t port) {
    MEMSERVER_PROFILE_INIT(METADATA_CLIENT)
    MEMSERVER_PROFILE_START_TIME(METADATA_CLIENT)
    std::ostringstream message;
    std::string name_s(name);
    name_s += ":" + std::to_string(port);

    uid = (uint32_t)getuid();
    gid = (uint32_t)getgid();

    /** Creating a channel and stub **/
    this->stub = Fam_Metadata_Rpc::NewStub(
        grpc::CreateChannel(name_s, ::grpc::InsecureChannelCredentials()));

    if (!this->stub) {
        message << "Fam Grpc Initialialization failed: stub is null";
        throw Metadata_Service_Exception(FAM_ERR_RPC, message.str().c_str());
    }

    Fam_Metadata_Gen_Request req;
    Fam_Metadata_Gen_Response res;

    ::grpc::ClientContext ctx;
    /** sending a start signal to server **/
    ::grpc::Status status = stub->signal_start(&ctx, req, &res);
    if (!status.ok()) {
        message << "Fam Metadata Service: " << status.error_message().c_str()
                << ":" << name_s;
        throw Metadata_Service_Exception(FAM_ERR_RPC, message.str().c_str());
    }
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
        lastBytes = res.addrname(readCount);
        memcpy(((uint32_t *)fabricAddr + readCount), &lastBytes,
               lastBytesCount);
    }
    return fabricAddr;
}
// end of grpc code

void Fam_Metadata_Service_Thallium_Client::connect(const char *name,
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
    rp_metadata_insert_region = myEngine.define("metadata_insert_region");
    rp_metadata_delete_region = myEngine.define("metadata_delete_region");
    rp_metadata_find_region = myEngine.define("metadata_find_region");
    rp_metadata_modify_region = myEngine.define("metadata_modify_region");
    rp_metadata_insert_dataitem = myEngine.define("metadata_insert_dataitem");
    rp_metadata_delete_dataitem = myEngine.define("metadata_delete_dataitem");
    rp_metadata_find_dataitem = myEngine.define("metadata_find_dataitem");
    rp_metadata_modify_dataitem = myEngine.define("metadata_modify_dataitem");
    rp_metadata_check_region_permissions =
        myEngine.define("metadata_check_region_permissions");
    rp_metadata_check_item_permissions =
        myEngine.define("metadata_check_item_permissions");
    rp_metadata_maxkeylen = myEngine.define("metadata_maxkeylen");
    rp_metadata_update_memoryserver =
        myEngine.define("metadata_update_memoryserver");
    rp_metadata_reset_bitmap = myEngine.define("metadata_reset_bitmap");
    rp_metadata_validate_and_create_region =
        myEngine.define("metadata_validate_and_create_region");
    rp_metadata_validate_and_destroy_region =
        myEngine.define("metadata_validate_and_destroy_region");
    rp_metadata_validate_and_deallocate_dataitem =
        myEngine.define("metadata_validate_and_deallocate_dataitem");
    rp_metadata_validate_and_allocate_dataitem =
        myEngine.define("metadata_validate_and_allocate_dataitem");
    rp_metadata_find_region_and_check_permissions =
        myEngine.define("metadata_find_region_and_check_permissions");
    rp_metadata_find_dataitem_and_check_permissions =
        myEngine.define("metadata_find_dataitem_and_check_permissions");
    rp_get_memory_server_list = myEngine.define("get_memory_server_list");
    rp_reset_profile = myEngine.define("reset_profile").disable_response();
    rp_dump_profile = myEngine.define("dump_profile").disable_response();
}

Fam_Metadata_Service_Thallium_Client::~Fam_Metadata_Service_Thallium_Client() {
    METADATA_THALLIUM_CLIENT_PROFILE_DUMP();
}

void Fam_Metadata_Service_Thallium_Client::reset_profile() {
    MEMSERVER_PROFILE_INIT(METADATA_THALLIUM_CLIENT)
    MEMSERVER_PROFILE_START_TIME(METADATA_THALLIUM_CLIENT)
    rp_reset_profile.on(ph)();
}

void Fam_Metadata_Service_Thallium_Client::dump_profile() {
    METADATA_THALLIUM_CLIENT_PROFILE_DUMP();
    rp_dump_profile.on(ph)();
}

void Fam_Metadata_Service_Thallium_Client::metadata_insert_region(
    const uint64_t regionId, const std::string regionName,
    Fam_Region_Metadata *region) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_id(regionId);
    metaRequest.set_key_region_name(regionName);
    metaRequest.set_region_id(region->regionId);
    metaRequest.set_name(region->name);
    metaRequest.set_offset(region->offset);
    metaRequest.set_size(region->size);
    metaRequest.set_perm(region->perm);
    metaRequest.set_uid(region->uid);
    metaRequest.set_gid(region->gid);
    metaRequest.set_redundancylevel(region->redundancyLevel);
    metaRequest.set_memorytype(region->memoryType);
    metaRequest.set_interleaveenable(region->interleaveEnable);
    metaRequest.set_permission_level(region->permissionLevel);
    metaRequest.set_memsrv_cnt(region->used_memsrv_cnt);
    metaRequest.set_memsrv_list(region->memServerIds,
                                (int)region->used_memsrv_cnt);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_insert_region.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_insert_region);
}

void Fam_Metadata_Service_Thallium_Client::metadata_delete_region(
    const uint64_t regionId) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_id(regionId);
    metaRequest.set_check_name_id(id);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_delete_region.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_delete_region);
}

void Fam_Metadata_Service_Thallium_Client::metadata_delete_region(
    const std::string regionName) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_name(regionName);
    metaRequest.set_check_name_id(name);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_delete_region.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_delete_region);
}

bool Fam_Metadata_Service_Thallium_Client::metadata_find_region(
    const uint64_t regionId, Fam_Region_Metadata &region) {
    Fam_Metadata_Thallium_Request metaRequest;
    Fam_Metadata_Thallium_Response metaResponse;
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()

    metaRequest.set_key_region_id(regionId);
    metaRequest.set_check_name_id(id);
    metaResponse = rp_metadata_find_region.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    if (metaResponse.get_isfound()) {
        region.regionId = metaResponse.get_region_id();
        region.offset = metaResponse.get_offset();
        strncpy(region.name, (metaResponse.get_name()).c_str(),
                metaResponse.get_maxkeylen());
        region.perm = (mode_t)metaResponse.get_perm();
        region.size = metaResponse.get_size();
        region.uid = metaResponse.get_uid();
        region.gid = metaResponse.get_gid();
        region.used_memsrv_cnt = metaResponse.get_memsrv_cnt();
        region.redundancyLevel =
            (Fam_Redundancy_Level)metaResponse.get_redundancylevel();
        region.memoryType = (Fam_Memory_Type)metaResponse.get_memorytype();
        region.interleaveEnable =
            (Fam_Interleave_Enable)metaResponse.get_interleaveenable();
        region.interleaveSize = metaResponse.get_interleavesize();
        region.permissionLevel =
            (Fam_Permission_Level)metaResponse.get_permission_level();
        for (int i = 0; i < (int)region.used_memsrv_cnt; i++) {
            region.memServerIds[i] = metaResponse.get_memsrv_list()[i];
        }
    }
    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_find_region);
    return metaResponse.get_isfound();
}

bool Fam_Metadata_Service_Thallium_Client::metadata_find_region(
    const std::string regionName, Fam_Region_Metadata &region) {
    Fam_Metadata_Thallium_Request metaRequest;
    Fam_Metadata_Thallium_Response metaResponse;
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()

    metaRequest.set_key_region_name(regionName);
    metaRequest.set_check_name_id(name);
    metaResponse = rp_metadata_find_region.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    if (metaResponse.get_isfound()) {
        region.regionId = metaResponse.get_region_id();
        region.offset = metaResponse.get_offset();
        strncpy(region.name, (metaResponse.get_name()).c_str(),
                metaResponse.get_maxkeylen());
        region.perm = (mode_t)metaResponse.get_perm();
        region.size = metaResponse.get_size();
        region.uid = metaResponse.get_uid();
        region.gid = metaResponse.get_gid();
        region.used_memsrv_cnt = metaResponse.get_memsrv_cnt();
        region.redundancyLevel =
            (Fam_Redundancy_Level)metaResponse.get_redundancylevel();
        region.memoryType = (Fam_Memory_Type)metaResponse.get_memorytype();
        region.interleaveEnable =
            (Fam_Interleave_Enable)metaResponse.get_interleaveenable();
        region.interleaveSize = metaResponse.get_interleavesize();
        region.permissionLevel =
            (Fam_Permission_Level)metaResponse.get_permission_level();
        for (int i = 0; i < (int)region.used_memsrv_cnt; i++) {
            region.memServerIds[i] = metaResponse.get_memsrv_list()[i];
        }
    }
    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_find_region);
    return metaResponse.get_isfound();
}

void Fam_Metadata_Service_Thallium_Client::metadata_modify_region(
    const uint64_t regionId, Fam_Region_Metadata *region) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_id(regionId);
    metaRequest.set_region_id(regionId);
    metaRequest.set_name(region->name);
    metaRequest.set_offset(region->offset);
    metaRequest.set_size(region->size);
    metaRequest.set_perm(region->perm);
    metaRequest.set_uid(region->uid);
    metaRequest.set_gid(region->gid);
    metaRequest.set_redundancylevel(region->redundancyLevel);
    metaRequest.set_memorytype(region->memoryType);
    metaRequest.set_interleaveenable(region->interleaveEnable);
    metaRequest.set_interleavesize(region->interleaveSize);
    metaRequest.set_permission_level(region->permissionLevel);
    metaRequest.set_memsrv_cnt(region->used_memsrv_cnt);
    metaRequest.set_memsrv_list(region->memServerIds,
                                (int)region->used_memsrv_cnt);
    metaRequest.set_check_name_id(id);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_modify_region.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_modify_region);
}

void Fam_Metadata_Service_Thallium_Client::metadata_modify_region(
    const std::string regionName, Fam_Region_Metadata *region) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_name(regionName);
    metaRequest.set_region_id(region->regionId);
    metaRequest.set_name(regionName);
    metaRequest.set_offset(region->offset);
    metaRequest.set_size(region->size);
    metaRequest.set_perm(region->perm);
    metaRequest.set_uid(region->uid);
    metaRequest.set_gid(region->gid);
    metaRequest.set_redundancylevel(region->redundancyLevel);
    metaRequest.set_memorytype(region->memoryType);
    metaRequest.set_interleaveenable(region->interleaveEnable);
    metaRequest.set_interleavesize(region->interleaveSize);
    metaRequest.set_permission_level(region->permissionLevel);
    metaRequest.set_memsrv_cnt(region->used_memsrv_cnt);
    metaRequest.set_memsrv_list(region->memServerIds,
                                (int)region->used_memsrv_cnt);
    metaRequest.set_check_name_id(name);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_modify_region.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_modify_region);
}

void Fam_Metadata_Service_Thallium_Client::metadata_insert_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_id(regionId);
    metaRequest.set_key_dataitem_id(dataitemId);
    metaRequest.set_key_dataitem_name(dataitemName);
    metaRequest.set_region_id(regionId);
    metaRequest.set_name(dataitem->name);
    metaRequest.set_offsets(dataitem->offsets, (int)dataitem->used_memsrv_cnt);
    metaRequest.set_size(dataitem->size);
    metaRequest.set_perm(dataitem->perm);
    metaRequest.set_uid(dataitem->uid);
    metaRequest.set_gid(dataitem->gid);
    metaRequest.set_memsrv_cnt(dataitem->used_memsrv_cnt);
    metaRequest.set_interleave_size(dataitem->interleaveSize);
    metaRequest.set_permission_level(dataitem->permissionLevel);
    metaRequest.set_memsrv_list(dataitem->memoryServerIds,
                                (int)dataitem->used_memsrv_cnt);
    metaRequest.set_check_name_id(id);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_insert_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_insert_dataitem);
}

void Fam_Metadata_Service_Thallium_Client::metadata_insert_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_name(regionName);
    metaRequest.set_key_dataitem_id(dataitemId);
    metaRequest.set_key_dataitem_name(dataitemName);
    metaRequest.set_region_id(dataitem->regionId);
    metaRequest.set_name(dataitem->name);
    metaRequest.set_offsets(dataitem->offsets, (int)dataitem->used_memsrv_cnt);
    metaRequest.set_size(dataitem->size);
    metaRequest.set_perm(dataitem->perm);
    metaRequest.set_uid(dataitem->uid);
    metaRequest.set_gid(dataitem->gid);
    metaRequest.set_memsrv_cnt(dataitem->used_memsrv_cnt);
    metaRequest.set_interleave_size(dataitem->interleaveSize);
    metaRequest.set_permission_level(dataitem->permissionLevel);
    metaRequest.set_memsrv_list(dataitem->memoryServerIds,
                                (int)dataitem->used_memsrv_cnt);
    metaRequest.set_check_name_id(name);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_insert_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_insert_dataitem);
}

void Fam_Metadata_Service_Thallium_Client::metadata_delete_dataitem(
    const uint64_t dataitemId, const std::string regionName) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_name(regionName);
    metaRequest.set_key_dataitem_id(dataitemId);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_ID_REGION_NAME);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_delete_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_delete_dataitem);
}

void Fam_Metadata_Service_Thallium_Client::metadata_delete_dataitem(
    const uint64_t dataitemId, const uint64_t regionId) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_id(regionId);
    metaRequest.set_key_dataitem_id(dataitemId);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_ID_REGION_ID);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_delete_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_delete_dataitem);
}

void Fam_Metadata_Service_Thallium_Client::metadata_delete_dataitem(
    const std::string dataitemName, const std::string regionName) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_name(regionName);
    metaRequest.set_key_dataitem_name(dataitemName);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_NAME_REGION_NAME);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_delete_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_delete_dataitem);
}

void Fam_Metadata_Service_Thallium_Client::metadata_delete_dataitem(
    const std::string dataitemName, const uint64_t regionId) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_id(regionId);
    metaRequest.set_key_dataitem_name(dataitemName);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_NAME_REGION_ID);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_delete_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_delete_dataitem);
}

bool Fam_Metadata_Service_Thallium_Client::metadata_find_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Thallium_Request metaRequest;
    Fam_Metadata_Thallium_Response metaResponse;
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()

    metaRequest.set_key_region_id(regionId);
    metaRequest.set_key_dataitem_id(dataitemId);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_ID_REGION_ID);
    metaResponse = rp_metadata_find_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    if (metaResponse.get_isfound()) {
        dataitem.regionId = metaResponse.get_region_id();
        for (int i = 0; i < (int)metaResponse.get_offsets().size(); i++) {
            dataitem.offsets[i] = metaResponse.get_offsets()[i];
        }
        dataitem.size = metaResponse.get_size();
        dataitem.perm = (mode_t)metaResponse.get_perm();
        strncpy(dataitem.name, (metaResponse.get_name()).c_str(),
                metaResponse.get_maxkeylen());
        dataitem.uid = metaResponse.get_uid();
        dataitem.gid = metaResponse.get_gid();
        dataitem.used_memsrv_cnt = metaResponse.get_memsrv_list().size();
        dataitem.interleaveSize = metaResponse.get_interleave_size();
        dataitem.permissionLevel =
            (Fam_Permission_Level)metaResponse.get_permission_level();
        for (int i = 0; i < (int)metaResponse.get_memsrv_list().size(); i++) {
            dataitem.memoryServerIds[i] = metaResponse.get_memsrv_list()[i];
        }
    }
    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_find_dataitem);
    return metaResponse.get_isfound();
}

bool Fam_Metadata_Service_Thallium_Client::metadata_find_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Thallium_Request metaRequest;
    Fam_Metadata_Thallium_Response metaResponse;
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()

    metaRequest.set_key_region_name(regionName);
    metaRequest.set_key_dataitem_id(dataitemId);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_ID_REGION_NAME);
    metaResponse = rp_metadata_find_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    if (metaResponse.get_isfound()) {
        dataitem.regionId = metaResponse.get_region_id();
        for (uint64_t i = 0; i < metaResponse.get_memsrv_cnt(); i++) {
            dataitem.offsets[i] = metaResponse.get_offsets()[(int)i];
        }
        dataitem.size = metaResponse.get_size();
        dataitem.perm = (mode_t)metaResponse.get_perm();
        strncpy(dataitem.name, (metaResponse.get_name()).c_str(),
                metaResponse.get_maxkeylen());
        dataitem.uid = metaResponse.get_uid();
        dataitem.gid = metaResponse.get_gid();
        dataitem.used_memsrv_cnt = metaResponse.get_memsrv_cnt();
        dataitem.interleaveSize = metaResponse.get_interleave_size();
        dataitem.permissionLevel =
            (Fam_Permission_Level)metaResponse.get_permission_level();
        for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
            dataitem.memoryServerIds[i] = metaResponse.get_memsrv_list()[i];
        }
    }
    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_find_dataitem);
    return metaResponse.get_isfound();
}

bool Fam_Metadata_Service_Thallium_Client::metadata_find_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Thallium_Request metaRequest;
    Fam_Metadata_Thallium_Response metaResponse;
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()

    metaRequest.set_key_region_id(regionId);
    metaRequest.set_key_dataitem_name(dataitemName);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_NAME_REGION_ID);
    metaResponse = rp_metadata_find_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    if (metaResponse.get_isfound()) {
        dataitem.regionId = metaResponse.get_region_id();
        for (uint64_t i = 0; i < metaResponse.get_memsrv_cnt(); i++) {
            dataitem.offsets[i] = metaResponse.get_offsets()[(int)i];
        }
        dataitem.size = metaResponse.get_size();
        dataitem.perm = (mode_t)metaResponse.get_perm();
        strncpy(dataitem.name, (metaResponse.get_name()).c_str(),
                metaResponse.get_maxkeylen());
        dataitem.uid = metaResponse.get_uid();
        dataitem.gid = metaResponse.get_gid();
        dataitem.used_memsrv_cnt = metaResponse.get_memsrv_cnt();
        dataitem.interleaveSize = metaResponse.get_interleave_size();
        dataitem.permissionLevel =
            (Fam_Permission_Level)metaResponse.get_permission_level();
        for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
            dataitem.memoryServerIds[i] = metaResponse.get_memsrv_list()[i];
        }
    }
    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_find_dataitem);
    return metaResponse.get_isfound();
}

bool Fam_Metadata_Service_Thallium_Client::metadata_find_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Thallium_Request metaRequest;
    Fam_Metadata_Thallium_Response metaResponse;
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()

    metaRequest.set_key_region_name(regionName);
    metaRequest.set_key_dataitem_name(dataitemName);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_NAME_REGION_NAME);
    metaResponse = rp_metadata_find_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    if (metaResponse.get_isfound()) {
        dataitem.regionId = metaResponse.get_region_id();
        for (uint64_t i = 0; i < metaResponse.get_memsrv_cnt(); i++) {
            dataitem.offsets[i] = metaResponse.get_offsets()[(int)i];
        }
        dataitem.size = metaResponse.get_size();
        dataitem.perm = (mode_t)metaResponse.get_perm();
        strncpy(dataitem.name, (metaResponse.get_name()).c_str(),
                metaResponse.get_maxkeylen());
        dataitem.uid = metaResponse.get_uid();
        dataitem.gid = metaResponse.get_gid();
        dataitem.used_memsrv_cnt = metaResponse.get_memsrv_cnt();
        dataitem.interleaveSize = metaResponse.get_interleave_size();
        dataitem.permissionLevel =
            (Fam_Permission_Level)metaResponse.get_permission_level();
        for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
            dataitem.memoryServerIds[i] = metaResponse.get_memsrv_list()[i];
        }
    }
    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_find_dataitem);
    return metaResponse.get_isfound();
}

void Fam_Metadata_Service_Thallium_Client::metadata_modify_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_id(regionId);
    metaRequest.set_key_dataitem_id(dataitemId);
    metaRequest.set_region_id(regionId);
    metaRequest.set_name(dataitem->name);
    metaRequest.set_offsets(dataitem->offsets, (int)dataitem->used_memsrv_cnt);
    metaRequest.set_uid(dataitem->uid);
    metaRequest.set_gid(dataitem->gid);
    metaRequest.set_size(dataitem->size);
    metaRequest.set_perm(dataitem->perm);
    metaRequest.set_memsrv_cnt(dataitem->used_memsrv_cnt);
    metaRequest.set_interleave_size(dataitem->interleaveSize);
    metaRequest.set_permission_level(dataitem->permissionLevel);
    metaRequest.set_memsrv_list(dataitem->memoryServerIds,
                                (int)dataitem->used_memsrv_cnt);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_ID_REGION_ID);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_modify_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_modify_dataitem);
}

void Fam_Metadata_Service_Thallium_Client::metadata_modify_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_name(regionName);
    metaRequest.set_key_dataitem_id(dataitemId);
    metaRequest.set_region_id(dataitem->regionId);
    metaRequest.set_name(dataitem->name);
    metaRequest.set_offsets(dataitem->offsets, (int)dataitem->used_memsrv_cnt);
    metaRequest.set_uid(dataitem->uid);
    metaRequest.set_gid(dataitem->gid);
    metaRequest.set_size(dataitem->size);
    metaRequest.set_perm(dataitem->perm);
    metaRequest.set_memsrv_cnt(dataitem->used_memsrv_cnt);
    metaRequest.set_interleave_size(dataitem->interleaveSize);
    metaRequest.set_permission_level(dataitem->permissionLevel);
    metaRequest.set_memsrv_list(dataitem->memoryServerIds,
                                (int)dataitem->used_memsrv_cnt);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_ID_REGION_NAME);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_modify_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_modify_dataitem);
}

void Fam_Metadata_Service_Thallium_Client::metadata_modify_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_id(regionId);
    metaRequest.set_key_dataitem_name(dataitemName);
    metaRequest.set_region_id(dataitem->regionId);
    metaRequest.set_name(dataitem->name);
    metaRequest.set_offsets(dataitem->offsets, (int)dataitem->used_memsrv_cnt);
    metaRequest.set_uid(dataitem->uid);
    metaRequest.set_gid(dataitem->gid);
    metaRequest.set_size(dataitem->size);
    metaRequest.set_perm(dataitem->perm);
    metaRequest.set_memsrv_cnt(dataitem->used_memsrv_cnt);
    metaRequest.set_interleave_size(dataitem->interleaveSize);
    metaRequest.set_permission_level(dataitem->permissionLevel);
    metaRequest.set_memsrv_list(dataitem->memoryServerIds,
                                (int)dataitem->used_memsrv_cnt);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_NAME_REGION_ID);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_modify_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_modify_dataitem);
}

void Fam_Metadata_Service_Thallium_Client::metadata_modify_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_region_name(regionName);
    metaRequest.set_key_dataitem_name(dataitemName);
    metaRequest.set_region_id(dataitem->regionId);
    metaRequest.set_name(dataitem->name);
    metaRequest.set_offsets(dataitem->offsets, (int)dataitem->used_memsrv_cnt);
    metaRequest.set_uid(dataitem->uid);
    metaRequest.set_gid(dataitem->gid);
    metaRequest.set_size(dataitem->size);
    metaRequest.set_perm(dataitem->perm);
    metaRequest.set_memsrv_cnt(dataitem->used_memsrv_cnt);
    metaRequest.set_interleave_size(dataitem->interleaveSize);
    metaRequest.set_permission_level(dataitem->permissionLevel);
    metaRequest.set_memsrv_list(dataitem->memoryServerIds,
                                (int)dataitem->used_memsrv_cnt);
    metaRequest.set_type_flag(HAS_KEY_DATAITEM_NAME_REGION_NAME);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_modify_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_modify_dataitem);
}

bool Fam_Metadata_Service_Thallium_Client::metadata_check_permissions(
    Fam_DataItem_Metadata *dataitem, metadata_region_item_op_t op, uint32_t uid,
    uint32_t gid) {
    Fam_Metadata_Thallium_Request metaRequest;
    Fam_Metadata_Thallium_Response metaResponse;
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()

    metaRequest.set_uid_meta(dataitem->uid);
    metaRequest.set_gid_meta(dataitem->gid);
    metaRequest.set_uid_in(uid);
    metaRequest.set_gid_in(gid);
    metaRequest.set_perm(dataitem->perm);
    metaRequest.set_ops(op);
    metaRequest.set_check_name_id(id);
    metaResponse = rp_metadata_check_item_permissions.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_check_permissions);
    return metaResponse.get_is_permitted();
}

bool Fam_Metadata_Service_Thallium_Client::metadata_check_permissions(
    Fam_Region_Metadata *region, metadata_region_item_op_t op, uint32_t uid,
    uint32_t gid) {
    Fam_Metadata_Thallium_Request metaRequest;
    Fam_Metadata_Thallium_Response metaResponse;
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()

    metaRequest.set_uid_meta(region->uid);
    metaRequest.set_gid_meta(region->gid);
    metaRequest.set_uid_in(uid);
    metaRequest.set_gid_in(gid);
    metaRequest.set_perm(region->perm);
    metaRequest.set_ops(op);
    metaRequest.set_check_name_id(name);
    metaResponse = rp_metadata_check_region_permissions.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_check_permissions);
    return metaResponse.get_is_permitted();
}

size_t Fam_Metadata_Service_Thallium_Client::metadata_maxkeylen() {
    Fam_Metadata_Thallium_Request metaRequest;
    Fam_Metadata_Thallium_Response metaResponse;
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()

    metaResponse = rp_metadata_maxkeylen.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_maxkeylen);
    return (size_t)metaResponse.get_maxkeylen();
}

void Fam_Metadata_Service_Thallium_Client::metadata_update_memoryserver(
    int nmemServersPersistent, std::vector<uint64_t> memsrv_persistent_id_list,
    int nmemServersVolatile, std::vector<uint64_t> memsrv_volatile_id_list) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_nmemserverspersistent(nmemServersPersistent);
    metaRequest.set_memsrv_persistent_list(memsrv_persistent_id_list);
    metaRequest.set_nmemserversvolatile(nmemServersVolatile);
    metaRequest.set_memsrv_volatile_list(memsrv_volatile_id_list);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_update_memoryserver.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_update_memoryserver);
}

void Fam_Metadata_Service_Thallium_Client::metadata_reset_bitmap(
    uint64_t regionId) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_region_id(regionId);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_reset_bitmap.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_metaResponseet_bitmap);
}

void Fam_Metadata_Service_Thallium_Client::metadata_validate_and_create_region(
    const std::string regionname, size_t size, uint64_t *regionid,
    Fam_Region_Attributes *regionAttributes, std::list<int> *memory_server_list,
    int user_policy) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_region_id(*regionid);
    metaRequest.set_name(regionname);
    metaRequest.set_size(size);
    metaRequest.set_user_policy(user_policy);
    metaRequest.set_redundancylevel(regionAttributes->redundancyLevel);
    metaRequest.set_memorytype(regionAttributes->memoryType);
    metaRequest.set_interleaveenable(regionAttributes->interleaveEnable);
    metaRequest.set_permission_level(regionAttributes->permissionLevel);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_validate_and_create_region.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    *regionid = metaResponse.get_region_id();
    int memsrv_count = (int)metaResponse.get_memsrv_list().size();
    for (int i = 0; i < memsrv_count; i++) {
        memory_server_list->push_back((int)metaResponse.get_memsrv_list()[i]);
    }

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_validate_and_create_region);
}

void Fam_Metadata_Service_Thallium_Client::metadata_validate_and_destroy_region(
    const uint64_t regionId, uint32_t uid, uint32_t gid,
    std::list<int> *memory_server_list) {

    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_region_id(regionId);
    metaRequest.set_uid(uid);
    metaRequest.set_gid(gid);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_validate_and_destroy_region.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    int memsrv_count = (int)metaResponse.get_memsrv_list().size();
    for (int i = 0; i < memsrv_count; i++) {
        memory_server_list->push_back((int)metaResponse.get_memsrv_list()[i]);
    }
    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_validate_and_destroy_region);
}

void Fam_Metadata_Service_Thallium_Client::
    metadata_find_region_and_check_permissions(metadata_region_item_op_t op,
                                               const uint64_t regionId,
                                               uint32_t uid, uint32_t gid,
                                               Fam_Region_Metadata &region) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_op(op);
    metaRequest.set_key_region_id(regionId);
    metaRequest.set_uid(uid);
    metaRequest.set_gid(gid);
    metaRequest.set_check_name_id(id);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_find_region_and_check_permissions.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    region.regionId = metaResponse.get_region_id();
    region.offset = metaResponse.get_offset();
    strncpy(region.name, (metaResponse.get_name()).c_str(),
            metaResponse.get_maxkeylen());
    region.perm = (mode_t)metaResponse.get_perm();
    region.size = metaResponse.get_size();
    region.uid = metaResponse.get_uid();
    region.gid = metaResponse.get_gid();
    region.used_memsrv_cnt = metaResponse.get_memsrv_cnt();
    region.redundancyLevel =
        (Fam_Redundancy_Level)metaResponse.get_redundancylevel();
    region.memoryType = (Fam_Memory_Type)metaResponse.get_memorytype();
    region.interleaveEnable =
        (Fam_Interleave_Enable)metaResponse.get_interleaveenable();
    region.interleaveSize = metaResponse.get_interleavesize();
    region.permissionLevel =
        (Fam_Permission_Level)metaResponse.get_permission_level();
    for (int i = 0; i < (int)region.used_memsrv_cnt; i++) {
        region.memServerIds[i] = metaResponse.get_memsrv_list()[i];
    }

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_find_region_and_check_permissions);
}

void Fam_Metadata_Service_Thallium_Client::
    metadata_find_region_and_check_permissions(metadata_region_item_op_t op,
                                               const std::string regionName,
                                               uint32_t uid, uint32_t gid,
                                               Fam_Region_Metadata &region) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_op(op);
    metaRequest.set_key_region_name(regionName);
    metaRequest.set_uid(uid);
    metaRequest.set_gid(gid);
    metaRequest.set_check_name_id(name);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_find_region_and_check_permissions.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    region.regionId = metaResponse.get_region_id();
    region.offset = metaResponse.get_offset();
    strncpy(region.name, (metaResponse.get_name()).c_str(),
            metaResponse.get_maxkeylen());
    region.perm = (mode_t)metaResponse.get_perm();
    region.size = metaResponse.get_size();
    region.uid = metaResponse.get_uid();
    region.gid = metaResponse.get_gid();
    region.used_memsrv_cnt = metaResponse.get_memsrv_cnt();
    region.redundancyLevel =
        (Fam_Redundancy_Level)metaResponse.get_redundancylevel();
    region.memoryType = (Fam_Memory_Type)metaResponse.get_memorytype();
    region.interleaveEnable =
        (Fam_Interleave_Enable)metaResponse.get_interleaveenable();
    region.interleaveSize = metaResponse.get_interleavesize();
    region.permissionLevel =
        (Fam_Permission_Level)metaResponse.get_permission_level();
    for (int i = 0; i < (int)region.used_memsrv_cnt; i++) {
        region.memServerIds[i] = metaResponse.get_memsrv_list()[i];
    }

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_find_region_and_check_permissions);
}

void Fam_Metadata_Service_Thallium_Client::
    metadata_find_dataitem_and_check_permissions(
        metadata_region_item_op_t op, const uint64_t dataitemId,
        const uint64_t regionId, uint32_t uid, uint32_t gid,
        Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Thallium_Request metaRequest;

    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    metaRequest.set_key_region_id(regionId);
    metaRequest.set_key_dataitem_id(dataitemId);
    metaRequest.set_op(op);
    metaRequest.set_uid(uid);
    metaRequest.set_gid(gid);
    metaRequest.set_check_name_id(id);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_find_dataitem_and_check_permissions.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    dataitem.regionId = metaResponse.get_region_id();
    dataitem.used_memsrv_cnt = metaResponse.get_memsrv_list().size();
    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        dataitem.offsets[i] = metaResponse.get_offsets()[i];
    }

    dataitem.size = metaResponse.get_size();
    dataitem.perm = (mode_t)metaResponse.get_perm();
    strncpy(dataitem.name, (metaResponse.get_name()).c_str(),
            metaResponse.get_maxkeylen());
    dataitem.uid = metaResponse.get_uid();
    dataitem.gid = metaResponse.get_gid();
    dataitem.interleaveSize = metaResponse.get_interleave_size();
    dataitem.permissionLevel =
        (Fam_Permission_Level)metaResponse.get_permission_level();
    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        dataitem.memoryServerIds[i] = metaResponse.get_memsrv_list()[i];
    }
    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_find_dataitem_and_check_permissions);
}

void Fam_Metadata_Service_Thallium_Client::
    metadata_find_dataitem_and_check_permissions(
        metadata_region_item_op_t op, const std::string dataitemName,
        const std::string regionName, uint32_t uid, uint32_t gid,
        Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Thallium_Request metaRequest;

    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    metaRequest.set_key_region_name(regionName);
    metaRequest.set_key_dataitem_name(dataitemName);
    metaRequest.set_op(op);
    metaRequest.set_uid(uid);
    metaRequest.set_gid(gid);
    metaRequest.set_check_name_id(name);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_find_dataitem_and_check_permissions.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    dataitem.regionId = metaResponse.get_region_id();
    dataitem.used_memsrv_cnt = metaResponse.get_memsrv_list().size();
    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        dataitem.offsets[i] = metaResponse.get_offsets()[i];
    }
    dataitem.size = metaResponse.get_size();
    dataitem.perm = (mode_t)metaResponse.get_perm();
    strncpy(dataitem.name, (metaResponse.get_name()).c_str(),
            metaResponse.get_maxkeylen());
    dataitem.uid = metaResponse.get_uid();
    dataitem.gid = metaResponse.get_gid();
    dataitem.interleaveSize = metaResponse.get_interleave_size();
    dataitem.permissionLevel =
        (Fam_Permission_Level)metaResponse.get_permission_level();
    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        dataitem.memoryServerIds[i] = metaResponse.get_memsrv_list()[i];
    }
    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_find_dataitem_and_check_permissions);
}

void Fam_Metadata_Service_Thallium_Client::
    metadata_validate_and_allocate_dataitem(
        const std::string dataitemName, const uint64_t regionId, uint32_t uid,
        uint32_t gid, size_t size, std::list<int> *memory_server_list,
        size_t *interleaveSize, Fam_Permission_Level *permissionLevel,
        mode_t *regionPermission, int user_policy) {
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_key_dataitem_name(dataitemName);
    metaRequest.set_region_id(regionId);
    metaRequest.set_uid(uid);
    metaRequest.set_gid(gid);
    metaRequest.set_size(size);
    metaRequest.set_user_policy(user_policy);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_validate_and_allocate_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    int memsrv_count = (int)metaResponse.get_memsrv_list().size();
    *interleaveSize = metaResponse.get_interleave_size();
    *permissionLevel =
        (Fam_Permission_Level)metaResponse.get_permission_level();
    *regionPermission = (mode_t)metaResponse.get_region_permission();
    for (int i = 0; i < memsrv_count; i++) {
        memory_server_list->push_back((int)(metaResponse.get_memsrv_list()[i]));
    }
    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_validate_and_allocate_dataitem);
}

void Fam_Metadata_Service_Thallium_Client::
    metadata_validate_and_deallocate_dataitem(const uint64_t regionId,
                                              const uint64_t dataitemId,
                                              uint32_t uid, uint32_t gid,
                                              Fam_DataItem_Metadata &dataitem) {

    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;

    metaRequest.set_region_id(regionId);
    metaRequest.set_key_dataitem_id(dataitemId);
    metaRequest.set_uid(uid);
    metaRequest.set_gid(gid);
    Fam_Metadata_Thallium_Response metaResponse =
        rp_metadata_validate_and_deallocate_dataitem.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    dataitem.regionId = metaResponse.get_region_id();
    dataitem.used_memsrv_cnt = metaResponse.get_memsrv_list().size();
    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        dataitem.offsets[i] = metaResponse.get_offsets()[i];
    }
    dataitem.size = metaResponse.get_size();
    dataitem.perm = (mode_t)metaResponse.get_perm();
    strncpy(dataitem.name, (metaResponse.get_name()).c_str(),
            metaResponse.get_maxkeylen());
    dataitem.uid = metaResponse.get_uid();
    dataitem.gid = metaResponse.get_gid();
    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        dataitem.memoryServerIds[i] = metaResponse.get_memsrv_list()[i];
    }
    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_metadata_validate_and_deallocate_dataitem);
}

std::list<int> Fam_Metadata_Service_Thallium_Client::get_memory_server_list(
    uint64_t regionId) {
    std::list<int> memory_server_list;
    METADATA_THALLIUM_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Thallium_Request metaRequest;
    Fam_Metadata_Thallium_Response metaResponse;

    metaRequest.set_region_id(regionId);
    metaResponse = rp_get_memory_server_list.on(ph)(metaRequest);
    RPC_STATUS_CHECK(Metadata_Service_Exception, metaResponse)

    int memsrv_count = (int)metaResponse.get_memsrv_list().size();
    for (int i = 0; i < memsrv_count; i++) {
        memory_server_list.push_back((int)metaResponse.get_memsrv_list()[i]);
    }

    METADATA_THALLIUM_CLIENT_PROFILE_END_OPS(
        thallium_client_get_memory_server_list);
    return memory_server_list;
}
} // namespace metadata
