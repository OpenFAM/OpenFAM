/*
 * fam_memory_service_server.cpp
 * Copyright (c) 2019-2021,2023 Hewlett Packard Enterprise Development, LP. All
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
#include "fam_memory_service_server.h"
#include "common/fam_memserver_profile.h"
#include <thread>

#include <boost/atomic.hpp>

#include <chrono>
#include <iomanip>
#include <string.h>
#include <unistd.h>

using namespace std;
using namespace chrono;
namespace openfam {
MEMSERVER_PROFILE_START(MEMORY_SERVICE_SERVER)
#ifdef MEMSERVER_PROFILE
#define MEMORY_SERVICE_SERVER_PROFILE_START_OPS()                              \
    {                                                                          \
        Profile_Time start = MEMORY_SERVICE_SERVER_get_time();

#define MEMORY_SERVICE_SERVER_PROFILE_END_OPS(apiIdx)                          \
    Profile_Time end = MEMORY_SERVICE_SERVER_get_time();                       \
    Profile_Time total =                                                       \
        MEMORY_SERVICE_SERVER_time_diff_nanoseconds(start, end);               \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(MEMORY_SERVICE_SERVER, prof_##apiIdx,   \
                                       total)                                  \
    }
#define MEMORY_SERVICE_SERVER_PROFILE_DUMP()                                   \
    memory_service_server_profile_dump()
#else
#define MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
#define MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_apiIdx)
#define MEMORY_SERVICE_SERVER_PROFILE_DUMP()
#endif

void memory_service_server_profile_dump() {
    MEMSERVER_PROFILE_END(MEMORY_SERVICE_SERVER);
    MEMSERVER_DUMP_PROFILE_BANNER(MEMORY_SERVICE_SERVER)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(MEMORY_SERVICE_SERVER, name, prof_##name)
#include "memory_service/memory_service_server_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(MEMORY_SERVICE_SERVER, prof_##name)
#include "memory_service/memory_service_server_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(MEMORY_SERVICE_SERVER)
}

Fam_Memory_Service_Server::Fam_Memory_Service_Server(
    Fam_Memory_Service_Direct *direct_memoryService_) {
    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_SERVER)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_SERVER)
    // memoryService = new Fam_Memory_Service_Direct(memserver_id);
    memoryService = direct_memoryService_;
}

void Fam_Memory_Service_Server::run() {
    //    memoryService->init_atomic_queue();
    const char *serverAddress;
    serverAddress = strdup((memoryService->get_rpcaddr()).c_str());
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

Fam_Memory_Service_Server::~Fam_Memory_Service_Server() {
    delete memoryService;
}

::grpc::Status Fam_Memory_Service_Server::signal_start(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Service_General_Request *request,
    ::Fam_Memory_Service_Start_Response *response) {
    __sync_add_and_fetch(&numClients, 1);

    // code to return libfabric addr
    size_t addrSize = memoryService->get_addr_size();
    void *addr = memoryService->get_addr();

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

#ifdef USE_THALLIUM
    // code to return thallium controlpath_addr
    char *rpc_addr =
        (char *)malloc(memoryService->get_controlpath_addr().size());
    memcpy(rpc_addr, memoryService->get_controlpath_addr().c_str(),
           memoryService->get_controlpath_addr().size());
    size_t rpc_addrSize = memoryService->get_controlpath_addr().size();

    response->set_rpc_addrnamelen(rpc_addrSize);
    int rpc_count = (int)(rpc_addrSize / sizeof(uint32_t));
    for (int ndx = 0; ndx < rpc_count; ndx++) {
        response->add_rpc_addrname(*((uint32_t *)rpc_addr + ndx));
    }

    // Only if addrSize is not multiple of 4 (fixed32)
    int rpc_lastBytesCount = 0;
    uint32_t rpc_lastBytes = 0;

    rpc_lastBytesCount = (int)(rpc_addrSize % sizeof(uint32_t));
    if (rpc_lastBytesCount > 0) {
        memcpy(&rpc_lastBytes, ((uint32_t *)rpc_addr + rpc_count),
               rpc_lastBytesCount);
        response->add_rpc_addrname(rpc_lastBytes);
    }
    free(rpc_addr);
#endif

    Fam_Memory_Type memory_type = memoryService->get_memtype();
    response->set_memorytype(memory_type);
    response->set_memserver_id(memoryService->get_memory_server_id());

    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::signal_termination(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Service_General_Request *request,
    ::Fam_Memory_Service_General_Response *response) {
    __sync_add_and_fetch(&numClients, -1);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::reset_profile(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Service_General_Request *request,
    ::Fam_Memory_Service_General_Response *response) {

    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_SERVER)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_SERVER)
    memoryService->reset_profile();
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::dump_profile(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Service_General_Request *request,
    ::Fam_Memory_Service_General_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_DUMP();
    memoryService->dump_profile();
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::create_region(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->create_region((uint64_t)request->region_id(),
                                     (size_t)request->size());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_create_region);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::destroy_region(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    uint64_t resourceStatus;
    try {
        memoryService->destroy_region(request->region_id(), &resourceStatus);
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_resource_status(resourceStatus);
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_destroy_region);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::resize_region(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {

    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->resize_region(request->region_id(), request->size());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_resize_region);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Memory_Service_Server::allocate(::grpc::ServerContext *context,
                                    const ::Fam_Memory_Service_Request *request,
                                    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    try {
        info = memoryService->allocate(request->region_id(),
                                       (size_t)request->size());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_offset(info.offset);

    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_allocate);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::deallocate(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->deallocate(request->region_id(), request->offset());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_deallocate);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Memory_Service_Server::copy(::grpc::ServerContext *context,
                                const ::Fam_Memory_Copy_Request *request,
                                ::Fam_Memory_Copy_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        uint64_t *srcOffsets = (uint64_t *)malloc(
            sizeof(uint64_t) * request->src_used_memsrv_cnt());
        uint64_t *srcKeys = (uint64_t *)malloc(sizeof(uint64_t) *
                                               request->src_used_memsrv_cnt());
        uint64_t *srcBaseAddrList = (uint64_t *)malloc(
            sizeof(uint64_t) * request->src_used_memsrv_cnt());
        uint64_t *srcMemserverIds = (uint64_t *)malloc(
            sizeof(uint64_t) * request->src_used_memsrv_cnt());

        for (uint64_t i = 0; i < request->src_used_memsrv_cnt(); i++) {
            srcOffsets[i] = request->src_offsets((int)i);
            srcKeys[i] = request->src_keys((int)i);
            srcBaseAddrList[i] = request->src_base_addr((int)i);
            srcMemserverIds[i] = request->src_ids((int)i);
        }
        memoryService->copy(request->src_region_id(), srcOffsets,
                            request->src_used_memsrv_cnt(),
                            request->src_copy_start(), request->src_copy_end(),
                            srcKeys, srcBaseAddrList, request->dest_region_id(),
                            request->dest_offset(),
                            request->dest_used_memsrv_cnt(), srcMemserverIds,
                            request->src_interleave_size(),
                            request->dest_interleave_size(), request->size());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_copy);
    // Return status OK
    return ::grpc::Status::OK;
}
::grpc::Status Fam_Memory_Service_Server::backup(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Backup_Restore_Request *request,
    ::Fam_Memory_Backup_Restore_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->backup(
            request->region_id(), request->offset(), request->size(),
            request->chunk_size(), request->used_memserver_cnt(),
            request->file_start_pos(), request->bname(), request->uid(),
            request->gid(), request->mode(), request->diname(),
            request->item_size(), request->write_metadata());

    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_backup);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::restore(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Backup_Restore_Request *request,
    ::Fam_Memory_Backup_Restore_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->restore(request->region_id(), request->offset(),
                               request->size(), request->chunk_size(),
                               request->used_memserver_cnt(),
                               request->file_start_pos(), request->bname());

    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_restore);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::get_backup_info(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Backup_Info_Request *request,
    ::Fam_Memory_Backup_Info_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    Fam_Backup_Info info;
    try {
        info = memoryService->get_backup_info(request->bname(), request->uid(),
                                              request->gid(), request->mode());
        response->set_name(info.bname);
        response->set_mode(info.mode);
        response->set_size(info.size);
        response->set_uid(info.uid);
        response->set_gid(info.gid);
    } catch (Fam_Exception &e) {
        response->set_size(-1);
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_get_backup_info);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::list_backup(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Backup_List_Request *request,
    ::Fam_Memory_Backup_List_Response *response) {

    try {
        string info = memoryService->list_backup(
            request->bname(), request->uid(), request->gid(), request->mode());
        response->set_contents(info);
    } catch (Fam_Exception &e) {
        std::string info = string("Backup Listing failed.");
        response->set_contents(info);
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::delete_backup(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Backup_List_Request *request,
    ::Fam_Memory_Backup_List_Response *response) {

    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->delete_backup(request->bname());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_delete_backup);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::acquire_CAS_lock(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->acquire_CAS_lock(request->offset());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_acquire_CAS_lock);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::release_CAS_lock(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->release_CAS_lock(request->offset());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_release_CAS_lock);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::get_local_pointer(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    void *localPointer;
    try {
        localPointer = memoryService->get_local_pointer(request->region_id(),
                                                        request->offset());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_base((uint64_t)localPointer);
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_get_local_pointer);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::register_region_memory(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()

    try {
        memoryService->register_region_memory(request->region_id(),
                                              request->rw_flag());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_register_region_memory);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::open_region_with_registration(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    Fam_Region_Memory regionMemory;
    try {
        regionMemory = memoryService->open_region_with_registration(
            request->region_id(), request->rw_flag());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    int numExtents = (int)regionMemory.keys.size();
    for (int i = 0; i < numExtents; i++) {
        response->add_region_keys(regionMemory.keys[i]);
        response->add_region_base(regionMemory.base[i]);
    }

    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_get_region_key);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::open_region_without_registration(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()

    try {
        memoryService->open_region_without_registration(request->region_id());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_get_region_key);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::close_region(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()

    uint64_t status;
    try {
        status = memoryService->close_region(request->region_id());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_resource_status(status);

    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_close_region);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::get_region_memory(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    Fam_Region_Memory regionMemory;
    try {
        regionMemory = memoryService->get_region_memory(request->region_id(),
                                                        request->rw_flag());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    int numExtents = (int)regionMemory.keys.size();
    for (int i = 0; i < numExtents; i++) {
        response->add_region_keys(regionMemory.keys[i]);
        response->add_region_base(regionMemory.base[i]);
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_get_region_key);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::get_dataitem_memory(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    Fam_Dataitem_Memory dataitemMemory;
    try {
        dataitemMemory = memoryService->get_dataitem_memory(
            request->region_id(), request->offset(), request->size(),
            request->rw_flag());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_key(dataitemMemory.key);
    response->set_base(dataitemMemory.base);
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_get_key);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::get_atomic(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Atomic_Get_Request *request,
    ::Fam_Memory_Atomic_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->get_atomic(
            request->regionid(), request->srcoffset(), request->dstoffset(),
            request->nbytes(), request->key(), request->src_base_addr(),
            request->nodeaddr().c_str(), request->nodeaddrsize());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_get_atomic);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::put_atomic(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Atomic_Put_Request *request,
    ::Fam_Memory_Atomic_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->put_atomic(
            request->regionid(), request->srcoffset(), request->dstoffset(),
            request->nbytes(), request->key(), request->src_base_addr(),
            request->nodeaddr().c_str(), request->nodeaddrsize(),
            request->data().c_str());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_put_atomic);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::scatter_strided_atomic(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Atomic_SG_Strided_Request *request,
    ::Fam_Memory_Atomic_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->scatter_strided_atomic(
            request->regionid(), request->offset(), request->nelements(),
            request->firstelement(), request->stride(), request->elementsize(),
            request->key(), request->src_base_addr(),
            request->nodeaddr().c_str(), request->nodeaddrsize());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_scatter_strided_atomic);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::gather_strided_atomic(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Atomic_SG_Strided_Request *request,
    ::Fam_Memory_Atomic_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->gather_strided_atomic(
            request->regionid(), request->offset(), request->nelements(),
            request->firstelement(), request->stride(), request->elementsize(),
            request->key(), request->src_base_addr(),
            request->nodeaddr().c_str(), request->nodeaddrsize());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_gather_strided_atomic);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::scatter_indexed_atomic(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Atomic_SG_Indexed_Request *request,
    ::Fam_Memory_Atomic_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->scatter_indexed_atomic(
            request->regionid(), request->offset(), request->nelements(),
            request->elementindex().c_str(), request->elementsize(),
            request->key(), request->src_base_addr(),
            request->nodeaddr().c_str(), request->nodeaddrsize());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_scatter_indexed_atomic);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::gather_indexed_atomic(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Atomic_SG_Indexed_Request *request,
    ::Fam_Memory_Atomic_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        memoryService->gather_indexed_atomic(
            request->regionid(), request->offset(), request->nelements(),
            request->elementindex().c_str(), request->elementsize(),
            request->key(), request->src_base_addr(),
            request->nodeaddr().c_str(), request->nodeaddrsize());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_gather_indexed_atomic);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::update_memserver_addrlist(
    ::grpc::ServerContext *context,
    const ::Fam_Memory_Service_Addr_Info *request,
    ::Fam_Memory_Service_General_Response *response) {
    MEMORY_SERVICE_SERVER_PROFILE_START_OPS()
    try {
        void *memServerInfoBuffer = NULL;
        if (request->memserverinfo_size()) {
            memServerInfoBuffer = calloc(1, request->memserverinfo_size());
            memcpy(memServerInfoBuffer,
                   (void *)request->memserverinfo().c_str(),
                   request->memserverinfo_size());
        }
        memoryService->update_memserver_addrlist(memServerInfoBuffer,
                                                 request->memserverinfo_size(),
                                                 request->num_memservers());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    MEMORY_SERVICE_SERVER_PROFILE_END_OPS(mem_server_update_memserver_addrlist);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Memory_Service_Server::create_region_failure_cleanup(
    ::grpc::ServerContext *context, const ::Fam_Memory_Service_Request *request,
    ::Fam_Memory_Service_Response *response) {
    try {
        memoryService->create_region_failure_cleanup(request->region_id());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    // Return status OK
    return ::grpc::Status::OK;
}

} // namespace openfam
