/*
 * fam_cis_server.cpp
 * Copyright (c) 2019-2020 Hewlett Packard Enterprise Development, LP. All
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

void Fam_CIS_Server::progress_thread() {
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        while (1) {
            if (!haltProgress)
                famOps->quiet();
            else
                break;
        }
    }
}
void Fam_CIS_Server::cis_server_initialize(char *name, char *service,
                                           char *provider,
                                           Fam_CIS_Direct *__famCIS) {
    ostringstream message;
    message << "Error while initializing RPC service : ";
    numClients = 0;
    shouldShutdown = false;
    famCIS = __famCIS;
    MEMSERVER_PROFILE_INIT(CIS_SERVER)
    MEMSERVER_PROFILE_START_TIME(CIS_SERVER)
    fiMrs = NULL;
    fenceMr = 0;

    famOps =
        new Fam_Ops_Libfabric(name, service, true, provider,
                              FAM_THREAD_MULTIPLE, NULL, FAM_CONTEXT_DEFAULT);
    int ret = famOps->initialize();
    if (ret < 0) {
        message << "famOps initialization failed";
        throw CIS_Exception(OPS_INIT_FAILED, message.str().c_str());
    }
    struct fi_info *fi = famOps->get_fi();
    if (fi->domain_attr->control_progress == FI_PROGRESS_MANUAL ||
        fi->domain_attr->data_progress == FI_PROGRESS_MANUAL) {
        libfabricProgressMode = FI_PROGRESS_MANUAL;
    }

    register_fence_memory();

    for (int i = 0; i < CAS_LOCK_CNT; i++) {
        (void)pthread_mutex_init(&casLock[i], NULL);
    }
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        haltProgress = false;
        progressThread = std::thread(&Fam_CIS_Server::progress_thread, this);
    }
}

void Fam_CIS_Server::cis_server_finalize() {
    famCIS->cis_direct_finalize();
    deregister_fence_memory();
    for (int i = 0; i < CAS_LOCK_CNT; i++) {
        (void)pthread_mutex_destroy(&casLock[i]);
    }
    famOps->finalize();
}

Fam_CIS_Server::~Fam_CIS_Server() {
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        haltProgress = true;
        progressThread.join();
    }
    famOps->finalize();
    delete famOps;
    delete famCIS;
}

::grpc::Status Fam_CIS_Server::signal_start(::grpc::ServerContext *context,
                                            const ::Fam_Request *request,
                                            ::Fam_Start_Response *response) {
    __sync_add_and_fetch(&numClients, 1);

    size_t addrSize = famOps->get_addr_size();
    void *addr = famOps->get_addr();

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
    fabric_reset_profile();
    famCIS->reset_profile();
    return ::grpc::Status::OK;
}

void Fam_CIS_Server::dump_profile() {
    CIS_SERVER_PROFILE_DUMP();
    fabric_dump_profile();
    famCIS->dump_profile();
}

::grpc::Status Fam_CIS_Server::generate_profile(::grpc::ServerContext *context,
                                                const ::Fam_Request *request,
                                                ::Fam_Response *response) {
#ifdef MEMSERVER_PROFILE
    kill(getpid(), SIGINT);
#endif
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::create_region(::grpc::ServerContext *context,
                              const ::Fam_Region_Request *request,
                              ::Fam_Region_Response *response) {
    CIS_SERVER_PROFILE_START_OPS()
    Fam_Region_Item_Info info;
    try {
        info = famCIS->create_region(request->name(), (size_t)request->size(),
                                     (mode_t)request->perm(),
                                     static_cast<Fam_Redundancy_Level>(0), 0,
                                     request->uid(), request->gid());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
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
        famCIS->destroy_region(request->regionid(), 0, request->uid(),
                               request->gid());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    deregister_region_memory(request->regionid());

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
        famCIS->resize_region(request->regionid(), request->size(), 0,
                              request->uid(), request->gid());
    } catch (Fam_Exception &e) {
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
    uint64_t key;
    try {
        info = famCIS->allocate(request->name(), (size_t)request->size(),
                                (mode_t)request->perm(), request->regionid(), 0,
                                request->uid(), request->gid());
        // Generate and register key for datapath access
        register_memory(info, request->uid(), request->gid(), key);
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_key(key);
    response->set_regionid(request->regionid());
    response->set_offset(info.offset);
    if (strncmp(famOps->get_provider(), "verbs", 5) == 0)
        response->set_base((uint64_t)info.base);
    else
        response->set_base((uint64_t)0);
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
        famCIS->deallocate(request->regionid(), request->offset(), 0,
                           request->uid(), request->gid());
        deregister_memory(request->regionid(), request->offset());
    } catch (Fam_Exception &e) {
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
        famCIS->change_region_permission(request->regionid(),
                                         (mode_t)request->perm(), 0,
                                         request->uid(), request->gid());
    } catch (Fam_Exception &e) {
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
            request->regionid(), request->offset(), (mode_t)request->perm(), 0,
            request->uid(), request->gid());
    } catch (Fam_Exception &e) {
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
        info = famCIS->lookup_region(request->name(), 0, request->uid(),
                                     request->gid());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_regionid(info.regionId);
    response->set_offset(info.offset);
    response->set_size(info.size);
    response->set_perm(info.perm);
    response->set_name(info.name);
    response->set_maxnamelen(info.maxNameLen);

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
        info = famCIS->lookup(request->name(), request->regionname(), 0,
                              request->uid(), request->gid());
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

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
            request->regionid(), 0, request->uid(), request->gid());
    } catch (Fam_Exception &e) {
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
    uint64_t key;
    try {
        info = famCIS->check_permission_get_item_info(
            request->regionid(), request->offset(), 0, request->uid(),
            request->gid());
        register_memory(info, request->uid(), request->gid(), key);
    } catch (Fam_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    response->set_key(key);
    response->set_size(info.size);
    response->set_perm(info.perm);
    response->set_name(info.name);
    response->set_maxnamelen(info.maxNameLen);
    if (strncmp(famOps->get_provider(), "verbs", 5) == 0)
        response->set_base((uint64_t)info.base);
    else
        response->set_base((uint64_t)0);

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
        info = famCIS->check_permission_get_item_info(
            request->regionid(), request->offset(), 0, request->uid(),
            request->gid());
    } catch (Fam_Exception &e) {
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

uint64_t Fam_CIS_Server::generate_access_key(uint64_t regionId,
                                             uint64_t dataitemId,
                                             bool permission) {
    uint64_t key = 0;

    key |= (regionId & REGIONID_MASK) << REGIONID_SHIFT;
    key |= (dataitemId & DATAITEMID_MASK) << DATAITEMID_SHIFT;
    key |= permission;

    return key;
}

void Fam_CIS_Server::register_fence_memory() {
    ostringstream message;
    message << "Error while registering fence memory : ";
    int ret;
    fid_mr *mr = 0;
    uint64_t key = FAM_FENCE_KEY;
    void *localPointer;
    size_t len = (size_t)sysconf(_SC_PAGESIZE);
    int fd = -1;

    localPointer = mmap(NULL, len, PROT_WRITE, MAP_SHARED | MAP_ANON, fd, 0);
    if (localPointer == MAP_FAILED) {
    }

    // register the memory location with libfabric
    if (fenceMr == 0) {
        ret = fabric_register_mr(localPointer, len, &key, famOps->get_domain(),
                                 1, mr);
        if (ret < 0) {
            message << "failed to register with fabric";
            throw CIS_Exception(FENCE_REG_FAILED, message.str().c_str());
        }
        fenceMr = mr;
    }
}

void Fam_CIS_Server::register_memory(Fam_Region_Item_Info info, uint32_t uid,
                                     uint32_t gid, uint64_t &key) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    message << "Error while registering memory : ";
    uint64_t dataitemId = info.offset / MIN_OBJ_SIZE;
    if (fiMrs == NULL)
        fiMrs = famOps->get_fiMrs();
    fid_mr *mr = 0;
    int ret = 0;
    uint64_t mrkey = 0;
    bool rwflag;
    Fam_Region_Map_t *fiRegionMap = NULL;
    Fam_Region_Map_t *fiRegionMapDiscard = NULL;

    void *localPointer = info.base;

    if (info.key == (FAM_WRITE_KEY_SHM | FAM_READ_KEY_SHM)) {
        key = mrkey = generate_access_key(info.regionId, dataitemId, 1);
        rwflag = 1;
    } else if (info.key == FAM_READ_KEY_SHM) {
        key = mrkey = generate_access_key(info.regionId, dataitemId, 0);
        rwflag = 0;
    } else {
        message << "not permitted to access dataitem";
        throw CIS_Exception(NO_PERMISSION, message.str().c_str());
    }

    // register the data item with required permission with libfabric
    // Start by taking a readlock on fiMrs
    pthread_rwlock_rdlock(famOps->get_mr_lock());
    auto regionMrObj = fiMrs->find(info.regionId);
    if (regionMrObj == fiMrs->end()) {
        // Create a RegionMap
        fiRegionMap = (Fam_Region_Map_t *)calloc(1, sizeof(Fam_Region_Map_t));
        fiRegionMap->regionId = info.regionId;
        fiRegionMap->fiRegionMrs = new std::map<uint64_t, fid_mr *>();
        pthread_rwlock_init(&fiRegionMap->fiRegionLock, NULL);
        // RegionMap not found, release read lock, take a write lock
        pthread_rwlock_unlock(famOps->get_mr_lock());
        pthread_rwlock_wrlock(famOps->get_mr_lock());
        // Check again if regionMap added by another thread.
        regionMrObj = fiMrs->find(info.regionId);
        if (regionMrObj == fiMrs->end()) {
            // Add the fam region map into fiMrs
            fiMrs->insert({info.regionId, fiRegionMap});
        } else {
            // Region Map already added by another thread,
            // discard the one created here.
            fiRegionMapDiscard = fiRegionMap;
            fiRegionMap = regionMrObj->second;
        }
    } else {
        fiRegionMap = regionMrObj->second;
    }

    // Take a writelock on fiRegionMap
    pthread_rwlock_wrlock(&fiRegionMap->fiRegionLock);
    // Release lock on fiMrs
    pthread_rwlock_unlock(famOps->get_mr_lock());

    // Delete the discarded region map here.
    if (fiRegionMapDiscard != NULL) {
        delete fiRegionMapDiscard->fiRegionMrs;
        free(fiRegionMapDiscard);
    }

    auto mrObj = fiRegionMap->fiRegionMrs->find(key);
    if (mrObj == fiRegionMap->fiRegionMrs->end()) {
        ret = fabric_register_mr(localPointer, info.size, &mrkey,
                                 famOps->get_domain(), rwflag, mr);
        if (ret < 0) {
            pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
            message << "failed to register with fabric";
            throw CIS_Exception(ITEM_REGISTRATION_FAILED,
                                message.str().c_str());
        }

        fiRegionMap->fiRegionMrs->insert({key, mr});
    } else {
        mrkey = fi_mr_key(mrObj->second);
    }
    // Always return mrkey, which might be different than key.
    key = mrkey;

    pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
    CIS_SERVER_PROFILE_END_OPS(deregister_memory);
}

void Fam_CIS_Server::deregister_fence_memory() {
    ostringstream message;
    message << "Error while deregistering fence memory : ";
    int ret = 0;
    if (fenceMr != 0) {
        ret = fabric_deregister_mr(fenceMr);
        if (ret < 0) {
            message << "failed to deregister with fabric";
            throw CIS_Exception(FENCE_DEREG_FAILED, message.str().c_str());
        }
    }
    fenceMr = 0;
}

void Fam_CIS_Server::deregister_memory(uint64_t regionId, uint64_t offset) {
    CIS_SERVER_PROFILE_START_OPS()
    ostringstream message;
    message << "Error while deregistering memory : ";

    int ret = 0;
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    uint64_t rKey = generate_access_key(regionId, dataitemId, 0);
    uint64_t rwKey = generate_access_key(regionId, dataitemId, 1);
    Fam_Region_Map_t *fiRegionMap;

    if (fiMrs == NULL)
        fiMrs = famOps->get_fiMrs();

    // Take read lock on fiMrs
    pthread_rwlock_rdlock(famOps->get_mr_lock());
    auto regionMrObj = fiMrs->find(regionId);
    if (regionMrObj == fiMrs->end()) {
        pthread_rwlock_unlock(famOps->get_mr_lock());
        return;
    } else {
        fiRegionMap = regionMrObj->second;
    }

    // Take a writelock on fiRegionMap
    pthread_rwlock_wrlock(&fiRegionMap->fiRegionLock);
    // Release lock on fiMrs
    pthread_rwlock_unlock(famOps->get_mr_lock());

    auto rMr = fiRegionMap->fiRegionMrs->find(rKey);
    auto rwMr = fiRegionMap->fiRegionMrs->find(rwKey);
    if (rMr != fiRegionMap->fiRegionMrs->end()) {
        ret = fabric_deregister_mr(rMr->second);
        if (ret < 0) {
            pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
            message << "failed to deregister with fabric";
            throw CIS_Exception(ITEM_DEREGISTRATION_FAILED,
                                message.str().c_str());
        }
        fiRegionMap->fiRegionMrs->erase(rMr);
    }

    if (rwMr != fiRegionMap->fiRegionMrs->end()) {
        ret = fabric_deregister_mr(rwMr->second);
        if (ret < 0) {
            pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
            message << "failed to deregister with fabric";
            throw CIS_Exception(ITEM_DEREGISTRATION_FAILED,
                                message.str().c_str());
        }
        fiRegionMap->fiRegionMrs->erase(rwMr);
    }

    pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
    CIS_SERVER_PROFILE_END_OPS(deregister_memory);
}

void Fam_CIS_Server::deregister_region_memory(uint64_t regionId) {
    int ret = 0;
    Fam_Region_Map_t *fiRegionMap;

    if (fiMrs == NULL)
        fiMrs = famOps->get_fiMrs();

    // Take write lock on fiMrs
    pthread_rwlock_wrlock(famOps->get_mr_lock());

    auto regionMrObj = fiMrs->find(regionId);
    if (regionMrObj == fiMrs->end()) {
        pthread_rwlock_unlock(famOps->get_mr_lock());
        return;
    } else {
        fiRegionMap = regionMrObj->second;
    }

    // Take a writelock on fiRegionMap
    pthread_rwlock_wrlock(&fiRegionMap->fiRegionLock);
    // Remove region map from fiMrs
    fiMrs->erase(regionMrObj);
    // Release lock on fiMrs
    pthread_rwlock_unlock(famOps->get_mr_lock());

    // Unregister all dataItem memory from region map
    for (auto mr : *(fiRegionMap->fiRegionMrs)) {
        ret = fabric_deregister_mr(mr.second);
        if (ret < 0) {
            cout << "destroy region<" << fiRegionMap->regionId
                 << ">: memory deregister failed with errno(" << ret << ")"
                 << endl;
        }
    }

    pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
    fiRegionMap->fiRegionMrs->clear();
    delete fiRegionMap->fiRegionMrs;
    free(fiRegionMap);
}

::grpc::Status
Fam_CIS_Server::acquire_CAS_lock(::grpc::ServerContext *context,
                                 const ::Fam_Dataitem_Request *request,
                                 ::Fam_Dataitem_Response *response) {
    int idx = LOCKHASH(request->offset());
    pthread_mutex_lock(&casLock[idx]);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_Server::release_CAS_lock(::grpc::ServerContext *context,
                                 const ::Fam_Dataitem_Request *request,
                                 ::Fam_Dataitem_Response *response) {
    int idx = LOCKHASH(request->offset());
    pthread_mutex_unlock(&casLock[idx]);

    // Return status OK
    return ::grpc::Status::OK;
}

} // namespace openfam
