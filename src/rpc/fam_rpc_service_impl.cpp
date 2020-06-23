/*
 * fam_rpc_service_impl.cpp
 * Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All rights
 * reserved. Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
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
#include "fam_rpc_service_impl.h"
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
MEMSERVER_PROFILE_START(RPC_SERVICE)
#ifdef MEMSERVER_PROFILE
#define RPC_SERVICE_PROFILE_START_OPS()                                        \
    {                                                                          \
        Profile_Time start = RPC_SERVICE_get_time();

#define RPC_SERVICE_PROFILE_END_OPS(apiIdx)                                    \
    Profile_Time end = RPC_SERVICE_get_time();                                 \
    Profile_Time total = RPC_SERVICE_time_diff_nanoseconds(start, end);        \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(RPC_SERVICE, prof_##apiIdx, total)      \
    }
#define RPC_SERVICE_PROFILE_DUMP() rpc_service_profile_dump()
#else
#define RPC_SERVICE_PROFILE_START_OPS()
#define RPC_SERVICE_PROFILE_END_OPS(apiIdx)
#define RPC_SERVICE_PROFILE_DUMP()
#endif

void rpc_service_profile_dump() {
    MEMSERVER_PROFILE_END(RPC_SERVICE);
    MEMSERVER_DUMP_PROFILE_BANNER(RPC_SERVICE)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(RPC_SERVICE, name, prof_##name)
#include "rpc/rpc_service_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(RPC_SERVICE, prof_##name)
#include "rpc/rpc_service_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(RPC_SERVICE)
}

void Fam_Rpc_Service_Impl::progress_thread() {
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        while (1) {
            if (!haltProgress)
                famOps->quiet();
            else
                break;
        }
    }
}
void Fam_Rpc_Service_Impl::rpc_service_initialize(
    char *name, char *service, char *provider, Memserver_Allocator *memAlloc) {
    ostringstream message;
    message << "Error while initializing RPC service : ";
    numClients = 0;
    shouldShutdown = false;
    allocator = memAlloc;
    MEMSERVER_PROFILE_INIT(RPC_SERVICE)
    MEMSERVER_PROFILE_START_TIME(RPC_SERVICE)
    fiMrs = NULL;
    fenceMr = 0;

    famOps =
        new Fam_Ops_Libfabric(name, service, true, provider,
                              FAM_THREAD_MULTIPLE, NULL, FAM_CONTEXT_DEFAULT);
    int ret = famOps->initialize();
    if (ret < 0) {
        message << "famOps initialization failed";
        throw Memserver_Exception(OPS_INIT_FAILED, message.str().c_str());
    }
    struct fi_info *fi = famOps->get_fi();
    if (fi->domain_attr->control_progress == FI_PROGRESS_MANUAL ||
        fi->domain_attr->data_progress == FI_PROGRESS_MANUAL) {
        libfabricProgressMode = FI_PROGRESS_MANUAL;
    }
    ret = register_fence_memory();
    if (ret < 0) {
        message << "Failed to register memory for fence operation";
        throw Memserver_Exception(FENCE_REG_FAILED, message.str().c_str());
    }
    for (int i = 0; i < CAS_LOCK_CNT; i++) {
        (void)pthread_mutex_init(&casLock[i], NULL);
    }
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        haltProgress = false;
        progressThread =
            std::thread(&Fam_Rpc_Service_Impl::progress_thread, this);
    }
}

void Fam_Rpc_Service_Impl::rpc_service_finalize() {
    allocator->memserver_allocator_finalize();
    deregister_fence_memory();
    for (int i = 0; i < CAS_LOCK_CNT; i++) {
        (void)pthread_mutex_destroy(&casLock[i]);
    }
    famOps->finalize();
}

Fam_Rpc_Service_Impl::~Fam_Rpc_Service_Impl() {
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        haltProgress = true;
        progressThread.join();
    }
    famOps->finalize();
    delete famOps;
    delete allocator;
}

::grpc::Status
Fam_Rpc_Service_Impl::signal_start(::grpc::ServerContext *context,
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
Fam_Rpc_Service_Impl::signal_termination(::grpc::ServerContext *context,
                                         const ::Fam_Request *request,
                                         ::Fam_Response *response) {
    __sync_add_and_fetch(&numClients, -1);
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::reset_profile(::grpc::ServerContext *context,
                                    const ::Fam_Request *request,
                                    ::Fam_Response *response) {

    MEMSERVER_PROFILE_INIT(RPC_SERVICE)
    MEMSERVER_PROFILE_START_TIME(RPC_SERVICE)
    fabric_reset_profile();
    allocator->reset_profile();
    return ::grpc::Status::OK;
}

void Fam_Rpc_Service_Impl::dump_profile() {
    RPC_SERVICE_PROFILE_DUMP();
    fabric_dump_profile();
    allocator->dump_profile();
}

::grpc::Status
Fam_Rpc_Service_Impl::generate_profile(::grpc::ServerContext *context,
                                       const ::Fam_Request *request,
                                       ::Fam_Response *response) {
#ifdef MEMSERVER_PROFILE
    kill(getpid(), SIGINT);
#endif
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::create_region(::grpc::ServerContext *context,
                                    const ::Fam_Region_Request *request,
                                    ::Fam_Region_Response *response) {
    RPC_SERVICE_PROFILE_START_OPS()
    uint64_t regionId;
    try {
        allocator->create_region(
            request->name(), regionId, (size_t)request->size(),
            (mode_t)request->perm(), request->uid(), request->gid());
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    response->set_regionid(regionId);
    response->set_offset(INVALID_OFFSET);

    RPC_SERVICE_PROFILE_END_OPS(create_region);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::destroy_region(::grpc::ServerContext *context,
                                     const ::Fam_Region_Request *request,
                                     ::Fam_Region_Response *response) {
    RPC_SERVICE_PROFILE_START_OPS()
    ostringstream message;
    try {
        allocator->destroy_region(request->regionid(), request->uid(),
                                  request->gid());
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    int ret = deregister_region_memory(request->regionid());

    if (ret < 0) {
        response->set_errorcode(FAM_ERR_RESOURCE);
        message << "Error while destroy region: ";
        message << "dataitem deregistration failed";
        response->set_errormsg(message.str());
        return ::grpc::Status::OK;
    }

    RPC_SERVICE_PROFILE_END_OPS(destroy_region);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::resize_region(::grpc::ServerContext *context,
                                    const ::Fam_Region_Request *request,
                                    ::Fam_Region_Response *response) {

    RPC_SERVICE_PROFILE_START_OPS()
    try {
        allocator->resize_region(request->regionid(), request->uid(),
                                 request->gid(), request->size());
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    RPC_SERVICE_PROFILE_END_OPS(resize_region);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::allocate(::grpc::ServerContext *context,
                               const ::Fam_Dataitem_Request *request,
                               ::Fam_Dataitem_Response *response) {
    RPC_SERVICE_PROFILE_START_OPS()
    ostringstream message;
    uint64_t offset;
    uint64_t key;
    Fam_DataItem_Metadata dataitem;
    void *localPointer;
    int ret = 0;
    if (request->dup()) {
        try {
            allocator->get_dataitem(request->regionid(), request->offset(),
                                    request->uid(), request->gid(), dataitem);
        } catch (Memserver_Exception &e) {
            response->set_errorcode(e.fam_error());
            response->set_errormsg(e.fam_error_msg());
            return ::grpc::Status::OK;
        }

        try {
            allocator->allocate("", request->regionid(), (size_t)dataitem.size,
                                offset, dataitem.perm, request->uid(),
                                request->gid(), dataitem, localPointer);
        } catch (Memserver_Exception &e) {
            response->set_errorcode(e.fam_error());
            response->set_errormsg(e.fam_error_msg());
            return ::grpc::Status::OK;
        }
    } else {
        try {
            allocator->allocate(request->name(), request->regionid(),
                                (size_t)request->size(), offset,
                                (mode_t)request->perm(), request->uid(),
                                request->gid(), dataitem, localPointer);
        } catch (Memserver_Exception &e) {
            response->set_errorcode(e.fam_error());
            response->set_errormsg(e.fam_error_msg());
            return ::grpc::Status::OK;
        }
    }

    // Generate and register key for datapath access
    try {
        ret = register_memory(dataitem, localPointer, request->uid(),
                              request->gid(), key);
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    if (ret < 0) {
        if (ret == NOT_PERMITTED) {
            response->set_errorcode(FAM_ERR_NOPERM);
            message << "Error while allocating dataitem : ";
            message << "No permission, dataitem registration failed";
            response->set_errormsg(message.str());
            return ::grpc::Status::OK;
        } else {
            response->set_errorcode(FAM_ERR_RESOURCE);
            message << "Error while allocating dataitem : ";
            message << "dataitem registration failed";
            response->set_errormsg(message.str());
            return ::grpc::Status::OK;
        }
    }

    response->set_key(key);
    response->set_regionid(request->regionid());
    response->set_offset(offset);
    if (strncmp(famOps->get_provider(), "verbs", 5) == 0)
        response->set_base((uint64_t)localPointer);
    else
        response->set_base((uint64_t)0);
    RPC_SERVICE_PROFILE_END_OPS(allocate);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::deallocate(::grpc::ServerContext *context,
                                 const ::Fam_Dataitem_Request *request,
                                 ::Fam_Dataitem_Response *response) {
    RPC_SERVICE_PROFILE_START_OPS()
    ostringstream message;
    try {
        allocator->deallocate(request->regionid(), request->offset(),
                              request->uid(), request->gid());
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    int ret = deregister_memory(request->regionid(), request->offset());

    if (ret < 0) {
        response->set_errorcode(FAM_ERR_RESOURCE);
        message << "Error while deallocating dataitem : ";
        message << "dataitem deregistration failed";
        response->set_errormsg(message.str());
        return ::grpc::Status::OK;
    }
    RPC_SERVICE_PROFILE_END_OPS(deallocate);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Rpc_Service_Impl::change_region_permission(
    ::grpc::ServerContext *context, const ::Fam_Region_Request *request,
    ::Fam_Region_Response *response) {
    RPC_SERVICE_PROFILE_START_OPS()
    try {
        allocator->change_region_permission(request->regionid(),
                                            (mode_t)request->perm(),
                                            request->uid(), request->gid());
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }
    RPC_SERVICE_PROFILE_END_OPS(change_region_permission);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Rpc_Service_Impl::change_dataitem_permission(
    ::grpc::ServerContext *context, const ::Fam_Dataitem_Request *request,
    ::Fam_Dataitem_Response *response) {
    RPC_SERVICE_PROFILE_START_OPS()
    try {
        allocator->change_dataitem_permission(
            request->regionid(), request->offset(), (mode_t)request->perm(),
            request->uid(), request->gid());
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    RPC_SERVICE_PROFILE_END_OPS(change_dataitem_permission);
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::lookup_region(::grpc::ServerContext *context,
                                    const ::Fam_Region_Request *request,
                                    ::Fam_Region_Response *response) {
    RPC_SERVICE_PROFILE_START_OPS()
    ostringstream message;
    Fam_Region_Metadata region;
    try {
        allocator->get_region(request->name(), request->uid(), request->gid(),
                              region);
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    if (request->uid() == region.uid) {
        response->set_regionid(region.regionId);
        response->set_offset(region.offset);
        response->set_size(region.size);
        response->set_perm(region.perm);
        response->set_name(region.name);
    } else if (allocator->check_region_permission(region, 0, request->uid(),
                                                  request->gid())) {
        response->set_regionid(region.regionId);
        response->set_offset(region.offset);
        response->set_size(region.size);
        response->set_perm(region.perm);
        response->set_name(region.name);
    } else {
        response->set_errorcode(FAM_ERR_NOPERM);
        message << "Error while looking up for region : ";
        message << "Region access in not permitted";
        response->set_errormsg(message.str());
        return ::grpc::Status::OK;
    }
    RPC_SERVICE_PROFILE_END_OPS(lookup_region);
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::lookup(::grpc::ServerContext *context,
                             const ::Fam_Dataitem_Request *request,
                             ::Fam_Dataitem_Response *response) {
    RPC_SERVICE_PROFILE_START_OPS()
    Fam_DataItem_Metadata dataitem;
    ostringstream message;
    try {
        allocator->get_dataitem(request->name(), request->regionname(),
                                request->uid(), request->gid(), dataitem);
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    if (request->uid() == dataitem.uid) {
        response->set_regionid(dataitem.regionId);
        response->set_offset(dataitem.offset);
        response->set_size(dataitem.size);
        response->set_perm(dataitem.perm);
        response->set_name(dataitem.name);
    } else if (allocator->check_dataitem_permission(dataitem, 0, request->uid(),
                                                    request->gid())) {
        response->set_regionid(dataitem.regionId);
        response->set_offset(dataitem.offset);
        response->set_size(dataitem.size);
        response->set_perm(dataitem.perm);
        response->set_name(dataitem.name);
    } else {
        response->set_errorcode(FAM_ERR_NOPERM);
        message << "Error while looking up for region : ";
        message << "Region access in not permitted";
        response->set_errormsg(message.str());
        return ::grpc::Status::OK;
    }
    // Return status OK
    RPC_SERVICE_PROFILE_END_OPS(lookup);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Rpc_Service_Impl::check_permission_get_region_info(
    ::grpc::ServerContext *context, const ::Fam_Region_Request *request,
    ::Fam_Region_Response *response) {

    RPC_SERVICE_PROFILE_START_OPS()
    Fam_Region_Metadata region;
    ostringstream message;
    try {
        allocator->get_region(request->regionid(), request->uid(),
                              request->gid(), region);
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    if (allocator->check_region_permission(region, 0, request->uid(),
                                           request->gid())) {
        response->set_size(region.size);
        response->set_perm(region.perm);
        response->set_name(region.name);
    } else {
        message << "Error while checking permission and getting info for "
                   "region : ";
        message << "No permission to access region";
        response->set_errorcode(FAM_ERR_NOPERM);
        response->set_errormsg(message.str());
    }
    RPC_SERVICE_PROFILE_END_OPS(check_permission_get_region_info);
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Rpc_Service_Impl::check_permission_get_item_info(
    ::grpc::ServerContext *context, const ::Fam_Dataitem_Request *request,
    ::Fam_Dataitem_Response *response) {

    RPC_SERVICE_PROFILE_START_OPS()
    Fam_DataItem_Metadata dataitem;
    uint64_t key;
    ostringstream message;
    try {
        allocator->get_dataitem(request->regionid(), request->offset(),
                                request->uid(), request->gid(), dataitem);
        response->set_size(dataitem.size);
        response->set_perm(dataitem.perm);
        response->set_name(dataitem.name);
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    void *localPointer = 0;
    int ret;
    try {
        ret = register_memory(dataitem, localPointer, request->uid(),
                              request->gid(), key);
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    if (ret < 0) {
        if (ret == NOT_PERMITTED) {
            response->set_errorcode(FAM_ERR_NOPERM);
            message << "Error while checking permission and getting info for "
                       "dataitem : ";
            message << "No permission, dataitem registration failed";
            response->set_errormsg(message.str());
            return ::grpc::Status::OK;
        } else {
            response->set_errorcode(FAM_ERR_RESOURCE);
            message << "Error while checking permission and getting info for "
                       "dataitem";
            message << "dataitem registration failed";
            response->set_errormsg(message.str());
            return ::grpc::Status::OK;
        }
    }
    response->set_regionid(dataitem.regionId);
    response->set_offset(dataitem.offset);
    response->set_key(key);
    if (strncmp(famOps->get_provider(), "verbs", 5) == 0)
        response->set_base((uint64_t)localPointer);
    else
        response->set_base((uint64_t)0);
    response->set_size(dataitem.size);
    response->set_perm(dataitem.perm);
    response->set_name(dataitem.name);

    RPC_SERVICE_PROFILE_END_OPS(check_permission_get_item_info);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::get_stat_info(::grpc::ServerContext *context,
                                    const ::Fam_Dataitem_Request *request,
                                    ::Fam_Dataitem_Response *response) {

    RPC_SERVICE_PROFILE_START_OPS()
    Fam_DataItem_Metadata dataitem;
    ostringstream message;
    try {
        allocator->get_dataitem(request->regionid(), request->offset(),
                                request->uid(), request->gid(), dataitem);
        response->set_size(dataitem.size);
        response->set_perm(dataitem.perm);
        response->set_name(dataitem.name);
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    RPC_SERVICE_PROFILE_END_OPS(get_stat_info);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Rpc_Service_Impl::copy(::grpc::ServerContext *context,
                                          const ::Fam_Copy_Request *request,
                                          ::Fam_Copy_Response *response) {
    return ::grpc::Status::OK;
}

uint64_t Fam_Rpc_Service_Impl::generate_access_key(uint64_t regionId,
                                                   uint64_t dataitemId,
                                                   bool permission) {
    uint64_t key = 0;

    key |= (regionId & REGIONID_MASK) << REGIONID_SHIFT;
    key |= (dataitemId & DATAITEMID_MASK) << DATAITEMID_SHIFT;
    key |= permission;

    return key;
}

int Fam_Rpc_Service_Impl::register_fence_memory() {

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
            cout << "error: memory register failed" << endl;
            return ITEM_REGISTRATION_FAILED;
        }
        fenceMr = mr;
    }
    // Return status OK
    return 0;
}

int Fam_Rpc_Service_Impl::register_memory(Fam_DataItem_Metadata dataitem,
                                          void *&localPointer, uint32_t uid,
                                          uint32_t gid, uint64_t &key) {
    uint64_t dataitemId = dataitem.offset / MIN_OBJ_SIZE;
    if (fiMrs == NULL)
        fiMrs = famOps->get_fiMrs();
    fid_mr *mr = 0;
    int ret = 0;
    uint64_t mrkey = 0;
    bool rwflag;
    Fam_Region_Map_t *fiRegionMap = NULL;
    Fam_Region_Map_t *fiRegionMapDiscard = NULL;

    if (!localPointer) {
        localPointer =
            allocator->get_local_pointer(dataitem.regionId, dataitem.offset);
    }

    if (allocator->check_dataitem_permission(dataitem, 1, uid, gid)) {
        key = mrkey = generate_access_key(dataitem.regionId, dataitemId, 1);
        rwflag = 1;
    } else if (allocator->check_dataitem_permission(dataitem, 0, uid, gid)) {
        key = mrkey = generate_access_key(dataitem.regionId, dataitemId, 0);
        rwflag = 0;
    } else {
        cout << "error: Not permitted to register dataitem" << endl;
        return NOT_PERMITTED;
    }

    // register the data item with required permission with libfabric
    // Start by taking a readlock on fiMrs
    pthread_rwlock_rdlock(famOps->get_mr_lock());
    auto regionMrObj = fiMrs->find(dataitem.regionId);
    if (regionMrObj == fiMrs->end()) {
        // Create a RegionMap
        fiRegionMap = (Fam_Region_Map_t *)calloc(1, sizeof(Fam_Region_Map_t));
        fiRegionMap->regionId = dataitem.regionId;
        fiRegionMap->fiRegionMrs = new std::map<uint64_t, fid_mr *>();
        pthread_rwlock_init(&fiRegionMap->fiRegionLock, NULL);
        // RegionMap not found, release read lock, take a write lock
        pthread_rwlock_unlock(famOps->get_mr_lock());
        pthread_rwlock_wrlock(famOps->get_mr_lock());
        // Check again if regionMap added by another thread.
        regionMrObj = fiMrs->find(dataitem.regionId);
        if (regionMrObj == fiMrs->end()) {
            // Add the fam region map into fiMrs
            fiMrs->insert({dataitem.regionId, fiRegionMap});
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
        ret = fabric_register_mr(localPointer, dataitem.size, &mrkey,
                                 famOps->get_domain(), rwflag, mr);
        if (ret < 0) {
            pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
            cout << "error: memory register failed" << endl;
            return ITEM_REGISTRATION_FAILED;
        }

        fiRegionMap->fiRegionMrs->insert({key, mr});
    } else {
        mrkey = fi_mr_key(mrObj->second);
    }
    // Always return mrkey, which might be different than key.
    key = mrkey;

    pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
    // Return status OK
    return 0;
}

int Fam_Rpc_Service_Impl::deregister_fence_memory() {

    int ret = 0;
    if (fenceMr != 0) {
        ret = fabric_deregister_mr(fenceMr);
        if (ret < 0) {
            cout << "error: memory deregister failed" << endl;
            return ITEM_DEREGISTRATION_FAILED;
        }
    }
    fenceMr = 0;
    return 0;
}

int Fam_Rpc_Service_Impl::deregister_memory(uint64_t regionId,
                                            uint64_t offset) {
    RPC_SERVICE_PROFILE_START_OPS()

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
        return 0;
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
            cout << "error: memory deregister failed" << endl;
            return ITEM_DEREGISTRATION_FAILED;
        }
        fiRegionMap->fiRegionMrs->erase(rMr);
    }

    if (rwMr != fiRegionMap->fiRegionMrs->end()) {
        ret = fabric_deregister_mr(rwMr->second);
        if (ret < 0) {
            pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
            cout << "error: memory deregister failed" << endl;
            return ITEM_DEREGISTRATION_FAILED;
        }
        fiRegionMap->fiRegionMrs->erase(rwMr);
    }

    pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
    RPC_SERVICE_PROFILE_END_OPS(deregister_memory);
    return 0;
}

int Fam_Rpc_Service_Impl::deregister_region_memory(uint64_t regionId) {
    int ret = 0;
    Fam_Region_Map_t *fiRegionMap;

    if (fiMrs == NULL)
        fiMrs = famOps->get_fiMrs();

    // Take write lock on fiMrs
    pthread_rwlock_wrlock(famOps->get_mr_lock());

    auto regionMrObj = fiMrs->find(regionId);
    if (regionMrObj == fiMrs->end()) {
        pthread_rwlock_unlock(famOps->get_mr_lock());
        return 0;
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

    return 0;
}

::grpc::Status
Fam_Rpc_Service_Impl::acquire_CAS_lock(::grpc::ServerContext *context,
                                       const ::Fam_Dataitem_Request *request,
                                       ::Fam_Dataitem_Response *response) {
    int idx = LOCKHASH(request->offset());
    pthread_mutex_lock(&casLock[idx]);

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::release_CAS_lock(::grpc::ServerContext *context,
                                       const ::Fam_Dataitem_Request *request,
                                       ::Fam_Dataitem_Response *response) {
    int idx = LOCKHASH(request->offset());
    pthread_mutex_unlock(&casLock[idx]);

    // Return status OK
    return ::grpc::Status::OK;
}

} // namespace openfam
