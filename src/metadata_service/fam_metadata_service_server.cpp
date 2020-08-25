/*
 *   fam_metadata_service_server.cpp
 *   Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All rights
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

#include "fam_metadata_service_server.h"
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

Fam_Metadata_Service_Server::Fam_Metadata_Service_Server(uint64_t rpcPort,
                                                         char *name)
    : serverAddress(name), port(rpcPort) {
    metadataService = new Fam_Metadata_Service_Direct();
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
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::signal_termination(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Gen_Request *request,
    ::Fam_Metadata_Gen_Response *response) {
    __sync_add_and_fetch(&numClients, -1);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_insert_region(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
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
    try {
        metadataService->metadata_insert_region(
            request->key_region_id(), request->key_region_name(), region);
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
    ::Fam_Metadata_Response *response) {
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
    }
    METADATA_SERVER_PROFILE_END_OPS(server_metadata_find_region);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Metadata_Service_Server::metadata_modify_region(
    ::grpc::ServerContext *context, const ::Fam_Metadata_Request *request,
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
    dataitem->offset = request->offset();
    dataitem->size = request->size();
    dataitem->perm = (mode_t)request->perm();
    dataitem->uid = request->uid();
    dataitem->gid = request->gid();
    try {
        if (request->has_key_region_id())
            metadataService->metadata_insert_dataitem(
                request->key_dataitem_id(), request->key_region_id(), dataitem,
                request->key_dataitem_name());
        else
            metadataService->metadata_insert_dataitem(
                request->key_dataitem_id(), request->key_region_name(),
                dataitem, request->key_dataitem_name());
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
        if (request->has_key_dataitem_id() && request->has_key_region_id())
            ret = metadataService->metadata_find_dataitem(
                request->key_dataitem_id(), request->key_region_id(), dataitem);
        else if (request->has_key_dataitem_id() &&
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
        response->set_offset(dataitem.offset);
        response->set_size(dataitem.size);
        response->set_perm(dataitem.perm);
        response->set_uid(dataitem.uid);
        response->set_gid(dataitem.gid);
        response->set_maxkeylen(metadataService->metadata_maxkeylen());
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
    dataitem->offset = request->offset();
    dataitem->size = request->size();
    dataitem->perm = (mode_t)request->perm();
    dataitem->uid = request->uid();
    dataitem->gid = request->gid();
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
} // namespace metadata
