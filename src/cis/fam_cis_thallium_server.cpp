/*
 * fam_cis_thallium_server.cpp
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
#include "common/fam_config_info.h"
#include "common/fam_memserver_profile.h"
#include "cis/fam_cis_thallium_server.h"
#include "fam_cis_thallium_rpc_structures.h"
#include <thread>

#include <boost/atomic.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string.h>
#include <string>
#include <unistd.h>

#define NUM_THREADS 12
#define ok true
#define error false
using namespace std;
using namespace chrono;
namespace tl = thallium;

namespace openfam {
MEMSERVER_PROFILE_START(CIS_THALLIUM_SERVER)
#ifdef MEMSERVER_PROFILE
#define CIS_THALLIUM_SERVER_PROFILE_START_OPS()                                \
    {                                                                          \
        Profile_Time start = CIS_THALLIUM_SERVER_get_time();

#define CIS_THALLIUM_SERVER_PROFILE_END_OPS(apiIdx)                            \
    Profile_Time end = CIS_THALLIUM_SERVER_get_time();                         \
    Profile_Time total =                                                       \
        CIS_THALLIUM_SERVER_time_diff_nanoseconds(start, end);                 \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(CIS_THALLIUM_SERVER, prof_##apiIdx,     \
                                       total)                                  \
    }
#define CIS_THALLIUM_SERVER_PROFILE_DUMP() cis_thallium_server_profile_dump()
#else
#define CIS_THALLIUM_SERVER_PROFILE_START_OPS()
#define CIS_THALLIUM_SERVER_PROFILE_END_OPS(apiIdx)
#define CIS_THALLIUM_SERVER_PROFILE_DUMP()
#endif

void cis_thallium_server_profile_dump() {
    MEMSERVER_PROFILE_END(CIS_THALLIUM_SERVER);
    MEMSERVER_DUMP_PROFILE_BANNER(CIS_THALLIUM_SERVER)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(CIS_THALLIUM_SERVER, name, prof_##name)
#include "cis/cis_thallium_server_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(CIS_THALLIUM_SERVER, prof_##name)
#include "cis/cis_thallium_server_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(CIS_THALLIUM_SERVER)
}

Fam_CIS_Thallium_Server::Fam_CIS_Thallium_Server(Fam_CIS_Direct *direct_CIS_,
                                                 const tl::engine &myEngine)
    : tl::provider<Fam_CIS_Thallium_Server>(myEngine, 10) {
    numClients = 0;
    direct_CIS = direct_CIS_;
    this->myEngine = myEngine;
    MEMSERVER_PROFILE_INIT(CIS_THALLIUM_SERVER)
    MEMSERVER_PROFILE_START_TIME(CIS_THALLIUM_SERVER)
}

void Fam_CIS_Thallium_Server::run() {
    string addr(myEngine.self());
    direct_CIS->set_controlpath_addr(addr);

    // creating argobots pool with ES
    std::vector<tl::managed<tl::xstream>> ess;
    tl::managed<tl::pool> myPool = tl::pool::create(tl::pool::access::spmc);
    for (int i = 0; i < 12; i++) {
        tl::managed<tl::xstream> es =
            tl::xstream::create(tl::scheduler::predef::deflt, *myPool);
        ess.push_back(std::move(es));
    }

    // server rpc structure definitions with argobots pool
    define("reset_profile", &Fam_CIS_Thallium_Server::reset_profile);
    define("dump_profile", &Fam_CIS_Thallium_Server::dump_profile);
    define("get_num_memory_servers",
           &Fam_CIS_Thallium_Server::get_num_memory_servers, *myPool);
    define("create_region", &Fam_CIS_Thallium_Server::create_region, *myPool);
    define("destroy_region", &Fam_CIS_Thallium_Server::destroy_region, *myPool);
    define("resize_region", &Fam_CIS_Thallium_Server::resize_region, *myPool);
    define("allocate", &Fam_CIS_Thallium_Server::allocate, *myPool);
    define("deallocate", &Fam_CIS_Thallium_Server::deallocate, *myPool);
    define("change_region_permission",
           &Fam_CIS_Thallium_Server::change_region_permission, *myPool);
    define("change_dataitem_permission",
           &Fam_CIS_Thallium_Server::change_dataitem_permission, *myPool);
    define("lookup_region", &Fam_CIS_Thallium_Server::lookup_region, *myPool);
    define("lookup", &Fam_CIS_Thallium_Server::lookup, *myPool);
    define("check_permission_get_region_info",
           &Fam_CIS_Thallium_Server::check_permission_get_region_info, *myPool);
    define("check_permission_get_item_info",
           &Fam_CIS_Thallium_Server::check_permission_get_item_info, *myPool);
    define("get_stat_info", &Fam_CIS_Thallium_Server::get_stat_info, *myPool);
    define("copy", &Fam_CIS_Thallium_Server::copy, *myPool);
    define("backup", &Fam_CIS_Thallium_Server::backup, *myPool);
    define("restore", &Fam_CIS_Thallium_Server::restore, *myPool);
    define("acquire_CAS_lock", &Fam_CIS_Thallium_Server::acquire_CAS_lock,
           *myPool);
    define("release_CAS_lock", &Fam_CIS_Thallium_Server::release_CAS_lock,
           *myPool);
    define("get_backup_info", &Fam_CIS_Thallium_Server::get_backup_info,
           *myPool);
    define("list_backup", &Fam_CIS_Thallium_Server::list_backup, *myPool);
    define("delete_backup", &Fam_CIS_Thallium_Server::delete_backup, *myPool);
    define("get_memserverinfo_size",
           &Fam_CIS_Thallium_Server::get_memserverinfo_size, *myPool);
    define("get_memserverinfo", &Fam_CIS_Thallium_Server::get_memserverinfo,
           *myPool);
    define("get_atomic", &Fam_CIS_Thallium_Server::get_atomic, *myPool);
    define("put_atomic", &Fam_CIS_Thallium_Server::put_atomic, *myPool);
    define("scatter_strided_atomic",
           &Fam_CIS_Thallium_Server::scatter_strided_atomic, *myPool);
    define("gather_strided_atomic",
           &Fam_CIS_Thallium_Server::gather_strided_atomic, *myPool);
    define("scatter_indexed_atomic",
           &Fam_CIS_Thallium_Server::scatter_indexed_atomic, *myPool);
    define("gather_indexed_atomic",
           &Fam_CIS_Thallium_Server::gather_indexed_atomic, *myPool);
    define("get_region_memory", &Fam_CIS_Thallium_Server::get_region_memory,
           *myPool);
    define("open_region_with_registration",
           &Fam_CIS_Thallium_Server::open_region_with_registration, *myPool);
    define("open_region_without_registration",
           &Fam_CIS_Thallium_Server::open_region_without_registration, *myPool);
    define("close_region", &Fam_CIS_Thallium_Server::close_region, *myPool);
    myEngine.wait_for_finalize();

    for (int i = 0; i < NUM_THREADS; i++) {
        ess[i]->join();
    }
}

Fam_CIS_Thallium_Server::~Fam_CIS_Thallium_Server() {}

void Fam_CIS_Thallium_Server::reset_profile() {
    MEMSERVER_PROFILE_INIT(CIS_THALLIUM_SERVER)
    MEMSERVER_PROFILE_START_TIME(CIS_THALLIUM_SERVER)
}

void Fam_CIS_Thallium_Server::dump_profile() {
    CIS_THALLIUM_SERVER_PROFILE_DUMP();
}

void Fam_CIS_Thallium_Server::generate_profile() {
    CIS_THALLIUM_SERVER_PROFILE_DUMP();
    // direct_CIS->dump_profile();
    // cisResponse.set_status(error);
}

void Fam_CIS_Thallium_Server::get_num_memory_servers(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    uint64_t numMemoryServer;
    try {
        numMemoryServer = direct_CIS->get_num_memory_servers();
        cisResponse.set_num_memory_server(numMemoryServer);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(get_num_memory_servers);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::create_region(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    Fam_Region_Attributes *regionAttributes = new (Fam_Region_Attributes);
    regionAttributes->redundancyLevel =
        (Fam_Redundancy_Level)cisRequest.get_redundancylevel();
    regionAttributes->memoryType = (Fam_Memory_Type)cisRequest.get_memorytype();
    regionAttributes->interleaveEnable =
        (Fam_Interleave_Enable)cisRequest.get_interleaveenable();
    regionAttributes->permissionLevel =
        (Fam_Permission_Level)cisRequest.get_permissionlevel();
    try {
        info = direct_CIS->create_region(
            cisRequest.get_name(), (size_t)cisRequest.get_size(),
            (mode_t)cisRequest.get_perm(), regionAttributes,
            cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_regionid(info.regionId);
        cisResponse.set_offset(info.offset);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(create_region);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::destroy_region(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->destroy_region(cisRequest.get_regionid(),
                                   cisRequest.get_memserver_id(),
                                   cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(destroy_region);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::resize_region(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->resize_region(cisRequest.get_regionid(),
                                  cisRequest.get_size(),
                                  cisRequest.get_memserver_id(),
                                  cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(resize_region);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::allocate(const tl::request &req,
                                       Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    try {
        info = direct_CIS->allocate(
            cisRequest.get_name(), (size_t)cisRequest.get_size(),
            (mode_t)cisRequest.get_perm(), cisRequest.get_regionid(),
            cisRequest.get_memserver_id(), cisRequest.get_uid(),
            cisRequest.get_gid());
        cisResponse.set_interleave_size(info.interleaveSize);
        cisResponse.set_permission_level(info.permissionLevel);
        cisResponse.set_perm(info.perm);

        cisResponse.set_memsrv_list(info.memoryServerIds,
                                    (int)info.used_memsrv_cnt);

        if (info.permissionLevel == REGION) {
            cisResponse.set_offsets(info.dataitemOffsets,
                                    (int)info.used_memsrv_cnt);
        } else {
            if (info.itemRegistrationStatus) {
                cisResponse.set_keys(info.dataitemKeys,
                                     (int)info.used_memsrv_cnt);
                cisResponse.set_base_addr_list(info.baseAddressList,
                                               (int)info.used_memsrv_cnt);
            }
            cisResponse.set_item_registration_status(
                info.itemRegistrationStatus);
            uint64_t info_dataitemOffsets_arr[1] = {info.dataitemOffsets[0]};
            cisResponse.set_offsets(info_dataitemOffsets_arr,
                                    (int)sizeof(info_dataitemOffsets_arr));
        }
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(allocate);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::deallocate(const tl::request &req,
                                         Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->deallocate(cisRequest.get_regionid(),
                               cisRequest.get_offset(),
                               cisRequest.get_memserver_id(),
                               cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(deallocate);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::change_region_permission(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->change_region_permission(
            cisRequest.get_regionid(), (mode_t)cisRequest.get_perm(),
            cisRequest.get_memserver_id(), cisRequest.get_uid(),
            cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(change_region_permission);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::change_dataitem_permission(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->change_dataitem_permission(
            cisRequest.get_regionid(), cisRequest.get_offset(),
            (mode_t)cisRequest.get_perm(), cisRequest.get_memserver_id(),
            cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(change_dataitem_permission);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::lookup_region(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    try {
        info = direct_CIS->lookup_region(
            cisRequest.get_name(), cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_regionid(info.regionId);
        cisResponse.set_offset(info.offset);
        cisResponse.set_size(info.size);
        cisResponse.set_perm(info.perm);
        cisResponse.set_name(info.name);
        cisResponse.set_maxnamelen(info.maxNameLen);
        cisResponse.set_redundancylevel(info.redundancyLevel);
        cisResponse.set_memorytype(info.memoryType);
        cisResponse.set_interleaveenable(info.interleaveEnable);
        cisResponse.set_permissionlevel(info.permissionLevel);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(lookup_region);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::lookup(const tl::request &req,
                                     Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    try {
        info = direct_CIS->lookup(cisRequest.get_name(),
                                  cisRequest.get_regionname(),
                                  cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_regionid(info.regionId);
        cisResponse.set_size(info.size);
        cisResponse.set_perm(info.perm);
        cisResponse.set_name(info.name);
        cisResponse.set_maxnamelen(info.maxNameLen);
        cisResponse.set_offset(info.offset);
        cisResponse.set_uid(info.uid);
        cisResponse.set_gid(info.gid);
        cisResponse.set_interleave_size(info.interleaveSize);
        cisResponse.set_memsrv_list(info.memoryServerIds,
                                    (int)info.used_memsrv_cnt);
        cisResponse.set_permissionlevel(info.permissionLevel);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(lookup);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::check_permission_get_region_info(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    try {
        info = direct_CIS->check_permission_get_region_info(
            cisRequest.get_regionid(), cisRequest.get_memserver_id(),
            cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_size(info.size);
        cisResponse.set_perm(info.perm);
        cisResponse.set_name(info.name);
        cisResponse.set_maxnamelen(info.maxNameLen);
        cisResponse.set_uid(info.uid);
        cisResponse.set_gid(info.gid);
        cisResponse.set_redundancylevel(info.redundancyLevel);
        cisResponse.set_memorytype(info.memoryType);
        cisResponse.set_interleaveenable(info.interleaveEnable);
        cisResponse.set_interleavesize(info.interleaveSize);
        cisResponse.set_permissionlevel(info.permissionLevel);
        cisResponse.set_memsrv_list(info.memoryServerIds,
                                    (int)info.used_memsrv_cnt);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(check_permission_get_region_info);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::check_permission_get_item_info(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    try {
        info = direct_CIS->check_permission_get_item_info(
            cisRequest.get_regionid(), cisRequest.get_offset(),
            cisRequest.get_memserver_id(), cisRequest.get_uid(),
            cisRequest.get_gid());
        cisResponse.set_size(info.size);
        cisResponse.set_perm(info.perm);
        cisResponse.set_name(info.name);
        cisResponse.set_maxnamelen(info.maxNameLen);
        cisResponse.set_interleave_size(info.interleaveSize);
        cisResponse.set_permission_level(info.permissionLevel);
        cisResponse.set_memsrv_list(info.memoryServerIds,
                                    (int)info.used_memsrv_cnt);

        if (info.permissionLevel == REGION) {
            cisResponse.set_offsets(info.dataitemOffsets,
                                    (int)info.used_memsrv_cnt);
        } else {
            cisResponse.set_keys(info.dataitemKeys, (int)info.used_memsrv_cnt);
            cisResponse.set_base_addr_list(info.baseAddressList,
                                           (int)info.used_memsrv_cnt);
            uint64_t info_dataitemOffsets_arr[1] = {info.dataitemOffsets[0]};
            cisResponse.set_offsets(info_dataitemOffsets_arr,
                                    (int)sizeof(info_dataitemOffsets_arr));
        }
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(check_permission_get_item_info);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::get_stat_info(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    try {
        info = direct_CIS->get_stat_info(
            cisRequest.get_regionid(), cisRequest.get_offset(),
            cisRequest.get_memserver_id(), cisRequest.get_uid(),
            cisRequest.get_gid());
        cisResponse.set_size(info.size);
        cisResponse.set_perm(info.perm);
        cisResponse.set_name(info.name);
        cisResponse.set_uid(info.uid);
        cisResponse.set_gid(info.gid);
        cisResponse.set_maxnamelen(info.maxNameLen);
        cisResponse.set_interleave_size(info.interleaveSize);
        cisResponse.set_permission_level(info.permissionLevel);
        cisResponse.set_memsrv_list(info.memoryServerIds,
                                    (int)info.used_memsrv_cnt);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(get_stat_info);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::copy(const tl::request &req,
                                   Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    try {
        uint64_t *srcKeys = (uint64_t *)malloc(
            sizeof(uint64_t) * cisRequest.get_src_used_memsrv_cnt());
        uint64_t *srcBaseAddrList = (uint64_t *)malloc(
            sizeof(uint64_t) * cisRequest.get_src_used_memsrv_cnt());

        for (uint64_t i = 0; i < cisRequest.get_src_used_memsrv_cnt(); i++) {
            srcKeys[i] = cisRequest.get_src_keys()[(int)i];
            srcBaseAddrList[i] = cisRequest.get_src_base_addr()[(int)i];
        }
        direct_CIS->copy(
            cisRequest.get_src_region_id(), cisRequest.get_src_offset(),
            cisRequest.get_src_used_memsrv_cnt(),
            cisRequest.get_src_copy_start(), srcKeys, srcBaseAddrList,
            cisRequest.get_dest_region_id(), cisRequest.get_dest_offset(),
            cisRequest.get_dest_copy_start(), cisRequest.get_copy_size(),
            cisRequest.get_first_src_memsrv_id(),
            cisRequest.get_first_dest_memsrv_id(), cisRequest.get_gid(),
            cisRequest.get_uid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::backup(const tl::request &req,
                                     Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    try {
        direct_CIS->backup(cisRequest.get_regionid(), cisRequest.get_offset(),
                           cisRequest.get_memserver_id(),
                           cisRequest.get_bname(), cisRequest.get_uid(),
                           cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::restore(const tl::request &req,
                                      Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    try {
        direct_CIS->restore(cisRequest.get_regionid(), cisRequest.get_offset(),
                            cisRequest.get_memserver_id(),
                            cisRequest.get_bname(), cisRequest.get_uid(),
                            cisRequest.get_gid());

        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::acquire_CAS_lock(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->acquire_CAS_lock(cisRequest.get_offset(),
                                     cisRequest.get_memserver_id());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(acquire_CAS_lock);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::release_CAS_lock(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->release_CAS_lock(cisRequest.get_offset(),
                                     cisRequest.get_memserver_id());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(release_CAS_lock);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::get_backup_info(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Backup_Info info;
    try {
        info = direct_CIS->get_backup_info(
            cisRequest.get_bname(), cisRequest.get_memserver_id(),
            cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_size(info.size);
        cisResponse.set_name(info.bname);
        cisResponse.set_mode(info.mode);
        cisResponse.set_uid(info.uid);
        cisResponse.set_gid(info.gid);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_size(-1);
        cisResponse.set_name("");
        cisResponse.set_mode(-1);
        cisResponse.set_uid(-1);
        cisResponse.set_gid(-1);
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(get_backup_info);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::list_backup(const tl::request &req,
                                          Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    string contents;
    try {
        contents = direct_CIS->list_backup(
            cisRequest.get_bname(), cisRequest.get_memserver_id(),
            cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_contents(contents);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_contents(string("Backup Listing failed."));
        cisResponse.set_status(error);
    }
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::delete_backup(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    try {
        direct_CIS->delete_backup(cisRequest.get_bname(),
                                  cisRequest.get_memserver_id(),
                                  cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::get_memserverinfo_size(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    size_t memServerInfoSize;
    try {
        memServerInfoSize = direct_CIS->get_memserverinfo_size();
        cisResponse.set_memserverinfo_size(memServerInfoSize);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(get_memserverinfo_size);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::get_memserverinfo(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    size_t memServerInfoSize;
    void *memServerInfoBuffer;
    try {
        memServerInfoSize = direct_CIS->get_memserverinfo_size();
        memServerInfoBuffer = calloc(1, memServerInfoSize);
        direct_CIS->get_memserverinfo(memServerInfoBuffer);
        cisResponse.set_memserverinfo_size(memServerInfoSize);
        cisResponse.set_memserverinfo((const char *)memServerInfoBuffer,
                                      (int)memServerInfoSize);
        free(memServerInfoBuffer);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(get_memserverinfo);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::get_atomic(const tl::request &req,
                                         Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->get_atomic(
            cisRequest.get_regionid(), cisRequest.get_srcoffset(),
            cisRequest.get_dstoffset(), cisRequest.get_nbytes(),
            cisRequest.get_key(), cisRequest.get_srcbaseaddr(),
            cisRequest.get_nodeaddr().c_str(), cisRequest.get_nodeaddrsize(),
            cisRequest.get_memserver_id(), cisRequest.get_uid(),
            cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(get_atomic);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::put_atomic(const tl::request &req,
                                         Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->put_atomic(
            cisRequest.get_regionid(), cisRequest.get_srcoffset(),
            cisRequest.get_dstoffset(), cisRequest.get_nbytes(),
            cisRequest.get_key(), cisRequest.get_srcbaseaddr(),
            cisRequest.get_nodeaddr().c_str(), cisRequest.get_nodeaddrsize(),
            cisRequest.get_data().c_str(), cisRequest.get_memserver_id(),
            cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(put_atomic);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::scatter_strided_atomic(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->scatter_strided_atomic(
            cisRequest.get_regionid(), cisRequest.get_offset(),
            cisRequest.get_nelements(), cisRequest.get_firstelement(),
            cisRequest.get_stride(), cisRequest.get_elementsize(),
            cisRequest.get_key(), cisRequest.get_srcbaseaddr(),
            cisRequest.get_nodeaddr().c_str(), cisRequest.get_nodeaddrsize(),
            cisRequest.get_memserver_id(), cisRequest.get_uid(),
            cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(scatter_strided_atomic);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::gather_strided_atomic(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->gather_strided_atomic(
            cisRequest.get_regionid(), cisRequest.get_offset(),
            cisRequest.get_nelements(), cisRequest.get_firstelement(),
            cisRequest.get_stride(), cisRequest.get_elementsize(),
            cisRequest.get_key(), cisRequest.get_srcbaseaddr(),
            cisRequest.get_nodeaddr().c_str(), cisRequest.get_nodeaddrsize(),
            cisRequest.get_memserver_id(), cisRequest.get_uid(),
            cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(gather_strided_atomic);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::scatter_indexed_atomic(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->scatter_indexed_atomic(
            cisRequest.get_regionid(), cisRequest.get_offset(),
            cisRequest.get_nelements(), cisRequest.get_elementindex().c_str(),
            cisRequest.get_elementsize(), cisRequest.get_key(),
            cisRequest.get_srcbaseaddr(), cisRequest.get_nodeaddr().c_str(),
            cisRequest.get_nodeaddrsize(), cisRequest.get_memserver_id(),
            cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(scatter_indexed_atomic);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::gather_indexed_atomic(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->gather_indexed_atomic(
            cisRequest.get_regionid(), cisRequest.get_offset(),
            cisRequest.get_nelements(), cisRequest.get_elementindex().c_str(),
            cisRequest.get_elementsize(), cisRequest.get_key(),
            cisRequest.get_srcbaseaddr(), cisRequest.get_nodeaddr().c_str(),
            cisRequest.get_nodeaddrsize(), cisRequest.get_memserver_id(),
            cisRequest.get_uid(), cisRequest.get_gid());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(gather_indexed_atomic);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::get_region_memory(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    ostringstream message;
    Fam_Region_Memory_Map regionMemoryMap;
    try {
        direct_CIS->get_region_memory(cisRequest.get_regionid(),
                                      cisRequest.get_uid(),
                                      cisRequest.get_gid(), &regionMemoryMap);
        std::vector<Fam_CIS_Thallium_Response::Region_Key_Map>
            regionKeyMapVector;
        for (auto keyMap : regionMemoryMap) {
            Fam_CIS_Thallium_Response::Region_Key_Map regionKeyMap;
            regionKeyMap.set_memserver_id(keyMap.first);
            auto regionMemory = keyMap.second;
            int numExtents = (int)regionMemory.keys.size();
            regionKeyMap.set_region_keys(regionMemory.keys.data(), numExtents);
            regionKeyMap.set_region_base(regionMemory.base.data(), numExtents);
            regionKeyMapVector.push_back(regionKeyMap);
        }
        cisResponse.set_region_key_map(regionKeyMapVector);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(get_region_memory);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::open_region_with_registration(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Region_Memory_Map regionMemoryMap;
    std::vector<uint64_t> memserverIds;
    try {
        direct_CIS->open_region_with_registration(
            cisRequest.get_regionid(), cisRequest.get_uid(),
            cisRequest.get_gid(), &memserverIds, &regionMemoryMap);
        std::vector<Fam_CIS_Thallium_Response::Region_Key_Map>
            regionKeyMapVector;
        for (auto keyMap : regionMemoryMap) {
            Fam_CIS_Thallium_Response::Region_Key_Map regionKeyMap;
            regionKeyMap.set_memserver_id(keyMap.first);
            auto regionMemory = keyMap.second;
            int numExtents = (int)regionMemory.keys.size();
            regionKeyMap.set_region_keys(regionMemory.keys.data(), numExtents);
            regionKeyMap.set_region_base(regionMemory.base.data(), numExtents);
            regionKeyMapVector.push_back(regionKeyMap);
        }
        cisResponse.set_region_key_map(regionKeyMapVector);
        cisResponse.set_memsrv_list(memserverIds);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(open_region_with_registration);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::open_region_without_registration(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()

    Fam_Region_Memory_Map regionMemoryMap;
    std::vector<uint64_t> memserverIds;
    try {
        direct_CIS->open_region_without_registration(cisRequest.get_regionid(),
                                                     &memserverIds);
        cisResponse.set_memsrv_list(memserverIds);
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(open_region_without_registration);
    HANDLE_ERROR(req.respond(cisResponse));
}

void Fam_CIS_Thallium_Server::close_region(
    const tl::request &req, Fam_CIS_Thallium_Request cisRequest) {
    Fam_CIS_Thallium_Response cisResponse;
    CIS_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_CIS->close_region(cisRequest.get_regionid(),
                                 cisRequest.get_memsrv_list());
        cisResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        cisResponse.set_errorcode(e.fam_error());
        cisResponse.set_errormsg(e.fam_error_msg());
        cisResponse.set_status(error);
    }
    CIS_THALLIUM_SERVER_PROFILE_END_OPS(close_region);
    HANDLE_ERROR(req.respond(cisResponse));
}

} // namespace openfam
