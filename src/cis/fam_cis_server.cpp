/*
 * fam_cis_server.cpp
 * Copyright (c) 2019-2021 Hewlett Packard Enterprise Development, LP. All
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
#include "cis/fam_cis_server.h"
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
MEMSERVER_PROFILE_START(CIS_SERVER)
#ifdef MEMSERVER_PROFILE
#define CIS_SERVER_PROFILE_START_OPS()                                         \
    {                                                                          \
        Profile_Time start = CIS_SERVER_get_time();

#define CIS_SERVER_PROFILE_END_OPS(apiIdx)                                     \
    Profile_Time end = CIS_SERVER_get_time();                                  \
    Profile_Time total = CIS_SERVER_time_diff_nanoseconds(start, end);         \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(CIS_SERVER, prof_##apiIdx, total)       \
    }
#define CIS_SERVER_PROFILE_DUMP() cis_server_profile_dump()
#else
#define CIS_SERVER_PROFILE_START_OPS()
#define CIS_SERVER_PROFILE_END_OPS(apiIdx)
#define CIS_SERVER_PROFILE_DUMP()
#endif

void cis_server_profile_dump() {
    MEMSERVER_PROFILE_END(CIS_SERVER);
    MEMSERVER_DUMP_PROFILE_BANNER(CIS_SERVER)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(CIS_SERVER, name, prof_##name)
#include "cis/cis_server_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name) MEMSERVER_PROFILE_TOTAL(CIS_SERVER, prof_##name)
#include "cis/cis_server_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(CIS_SERVER)
}

void Fam_CIS_Server::cis_server_initialize(Fam_CIS_Direct *__famCIS) {
    ostringstream message;
    message << "Error while initializing RPC service : ";
    numClients = 0;
    famCIS = __famCIS;
    MEMSERVER_PROFILE_INIT(CIS_SERVER)
    MEMSERVER_PROFILE_START_TIME(CIS_SERVER)
}

Fam_CIS_Server::~Fam_CIS_Server() { delete famCIS; }

::grpc::Status Fam_CIS_Server::signal_start(::grpc::ServerContext *context,
                                            const ::Fam_Request *request,
                                            ::Fam_Response *response) {
    __sync_add_and_fetch(&numClients, 1);
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::signal_termination(::grpc::ServerContext *context,
                                   const ::Fam_Request *request,
                                   ::Fam_Response *response) {
    __sync_add_and_fetch(&numClients, -1);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::reset_profile(::grpc::ServerContext *context,
                                             const ::Fam_Request *request,
                                             ::Fam_Response *response) {

    MEMSERVER_PROFILE_INIT(CIS_SERVER)
    MEMSERVER_PROFILE_START_TIME(CIS_SERVER)
    famCIS->reset_profile();
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::generate_profile(::grpc::ServerContext *context,
                                                const ::Fam_Request *request,
                                                ::Fam_Response *response) {
    CIS_SERVER_PROFILE_DUMP();
    famCIS->dump_profile();
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::get_num_memory_servers(::grpc::ServerContext *context,
                                       const ::Fam_Request *request,
                                       ::Fam_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    uint64_t numMemoryServer;
    try {
        numMemoryServer = famCIS->get_num_memory_servers();
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_num_memory_server(numMemoryServer);

    CIS_SERVER_PROFILE_END_OPS(get_num_memory_servers);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::create_region(::grpc::ServerContext *context,
                              const ::Fam_Region_Request *request,
                              ::Fam_Region_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    Fam_Region_Attributes *regionAttributes = new (Fam_Region_Attributes);
    regionAttributes->redundancyLevel =
        (Fam_Redundancy_Level)request->redundancylevel();
    regionAttributes->memoryType = (Fam_Memory_Type)request->memorytype();
    regionAttributes->interleaveEnable =
        (Fam_Interleave_Enable)request->interleaveenable();
    try {
        info = famCIS->create_region(request->name(), (size_t)request->size(),
                                     (mode_t)request->perm(), regionAttributes,
                                     request->uid(), request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_memserver_id(info.memoryServerId);
    response->set_regionid(info.regionId);
    response->set_offset(info.offset);

    CIS_SERVER_PROFILE_END_OPS(create_region);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::destroy_region(::grpc::ServerContext *context,
                               const ::Fam_Region_Request *request,
                               ::Fam_Region_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    try {
        famCIS->destroy_region(request->regionid(), request->memserver_id(),
                               request->uid(), request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    CIS_SERVER_PROFILE_END_OPS(destroy_region);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::resize_region(::grpc::ServerContext *context,
                              const ::Fam_Region_Request *request,
                              ::Fam_Region_Response *response) {

    CIS_SERVER_PROFILE_START_OPS()
    try {
        famCIS->resize_region(request->regionid(), request->size(),
                              request->memserver_id(), request->uid(),
                              request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    CIS_SERVER_PROFILE_END_OPS(resize_region);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::allocate(::grpc::ServerContext *context,
                                        const ::Fam_Dataitem_Request *request,
                                        ::Fam_Dataitem_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    Fam_Region_Item_Info info;
    try {
        info = famCIS->allocate(request->name(), (size_t)request->size(),
                                (mode_t)request->perm(), request->regionid(),
                                request->memserver_id(), request->uid(),
                                request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_key(info.key);
    response->set_regionid(request->regionid());
    response->set_offset(info.offset);
    response->set_base((uint64_t)info.base);
    response->set_memserver_id(info.memoryServerId);
    CIS_SERVER_PROFILE_END_OPS(allocate);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::deallocate(::grpc::ServerContext *context,
                                          const ::Fam_Dataitem_Request *request,
                                          ::Fam_Dataitem_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    try {
        famCIS->deallocate(request->regionid(), request->offset(),
                           request->memserver_id(), request->uid(),
                           request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    CIS_SERVER_PROFILE_END_OPS(deallocate);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::change_region_permission(::grpc::ServerContext *context,
                                         const ::Fam_Region_Request *request,
                                         ::Fam_Region_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    try {
        famCIS->change_region_permission(
            request->regionid(), (mode_t)request->perm(),
            request->memserver_id(), request->uid(), request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    CIS_SERVER_PROFILE_END_OPS(change_region_permission);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::change_dataitem_permission(
    ::grpc::ServerContext *context, const ::Fam_Dataitem_Request *request,
    ::Fam_Dataitem_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    try {
        famCIS->change_dataitem_permission(
            request->regionid(), request->offset(), (mode_t)request->perm(),
            request->memserver_id(), request->uid(), request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    CIS_SERVER_PROFILE_END_OPS(change_dataitem_permission);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::lookup_region(::grpc::ServerContext *context,
                              const ::Fam_Region_Request *request,
                              ::Fam_Region_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    Fam_Region_Item_Info info;
    try {
        info = famCIS->lookup_region(request->name(), request->uid(),
                                     request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_memserver_id(info.memoryServerId);
    response->set_regionid(info.regionId);
    response->set_offset(info.offset);
    response->set_size(info.size);
    response->set_perm(info.perm);
    response->set_name(info.name);
    response->set_maxnamelen(info.maxNameLen);
    response->set_redundancylevel(info.redundancyLevel);
    response->set_memorytype(info.memoryType);
    response->set_interleaveenable(info.interleaveEnable);

    CIS_SERVER_PROFILE_END_OPS(lookup_region);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::lookup(::grpc::ServerContext *context,
                                      const ::Fam_Dataitem_Request *request,
                                      ::Fam_Dataitem_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    Fam_Region_Item_Info info;
    try {
        info = famCIS->lookup(request->name(), request->regionname(),
                              request->uid(), request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_memserver_id(info.memoryServerId);
    response->set_regionid(info.regionId);
    response->set_offset(info.offset);
    response->set_size(info.size);
    response->set_perm(info.perm);
    response->set_name(info.name);
    response->set_maxnamelen(info.maxNameLen);
    // Return status OK
    CIS_SERVER_PROFILE_END_OPS(lookup);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::check_permission_get_region_info(
    ::grpc::ServerContext *context, const ::Fam_Region_Request *request,
    ::Fam_Region_Response *response) {

    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    Fam_Region_Item_Info info;
    try {
        info = famCIS->check_permission_get_region_info(
            request->regionid(), request->memserver_id(), request->uid(),
            request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_size(info.size);
    response->set_perm(info.perm);
    response->set_name(info.name);
    response->set_maxnamelen(info.maxNameLen);
    CIS_SERVER_PROFILE_END_OPS(check_permission_get_region_info);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::check_permission_get_item_info(
    ::grpc::ServerContext *context, const ::Fam_Dataitem_Request *request,
    ::Fam_Dataitem_Response *response) {

    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    Fam_Region_Item_Info info;
    try {
        info = famCIS->check_permission_get_item_info(
            request->regionid(), request->offset(), request->memserver_id(),
            request->uid(), request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_key(info.key);
    response->set_size(info.size);
    response->set_perm(info.perm);
    response->set_name(info.name);
    response->set_maxnamelen(info.maxNameLen);
    response->set_base((uint64_t)info.base);

    CIS_SERVER_PROFILE_END_OPS(check_permission_get_item_info);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::get_stat_info(::grpc::ServerContext *context,
                              const ::Fam_Dataitem_Request *request,
                              ::Fam_Dataitem_Response *response) {

    CIS_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    ostringstream message;
    try {
        info = famCIS->get_stat_info(request->regionid(), request->offset(),
                                     request->memserver_id(), request->uid(),
                                     request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_size(info.size);
    response->set_perm(info.perm);
    response->set_name(info.name);
    response->set_maxnamelen(info.maxNameLen);

    CIS_SERVER_PROFILE_END_OPS(get_stat_info);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::copy(::grpc::ServerContext *context,
                                    const ::Fam_Copy_Request *request,
                                    ::Fam_Copy_Response *response) {
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::backup(::grpc::ServerContext *context,
                       const ::Fam_Backup_Restore_Request *request,
                       ::Fam_Backup_Restore_Response *response) {
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::restore(::grpc::ServerContext *context,
                        const ::Fam_Backup_Restore_Request *request,
                        ::Fam_Backup_Restore_Response *response) {
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::acquire_CAS_lock(::grpc::ServerContext *context,
                                 const ::Fam_Dataitem_Request *request,
                                 ::Fam_Dataitem_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    try {
        famCIS->acquire_CAS_lock(request->offset(), request->memserver_id());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    CIS_SERVER_PROFILE_END_OPS(acquire_CAS_lock);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::release_CAS_lock(::grpc::ServerContext *context,
                                 const ::Fam_Dataitem_Request *request,
                                 ::Fam_Dataitem_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    try {
        famCIS->release_CAS_lock(request->offset(), request->memserver_id());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    CIS_SERVER_PROFILE_END_OPS(release_CAS_lock);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::get_backup_info(::grpc::ServerContext *context,
                                const ::Fam_Backup_Info_Request *request,
                                ::Fam_Backup_Info_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    Fam_Backup_Info info;
    try {
        info = famCIS->get_backup_info(request->filename(),
                                       request->memserver_id());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        response->set_size(-1);
        response->set_name(NULL);
        response->set_mode(-1);
        response->set_uid(-1);
        response->set_gid(-1);

        return ::grpc::Status::OK;
    }
    response->set_size(info.size);
    response->set_name(info.name);
    response->set_mode(info.mode);
    response->set_uid(info.uid);
    response->set_gid(info.gid);
    CIS_SERVER_PROFILE_END_OPS(get_backup_info);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::get_addr_size(::grpc::ServerContext *context,
                              const ::Fam_Address_Request *request,
                              ::Fam_Address_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    size_t addrSize;
    try {
        addrSize = famCIS->get_addr_size(request->memserver_id());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_addrnamelen(addrSize);
    CIS_SERVER_PROFILE_END_OPS(get_addr_size);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::get_addr(::grpc::ServerContext *context,
                                        const ::Fam_Address_Request *request,
                                        ::Fam_Address_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    void *addr;
    size_t addrSize;
    try {
        addrSize = famCIS->get_addr_size(request->memserver_id());
        addr = calloc(1, addrSize);
        famCIS->get_addr(addr, request->memserver_id());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

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
    if (addr)
        free(addr);

    CIS_SERVER_PROFILE_END_OPS(get_addr);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::get_memserverinfo_size(::grpc::ServerContext *context,
                                       const ::Fam_Request *request,
                                       ::Fam_Memserverinfo_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    size_t memServerInfoSize;
    try {
        memServerInfoSize = famCIS->get_memserverinfo_size();
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_memserverinfo_size(memServerInfoSize);
    CIS_SERVER_PROFILE_END_OPS(get_memserverinfo_size);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::get_memserverinfo(::grpc::ServerContext *context,
                                  const ::Fam_Request *request,
                                  ::Fam_Memserverinfo_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    size_t memServerInfoSize;
    void *memServerInfoBuffer;
    try {
        memServerInfoSize = famCIS->get_memserverinfo_size();
        memServerInfoBuffer = calloc(1, memServerInfoSize);
        famCIS->get_memserverinfo(memServerInfoBuffer);
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_memserverinfo_size(memServerInfoSize);
    response->set_memserverinfo(memServerInfoBuffer, memServerInfoSize);
    free(memServerInfoBuffer);
    CIS_SERVER_PROFILE_END_OPS(get_memserverinfo);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::get_atomic(::grpc::ServerContext *context,
                           const ::Fam_Atomic_Get_Request *request,
                           ::Fam_Atomic_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    try {
        famCIS->get_atomic(request->regionid(), request->srcoffset(),
                           request->dstoffset(), request->nbytes(),
                           request->key(), request->nodeaddr().c_str(),
                           request->nodeaddrsize(), request->memserver_id(),
                           request->uid(), request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    CIS_SERVER_PROFILE_END_OPS(get_atomic);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::put_atomic(::grpc::ServerContext *context,
                           const ::Fam_Atomic_Put_Request *request,
                           ::Fam_Atomic_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    try {
        famCIS->put_atomic(
            request->regionid(), request->srcoffset(), request->dstoffset(),
            request->nbytes(), request->key(), request->nodeaddr().c_str(),
            request->nodeaddrsize(), request->data().c_str(),
            request->memserver_id(), request->uid(), request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    CIS_SERVER_PROFILE_END_OPS(put_atomic);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::scatter_strided_atomic(
    ::grpc::ServerContext *context,
    const ::Fam_Atomic_SG_Strided_Request *request,
    ::Fam_Atomic_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    try {
        famCIS->scatter_strided_atomic(
            request->regionid(), request->offset(), request->nelements(),
            request->firstelement(), request->stride(), request->elementsize(),
            request->key(), request->nodeaddr().c_str(),
            request->nodeaddrsize(), request->memserver_id(), request->uid(),
            request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    CIS_SERVER_PROFILE_END_OPS(scatter_strided_atomic);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::gather_strided_atomic(
    ::grpc::ServerContext *context,
    const ::Fam_Atomic_SG_Strided_Request *request,
    ::Fam_Atomic_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    try {
        famCIS->gather_strided_atomic(
            request->regionid(), request->offset(), request->nelements(),
            request->firstelement(), request->stride(), request->elementsize(),
            request->key(), request->nodeaddr().c_str(),
            request->nodeaddrsize(), request->memserver_id(), request->uid(),
            request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    CIS_SERVER_PROFILE_END_OPS(gather_strided_atomic);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::scatter_indexed_atomic(
    ::grpc::ServerContext *context,
    const ::Fam_Atomic_SG_Indexed_Request *request,
    ::Fam_Atomic_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    try {
        famCIS->scatter_indexed_atomic(
            request->regionid(), request->offset(), request->nelements(),
            request->elementindex().c_str(), request->elementsize(),
            request->key(), request->nodeaddr().c_str(),
            request->nodeaddrsize(), request->memserver_id(), request->uid(),
            request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    CIS_SERVER_PROFILE_END_OPS(scatter_indexed_atomic);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_CIS_Server::gather_indexed_atomic(
    ::grpc::ServerContext *context,
    const ::Fam_Atomic_SG_Indexed_Request *request,
    ::Fam_Atomic_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    try {
        famCIS->gather_indexed_atomic(
            request->regionid(), request->offset(), request->nelements(),
            request->elementindex().c_str(), request->elementsize(),
            request->key(), request->nodeaddr().c_str(),
            request->nodeaddrsize(), request->memserver_id(), request->uid(),
            request->gid());
    }
    catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    CIS_SERVER_PROFILE_END_OPS(gather_indexed_atomic);

    // Return status OK
    return ::grpc::Status::OK;
}

} // namespace openfam
