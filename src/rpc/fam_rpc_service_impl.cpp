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
#include <thread>
#include <unistd.h>

namespace openfam {

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
    delete allocator;
    delete famOps;
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
Fam_Rpc_Service_Impl::create_region(::grpc::ServerContext *context,
                                    const ::Fam_Region_Request *request,
                                    ::Fam_Region_Response *response) {
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

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::destroy_region(::grpc::ServerContext *context,
                                     const ::Fam_Region_Request *request,
                                     ::Fam_Region_Response *response) {
    try {
        allocator->destroy_region(request->regionid(), request->uid(),
                                  request->gid());
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::resize_region(::grpc::ServerContext *context,
                                    const ::Fam_Region_Request *request,
                                    ::Fam_Region_Response *response) {

    try {
        allocator->resize_region(request->regionid(), request->uid(),
                                 request->gid(), request->size());
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::allocate(::grpc::ServerContext *context,
                               const ::Fam_Dataitem_Request *request,
                               ::Fam_Dataitem_Response *response) {
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

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::deallocate(::grpc::ServerContext *context,
                                 const ::Fam_Dataitem_Request *request,
                                 ::Fam_Dataitem_Response *response) {
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

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Rpc_Service_Impl::change_region_permission(
    ::grpc::ServerContext *context, const ::Fam_Region_Request *request,
    ::Fam_Region_Response *response) {
    try {
        allocator->change_region_permission(request->regionid(),
                                            (mode_t)request->perm(),
                                            request->uid(), request->gid());
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Rpc_Service_Impl::change_dataitem_permission(
    ::grpc::ServerContext *context, const ::Fam_Dataitem_Request *request,
    ::Fam_Dataitem_Response *response) {
    try {
        allocator->change_dataitem_permission(
            request->regionid(), request->offset(), (mode_t)request->perm(),
            request->uid(), request->gid());
    } catch (Memserver_Exception &e) {
        response->set_errorcode(e.fam_error());
        response->set_errormsg(e.fam_error_msg());
        return ::grpc::Status::OK;
    }

    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status
Fam_Rpc_Service_Impl::lookup_region(::grpc::ServerContext *context,
                                    const ::Fam_Region_Request *request,
                                    ::Fam_Region_Response *response) {
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
        return ::grpc::Status::OK;
    } else if (allocator->check_region_permission(region, 0, request->uid(),
                                                  request->gid())) {
        response->set_regionid(region.regionId);
        response->set_offset(region.offset);
        response->set_size(region.size);
        return ::grpc::Status::OK;
    } else {
        response->set_errorcode(FAM_ERR_NOPERM);
        message << "Error while looking up for region : ";
        message << "Region access in not permitted";
        response->set_errormsg(message.str());
        return ::grpc::Status::OK;
    }
}

::grpc::Status
Fam_Rpc_Service_Impl::lookup(::grpc::ServerContext *context,
                             const ::Fam_Dataitem_Request *request,
                             ::Fam_Dataitem_Response *response) {
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
        return ::grpc::Status::OK;
    } else if (allocator->check_dataitem_permission(dataitem, 0, request->uid(),
                                                    request->gid())) {
        response->set_regionid(dataitem.regionId);
        response->set_offset(dataitem.offset);
        response->set_size(dataitem.size);
        return ::grpc::Status::OK;
    } else {
        response->set_errorcode(FAM_ERR_NOPERM);
        message << "Error while looking up for region : ";
        message << "Region access in not permitted";
        response->set_errormsg(message.str());
        return ::grpc::Status::OK;
    }
    /*
void *localPointer = 0;
int ret;
try {
    ret = register_memory(dataitem, localPointer, request->uid(),
                          request->gid(), key);
}
catch (Memserver_Exception &e) {
    response->set_errorcode(e.fam_error());
    response->set_errormsg(e.fam_error_msg());
    return ::grpc::Status::OK;
}

if (ret < 0) {
    if (ret == NOT_PERMITTED) {
        response->set_errorcode(FAM_ERR_NOPERM);
        message << "Error while looking up for dataitem : ";
        message << "No permission, dataitem registration failed";
        response->set_errormsg(message.str());
        return ::grpc::Status::OK;
    } else {
        response->set_errorcode(FAM_ERR_RESOURCE);
        message << "Error while looking up for dataitem : ";
        message << "dataitem registration failed";
        response->set_errormsg(message.str());
        return ::grpc::Status::OK;
    }
}
response->set_regionid(dataitem.regionId);
response->set_offset(dataitem.offset);
response->set_size(dataitem.size);
response->set_key(key);
    */
    // Return status OK
    return ::grpc::Status::OK;
}

::grpc::Status Fam_Rpc_Service_Impl::check_permission_get_region_info(
    ::grpc::ServerContext *context, const ::Fam_Region_Request *request,
    ::Fam_Region_Response *response) {

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
    } else {
        message << "Error while checking permission and getting info for "
                   "region : ";
        message << "No permission to access region";
        response->set_errorcode(FAM_ERR_NOPERM);
        response->set_errormsg(message.str());
    }

    return ::grpc::Status::OK;
}

::grpc::Status Fam_Rpc_Service_Impl::check_permission_get_item_info(
    ::grpc::ServerContext *context, const ::Fam_Dataitem_Request *request,
    ::Fam_Dataitem_Response *response) {

    Fam_DataItem_Metadata dataitem;
    uint64_t key;
    ostringstream message;
    try {
        allocator->get_dataitem(request->regionid(), request->offset(),
                                request->uid(), request->gid(), dataitem);
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
    response->set_size(dataitem.size);

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
    fiMrs = famOps->get_fiMrs();
    fid_mr *mr = 0;
    uint64_t key = FAM_FENCE_KEY;
    void *localPointer;
    size_t len = (size_t)sysconf(_SC_PAGESIZE);
    int fd = -1;

    localPointer = mmap(NULL, len, PROT_WRITE, MAP_SHARED | MAP_ANON, fd, 0);
    if (localPointer == MAP_FAILED) {
        cout << "mmap of fence memory failed" << endl;
    }

    // register the memory location with libfabric
    pthread_mutex_lock(famOps->get_mr_lock());
    auto mrObj = fiMrs->find(key);
    if (mrObj == fiMrs->end()) {
        ret = fabric_register_mr(localPointer, len, &key, famOps->get_domain(),
                                 1, mr);
        if (ret < 0) {
            pthread_mutex_unlock(famOps->get_mr_lock());
            cout << "error: memory register failed" << endl;
            return ITEM_REGISTRATION_FAILED;
        }

        fiMrs->insert({key, mr});
    }
    pthread_mutex_unlock(famOps->get_mr_lock());
    // Return status OK
    return 0;
}

int Fam_Rpc_Service_Impl::register_memory(Fam_DataItem_Metadata dataitem,
                                          void *localPointer, uint32_t uid,
                                          uint32_t gid, uint64_t &key) {
    uint64_t dataitemId = dataitem.offset / MIN_OBJ_SIZE;
    fiMrs = famOps->get_fiMrs();
    fid_mr *mr = 0;
    int ret = 0;
    if (!localPointer) {
        localPointer =
            allocator->get_local_pointer(dataitem.regionId, dataitem.offset);
    }

    if (allocator->check_dataitem_permission(dataitem, 1, uid, gid)) {
        key = generate_access_key(dataitem.regionId, dataitemId, 1);
        // register the data item with required permission with libfabric
        pthread_mutex_lock(famOps->get_mr_lock());
        auto mrObj = fiMrs->find(key);
        if (mrObj == fiMrs->end()) {
            ret = fabric_register_mr(localPointer, dataitem.size, &key,
                                     famOps->get_domain(), 1, mr);
            if (ret < 0) {
                pthread_mutex_unlock(famOps->get_mr_lock());
                cout << "error: memory register failed" << endl;
                return ITEM_REGISTRATION_FAILED;
            }

            fiMrs->insert({key, mr});
        }
        pthread_mutex_unlock(famOps->get_mr_lock());
        // Return status OK
        return 0;
    } else if (allocator->check_dataitem_permission(dataitem, 0, uid, gid)) {
        key = generate_access_key(dataitem.regionId, dataitemId, 0);
        // register the data item with required permission with libfabric
        pthread_mutex_lock(famOps->get_mr_lock());
        auto mrObj = fiMrs->find(key);
        if (mrObj == fiMrs->end()) {
            ret = fabric_register_mr(localPointer, dataitem.size, &key,
                                     famOps->get_domain(), 0, mr);
            if (ret < 0) {
                pthread_mutex_unlock(famOps->get_mr_lock());
                cout << "error: memory register failed" << endl;
                return ITEM_REGISTRATION_FAILED;
            }

            fiMrs->insert({key, mr});
        }
        pthread_mutex_unlock(famOps->get_mr_lock());
        // Return status OK
        return 0;
    } else {
        cout << "error: Not permitted to register dataitem" << endl;
        return NOT_PERMITTED;
    }
}

int Fam_Rpc_Service_Impl::deregister_fence_memory() {

    int ret = 0;
    uint64_t key = FAM_FENCE_KEY;
    pthread_mutex_lock(famOps->get_mr_lock());
    auto mr = fiMrs->find(key);
    if (mr != fiMrs->end()) {
        ret = fabric_deregister_mr(mr->second);
        if (ret < 0) {
            pthread_mutex_unlock(famOps->get_mr_lock());
            cout << "error: memory deregister failed" << endl;
            return ITEM_DEREGISTRATION_FAILED;
        }
        fiMrs->erase(mr);
    }

    pthread_mutex_unlock(famOps->get_mr_lock());
    return 0;
}

int Fam_Rpc_Service_Impl::deregister_memory(uint64_t regionId,
                                            uint64_t offset) {
    int ret = 0;
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    uint64_t rKey = generate_access_key(regionId, dataitemId, 0);
    uint64_t rwKey = generate_access_key(regionId, dataitemId, 1);

    pthread_mutex_lock(famOps->get_mr_lock());
    auto rMr = fiMrs->find(rKey);
    auto rwMr = fiMrs->find(rwKey);
    if (rMr != fiMrs->end()) {
        ret = fabric_deregister_mr(rMr->second);
        if (ret < 0) {
            pthread_mutex_unlock(famOps->get_mr_lock());
            cout << "error: memory deregister failed" << endl;
            return ITEM_DEREGISTRATION_FAILED;
        }
        fiMrs->erase(rMr);
    }

    if (rwMr != fiMrs->end()) {
        ret = fabric_deregister_mr(rwMr->second);
        if (ret < 0) {
            pthread_mutex_unlock(famOps->get_mr_lock());
            cout << "error: memory deregister failed" << endl;
            return ITEM_DEREGISTRATION_FAILED;
        }
        fiMrs->erase(rwMr);
    }

    pthread_mutex_unlock(famOps->get_mr_lock());
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
