/*
 *   fam_metadata_service_thallium_server.cpp
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

#include "fam_metadata_service_thallium_server.h"
#include "common/fam_config_info.h"
#include "common/fam_memserver_profile.h"
#include "fam_metadata_service_server.h"
#include "fam_metadata_service_thallium_rpc_structures.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thread>

#define NUM_THREADS 12
#define ok true
#define error false
#define HAS_KEY_DATAITEM_ID_REGION_ID 1
#define HAS_KEY_DATAITEM_ID_REGION_NAME 2
#define HAS_KEY_DATAITEM_NAME_REGION_ID 3
#define HAS_KEY_DATAITEM_NAME_REGION_NAME 4

using namespace std;

namespace tl = thallium;
namespace metadata {
MEMSERVER_PROFILE_START(METADATA_THALLIUM_SERVER)
#ifdef MEMSERVER_PROFILE
#define METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
{
    Profile_Time start = METADATA_THALLIUM_SERVER_get_time();

#define METADATA_THALLIUM_SERVER_PROFILE_END_OPS(apiIdx)                       \
    Profile_Time end = METADATA_THALLIUM_SERVER_get_time();                    \
    Profile_Time total =                                                       \
        METADATA_THALLIUM_SERVER_time_diff_nanoseconds(start, end);            \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(METADATA_THALLIUM_SERVER,               \
                                       prof_##apiIdx, total)                   \
    }
#define METADATA_THALLIUM_SERVER_PROFILE_DUMP()                                \
    metadata_thallium_server_profile_dump()
#else
#define METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
#define METADATA_THALLIUM_SERVER_PROFILE_END_OPS(apiIdx)
#define METADATA_THALLIUM_SERVER_PROFILE_DUMP()
#endif

    void metadata_thallium_server_profile_dump() {
        MEMSERVER_PROFILE_END(METADATA_THALLIUM_SERVER);
        MEMSERVER_DUMP_PROFILE_BANNER(METADATA_THALLIUM_SERVER)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(METADATA_THALLIUM_SERVER, name, prof_##name)
#include "metadata_service/metadata_thallium_server_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(METADATA_THALLIUM_SERVER, prof_##name)
#include "metadata_service/metadata_thallium_server_counters.tbl"
        MEMSERVER_DUMP_PROFILE_SUMMARY(METADATA_THALLIUM_SERVER)
    }

    Fam_Metadata_Service_Thallium_Server::Fam_Metadata_Service_Thallium_Server(
        Fam_Metadata_Service_Direct * direct_metadataService_,
        const tl::engine &myEngine)
        : tl::provider<Fam_Metadata_Service_Thallium_Server>(myEngine, 10) {
        direct_metadataService = direct_metadataService_;
        this->myEngine = myEngine;
        MEMSERVER_PROFILE_INIT(METADATA_THALLIUM_SERVER)
        MEMSERVER_PROFILE_START_TIME(METADATA_THALLIUM_SERVER)
    }

    Fam_Metadata_Service_Thallium_Server::
        ~Fam_Metadata_Service_Thallium_Server() {}

    void Fam_Metadata_Service_Thallium_Server::reset_profile() {
        MEMSERVER_PROFILE_INIT(METADATA_THALLIUM_SERVER)
        MEMSERVER_PROFILE_START_TIME(METADATA_THALLIUM_SERVER)
    }

    void Fam_Metadata_Service_Thallium_Server::dump_profile() {
        METADATA_THALLIUM_SERVER_PROFILE_DUMP();
    }

    void Fam_Metadata_Service_Thallium_Server::run() {
        string addr(myEngine.self());
        direct_metadataService->set_controlpath_addr(addr);

        // creating argobots pool with ES
        std::vector<tl::managed<tl::xstream>> ess;
        tl::managed<tl::pool> myPool = tl::pool::create(tl::pool::access::spmc);
        for (int i = 0; i < 12; i++) {
            tl::managed<tl::xstream> es =
                tl::xstream::create(tl::scheduler::predef::deflt, *myPool);
            ess.push_back(std::move(es));
        }

        // server rpc structure definitions with argobots pool
        define("metadata_insert_region",
               &Fam_Metadata_Service_Thallium_Server::metadata_insert_region,
               *myPool);
        define("metadata_delete_region",
               &Fam_Metadata_Service_Thallium_Server::metadata_delete_region,
               *myPool);
        define("metadata_find_region",
               &Fam_Metadata_Service_Thallium_Server::metadata_find_region,
               *myPool);
        define("metadata_modify_region",
               &Fam_Metadata_Service_Thallium_Server::metadata_modify_region,
               *myPool);
        define("metadata_insert_dataitem",
               &Fam_Metadata_Service_Thallium_Server::metadata_insert_dataitem,
               *myPool);
        define("metadata_delete_dataitem",
               &Fam_Metadata_Service_Thallium_Server::metadata_delete_dataitem,
               *myPool);
        define("metadata_find_dataitem",
               &Fam_Metadata_Service_Thallium_Server::metadata_find_dataitem,
               *myPool);
        define("metadata_modify_dataitem",
               &Fam_Metadata_Service_Thallium_Server::metadata_modify_dataitem,
               *myPool);
        define("metadata_check_region_permissions",
               &Fam_Metadata_Service_Thallium_Server::
                   metadata_check_region_permissions,
               *myPool);
        define("metadata_check_item_permissions",
               &Fam_Metadata_Service_Thallium_Server::
                   metadata_check_item_permissions,
               *myPool);
        define("metadata_maxkeylen",
               &Fam_Metadata_Service_Thallium_Server::metadata_maxkeylen,
               *myPool);
        define(
            "metadata_update_memoryserver",
            &Fam_Metadata_Service_Thallium_Server::metadata_update_memoryserver,
            *myPool);
        define("metadata_reset_bitmap",
               &Fam_Metadata_Service_Thallium_Server::metadata_reset_bitmap,
               *myPool);
        define("metadata_validate_and_create_region",
               &Fam_Metadata_Service_Thallium_Server::
                   metadata_validate_and_create_region,
               *myPool);
        define("metadata_validate_and_destroy_region",
               &Fam_Metadata_Service_Thallium_Server::
                   metadata_validate_and_destroy_region,
               *myPool);
        define("metadata_validate_and_deallocate_dataitem",
               &Fam_Metadata_Service_Thallium_Server::
                   metadata_validate_and_deallocate_dataitem,
               *myPool);
        define("metadata_validate_and_allocate_dataitem",
               &Fam_Metadata_Service_Thallium_Server::
                   metadata_validate_and_allocate_dataitem,
               *myPool);
        define("metadata_find_region_and_check_permissions",
               &Fam_Metadata_Service_Thallium_Server::
                   metadata_find_region_and_check_permissions,
               *myPool);
        define("metadata_find_dataitem_and_check_permissions",
               &Fam_Metadata_Service_Thallium_Server::
                   metadata_find_dataitem_and_check_permissions,
               *myPool);
        define("get_memory_server_list",
               &Fam_Metadata_Service_Thallium_Server::get_memory_server_list,
               *myPool);
        define("reset_profile",
               &Fam_Metadata_Service_Thallium_Server::reset_profile);
        define("dump_profile",
               &Fam_Metadata_Service_Thallium_Server::dump_profile);
        myEngine.wait_for_finalize();

        for (int i = 0; i < NUM_THREADS; i++) {
            ess[i]->join();
        }
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_insert_region(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        Fam_Region_Metadata *region = new Fam_Region_Metadata();
        region->regionId = metaRequest.get_region_id();
        strncpy(region->name, (metaRequest.get_name()).c_str(),
                direct_metadataService->metadata_maxkeylen());
        region->offset = INVALID_OFFSET;
        region->perm = (mode_t)metaRequest.get_perm();
        region->size = metaRequest.get_size();
        region->uid = metaRequest.get_uid();
        region->gid = metaRequest.get_gid();
        region->used_memsrv_cnt = metaRequest.get_memsrv_cnt();
        region->redundancyLevel =
            (Fam_Redundancy_Level)metaRequest.get_redundancylevel();
        region->memoryType = (Fam_Memory_Type)metaRequest.get_memorytype();
        region->interleaveEnable =
            (Fam_Interleave_Enable)metaRequest.get_interleaveenable();
        region->permissionLevel =
            (Fam_Permission_Level)metaRequest.get_permission_level();

        for (int ndx = 0; ndx < (int)region->used_memsrv_cnt; ndx++) {
            region->memServerIds[ndx] = metaRequest.get_memsrv_list()[ndx];
        }

        try {
            direct_metadataService->metadata_insert_region(
                metaRequest.get_key_region_id(),
                metaRequest.get_key_region_name(), region);
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_thallium_server_metadata_insert_region);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_delete_region(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        try {
            if (metaRequest.get_check_name_id())
                direct_metadataService->metadata_delete_region(
                    metaRequest.get_key_region_id());
            else
                direct_metadataService->metadata_delete_region(
                    metaRequest.get_key_region_name());
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_thallium_server_metadata_delete_region);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_find_region(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        Fam_Region_Metadata region;
        bool ret = 0;
        try {
            if (metaRequest.get_check_name_id())
                ret = direct_metadataService->metadata_find_region(
                    metaRequest.get_key_region_id(), region);
            else
                ret = direct_metadataService->metadata_find_region(
                    metaRequest.get_key_region_name(), region);
            metaResponse.set_isfound(ret);
            if (ret) {
                metaResponse.set_region_id(region.regionId);
                metaResponse.set_name(region.name);
                metaResponse.set_offset(region.offset);
                metaResponse.set_size(region.size);
                metaResponse.set_perm(region.perm);
                metaResponse.set_uid(region.uid);
                metaResponse.set_gid(region.gid);
                metaResponse.set_maxkeylen(
                    direct_metadataService->metadata_maxkeylen());
                metaResponse.set_memsrv_cnt(region.used_memsrv_cnt);
                metaResponse.set_redundancylevel(region.redundancyLevel);
                metaResponse.set_memorytype(region.redundancyLevel);
                metaResponse.set_interleaveenable(region.redundancyLevel);
                metaResponse.set_interleavesize(region.interleaveSize);
                metaResponse.set_permission_level(region.permissionLevel);
                metaResponse.set_memsrv_list(region.memServerIds,
                                             (int)region.used_memsrv_cnt);
            }
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_find_region);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_modify_region(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        Fam_Region_Metadata *region = new Fam_Region_Metadata();
        region->regionId = metaRequest.get_region_id();
        strncpy(region->name, (metaRequest.get_name()).c_str(),
                direct_metadataService->metadata_maxkeylen());
        region->offset = INVALID_OFFSET;
        region->perm = (mode_t)metaRequest.get_perm();
        region->size = metaRequest.get_size();
        region->uid = metaRequest.get_uid();
        region->gid = metaRequest.get_gid();
        region->used_memsrv_cnt = metaRequest.get_memsrv_cnt();
        region->redundancyLevel =
            (Fam_Redundancy_Level)metaRequest.get_redundancylevel();
        region->memoryType = (Fam_Memory_Type)metaRequest.get_memorytype();
        region->interleaveEnable =
            (Fam_Interleave_Enable)metaRequest.get_interleaveenable();
        region->interleaveSize = metaRequest.get_interleavesize();
        region->permissionLevel =
            (Fam_Permission_Level)metaRequest.get_permission_level();
        for (int id = 0; id < (int)region->used_memsrv_cnt; ++id) {
            region->memServerIds[id] = metaRequest.get_memsrv_list()[id];
        }
        try {
            if (metaRequest.get_key_region_id())
                direct_metadataService->metadata_modify_region(
                    metaRequest.get_key_region_id(), region);
            else
                direct_metadataService->metadata_modify_region(
                    metaRequest.get_key_region_name(), region);
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_modify_region);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_insert_dataitem(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        Fam_DataItem_Metadata *dataitem = new Fam_DataItem_Metadata();
        dataitem->regionId = metaRequest.get_region_id();
        strncpy(dataitem->name, (metaRequest.get_name()).c_str(),
                direct_metadataService->metadata_maxkeylen());
        dataitem->used_memsrv_cnt = metaRequest.get_memsrv_cnt();
        for (int ndx = 0; ndx < (int)dataitem->used_memsrv_cnt; ndx++) {
            dataitem->offsets[ndx] = metaRequest.get_offsets()[ndx];
        }
        dataitem->size = metaRequest.get_size();
        dataitem->perm = (mode_t)metaRequest.get_perm();
        dataitem->uid = metaRequest.get_uid();
        dataitem->gid = metaRequest.get_gid();
        dataitem->interleaveSize = metaRequest.get_interleave_size();
        dataitem->permissionLevel =
            (Fam_Permission_Level)metaRequest.get_permission_level();
        for (int ndx = 0; ndx < (int)dataitem->used_memsrv_cnt; ndx++) {
            dataitem->memoryServerIds[ndx] = metaRequest.get_memsrv_list()[ndx];
        }
        try {
            if (metaRequest.get_key_region_id())
                direct_metadataService->metadata_insert_dataitem(
                    metaRequest.get_key_dataitem_id(),
                    metaRequest.get_key_region_id(), dataitem,
                    metaRequest.get_key_dataitem_name());
            else
                direct_metadataService->metadata_insert_dataitem(
                    metaRequest.get_key_dataitem_id(),
                    metaRequest.get_key_region_name(), dataitem,
                    metaRequest.get_key_dataitem_name());
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_insert_dataitem);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_delete_dataitem(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        try {
            if (metaRequest.get_type_flag() == HAS_KEY_DATAITEM_ID_REGION_NAME)
                direct_metadataService->metadata_delete_dataitem(
                    metaRequest.get_key_dataitem_id(),
                    metaRequest.get_key_region_name());
            else if (metaRequest.get_type_flag() ==
                     HAS_KEY_DATAITEM_ID_REGION_ID)
                direct_metadataService->metadata_delete_dataitem(
                    metaRequest.get_key_dataitem_id(),
                    metaRequest.get_key_region_id());
            else if (metaRequest.get_type_flag() ==
                     HAS_KEY_DATAITEM_NAME_REGION_NAME)
                direct_metadataService->metadata_delete_dataitem(
                    metaRequest.get_key_dataitem_name(),
                    metaRequest.get_key_region_name());
            else
                (direct_metadataService->metadata_delete_dataitem(
                    metaRequest.get_key_dataitem_name(),
                    metaRequest.get_key_region_id()));
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_delete_dataitem);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_find_dataitem(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        Fam_DataItem_Metadata dataitem;
        bool ret = 0;
        try {
            if (metaRequest.get_type_flag() == HAS_KEY_DATAITEM_ID_REGION_ID)
                ret = direct_metadataService->metadata_find_dataitem(
                    metaRequest.get_key_dataitem_id(),
                    metaRequest.get_key_region_id(), dataitem);
            else if (metaRequest.get_type_flag() ==
                     HAS_KEY_DATAITEM_ID_REGION_NAME)
                ret = direct_metadataService->metadata_find_dataitem(
                    metaRequest.get_key_dataitem_id(),
                    metaRequest.get_key_region_name(), dataitem);
            else if (metaRequest.get_type_flag() ==
                     HAS_KEY_DATAITEM_NAME_REGION_ID)
                ret = direct_metadataService->metadata_find_dataitem(
                    metaRequest.get_key_dataitem_name(),
                    metaRequest.get_key_region_id(), dataitem);
            else
                ret = direct_metadataService->metadata_find_dataitem(
                    metaRequest.get_key_dataitem_name(),
                    metaRequest.get_key_region_name(), dataitem);
            metaResponse.set_isfound(ret);
            if (ret) {
                metaResponse.set_region_id(dataitem.regionId);
                metaResponse.set_name(dataitem.name);
                metaResponse.set_offsets(dataitem.offsets,
                                         (int)dataitem.used_memsrv_cnt);
                metaResponse.set_size(dataitem.size);
                metaResponse.set_perm(dataitem.perm);
                metaResponse.set_uid(dataitem.uid);
                metaResponse.set_gid(dataitem.gid);
                metaResponse.set_maxkeylen(
                    direct_metadataService->metadata_maxkeylen());
                metaResponse.set_interleave_size(dataitem.interleaveSize);
                metaResponse.set_permission_level(dataitem.permissionLevel);
                metaResponse.set_memsrv_list(dataitem.memoryServerIds,
                                             (int)dataitem.used_memsrv_cnt);
            }
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_find_dataitem);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_modify_dataitem(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        Fam_DataItem_Metadata *dataitem = new Fam_DataItem_Metadata();
        dataitem->regionId = metaRequest.get_region_id();
        strncpy(dataitem->name, (metaRequest.get_name()).c_str(),
                direct_metadataService->metadata_maxkeylen());
        dataitem->used_memsrv_cnt = metaRequest.get_memsrv_cnt();
        for (int ndx = 0; ndx < (int)dataitem->used_memsrv_cnt; ndx++) {
            dataitem->offsets[ndx] = metaRequest.get_offsets()[ndx];
        }
        dataitem->size = metaRequest.get_size();
        dataitem->perm = (mode_t)metaRequest.get_perm();
        dataitem->uid = metaRequest.get_uid();
        dataitem->gid = metaRequest.get_gid();
        dataitem->interleaveSize = (size_t)metaRequest.get_interleave_size();
        dataitem->permissionLevel =
            (Fam_Permission_Level)metaRequest.get_permission_level();
        for (int ndx = 0; ndx < (int)dataitem->used_memsrv_cnt; ndx++) {
            dataitem->memoryServerIds[ndx] = metaRequest.get_memsrv_list()[ndx];
        }
        try {
            if (metaRequest.get_type_flag() == HAS_KEY_DATAITEM_ID_REGION_ID) {
                direct_metadataService->metadata_modify_dataitem(
                    metaRequest.get_key_dataitem_id(),
                    metaRequest.get_key_region_id(), dataitem);
            } else if (metaRequest.get_type_flag() ==
                       HAS_KEY_DATAITEM_ID_REGION_NAME) {
                direct_metadataService->metadata_modify_dataitem(
                    metaRequest.get_key_dataitem_id(),
                    metaRequest.get_key_region_name(), dataitem);
            } else if (metaRequest.get_type_flag() ==
                       HAS_KEY_DATAITEM_NAME_REGION_ID) {
                direct_metadataService->metadata_modify_dataitem(
                    metaRequest.get_key_dataitem_name(),
                    metaRequest.get_key_region_id(), dataitem);
            } else {
                direct_metadataService->metadata_modify_dataitem(
                    metaRequest.get_key_dataitem_name(),
                    metaRequest.get_key_region_name(), dataitem);
            }
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_modify_dataitem);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void
    Fam_Metadata_Service_Thallium_Server::metadata_check_region_permissions(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        bool ret;
        Fam_Region_Metadata *region = new Fam_Region_Metadata();
        region->uid = metaRequest.get_uid_meta();
        region->gid = metaRequest.get_gid_meta();
        region->perm = (mode_t)metaRequest.get_perm();

        ret = direct_metadataService->metadata_check_permissions(
            region,
            static_cast<metadata_region_item_op_t>(metaRequest.get_ops()),
            metaRequest.get_uid_in(), metaRequest.get_gid_in());

        metaResponse.set_is_permitted(ret);
        metaResponse.set_status(ok);
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_check_region_permissions);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_check_item_permissions(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        bool ret;
        Fam_DataItem_Metadata *dataitem = new Fam_DataItem_Metadata();
        dataitem->uid = metaRequest.get_uid_meta();
        dataitem->gid = metaRequest.get_gid_meta();
        dataitem->perm = (mode_t)metaRequest.get_perm();

        ret = direct_metadataService->metadata_check_permissions(
            dataitem,
            static_cast<metadata_region_item_op_t>(metaRequest.get_ops()),
            metaRequest.get_uid_in(), metaRequest.get_gid_in());

        metaResponse.set_is_permitted(ret);
        metaResponse.set_status(ok);
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_check_item_permissions);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_maxkeylen(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        size_t ret = 0;
        try {
            ret = direct_metadataService->metadata_maxkeylen();
            metaResponse.set_maxkeylen(ret);
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_maxkeylen);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_update_memoryserver(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        std::vector<uint64_t> memsrv_persistent_id_list;
        std::vector<uint64_t> memsrv_volatile_id_list;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        for (int ndx = 0; ndx < (int)metaRequest.get_nmemserverspersistent();
             ndx++) {
            memsrv_persistent_id_list.push_back(
                metaRequest.get_memsrv_persistent_list()[ndx]);
        }
        for (int ndx = 0; ndx < (int)metaRequest.get_nmemserversvolatile();
             ndx++) {
            memsrv_volatile_id_list.push_back(
                metaRequest.get_memsrv_volatile_list()[ndx]);
        }
        direct_metadataService->metadata_update_memoryserver(
            metaRequest.get_nmemserverspersistent(), memsrv_persistent_id_list,
            metaRequest.get_nmemserversvolatile(), memsrv_volatile_id_list);
        metaResponse.set_status(ok);
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_update_memoryserver);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::metadata_reset_bitmap(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS()
        direct_metadataService->metadata_reset_bitmap(
            metaRequest.get_region_id());
        metaResponse.set_status(ok);
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_reset_bitmap);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void
    Fam_Metadata_Service_Thallium_Server::metadata_validate_and_create_region(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS();
        std::list<int> memoryServerIds;
        Fam_Region_Attributes *regionAttributes = new (Fam_Region_Attributes);
        regionAttributes->redundancyLevel =
            (Fam_Redundancy_Level)metaRequest.get_redundancylevel();
        regionAttributes->memoryType =
            (Fam_Memory_Type)metaRequest.get_memorytype();
        regionAttributes->interleaveEnable =
            (Fam_Interleave_Enable)metaRequest.get_interleaveenable();
        regionAttributes->permissionLevel =
            (Fam_Permission_Level)metaRequest.get_permission_level();

        uint64_t regionId;
        try {
            direct_metadataService->metadata_validate_and_create_region(
                metaRequest.get_name(), metaRequest.get_size(), &regionId,
                regionAttributes, &memoryServerIds,
                metaRequest.get_user_policy());
            metaResponse.set_region_id(regionId);
            uint64_t *temp_memsrv_list =
                (uint64_t *)malloc(sizeof(uint64_t) * memoryServerIds.size());
            int i = 0;
            for (auto it = memoryServerIds.begin(); it != memoryServerIds.end();
                 ++it) {
                temp_memsrv_list[i] = *it;
                i += 1;
            }
            metaResponse.set_memsrv_list(temp_memsrv_list,
                                         (int)memoryServerIds.size());
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_validate_and_create_region);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void
    Fam_Metadata_Service_Thallium_Server::metadata_validate_and_destroy_region(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS();
        std::list<int> memoryServerIds;
        try {
            direct_metadataService->metadata_validate_and_destroy_region(
                metaRequest.get_region_id(), metaRequest.get_uid(),
                metaRequest.get_gid(), &memoryServerIds);
            uint64_t *temp_memsrv_list =
                (uint64_t *)malloc(sizeof(uint64_t) * memoryServerIds.size());
            int i = 0;
            for (auto it = memoryServerIds.begin(); it != memoryServerIds.end();
                 ++it) {
                temp_memsrv_list[i] = *it;
                i += 1;
            }
            metaResponse.set_memsrv_list(temp_memsrv_list,
                                         (int)memoryServerIds.size());
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_validate_and_destroy_region);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::
        metadata_validate_and_allocate_dataitem(
            const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        std::list<int> memoryServerIds;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS();
        size_t interleaveSize;
        Fam_Permission_Level permissionLevel;
        mode_t regionPermission;
        try {
            direct_metadataService->metadata_validate_and_allocate_dataitem(
                metaRequest.get_key_dataitem_name(),
                metaRequest.get_region_id(), metaRequest.get_uid(),
                metaRequest.get_gid(), metaRequest.get_size(), &memoryServerIds,
                &interleaveSize, &permissionLevel, &regionPermission,
                metaRequest.get_user_policy());
            metaResponse.set_interleave_size(interleaveSize);
            metaResponse.set_permission_level(permissionLevel);
            metaResponse.set_region_permission(regionPermission);
            uint64_t *temp_memsrv_list =
                (uint64_t *)malloc(sizeof(uint64_t) * memoryServerIds.size());
            int i = 0;
            for (auto it = memoryServerIds.begin(); it != memoryServerIds.end();
                 ++it) {
                temp_memsrv_list[i] = *it;
                i += 1;
            }
            metaResponse.set_memsrv_list(temp_memsrv_list,
                                         (int)memoryServerIds.size());
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_validate_and_allocate_dataitem);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::
        metadata_validate_and_deallocate_dataitem(
            const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS();
        Fam_DataItem_Metadata dataitem;
        try {
            direct_metadataService->metadata_validate_and_deallocate_dataitem(
                metaRequest.get_region_id(), metaRequest.get_key_dataitem_id(),
                metaRequest.get_uid(), metaRequest.get_gid(), dataitem);
            metaResponse.set_region_id(dataitem.regionId);
            metaResponse.set_name(dataitem.name);
            metaResponse.set_offsets(dataitem.offsets,
                                     (int)dataitem.used_memsrv_cnt);
            metaResponse.set_size(dataitem.size);
            metaResponse.set_perm(dataitem.perm);
            metaResponse.set_uid(dataitem.uid);
            metaResponse.set_gid(dataitem.gid);
            metaResponse.set_maxkeylen(
                direct_metadataService->metadata_maxkeylen());
            metaResponse.set_memsrv_list(dataitem.memoryServerIds,
                                         (int)dataitem.used_memsrv_cnt);
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_validate_and_deallocate_dataitem);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::
        metadata_find_region_and_check_permissions(
            const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS();
        Fam_Region_Metadata region;
        try {
            if (metaRequest.get_check_name_id())
                direct_metadataService
                    ->metadata_find_region_and_check_permissions(
                        (metadata_region_item_op_t)metaRequest.get_op(),
                        metaRequest.get_key_region_id(), metaRequest.get_uid(),
                        metaRequest.get_gid(), region);
            else
                direct_metadataService
                    ->metadata_find_region_and_check_permissions(
                        (metadata_region_item_op_t)metaRequest.get_op(),
                        metaRequest.get_key_region_name(),
                        metaRequest.get_uid(), metaRequest.get_gid(), region);
            metaResponse.set_region_id(region.regionId);
            metaResponse.set_name(region.name);
            metaResponse.set_offset(region.offset);
            metaResponse.set_size(region.size);
            metaResponse.set_perm(region.perm);
            metaResponse.set_uid(region.uid);
            metaResponse.set_gid(region.gid);
            metaResponse.set_maxkeylen(
                direct_metadataService->metadata_maxkeylen());
            metaResponse.set_memsrv_cnt(region.used_memsrv_cnt);
            metaResponse.set_redundancylevel(region.redundancyLevel);
            metaResponse.set_memorytype(region.redundancyLevel);
            metaResponse.set_interleaveenable(region.redundancyLevel);
            metaResponse.set_interleavesize(region.interleaveSize);
            metaResponse.set_permission_level(region.permissionLevel);
            metaResponse.set_memsrv_list(region.memServerIds,
                                         (int)region.used_memsrv_cnt);
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_find_region_and_check_permissions);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::
        metadata_find_dataitem_and_check_permissions(
            const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS();
        Fam_DataItem_Metadata dataitem;
        try {
            if ((metaRequest.get_key_dataitem_id()) &&
                (metaRequest.get_check_name_id()))
                direct_metadataService
                    ->metadata_find_dataitem_and_check_permissions(
                        (metadata_region_item_op_t)metaRequest.get_op(),
                        metaRequest.get_key_dataitem_id(),
                        metaRequest.get_key_region_id(), metaRequest.get_uid(),
                        metaRequest.get_gid(), dataitem);
            else
                direct_metadataService
                    ->metadata_find_dataitem_and_check_permissions(
                        (metadata_region_item_op_t)metaRequest.get_op(),
                        metaRequest.get_key_dataitem_name(),
                        metaRequest.get_key_region_name(),
                        metaRequest.get_uid(), metaRequest.get_gid(), dataitem);
            metaResponse.set_region_id(dataitem.regionId);
            metaResponse.set_name(dataitem.name);
            metaResponse.set_offsets(dataitem.offsets,
                                     (int)dataitem.used_memsrv_cnt);
            metaResponse.set_size(dataitem.size);
            metaResponse.set_perm(dataitem.perm);
            metaResponse.set_uid(dataitem.uid);
            metaResponse.set_gid(dataitem.gid);
            metaResponse.set_maxkeylen(
                direct_metadataService->metadata_maxkeylen());
            metaResponse.set_interleave_size(dataitem.interleaveSize);
            metaResponse.set_memsrv_list(dataitem.memoryServerIds,
                                         (int)dataitem.used_memsrv_cnt);
            metaResponse.set_permission_level(dataitem.permissionLevel);
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(
            thallium_server_metadata_find_dataitem_and_check_permissions);
        HANDLE_ERROR(req.respond(metaResponse));
    }

    void Fam_Metadata_Service_Thallium_Server::get_memory_server_list(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest) {
        Fam_Metadata_Thallium_Response metaResponse;
        METADATA_THALLIUM_SERVER_PROFILE_START_OPS();
        std::list<int> memoryServerIds;
        try {
            memoryServerIds = direct_metadataService->get_memory_server_list(
                metaRequest.get_region_id());
            uint64_t *temp_memsrv_list =
                (uint64_t *)malloc(sizeof(uint64_t) * memoryServerIds.size());
            int i = 0;
            for (auto it = memoryServerIds.begin(); it != memoryServerIds.end();
                 ++it) {
                temp_memsrv_list[i] = *it;
                i += 1;
            }
            metaResponse.set_memsrv_list(temp_memsrv_list,
                                         (int)memoryServerIds.size());
            metaResponse.set_status(ok);
        } catch (Fam_Exception &e) {
            metaResponse.set_errorcode(e.fam_error());
            metaResponse.set_errormsg(e.fam_error_msg());
            metaResponse.set_status(error);
        }
        METADATA_THALLIUM_SERVER_PROFILE_END_OPS(server_get_memory_server_list);
        HANDLE_ERROR(req.respond(metaResponse));
    }

} // namespace metadata
