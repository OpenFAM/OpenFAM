/*
 * fam_rpc_service_impl.h
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

#ifndef FAM_RPC_SERVICE_H
#define FAM_RPC_SERVICE_H

#include <iostream>
#include <map>
#include <thread>
#include <unistd.h>

#include "grpcpp/grpcpp.h"

#include "allocator/memserver_allocator.h"
#include "allocator/rbtree.h"
#include "metadata/fam_metadata_manager.h"
#include "rpc/fam_rpc.grpc.pb.h"

#include "common/fam_internal.h"
#include "common/fam_libfabric.h"
#include "common/fam_ops_libfabric.h"
#include "common/fam_options.h"
#include "common/memserver_exception.h"

#include <boost/atomic.hpp>
#include <nvmm/heap.h>

namespace openfam {
#define NOT_PERMITTED -3
#define ITEM_REGISTRATION_FAILED -4
#define ITEM_DEREGISTRATION_FAILED -5

#define CAS_LOCK_CNT 128
#define LOCKHASH(offset) (offset >> 7) % CAS_LOCK_CNT

using namespace std;
using namespace nvmm;
using namespace metadata;

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

#define FAM_UNIMPLEMENTED_RPC_SERVICE()                                        \
    {                                                                          \
        cout << "Reached server..." << __func__                                \
             << " is Not Yet Implemented...!!!" << endl;                       \
    }

class Fam_Rpc_Service_Impl : public Fam_Rpc::Service {
  public:
    Fam_Rpc_Service_Impl() {}

    ~Fam_Rpc_Service_Impl();

    void rpc_service_initialize(char *name, char *service, char *provider,
                                Memserver_Allocator *memAlloc);

    void rpc_service_finalize();

    ::grpc::Status signal_start(::grpc::ServerContext *context,
                                const ::Fam_Request *request,
                                ::Fam_Start_Response *response) override;

    ::grpc::Status signal_termination(::grpc::ServerContext *context,
                                      const ::Fam_Request *request,
                                      ::Fam_Response *response) override;

    ::grpc::Status create_region(::grpc::ServerContext *context,
                                 const ::Fam_Region_Request *request,
                                 ::Fam_Region_Response *response) override;

    ::grpc::Status destroy_region(::grpc::ServerContext *context,
                                  const ::Fam_Region_Request *request,
                                  ::Fam_Region_Response *response) override;

    ::grpc::Status resize_region(::grpc::ServerContext *context,
                                 const ::Fam_Region_Request *request,
                                 ::Fam_Region_Response *response) override;

    ::grpc::Status allocate(::grpc::ServerContext *context,
                            const ::Fam_Dataitem_Request *request,
                            ::Fam_Dataitem_Response *response) override;

    ::grpc::Status deallocate(::grpc::ServerContext *context,
                              const ::Fam_Dataitem_Request *request,
                              ::Fam_Dataitem_Response *response) override;

    ::grpc::Status
    change_region_permission(::grpc::ServerContext *context,
                             const ::Fam_Region_Request *request,
                             ::Fam_Region_Response *response) override;
    ::grpc::Status
    change_dataitem_permission(::grpc::ServerContext *context,
                               const ::Fam_Dataitem_Request *request,
                               ::Fam_Dataitem_Response *response) override;

    ::grpc::Status lookup_region(::grpc::ServerContext *context,
                                 const ::Fam_Region_Request *request,
                                 ::Fam_Region_Response *response) override;

    ::grpc::Status lookup(::grpc::ServerContext *context,
                          const ::Fam_Dataitem_Request *request,
                          ::Fam_Dataitem_Response *response) override;

    ::grpc::Status
    check_permission_get_region_info(::grpc::ServerContext *context,
                                     const ::Fam_Region_Request *request,
                                     ::Fam_Region_Response *response) override;

    ::grpc::Status
    check_permission_get_item_info(::grpc::ServerContext *context,
                                   const ::Fam_Dataitem_Request *request,
                                   ::Fam_Dataitem_Response *response) override;

    ::grpc::Status copy(::grpc::ServerContext *context,
                        const ::Fam_Copy_Request *request,
                        ::Fam_Copy_Response *response) override;

    ::grpc::Status acquire_CAS_lock(::grpc::ServerContext *context,
                                    const ::Fam_Dataitem_Request *request,
                                    ::Fam_Dataitem_Response *response) override;

    ::grpc::Status release_CAS_lock(::grpc::ServerContext *context,
                                    const ::Fam_Dataitem_Request *request,
                                    ::Fam_Dataitem_Response *response) override;

  protected:
    uint64_t port;
    Memserver_Allocator *allocator;
    Fam_Ops_Libfabric *famOps;
    int libfabricProgressMode;
    std::thread progressThread;
    boost::atomic<bool> haltProgress;
    void progress_thread();

    int numClients;
    bool shouldShutdown;
    pthread_mutex_t casLock[CAS_LOCK_CNT];

    std::map<uint64_t, fid_mr *> *fiMrs;

    uint64_t generate_access_key(uint64_t regionId, uint64_t dataitemId,
                                 bool permission);

    int deregister_memory(uint64_t regionId, uint64_t offset);

    int register_memory(Fam_DataItem_Metadata dataitem, void *&localPointer,
                        uint32_t uid, uint32_t gid, uint64_t &key);

    int register_fence_memory();

    int deregister_fence_memory();
};

} // namespace openfam
#endif
