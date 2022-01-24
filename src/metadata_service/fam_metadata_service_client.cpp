/*
 *   fam_metadata_service_client.cpp
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

#include "fam_metadata_service_client.h"
#include "common/fam_memserver_profile.h"
using namespace openfam;

namespace metadata {
MEMSERVER_PROFILE_START(METADATA_CLIENT)
#ifdef MEMSERVER_PROFILE
#define METADATA_CLIENT_PROFILE_START_OPS()                                    \
    {                                                                          \
        Profile_Time start = METADATA_CLIENT_get_time();

#define METADATA_CLIENT_PROFILE_END_OPS(apiIdx)                                \
    Profile_Time end = METADATA_CLIENT_get_time();                             \
    Profile_Time total = METADATA_CLIENT_time_diff_nanoseconds(start, end);    \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(METADATA_CLIENT, prof_##apiIdx, total)  \
    }
#define METADATA_CLIENT_PROFILE_DUMP() metadata_client_profile_dump()
#else
#define METADATA_CLIENT_PROFILE_START_OPS()
#define METADATA_CLIENT_PROFILE_END_OPS(apiIdx)
#define METADATA_CLIENT_PROFILE_DUMP()
#endif

void metadata_client_profile_dump() {
    MEMSERVER_PROFILE_END(METADATA_CLIENT);
    MEMSERVER_DUMP_PROFILE_BANNER(METADATA_CLIENT)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(METADATA_CLIENT, name, prof_##name)
#include "metadata_service/metadata_client_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(METADATA_CLIENT, prof_##name)
#include "metadata_service/metadata_client_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(METADATA_CLIENT)
}

Fam_Metadata_Service_Client::Fam_Metadata_Service_Client(const char *name,
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
        throw Metadata_Service_Exception(FAM_ERR_RPC,
                                         (status.error_message()).c_str());
    }
}

Fam_Metadata_Service_Client::~Fam_Metadata_Service_Client() {
    Fam_Metadata_Gen_Request req;
    Fam_Metadata_Gen_Response res;

    ::grpc::ClientContext ctx;

    ::grpc::Status status = stub->signal_termination(&ctx, req, &res);
}

void Fam_Metadata_Service_Client::reset_profile() {
    MEMSERVER_PROFILE_INIT(METADATA_CLIENT)
    MEMSERVER_PROFILE_START_TIME(METADATA_CLIENT)

    Fam_Metadata_Gen_Request req;
    Fam_Metadata_Gen_Response res;

    ::grpc::ClientContext ctx;

    ::grpc::Status status = stub->reset_profile(&ctx, req, &res);
}

void Fam_Metadata_Service_Client::dump_profile() {
    METADATA_CLIENT_PROFILE_DUMP();

    Fam_Metadata_Gen_Request req;
    Fam_Metadata_Gen_Response res;

    ::grpc::ClientContext ctx;

    ::grpc::Status status = stub->dump_profile(&ctx, req, &res);
}

void Fam_Metadata_Service_Client::metadata_insert_region(
    const uint64_t regionId, const std::string regionName,
    Fam_Region_Metadata *region) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Region_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_id(regionId);
    req.set_key_region_name(regionName);
    req.set_region_id(region->regionId);
    req.set_name(region->name);
    req.set_offset(region->offset);
    req.set_size(region->size);
    req.set_perm(region->perm);
    req.set_uid(region->uid);
    req.set_gid(region->gid);
    req.set_redundancylevel(region->redundancyLevel);
    req.set_memorytype(region->memoryType);
    req.set_interleaveenable(region->interleaveEnable);
    req.set_memsrv_cnt(region->used_memsrv_cnt);
    for (int i = 0; i < (int)region->used_memsrv_cnt; i++) {
        req.add_memsrv_list(region->memServerIds[i]);
    }

    ::grpc::Status status = stub->metadata_insert_region(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_insert_region);
}

void Fam_Metadata_Service_Client::metadata_delete_region(
    const uint64_t regionId) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_id(regionId);

    ::grpc::Status status = stub->metadata_delete_region(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_delete_region);
}

void Fam_Metadata_Service_Client::metadata_delete_region(
    const std::string regionName) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_name(regionName);

    ::grpc::Status status = stub->metadata_delete_region(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_delete_region);
}

bool Fam_Metadata_Service_Client::metadata_find_region(
    const uint64_t regionId, Fam_Region_Metadata &region) {
    Fam_Metadata_Request req;
    Fam_Metadata_Region_Response res;
    ::grpc::ClientContext ctx;

    METADATA_CLIENT_PROFILE_START_OPS()
    req.set_key_region_id(regionId);

    ::grpc::Status status = stub->metadata_find_region(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    if (res.isfound()) {
        region.regionId = res.region_id();
        region.offset = res.offset();
        strncpy(region.name, res.name().c_str(), res.maxkeylen());
        region.perm = (mode_t)res.perm();
        region.size = res.size();
        region.uid = res.uid();
        region.gid = res.gid();
        region.used_memsrv_cnt = res.memsrv_cnt();
        for (int i = 0; i < (int)region.used_memsrv_cnt; i++) {
            region.memServerIds[i] = res.memsrv_list(i);
        }
    }
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_find_region);
    return res.isfound();
}

bool Fam_Metadata_Service_Client::metadata_find_region(
    const std::string regionName, Fam_Region_Metadata &region) {
    Fam_Metadata_Request req;
    Fam_Metadata_Region_Response res;
    ::grpc::ClientContext ctx;
    METADATA_CLIENT_PROFILE_START_OPS()

    req.set_key_region_name(regionName);

    ::grpc::Status status = stub->metadata_find_region(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    if (res.isfound()) {
        region.regionId = res.region_id();
        region.offset = res.offset();
        strncpy(region.name, res.name().c_str(), res.maxkeylen());
        region.perm = (mode_t)res.perm();
        region.size = res.size();
        region.uid = res.uid();
        region.gid = res.gid();
        region.used_memsrv_cnt = res.memsrv_cnt();
        for (int i = 0; i < (int)region.used_memsrv_cnt; i++) {
            region.memServerIds[i] = res.memsrv_list(i);
        }
    }
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_find_region);
    return res.isfound();
}

void Fam_Metadata_Service_Client::metadata_modify_region(
    const uint64_t regionId, Fam_Region_Metadata *region) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Region_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_id(regionId);
    req.set_region_id(regionId);
    req.set_name(region->name);
    req.set_offset(region->offset);
    req.set_size(region->size);
    req.set_perm(region->perm);
    req.set_uid(region->uid);
    req.set_gid(region->gid);
    req.set_memsrv_cnt(region->used_memsrv_cnt);
    for (int i = 0; i < (int)region->used_memsrv_cnt; i++) {
        req.add_memsrv_list(region->memServerIds[i]);
    }
    ::grpc::Status status = stub->metadata_modify_region(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_modify_region);
}

void Fam_Metadata_Service_Client::metadata_modify_region(
    const std::string regionName, Fam_Region_Metadata *region) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Region_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_name(regionName);
    req.set_region_id(region->regionId);
    req.set_name(regionName);
    req.set_offset(region->offset);
    req.set_size(region->size);
    req.set_perm(region->perm);
    req.set_uid(region->uid);
    req.set_gid(region->gid);
    req.set_memsrv_cnt(region->used_memsrv_cnt);
    for (int i = 0; i < (int)region->used_memsrv_cnt; i++) {
        req.add_memsrv_list(region->memServerIds[i]);
    }

    ::grpc::Status status = stub->metadata_modify_region(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_modify_region);
}

void Fam_Metadata_Service_Client::metadata_insert_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_id(regionId);
    req.set_key_dataitem_id(dataitemId);
    req.set_key_dataitem_name(dataitemName);
    req.set_region_id(regionId);
    req.set_name(dataitem->name);
    req.set_offset(dataitem->offset);
    req.set_size(dataitem->size);
    req.set_perm(dataitem->perm);
    req.set_uid(dataitem->uid);
    req.set_gid(dataitem->gid);
    req.set_memsrv_id(dataitem->memoryServerId);

    ::grpc::Status status = stub->metadata_insert_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_insert_dataitem);
}

void Fam_Metadata_Service_Client::metadata_insert_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_name(regionName);
    req.set_key_dataitem_id(dataitemId);
    req.set_key_dataitem_name(dataitemName);
    req.set_region_id(dataitem->regionId);
    req.set_name(dataitem->name);
    req.set_offset(dataitem->offset);
    req.set_size(dataitem->size);
    req.set_perm(dataitem->perm);
    req.set_uid(dataitem->uid);
    req.set_gid(dataitem->gid);
    req.set_memsrv_id(dataitem->memoryServerId);

    ::grpc::Status status = stub->metadata_insert_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_insert_dataitem);
}

void Fam_Metadata_Service_Client::metadata_delete_dataitem(
    const uint64_t dataitemId, const std::string regionName) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_name(regionName);
    req.set_key_dataitem_id(dataitemId);

    ::grpc::Status status = stub->metadata_delete_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_delete_dataitem);
}

void Fam_Metadata_Service_Client::metadata_delete_dataitem(
    const uint64_t dataitemId, const uint64_t regionId) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_id(regionId);
    req.set_key_dataitem_id(dataitemId);

    ::grpc::Status status = stub->metadata_delete_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_delete_dataitem);
}

void Fam_Metadata_Service_Client::metadata_delete_dataitem(
    const std::string dataitemName, const std::string regionName) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_name(regionName);
    req.set_key_dataitem_name(dataitemName);

    ::grpc::Status status = stub->metadata_delete_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_delete_dataitem);
}

void Fam_Metadata_Service_Client::metadata_delete_dataitem(
    const std::string dataitemName, const uint64_t regionId) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_id(regionId);
    req.set_key_dataitem_name(dataitemName);

    ::grpc::Status status = stub->metadata_delete_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_delete_dataitem);
}

bool Fam_Metadata_Service_Client::metadata_find_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;
    METADATA_CLIENT_PROFILE_START_OPS()

    req.set_key_region_id(regionId);
    req.set_key_dataitem_id(dataitemId);

    ::grpc::Status status = stub->metadata_find_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    if (res.isfound()) {
        dataitem.regionId = res.region_id();
        dataitem.offset = res.offset();
        dataitem.size = res.size();
        dataitem.perm = (mode_t)res.perm();
        strncpy(dataitem.name, res.name().c_str(), res.maxkeylen());
        dataitem.uid = res.uid();
        dataitem.gid = res.gid();
        dataitem.memoryServerId = res.memsrv_id();
    }
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_find_dataitem);
    return res.isfound();
}

bool Fam_Metadata_Service_Client::metadata_find_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;
    METADATA_CLIENT_PROFILE_START_OPS()

    req.set_key_region_name(regionName);
    req.set_key_dataitem_id(dataitemId);

    ::grpc::Status status = stub->metadata_find_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    if (res.isfound()) {
        dataitem.regionId = res.region_id();
        dataitem.offset = res.offset();
        dataitem.size = res.size();
        dataitem.perm = (mode_t)res.perm();
        strncpy(dataitem.name, res.name().c_str(), res.maxkeylen());
        dataitem.uid = res.uid();
        dataitem.gid = res.gid();
        dataitem.memoryServerId = res.memsrv_id();
    }
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_find_dataitem);
    return res.isfound();
}

bool Fam_Metadata_Service_Client::metadata_find_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;
    METADATA_CLIENT_PROFILE_START_OPS()

    req.set_key_region_id(regionId);
    req.set_key_dataitem_name(dataitemName);

    ::grpc::Status status = stub->metadata_find_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    if (res.isfound()) {
        dataitem.regionId = res.region_id();
        dataitem.offset = res.offset();
        dataitem.size = res.size();
        dataitem.perm = (mode_t)res.perm();
        strncpy(dataitem.name, res.name().c_str(), res.maxkeylen());
        dataitem.uid = res.uid();
        dataitem.gid = res.gid();
        dataitem.memoryServerId = res.memsrv_id();
    }
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_find_dataitem);
    return res.isfound();
}

bool Fam_Metadata_Service_Client::metadata_find_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;
    METADATA_CLIENT_PROFILE_START_OPS()

    req.set_key_region_name(regionName);
    req.set_key_dataitem_name(dataitemName);

    ::grpc::Status status = stub->metadata_find_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    if (res.isfound()) {
        dataitem.regionId = res.region_id();
        dataitem.offset = res.offset();
        dataitem.size = res.size();
        dataitem.perm = (mode_t)res.perm();
        strncpy(dataitem.name, res.name().c_str(), res.maxkeylen());
        dataitem.uid = res.uid();
        dataitem.gid = res.gid();
        dataitem.memoryServerId = res.memsrv_id();
    }
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_find_dataitem);
    return res.isfound();
}

void Fam_Metadata_Service_Client::metadata_modify_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_id(regionId);
    req.set_key_dataitem_id(dataitemId);
    req.set_region_id(regionId);
    req.set_name(dataitem->name);
    req.set_offset(dataitem->offset);
    req.set_uid(dataitem->uid);
    req.set_gid(dataitem->gid);
    req.set_size(dataitem->size);
    req.set_perm(dataitem->perm);
    req.set_memsrv_id(dataitem->memoryServerId);

    ::grpc::Status status = stub->metadata_modify_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_modify_dataitem);
}

void Fam_Metadata_Service_Client::metadata_modify_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_name(regionName);
    req.set_key_dataitem_id(dataitemId);
    req.set_region_id(dataitem->regionId);
    req.set_name(dataitem->name);
    req.set_offset(dataitem->offset);
    req.set_uid(dataitem->uid);
    req.set_gid(dataitem->gid);
    req.set_size(dataitem->size);
    req.set_perm(dataitem->perm);
    req.set_memsrv_id(dataitem->memoryServerId);

    ::grpc::Status status = stub->metadata_modify_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_modify_dataitem);
}

void Fam_Metadata_Service_Client::metadata_modify_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_id(regionId);
    req.set_key_dataitem_name(dataitemName);
    req.set_region_id(dataitem->regionId);
    req.set_name(dataitem->name);
    req.set_offset(dataitem->offset);
    req.set_uid(dataitem->uid);
    req.set_gid(dataitem->gid);
    req.set_size(dataitem->size);
    req.set_perm(dataitem->perm);
    req.set_memsrv_id(dataitem->memoryServerId);

    ::grpc::Status status = stub->metadata_modify_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_modify_dataitem);
}

void Fam_Metadata_Service_Client::metadata_modify_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_region_name(regionName);
    req.set_key_dataitem_name(dataitemName);
    req.set_region_id(dataitem->regionId);
    req.set_name(dataitem->name);
    req.set_offset(dataitem->offset);
    req.set_uid(dataitem->uid);
    req.set_gid(dataitem->gid);
    req.set_size(dataitem->size);
    req.set_perm(dataitem->perm);
    req.set_memsrv_id(dataitem->memoryServerId);

    ::grpc::Status status = stub->metadata_modify_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_modify_dataitem);
}

bool Fam_Metadata_Service_Client::metadata_check_permissions(
    Fam_DataItem_Metadata *dataitem, metadata_region_item_op_t op, uint32_t uid,
    uint32_t gid) {
    Fam_Permission_Request req;
    Fam_Permission_Response res;
    ::grpc::ClientContext ctx;
    METADATA_CLIENT_PROFILE_START_OPS()

    req.set_uid_meta(dataitem->uid);
    req.set_gid_meta(dataitem->gid);
    req.set_uid_in(uid);
    req.set_gid_in(gid);
    req.set_perm(dataitem->perm);
    req.set_ops(static_cast<::Fam_Permission_Request_meta_ops>(op));
    ::grpc::Status status =
        stub->metadata_check_item_permissions(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_check_permissions);
    return res.is_permitted();
}

bool Fam_Metadata_Service_Client::metadata_check_permissions(
    Fam_Region_Metadata *region, metadata_region_item_op_t op, uint32_t uid,
    uint32_t gid) {
    Fam_Permission_Request req;
    Fam_Permission_Response res;
    ::grpc::ClientContext ctx;
    METADATA_CLIENT_PROFILE_START_OPS()

    req.set_uid_meta(region->uid);
    req.set_gid_meta(region->gid);
    req.set_uid_in(uid);
    req.set_gid_in(gid);
    req.set_perm(region->perm);
    req.set_ops(static_cast<::Fam_Permission_Request_meta_ops>(op));
    ::grpc::Status status =
        stub->metadata_check_region_permissions(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_check_permissions);
    return res.is_permitted();
}

size_t Fam_Metadata_Service_Client::metadata_maxkeylen() {
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;
    METADATA_CLIENT_PROFILE_START_OPS()

    ::grpc::Status status = stub->metadata_maxkeylen(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_maxkeylen);
    return (size_t)res.maxkeylen();
}

void Fam_Metadata_Service_Client::metadata_update_memoryserver(
    int nmemServersPersistent, std::vector<uint64_t> memsrv_persistent_id_list,
    int nmemServersVolatile, std::vector<uint64_t> memsrv_volatile_id_list) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Memservcnt_Request req;
    Fam_Metadata_Gen_Response res;
    ::grpc::ClientContext ctx;
    req.set_nmemserverspersistent(nmemServersPersistent);
    for (int i = 0; i < (int)nmemServersPersistent; i++) {
        req.add_memsrv_persistent_list(memsrv_persistent_id_list[i]);
    }
    req.set_nmemserversvolatile(nmemServersVolatile);
    for (int i = 0; i < (int)nmemServersVolatile; i++) {
        req.add_memsrv_volatile_list(memsrv_volatile_id_list[i]);
    }

    ::grpc::Status status = stub->metadata_update_memoryserver(&ctx, req, &res);
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_update_memoryserver);
}

void Fam_Metadata_Service_Client::metadata_reset_bitmap(uint64_t regionId) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Gen_Response res;
    ::grpc::ClientContext ctx;
    req.set_region_id(regionId);
    ::grpc::Status status = stub->metadata_reset_bitmap(&ctx, req, &res);
    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_reset_bitmap);
}

void Fam_Metadata_Service_Client::metadata_validate_and_create_region(
    const std::string regionname, size_t size, uint64_t *regionid,
    Fam_Region_Attributes *regionAttributes, std::list<int> *memory_server_list,
    int user_policy) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Region_Info_Response res;
    ::grpc::ClientContext ctx;
    req.set_region_id(*regionid);
    req.set_name(regionname);
    req.set_size(size);
    req.set_user_policy(user_policy);
    req.set_redundancylevel(regionAttributes->redundancyLevel);
    req.set_memorytype(regionAttributes->memoryType);
    req.set_interleaveenable(regionAttributes->interleaveEnable);

    ::grpc::Status status =
        stub->metadata_validate_and_create_region(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)

    *regionid = res.region_id();
    int memsrv_count = res.memserv_list_size();
    for (int i = 0; i < memsrv_count; i++) {
        memory_server_list->push_back((res.memserv_list(i)));
    }

    METADATA_CLIENT_PROFILE_END_OPS(client_metadata_validate_and_create_region);
}

void Fam_Metadata_Service_Client::metadata_validate_and_destroy_region(
    const uint64_t regionId, uint32_t uid, uint32_t gid,
    std::list<int> *memory_server_list) {

    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Region_Info_Response res;
    ::grpc::ClientContext ctx;

    req.set_region_id(regionId);
    req.set_uid(uid);
    req.set_gid(gid);

    ::grpc::Status status =
        stub->metadata_validate_and_destroy_region(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    int memsrv_count = res.memserv_list_size();
    for (int i = 0; i < memsrv_count; i++) {
        memory_server_list->push_back((res.memserv_list(i)));
    }
    METADATA_CLIENT_PROFILE_END_OPS(
        client_metadata_validate_and_destroy_region);
}

void Fam_Metadata_Service_Client::metadata_find_region_and_check_permissions(
    metadata_region_item_op_t op, const uint64_t regionId, uint32_t uid,
    uint32_t gid, Fam_Region_Metadata &region) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Region_Request req;
    Fam_Metadata_Region_Response res;
    ::grpc::ClientContext ctx;
    req.set_op(op);
    req.set_key_region_id(regionId);
    req.set_uid(uid);
    req.set_gid(gid);

    ::grpc::Status status =
        stub->metadata_find_region_and_check_permissions(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    region.regionId = res.region_id();
    region.offset = res.offset();
    strncpy(region.name, res.name().c_str(), res.maxkeylen());
    region.perm = (mode_t)res.perm();
    region.size = res.size();
    region.uid = res.uid();
    region.gid = res.gid();
    region.used_memsrv_cnt = res.memsrv_cnt();
    for (int i = 0; i < (int)region.used_memsrv_cnt; i++) {
        region.memServerIds[i] = res.memsrv_list(i);
    }

    METADATA_CLIENT_PROFILE_END_OPS(
        client_metadata_find_region_and_check_permissions);
}

void Fam_Metadata_Service_Client::metadata_find_region_and_check_permissions(
    metadata_region_item_op_t op, const std::string regionName, uint32_t uid,
    uint32_t gid, Fam_Region_Metadata &region) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Region_Request req;
    Fam_Metadata_Region_Response res;
    ::grpc::ClientContext ctx;
    req.set_op(op);
    req.set_key_region_name(regionName);
    req.set_uid(uid);
    req.set_gid(gid);

    ::grpc::Status status =
        stub->metadata_find_region_and_check_permissions(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    region.regionId = res.region_id();
    region.offset = res.offset();
    strncpy(region.name, res.name().c_str(), res.maxkeylen());
    region.perm = (mode_t)res.perm();
    region.size = res.size();
    region.uid = res.uid();
    region.gid = res.gid();
    region.used_memsrv_cnt = res.memsrv_cnt();
    for (int i = 0; i < (int)region.used_memsrv_cnt; i++) {
        region.memServerIds[i] = res.memsrv_list(i);
    }

    METADATA_CLIENT_PROFILE_END_OPS(
        client_metadata_find_region_and_check_permissions);
}

void Fam_Metadata_Service_Client::metadata_find_dataitem_and_check_permissions(
    metadata_region_item_op_t op, const uint64_t dataitemId,
    const uint64_t regionId, uint32_t uid, uint32_t gid,
    Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;
    METADATA_CLIENT_PROFILE_START_OPS()
    req.set_key_region_id(regionId);
    req.set_key_dataitem_id(dataitemId);
    req.set_op(op);
    req.set_uid(uid);
    req.set_gid(gid);

    ::grpc::Status status =
        stub->metadata_find_dataitem_and_check_permissions(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    dataitem.regionId = res.region_id();
    dataitem.offset = res.offset();
    dataitem.size = res.size();
    dataitem.perm = (mode_t)res.perm();
    strncpy(dataitem.name, res.name().c_str(), res.maxkeylen());
    dataitem.uid = res.uid();
    dataitem.gid = res.gid();
    dataitem.memoryServerId = res.memsrv_id();
    METADATA_CLIENT_PROFILE_END_OPS(
        client_metadata_find_dataitem_and_check_permissions);
}

void Fam_Metadata_Service_Client::metadata_find_dataitem_and_check_permissions(
    metadata_region_item_op_t op, const std::string dataitemName,
    const std::string regionName, uint32_t uid, uint32_t gid,
    Fam_DataItem_Metadata &dataitem) {
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;
    METADATA_CLIENT_PROFILE_START_OPS()
    req.set_key_region_name(regionName);
    req.set_key_dataitem_name(dataitemName);
    req.set_op(op);
    req.set_uid(uid);
    req.set_gid(gid);

    ::grpc::Status status =
        stub->metadata_find_dataitem_and_check_permissions(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    dataitem.regionId = res.region_id();
    dataitem.offset = res.offset();
    dataitem.size = res.size();
    dataitem.perm = (mode_t)res.perm();
    strncpy(dataitem.name, res.name().c_str(), res.maxkeylen());
    dataitem.uid = res.uid();
    dataitem.gid = res.gid();
    dataitem.memoryServerId = res.memsrv_id();
    METADATA_CLIENT_PROFILE_END_OPS(
        client_metadata_find_dataitem_and_check_permissions);
}

void Fam_Metadata_Service_Client::metadata_validate_and_allocate_dataitem(
    const std::string dataitemName, const uint64_t regionId, uint32_t uid,
    uint32_t gid, uint64_t *memoryServerId) {
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_key_dataitem_name(dataitemName);
    req.set_region_id(regionId);
    req.set_uid(uid);
    req.set_gid(gid);
    ::grpc::Status status =
        stub->metadata_validate_and_allocate_dataitem(&ctx, req, &res);

    STATUS_CHECK(Metadata_Service_Exception)
    *memoryServerId = res.memsrv_id();
    METADATA_CLIENT_PROFILE_END_OPS(
        client_metadata_validate_and_allocate_dataitem);
}

void Fam_Metadata_Service_Client::metadata_validate_and_deallocate_dataitem(
    const uint64_t regionId, const uint64_t dataitemId, uint32_t uid,
    uint32_t gid) {

    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Response res;
    ::grpc::ClientContext ctx;

    req.set_region_id(regionId);
    req.set_key_dataitem_id(dataitemId);
    req.set_uid(uid);
    req.set_gid(gid);
    ::grpc::Status status =
        stub->metadata_validate_and_deallocate_dataitem(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)
    METADATA_CLIENT_PROFILE_END_OPS(
        client_metadata_validate_and_deallocate_dataitem);
}

std::list<int>
Fam_Metadata_Service_Client::get_memory_server_list(uint64_t regionId) {
    std::list<int> memory_server_list;
    METADATA_CLIENT_PROFILE_START_OPS()
    Fam_Metadata_Request req;
    Fam_Metadata_Region_Info_Response res;
    ::grpc::ClientContext ctx;

    req.set_region_id(regionId);

    ::grpc::Status status = stub->get_memory_server_list(&ctx, req, &res);
    STATUS_CHECK(Metadata_Service_Exception)

    int memsrv_count = res.memserv_list_size();
    for (int i = 0; i < memsrv_count; i++) {
        memory_server_list.push_back((res.memserv_list(i)));
    }

    METADATA_CLIENT_PROFILE_END_OPS(client_get_memory_server_list);
    return memory_server_list;
}
} // namespace metadata
