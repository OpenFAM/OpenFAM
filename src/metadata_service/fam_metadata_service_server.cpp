/*
 *   fam_metadata_service_server.cpp
 *   Copyright (c) 2020,2023 Hewlett Packard Enterprise Development, LP. All
 * rights reserved. Redistribution and use in source and binary forms, with or
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

#include "fam_metadata_service_server.h"
#include "common/fam_config_info.h"
#include "common/fam_memserver_profile.h"

namespace metadata {
MEMSERVER_PROFILE_START(METADATA_SERVER)
#ifdef MEMSERVER_PROFILE
#define METADATA_SERVER_PROFILE_START_OPS()                                    \
    {                                                                          \
        Profile_Time start = METADATA_SERVER_get_time();

#define METADATA_SERVER_PROFILE_END_OPS(apiIdx)                                \
    Profile_Time end = METADATA_SERVER_get_time();                             \
    Profile_Time total = METADATA_SERVER_time_diff_nanoseconds(start, end);    \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(METADATA_SERVER, prof_##apiIdx, total)  \
    }
#define METADATA_SERVER_PROFILE_DUMP() metadata_server_profile_dump()
#else
#define METADATA_SERVER_PROFILE_START_OPS()
#define METADATA_SERVER_PROFILE_END_OPS(apiIdx)
#define METADATA_SERVER_PROFILE_DUMP()
#endif

void metadata_server_profile_dump() {
    MEMSERVER_PROFILE_END(METADATA_SERVER);
    MEMSERVER_DUMP_PROFILE_BANNER(METADATA_SERVER)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(METADATA_SERVER, name, prof_##name)
#include "metadata_service/metadata_server_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(METADATA_SERVER, prof_##name)
#include "metadata_service/metadata_server_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(METADATA_SERVER)
}

Fam_Metadata_Service_Server::Fam_Metadata_Service_Server(
    uint64_t rpcPort, char *name,
    Fam_Metadata_Service_Direct *direct_metadataService_)
    : serverAddress(name), port(rpcPort) {
    metadataService = direct_metadataService_;
    numClients = 0;
    MEMSERVER_PROFILE_INIT(METADATA_SERVER)
    MEMSERVER_PROFILE_START_TIME(METADATA_SERVER)
}

Fam_Metadata_Service_Server::~Fam_Metadata_Service_Server() {
    delete metadataService;
}

::grpc::Status Fam_Metadata_Service_Server::reset_profile(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Gen_Request *request,
    ::Fam_Metadata_Gen_Response *response) {
    MEMSERVER_PROFILE_INIT(METADATA_SERVER)
    MEMSERVER_PROFILE_START_TIME(METADATA_SERVER)
    metadataService->reset_profile();
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::dump_profile(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Gen_Request *request,
    ::Fam_Metadata_Gen_Response *response) {
    METADATA_SERVER_PROFILE_DUMP();
    metadataService->dump_profile();
    return ::grpc::Status::OK;
}

void Fam_Metadata_Service_Server::run() {
    char address[ADDR_SIZE + sizeof(uint64_t)];
    sprintf(address, "%s:%lu", serverAddress, port);
    std::string serverAddress(address);

    ::grpc::ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate
    // with clients. In this case it corresponds to an *synchronous*
    // service.
    builder.RegisterService(this);

    // Finally assemble the server.
    server = builder.BuildAndStart();

    server->Wait();
}

::grpc::Status Fam_Metadata_Service_Server::signal_start(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Gen_Request *request,
    ::Fam_Metadata_Gen_Response *response) {
    __sync_add_and_fetch(&numClients, 1);

    char *addr = (char *)malloc(metadataService->get_controlpath_addr().size());
    memcpy(addr, metadataService->get_controlpath_addr().c_str(),
           metadataService->get_controlpath_addr().size());

    size_t addrSize = metadataService->get_controlpath_addr().size();

    response->set_addrnamelen(addrSize);
    int count = (int)(addrSize / sizeof(uint32_t));
    for (int ndx = 0; ndx < count; ndx++) {
        response->add_addrname(*((uint32_t *)addr + ndx));
    }

    // Only if addrSize is not multiple of 4 (fixed32)
    int lastBytesCount = 0;
    uint32_t lastBytes = 0;

    lastBytesCount = (int)(addrSize % sizeof(uint32_t));
    if (lastBytesCount > 0) {
        memcpy(&lastBytes, ((uint32_t *)addr + count), lastBytesCount);
        response->add_addrname(lastBytes);
    }

    free(addr);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::signal_termination(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Gen_Request *request,
    ::Fam_Metadata_Gen_Response *response) {
    __sync_add_and_fetch(&numClients, -1);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_insert_region(
    ::grpc::ServerContext *context,
    const ::Fam_Metadata_Region_Request *request,
    ::Fam_Metadata_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS()
    Fam_Region_Metadata *region = new Fam_Region_Metadata();
    region->regionId = request->region_id();
    strncpy(region->name, request->name().c_str(),
            metadataService->metadata_maxkeylen());
    region->offset = INVALID_OFFSET;
    region->perm = (mode_t)request->perm();
    region->size = request->size();
    region->uid = request->uid();
    region->gid = request->gid();
    region->used_memsrv_cnt = request->memsrv_cnt();
    region->redundancyLevel = (Fam_Redundancy_Level)request->redundancylevel();
    region->memoryType = (Fam_Memory_Type)request->memorytype();
    region->interleaveEnable =
        (Fam_Interleave_Enable)request->interleaveenable();
    region->permissionLevel = (Fam_Permission_Level)request->permission_level();

    for (int ndx = 0; ndx < (int)region->used_memsrv_cnt; ndx++) {
        region->memServerIds[ndx] = request->memsrv_list(ndx);
    }

    try {
        metadataService->metadata_insert_region(request->region_id(),
                                                request->name(), region);
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_insert_region);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_delete_region(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS()
    try {
        if (request->has_key_region_id())
            metadataService->metadata_delete_region(request->key_region_id());
        else
            metadataService->metadata_delete_region(request->key_region_name());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_delete_region);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_find_region(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Region_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS()
    Fam_Region_Metadata region;
    bool ret;
    try {
        if (request->has_key_region_id())
            ret = metadataService->metadata_find_region(
                request->key_region_id(), region);
        else
            ret = metadataService->metadata_find_region(
                request->key_region_name(), region);
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_isfound(ret);
    if (ret) {
        response->set_region_id(region.regionId);
        response->set_name(region.name);
        response->set_offset(region.offset);
        response->set_size(region.size);
        response->set_perm(region.perm);
        response->set_uid(region.uid);
        response->set_gid(region.gid);
        response->set_maxkeylen(metadataService->metadata_maxkeylen());
        response->set_memsrv_cnt(region.used_memsrv_cnt);
        response->set_redundancylevel(region.redundancyLevel);
        response->set_memorytype(region.redundancyLevel);
        response->set_interleaveenable(region.redundancyLevel);
        response->set_interleavesize(region.interleaveSize);
        response->set_permission_level(region.permissionLevel);
        for (int id = 0; id < (int)region.used_memsrv_cnt; ++id) {
            response->add_memsrv_list(region.memServerIds[id]);
        }
    }
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_find_region);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_modify_region(
    ::grpc::ServerContext *context,
    const ::Fam_Metadata_Region_Request *request,
    ::Fam_Metadata_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS()
    Fam_Region_Metadata *region = new Fam_Region_Metadata();
    region->regionId = request->region_id();
    strncpy(region->name, request->name().c_str(),
            metadataService->metadata_maxkeylen());
    region->offset = INVALID_OFFSET;
    region->perm = (mode_t)request->perm();
    region->size = request->size();
    region->uid = request->uid();
    region->gid = request->gid();
    region->used_memsrv_cnt = request->memsrv_cnt();
    region->redundancyLevel = (Fam_Redundancy_Level)request->redundancylevel();
    region->memoryType = (Fam_Memory_Type)request->memorytype();
    region->interleaveEnable =
        (Fam_Interleave_Enable)request->interleaveenable();
    region->interleaveSize = request->interleavesize();
    region->permissionLevel = (Fam_Permission_Level)request->permission_level();

    for (int ndx = 0; ndx < (int)region->used_memsrv_cnt; ndx++) {
        region->memServerIds[ndx] = request->memsrv_list(ndx);
    }

    try {
        if (request->has_key_region_id())
            metadataService->metadata_modify_region(request->key_region_id(),
                                                    region);
        else
            metadataService->metadata_modify_region(request->key_region_name(),
                                                    region);
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_modify_region);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_insert_dataitem(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS()
    Fam_DataItem_Metadata *dataitem = new Fam_DataItem_Metadata();
    dataitem->regionId = request->region_id();
    strncpy(dataitem->name, request->name().c_str(),
            metadataService->metadata_maxkeylen());
    dataitem->used_memsrv_cnt = request->memsrv_cnt();
    for (int ndx = 0; ndx < (int)dataitem->used_memsrv_cnt; ndx++) {
        dataitem->offsets[ndx] = request->offsets(ndx);
    }
    dataitem->size = request->size();
    dataitem->perm = (mode_t)request->perm();
    dataitem->uid = request->uid();
    dataitem->gid = request->gid();
    dataitem->interleaveSize = request->interleave_size();
    dataitem->permissionLevel =
        (Fam_Permission_Level)request->permission_level();
    for (int ndx = 0; ndx < (int)dataitem->used_memsrv_cnt; ndx++) {
        dataitem->memoryServerIds[ndx] = request->memsrv_list(ndx);
    }
    try {
        if (request->has_key_region_id()) {
            metadataService->metadata_insert_dataitem(
                request->dataitem_id(), request->key_region_id(), dataitem,
                request->name());
        } else
            metadataService->metadata_insert_dataitem(
                request->dataitem_id(), request->key_region_name(), dataitem,
                request->name());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_insert_dataitem);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_delete_dataitem(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS()
    try {
        if (request->has_key_dataitem_id() && request->has_key_region_id())
            metadataService->metadata_delete_dataitem(
                request->key_dataitem_id(), request->key_region_id());
        else if (request->has_key_dataitem_id() &&
                 request->has_key_region_name())
            metadataService->metadata_delete_dataitem(
                request->key_dataitem_id(), request->key_region_name());
        else if (request->has_key_dataitem_name() &&
                 request->has_key_region_id())
            metadataService->metadata_delete_dataitem(
                request->key_dataitem_name(), request->key_region_id());
        else
            metadataService->metadata_delete_dataitem(
                request->key_dataitem_name(), request->key_region_name());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_delete_dataitem);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_find_dataitem(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS()
    Fam_DataItem_Metadata dataitem;
    bool ret;
    try {
        if (request->has_key_dataitem_id() && request->has_key_region_id()) {
            ret = metadataService->metadata_find_dataitem(
                request->key_dataitem_id(), request->key_region_id(), dataitem);
        } else if (request->has_key_dataitem_id() &&
                   request->has_key_region_name())
            ret = metadataService->metadata_find_dataitem(
                request->key_dataitem_id(), request->key_region_name(),
                dataitem);
        else if (request->has_key_dataitem_name() &&
                 request->has_key_region_id())
            ret = metadataService->metadata_find_dataitem(
                request->key_dataitem_name(), request->key_region_id(),
                dataitem);
        else
            ret = metadataService->metadata_find_dataitem(
                request->key_dataitem_name(), request->key_region_name(),
                dataitem);
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_isfound(ret);
    if (ret) {
        response->set_region_id(dataitem.regionId);
        response->set_name(dataitem.name);
        for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
            response->add_offsets(dataitem.offsets[i]);
        }
        response->set_size(dataitem.size);
        response->set_perm(dataitem.perm);
        response->set_uid(dataitem.uid);
        response->set_gid(dataitem.gid);
        response->set_maxkeylen(metadataService->metadata_maxkeylen());
        response->set_interleave_size(dataitem.interleaveSize);
        response->set_permission_level(dataitem.permissionLevel);
        for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
            response->add_memsrv_list(dataitem.memoryServerIds[i]);
        }
    }
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_find_dataitem);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_modify_dataitem(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS()
    Fam_DataItem_Metadata *dataitem = new Fam_DataItem_Metadata();
    dataitem->regionId = request->region_id();
    strncpy(dataitem->name, request->name().c_str(),
            metadataService->metadata_maxkeylen());
    dataitem->used_memsrv_cnt = request->memsrv_cnt();
    for (int ndx = 0; ndx < (int)dataitem->used_memsrv_cnt; ndx++) {
        dataitem->offsets[ndx] = request->offsets(ndx);
    }
    dataitem->size = request->size();
    dataitem->perm = (mode_t)request->perm();
    dataitem->uid = request->uid();
    dataitem->gid = request->gid();
    dataitem->interleaveSize = (size_t)request->interleave_size();
    dataitem->permissionLevel =
        (Fam_Permission_Level)request->permission_level();
    for (int ndx = 0; ndx < (int)dataitem->used_memsrv_cnt; ndx++) {
        dataitem->memoryServerIds[ndx] = request->memsrv_list(ndx);
    }
    try {
        if (request->has_key_dataitem_id() && request->has_key_region_id()) {
            metadataService->metadata_modify_dataitem(
                request->key_dataitem_id(), request->key_region_id(), dataitem);
        } else if (request->has_key_dataitem_id() &&
                   request->has_key_region_name()) {
            metadataService->metadata_modify_dataitem(
                request->key_dataitem_id(), request->key_region_name(),
                dataitem);
        } else if (request->has_key_dataitem_name() &&
                   request->has_key_region_id()) {
            metadataService->metadata_modify_dataitem(
                request->key_dataitem_name(), request->key_region_id(),
                dataitem);
        } else {
            metadataService->metadata_modify_dataitem(
                request->key_dataitem_name(), request->key_region_name(),
                dataitem);
        }
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_modify_dataitem);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_check_region_permissions(
    ::grpc::ServerContext *context, const ::Fam_Permission_Request *request,
    ::Fam_Permission_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS()
    bool ret;
    const google::protobuf::EnumDescriptor *descriptor =
        Fam_Permission_Request_meta_ops_descriptor();
    string op_name = descriptor->FindValueByNumber(request->ops())->name();
    int ops_value = descriptor->FindValueByName(op_name)->number();
    Fam_Region_Metadata *region = new Fam_Region_Metadata();
    region->uid = request->uid_meta();
    region->gid = request->gid_meta();
    region->perm = (mode_t)request->perm();

    ret = metadataService->metadata_check_permissions(
        region, static_cast<metadata_region_item_op_t>(ops_value),
        request->uid_in(), request->gid_in());

    response->set_is_permitted(ret);
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_check_region_permissions);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_check_item_permissions(
    ::grpc::ServerContext *context, const ::Fam_Permission_Request *request,
    ::Fam_Permission_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS()
    bool ret;
    const google::protobuf::EnumDescriptor *descriptor =
        Fam_Permission_Request_meta_ops_descriptor();
    string op_name = descriptor->FindValueByNumber(request->ops())->name();
    int ops_value = descriptor->FindValueByName(op_name)->number();
    Fam_DataItem_Metadata *dataitem = new Fam_DataItem_Metadata();
    dataitem->uid = request->uid_meta();
    dataitem->gid = request->gid_meta();
    dataitem->perm = (mode_t)request->perm();

    ret = metadataService->metadata_check_permissions(
        dataitem, static_cast<metadata_region_item_op_t>(ops_value),
        request->uid_in(), request->gid_in());

    response->set_is_permitted(ret);
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_check_item_permissions);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_maxkeylen(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS()
    size_t ret;
    try {
        ret = metadataService->metadata_maxkeylen();
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_maxkeylen(ret);
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_maxkeylen);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_update_memoryserver(
    ::grpc::ServerContext *context, const ::Fam_Memservcnt_Request *request,
    ::Fam_Metadata_Gen_Response *response) {
    std::vector<uint64_t> memsrv_persistent_id_list;
    std::vector<uint64_t> memsrv_volatile_id_list;
    METADATA_SERVER_PROFILE_START_OPS()
    for (int ndx = 0; ndx < (int)request->nmemserverspersistent(); ndx++) {
        memsrv_persistent_id_list.push_back(
            request->memsrv_persistent_list(ndx));
    }
    for (int ndx = 0; ndx < (int)request->nmemserversvolatile(); ndx++) {
        memsrv_volatile_id_list.push_back(request->memsrv_volatile_list(ndx));
    }
    metadataService->metadata_update_memoryserver(
        request->nmemserverspersistent(), memsrv_persistent_id_list,
        request->nmemserversvolatile(), memsrv_volatile_id_list);
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_update_memoryserver);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_reset_bitmap(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Gen_Response *response) {

    METADATA_SERVER_PROFILE_START_OPS()
    metadataService->metadata_reset_bitmap(request->region_id());
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_reset_bitmap);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_validate_and_create_region(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Region_Info_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS();
    std::list<int> memoryServerIds;
    Fam_Region_Attributes *regionAttributes = new (Fam_Region_Attributes);
    regionAttributes->redundancyLevel =
        (Fam_Redundancy_Level)request->redundancylevel();
    regionAttributes->memoryType = (Fam_Memory_Type)request->memorytype();
    regionAttributes->interleaveEnable =
        (Fam_Interleave_Enable)request->interleaveenable();
    regionAttributes->permissionLevel =
        (Fam_Permission_Level)request->permission_level();

    uint64_t regionId;
    try {
        metadataService->metadata_validate_and_create_region(
            request->name(), request->size(), &regionId, regionAttributes,
            &memoryServerIds, request->user_policy());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_region_id(regionId);
    for (auto it = memoryServerIds.begin(); it != memoryServerIds.end(); ++it) {
        response->add_memsrv_list(*it);
    }
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_validate_and_create_region);
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Metadata_Service_Server::metadata_validate_and_destroy_region(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Region_Info_Response *response) {

    METADATA_SERVER_PROFILE_START_OPS();
    std::list<int> memoryServerIds;
    try {
        metadataService->metadata_validate_and_destroy_region(
            request->region_id(), request->uid(), request->gid(),
            &memoryServerIds);
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    for (auto it = memoryServerIds.begin(); it != memoryServerIds.end(); ++it) {
        response->add_memsrv_list(*it);
    }
    METADATA_SERVER_PROFILE_END_OPS(
        server_metadata_validate_and_destroy_region);
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Metadata_Service_Server::metadata_validate_and_allocate_dataitem(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Response *response) {
    std::list<int> memoryServerIds;
    METADATA_SERVER_PROFILE_START_OPS();
    size_t interleaveSize;
    Fam_Permission_Level permissionLevel;
    mode_t regionPermission;
    try {
        metadataService->metadata_validate_and_allocate_dataitem(
            request->key_dataitem_name(), request->region_id(), request->uid(),
            request->gid(), request->size(), &memoryServerIds, &interleaveSize,
            &permissionLevel, &regionPermission, request->user_policy());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_interleave_size(interleaveSize);
    response->set_permission_level(permissionLevel);
    response->set_region_permission(regionPermission);
    for (auto it = memoryServerIds.begin(); it != memoryServerIds.end(); ++it) {
        response->add_memsrv_list(*it);
    }
    METADATA_SERVER_PROFILE_END_OPS(
        server_metadata_validate_and_allocate_dataitem);
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Metadata_Service_Server::metadata_validate_and_deallocate_dataitem(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS();
    Fam_DataItem_Metadata dataitem;
    try {
        metadataService->metadata_validate_and_deallocate_dataitem(
            request->region_id(), request->key_dataitem_id(), request->uid(),
            request->gid(), dataitem);
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_region_id(dataitem.regionId);
    response->set_name(dataitem.name);
    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        response->add_offsets(dataitem.offsets[i]);
    }
    response->set_size(dataitem.size);
    response->set_perm(dataitem.perm);
    response->set_uid(dataitem.uid);
    response->set_gid(dataitem.gid);
    response->set_maxkeylen(metadataService->metadata_maxkeylen());
    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        response->add_memsrv_list(dataitem.memoryServerIds[i]);
    }

    METADATA_SERVER_PROFILE_END_OPS(
        server_metadata_validate_and_deallocate_dataitem);
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Metadata_Service_Server::metadata_find_region_and_check_permissions(
    ::grpc::ServerContext *context,
    const ::Fam_Metadata_Region_Request *request,
    ::Fam_Metadata_Region_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS();
    Fam_Region_Metadata region;
    try {
        if (request->has_key_region_id())
            metadataService->metadata_find_region_and_check_permissions(
                (metadata_region_item_op_t)request->op(),
                request->key_region_id(), request->uid(), request->gid(),
                region);
        else
            metadataService->metadata_find_region_and_check_permissions(
                (metadata_region_item_op_t)request->op(),
                request->key_region_name(), request->uid(), request->gid(),
                region);
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_region_id(region.regionId);
    response->set_name(region.name);
    response->set_offset(region.offset);
    response->set_size(region.size);
    response->set_perm(region.perm);
    response->set_uid(region.uid);
    response->set_gid(region.gid);
    response->set_maxkeylen(metadataService->metadata_maxkeylen());
    response->set_memsrv_cnt(region.used_memsrv_cnt);
    response->set_redundancylevel(region.redundancyLevel);
    response->set_memorytype(region.redundancyLevel);
    response->set_interleaveenable(region.redundancyLevel);
    response->set_interleavesize(region.interleaveSize);
    response->set_permission_level(region.permissionLevel);
    for (int id = 0; id < (int)region.used_memsrv_cnt; ++id) {
        response->add_memsrv_list(region.memServerIds[id]);
    }

    METADATA_SERVER_PROFILE_END_OPS(
        server_metadata_find_region_and_check_permissions);
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Metadata_Service_Server::metadata_find_dataitem_and_check_permissions(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS();
    Fam_DataItem_Metadata dataitem;

    try {
        if (request->has_key_dataitem_id() && request->has_key_region_id())
            metadataService->metadata_find_dataitem_and_check_permissions(
                (metadata_region_item_op_t)request->op(),
                request->key_dataitem_id(), request->key_region_id(),
                request->uid(), request->gid(), dataitem);
        else
            metadataService->metadata_find_dataitem_and_check_permissions(
                (metadata_region_item_op_t)request->op(),
                request->key_dataitem_name(), request->key_region_name(),
                request->uid(), request->gid(), dataitem);
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_region_id(dataitem.regionId);
    response->set_name(dataitem.name);
    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        response->add_offsets(dataitem.offsets[i]);
    }
    response->set_size(dataitem.size);
    response->set_perm(dataitem.perm);
    response->set_uid(dataitem.uid);
    response->set_gid(dataitem.gid);
    response->set_maxkeylen(metadataService->metadata_maxkeylen());
    response->set_interleave_size(dataitem.interleaveSize);
    response->set_permission_level(dataitem.permissionLevel);
    for (int i = 0; i < (int)dataitem.used_memsrv_cnt; i++) {
        response->add_memsrv_list(dataitem.memoryServerIds[i]);
    }
    METADATA_SERVER_PROFILE_END_OPS(
        server_metadata_find_dataitem_and_check_permissions);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::get_memory_server_list(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
    ::Fam_Metadata_Region_Info_Response *response) {
    METADATA_SERVER_PROFILE_START_OPS();
    std::list<int> memoryServerIds;
    try {
        memoryServerIds =
            metadataService->get_memory_server_list(request->region_id());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    for (auto it = memoryServerIds.begin(); it != memoryServerIds.end(); ++it) {
        response->add_memsrv_list(*it);
    }
    METADATA_SERVER_PROFILE_END_OPS(server_get_memory_server_list);
    return ::grpc::Status::OK;
}

} // namespace metadata
