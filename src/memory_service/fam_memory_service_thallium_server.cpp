/*
 * fam_memory_service_thallium_server.cpp
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
#include "fam_memory_service_thallium_server.h"
#include "common/fam_config_info.h"
#include "common/fam_memserver_profile.h"
#include "fam_memory_service_server.h"
#include "fam_memory_service_thallium_rpc_structures.h"
#include <boost/atomic.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string.h>
#include <string>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thread>
#include <unistd.h>

#define NUM_THREADS 12
#define ok true
#define error false
using namespace std;
using namespace chrono;
namespace tl = thallium;
namespace openfam {
MEMSERVER_PROFILE_START(MEMORY_SERVICE_THALLIUM_SERVER)
#ifdef MEMSERVER_PROFILE
#define MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()                     \
    {                                                                          \
        Profile_Time start = MEMORY_SERVICE_THALLIUM_SERVER_get_time();

#define MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(apiIdx)                 \
    Profile_Time end = MEMORY_SERVICE_THALLIUM_SERVER_get_time();              \
    Profile_Time total =                                                       \
        MEMORY_SERVICE_THALLIUM_SERVER_time_diff_nanoseconds(start, end);      \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(MEMORY_SERVICE_THALLIUM_SERVER,         \
                                       prof_##apiIdx, total)                   \
    }
#define MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_DUMP()                          \
    memory_service_thallium_server_profile_dump()
#else
#define MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
#define MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(                        \
    thallium_mem_server_apiIdx)
#define MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_DUMP()
#endif

void memory_service_thallium_server_profile_dump(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    MEMSERVER_PROFILE_END(MEMORY_SERVICE_THALLIUM_SERVER);
    MEMSERVER_DUMP_PROFILE_BANNER(MEMORY_SERVICE_THALLIUM_SERVER)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(MEMORY_SERVICE_THALLIUM_SERVER, name,          \
                                prof_##name)
#include "memory_service/memory_service_thallium_server_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(MEMORY_SERVICE_THALLIUM_SERVER, prof_##name)
#include "memory_service/memory_service_thallium_server_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(MEMORY_SERVICE_THALLIUM_SERVER)
}

Fam_Memory_Service_Thallium_Server::Fam_Memory_Service_Thallium_Server(
    Fam_Memory_Service_Direct *direct_memoryService_,
    const tl::engine &myEngine)
    : tl::provider<Fam_Memory_Service_Thallium_Server>(myEngine, 10) {
    direct_memoryService = direct_memoryService_;
    this->myEngine = myEngine;
    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_THALLIUM_SERVER)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_THALLIUM_SERVER)
}

void Fam_Memory_Service_Thallium_Server::run() {
    string addr(myEngine.self());
    direct_memoryService->set_controlpath_addr(addr);
    // creating argobots pool with ES
    std::vector<tl::managed<tl::xstream>> ess;
    tl::managed<tl::pool> myPool = tl::pool::create(tl::pool::access::spmc);
    for (int i = 0; i < 12; i++) {
        tl::managed<tl::xstream> es =
            tl::xstream::create(tl::scheduler::predef::deflt, *myPool);
        ess.push_back(std::move(es));
    }

    // server rpc structure definitions with argobots pool
    define("reset_profile", &Fam_Memory_Service_Thallium_Server::reset_profile);
    define("dump_profile", &Fam_Memory_Service_Thallium_Server::dump_profile);
    define("create_region", &Fam_Memory_Service_Thallium_Server::create_region,
           *myPool);
    define("destroy_region",
           &Fam_Memory_Service_Thallium_Server::destroy_region, *myPool);
    define("resize_region", &Fam_Memory_Service_Thallium_Server::resize_region,
           *myPool);
    define("allocate", &Fam_Memory_Service_Thallium_Server::allocate, *myPool);
    define("deallocate", &Fam_Memory_Service_Thallium_Server::deallocate,
           *myPool);
    define("copy", &Fam_Memory_Service_Thallium_Server::copy, *myPool);
    define("backup", &Fam_Memory_Service_Thallium_Server::backup, *myPool);
    define("restore", &Fam_Memory_Service_Thallium_Server::restore, *myPool);
    define("get_backup_info",
           &Fam_Memory_Service_Thallium_Server::get_backup_info, *myPool);
    define("list_backup", &Fam_Memory_Service_Thallium_Server::list_backup,
           *myPool);
    define("delete_backup", &Fam_Memory_Service_Thallium_Server::delete_backup,
           *myPool);
    define("get_local_pointer",
           &Fam_Memory_Service_Thallium_Server::get_local_pointer, *myPool);
    define("register_region_memory",
           &Fam_Memory_Service_Thallium_Server::register_region_memory,
           *myPool);
    define("get_region_memory",
           &Fam_Memory_Service_Thallium_Server::get_region_memory, *myPool);
    define("get_dataitem_memory",
           &Fam_Memory_Service_Thallium_Server::get_dataitem_memory, *myPool);
    define("acquire_CAS_lock",
           &Fam_Memory_Service_Thallium_Server::acquire_CAS_lock, *myPool);
    define("release_CAS_lock",
           &Fam_Memory_Service_Thallium_Server::release_CAS_lock, *myPool);
    define("get_atomic", &Fam_Memory_Service_Thallium_Server::get_atomic,
           *myPool);
    define("put_atomic", &Fam_Memory_Service_Thallium_Server::put_atomic,
           *myPool);
    define("scatter_strided_atomic",
           &Fam_Memory_Service_Thallium_Server::scatter_strided_atomic,
           *myPool);
    define("gather_strided_atomic",
           &Fam_Memory_Service_Thallium_Server::gather_strided_atomic, *myPool);
    define("scatter_indexed_atomic",
           &Fam_Memory_Service_Thallium_Server::scatter_indexed_atomic,
           *myPool);
    define("gather_indexed_atomic",
           &Fam_Memory_Service_Thallium_Server::gather_indexed_atomic, *myPool);
    define("update_memserver_addrlist",
           &Fam_Memory_Service_Thallium_Server::update_memserver_addrlist,
           *myPool);
    define("open_region_with_registration",
           &Fam_Memory_Service_Thallium_Server::open_region_with_registration,
           *myPool);
    define(
        "open_region_without_registration",
        &Fam_Memory_Service_Thallium_Server::open_region_without_registration,
        *myPool);
    define("close_region", &Fam_Memory_Service_Thallium_Server::close_region,
           *myPool);
    define("create_region_failure_cleanup",
           &Fam_Memory_Service_Thallium_Server::create_region_failure_cleanup,
           *myPool);
    myEngine.wait_for_finalize();

    for (int i = 0; i < NUM_THREADS; i++) {
        ess[i]->join();
    }
}

Fam_Memory_Service_Thallium_Server::~Fam_Memory_Service_Thallium_Server() {}

void Fam_Memory_Service_Thallium_Server::reset_profile() {
    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_THALLIUM_SERVER)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_THALLIUM_SERVER)
}

void Fam_Memory_Service_Thallium_Server::dump_profile() {
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_DUMP();
}

void Fam_Memory_Service_Thallium_Server::create_region(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->create_region(
            (uint64_t)memRequest.get_region_id(),
            (size_t)memRequest.get_size());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_create_region);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::destroy_region(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    uint64_t resourceStatus;
    try {
        direct_memoryService->destroy_region(memRequest.get_region_id(),
                                             &resourceStatus);
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    memResponse.set_resource_status(resourceStatus);
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_destroy_region);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::resize_region(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->resize_region(memRequest.get_region_id(),
                                            memRequest.get_size());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_resize_region);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::allocate(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    try {
        info = direct_memoryService->allocate(memRequest.get_region_id(),
                                              (size_t)memRequest.get_size());
        memResponse.set_key(info.key);
        memResponse.set_offset(info.offset);
        memResponse.set_base((uint64_t)info.base);
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_allocate);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::deallocate(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->deallocate(memRequest.get_region_id(),
                                         memRequest.get_offset());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_deallocate);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::copy(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        uint64_t *srcOffsets = (uint64_t *)malloc(
            sizeof(uint64_t) * memRequest.get_src_used_memsrv_cnt());
        uint64_t *srcKeys = (uint64_t *)malloc(
            sizeof(uint64_t) * memRequest.get_src_used_memsrv_cnt());
        uint64_t *srcBaseAddrList = (uint64_t *)malloc(
            sizeof(uint64_t) * memRequest.get_src_used_memsrv_cnt());
        uint64_t *srcMemserverIds = (uint64_t *)malloc(
            sizeof(uint64_t) * memRequest.get_src_used_memsrv_cnt());

        for (uint64_t i = 0; i < memRequest.get_src_used_memsrv_cnt(); i++) {
            srcOffsets[i] = memRequest.get_src_offsets()[(int)i];
            srcKeys[i] = memRequest.get_src_keys()[(int)i];
            srcBaseAddrList[i] = memRequest.get_src_base_addr_list()[(int)i];
            srcMemserverIds[i] = memRequest.get_src_ids()[(int)i];
        }
        direct_memoryService->copy(
            memRequest.get_src_region_id(), srcOffsets,
            memRequest.get_src_used_memsrv_cnt(),
            memRequest.get_src_copy_start(), memRequest.get_src_copy_end(),
            srcKeys, srcBaseAddrList, memRequest.get_dest_region_id(),
            memRequest.get_dest_offset(), memRequest.get_dest_used_memsrv_cnt(),
            srcMemserverIds, memRequest.get_src_interleave_size(),
            memRequest.get_dest_interleave_size(), memRequest.get_size());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(thallium_mem_server_copy);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::backup(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->backup(
            memRequest.get_region_id(), memRequest.get_offset(),
            memRequest.get_size(), memRequest.get_chunk_size(),
            memRequest.get_used_memserver_cnt(),
            memRequest.get_file_start_pos(), memRequest.get_bname(),
            memRequest.get_uid(), memRequest.get_gid(), memRequest.get_mode(),
            memRequest.get_diname(), memRequest.get_item_size(),
            memRequest.get_write_metadata());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(thallium_mem_server_backup);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::restore(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->restore(
            memRequest.get_region_id(), memRequest.get_offset(),
            memRequest.get_size(), memRequest.get_chunk_size(),
            memRequest.get_used_memserver_cnt(),
            memRequest.get_file_start_pos(), memRequest.get_bname());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(thallium_mem_server_restore);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::get_backup_info(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Backup_Info info;
    try {
        info = direct_memoryService->get_backup_info(
            memRequest.get_bname(), memRequest.get_uid(), memRequest.get_gid(),
            memRequest.get_mode());
        memResponse.set_name(info.bname);
        memResponse.set_mode(info.mode);
        memResponse.set_size(info.size);
        memResponse.set_uid(info.uid);
        memResponse.set_gid(info.gid);
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_size(-1);
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_get_backup_info);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::list_backup(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    try {
        string info = direct_memoryService->list_backup(
            memRequest.get_bname(), memRequest.get_uid(), memRequest.get_gid(),
            memRequest.get_mode());
        memResponse.set_contents(info);
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        std::string info = string("Backup Listing failed.");
        memResponse.set_contents(info);
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_list_backup);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::delete_backup(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->delete_backup(memRequest.get_bname());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_delete_backup);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::acquire_CAS_lock(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->acquire_CAS_lock(memRequest.get_offset());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_acquire_CAS_lock);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::release_CAS_lock(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->release_CAS_lock(memRequest.get_offset());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_release_CAS_lock);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::get_local_pointer(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    void *localPointer = 0;
    try {
        localPointer = direct_memoryService->get_local_pointer(
            memRequest.get_region_id(), memRequest.get_offset());
        memResponse.set_base((uint64_t)localPointer);
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_get_local_pointer);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::open_region_with_registration(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Region_Memory regionMemory;
    try {
        regionMemory = direct_memoryService->open_region_with_registration(
            memRequest.get_region_id(), memRequest.get_rw_flag());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    memResponse.set_region_keys(regionMemory.keys);
    memResponse.set_region_base(regionMemory.base);

    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_open_region_with_registration);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::open_region_without_registration(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->open_region_without_registration(
            memRequest.get_region_id());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }

    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_open_region_without_registration);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::close_region(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    uint64_t status = 0;
    try {
        status = direct_memoryService->close_region(memRequest.get_region_id());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }

    memResponse.set_resource_status(status);

    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_close_region);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::register_region_memory(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->register_region_memory(memRequest.get_region_id(),
                                                     memRequest.get_rw_flag());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_get_register_region_memory);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::get_region_memory(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Region_Memory regionMemory;
    try {
        regionMemory = direct_memoryService->get_region_memory(
            memRequest.get_region_id(), memRequest.get_rw_flag());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    memResponse.set_region_keys(regionMemory.keys);
    memResponse.set_region_base(regionMemory.base);
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_get_region_memory);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::get_dataitem_memory(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    Fam_Dataitem_Memory dataitemMemory;
    try {
        dataitemMemory = direct_memoryService->get_dataitem_memory(
            memRequest.get_region_id(), memRequest.get_offset(),
            memRequest.get_size(), memRequest.get_rw_flag());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    memResponse.set_key(dataitemMemory.key);
    memResponse.set_base(dataitemMemory.base);
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_get_dataitem_memory);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::get_atomic(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->get_atomic(
            memRequest.get_region_id(), memRequest.get_srcoffset(),
            memRequest.get_dstoffset(), memRequest.get_nbytes(),
            memRequest.get_key(), memRequest.get_src_base_addr(),
            memRequest.get_nodeaddr().c_str(), memRequest.get_nodeaddrsize());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_get_atomic);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::put_atomic(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->put_atomic(
            memRequest.get_region_id(), memRequest.get_srcoffset(),
            memRequest.get_dstoffset(), memRequest.get_nbytes(),
            memRequest.get_key(), memRequest.get_src_base_addr(),
            memRequest.get_nodeaddr().c_str(), memRequest.get_nodeaddrsize(),
            memRequest.get_data().c_str());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_put_atomic);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::scatter_strided_atomic(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->scatter_strided_atomic(
            memRequest.get_region_id(), memRequest.get_offset(),
            memRequest.get_nelements(), memRequest.get_firstelement(),
            memRequest.get_stride(), memRequest.get_elementsize(),
            memRequest.get_key(), memRequest.get_src_base_addr(),
            memRequest.get_nodeaddr().c_str(), memRequest.get_nodeaddrsize());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_scatter_strided_atomic);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::gather_strided_atomic(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->gather_strided_atomic(
            memRequest.get_region_id(), memRequest.get_offset(),
            memRequest.get_nelements(), memRequest.get_firstelement(),
            memRequest.get_stride(), memRequest.get_elementsize(),
            memRequest.get_key(), memRequest.get_src_base_addr(),
            memRequest.get_nodeaddr().c_str(), memRequest.get_nodeaddrsize());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_gather_strided_atomic);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::scatter_indexed_atomic(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->scatter_indexed_atomic(
            memRequest.get_region_id(), memRequest.get_offset(),
            memRequest.get_nelements(), memRequest.get_elementindex().c_str(),
            memRequest.get_elementsize(), memRequest.get_key(),
            memRequest.get_src_base_addr(), memRequest.get_nodeaddr().c_str(),
            memRequest.get_nodeaddrsize());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_scatter_indexed_atomic);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::gather_indexed_atomic(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->gather_indexed_atomic(
            memRequest.get_region_id(), memRequest.get_offset(),
            memRequest.get_nelements(), memRequest.get_elementindex().c_str(),
            memRequest.get_elementsize(), memRequest.get_key(),
            memRequest.get_src_base_addr(), memRequest.get_nodeaddr().c_str(),
            memRequest.get_nodeaddrsize());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_gather_indexed_atomic);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::update_memserver_addrlist(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        void *memServerInfoBuffer = NULL;
        if (memRequest.get_memserverinfo_size()) {
            memServerInfoBuffer =
                calloc(1, memRequest.get_memserverinfo_size());
            memcpy(memServerInfoBuffer,
                   (void *)memRequest.get_memserverinfo().c_str(),
                   memRequest.get_memserverinfo_size());
        }
        direct_memoryService->update_memserver_addrlist(
            memServerInfoBuffer, memRequest.get_memserverinfo_size(),
            memRequest.get_num_memservers());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_server_update_memserver_addrlist);
    HANDLE_ERROR(req.respond(memResponse));
}

void Fam_Memory_Service_Thallium_Server::create_region_failure_cleanup(
    const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest) {
    Fam_Memory_Service_Thallium_Response memResponse;
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_START_OPS()
    try {
        direct_memoryService->create_region_failure_cleanup(
            memRequest.get_region_id());
        memResponse.set_status(ok);
    } catch (Fam_Exception &e) {
        memResponse.set_errorcode(e.fam_error());
        memResponse.set_errormsg(e.fam_error_msg());
        memResponse.set_status(error);
    }
    MEMORY_SERVICE_THALLIUM_SERVER_PROFILE_END_OPS(
        thallium_mem_create_region_failure_cleanup);
    HANDLE_ERROR(req.respond(memResponse));
}
} // namespace openfam
