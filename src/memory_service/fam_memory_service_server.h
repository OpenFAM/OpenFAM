/*
 * fam_memory_service_server.h
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

#ifndef FAM_MEMORY_SERVICE_SERVER_H
#define FAM_MEMORY_SERVICE_SERVER_H

#include <iostream>
#include <map>
#include <thread>
#include <unistd.h>

#include "grpcpp/grpcpp.h"

#include "memory_service/fam_memory_service_direct.h"
#include "memory_service/fam_memory_service_rpc.grpc.pb.h"

namespace openfam {

using namespace std;
using namespace nvmm;
using namespace metadata;

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

class Fam_Memory_Service_Server : public Fam_Memory_Service_Rpc::Service {
  public:
    Fam_Memory_Service_Server(uint64_t memserver_id);

    ~Fam_Memory_Service_Server();

    void run();

    void reset_profile();

    void dump_profile();

    ::grpc::Status
    signal_start(::grpc::ServerContext *context,
                 const ::Fam_Memory_Service_General_Request *request,
                 ::Fam_Memory_Service_Start_Response *response) override;

    ::grpc::Status signal_termination(
        ::grpc::ServerContext *context,
        const ::Fam_Memory_Service_General_Request *request,
        ::Fam_Memory_Service_General_Response *response) override;

    ::grpc::Status
    reset_profile(::grpc::ServerContext *context,
                  const ::Fam_Memory_Service_General_Request *request,
                  ::Fam_Memory_Service_General_Response *response) override;

    ::grpc::Status
    dump_profile(::grpc::ServerContext *context,
                 const ::Fam_Memory_Service_General_Request *request,
                 ::Fam_Memory_Service_General_Response *response) override;
    ::grpc::Status
    create_region(::grpc::ServerContext *context,
                  const ::Fam_Memory_Service_Request *request,
                  ::Fam_Memory_Service_Response *response) override;

    ::grpc::Status
    destroy_region(::grpc::ServerContext *context,
                   const ::Fam_Memory_Service_Request *request,
                   ::Fam_Memory_Service_Response *response) override;

    ::grpc::Status
    resize_region(::grpc::ServerContext *context,
                  const ::Fam_Memory_Service_Request *request,
                  ::Fam_Memory_Service_Response *response) override;

    ::grpc::Status allocate(::grpc::ServerContext *context,
                            const ::Fam_Memory_Service_Request *request,
                            ::Fam_Memory_Service_Response *response) override;

    ::grpc::Status deallocate(::grpc::ServerContext *context,
                              const ::Fam_Memory_Service_Request *request,
                              ::Fam_Memory_Service_Response *response) override;

    ::grpc::Status copy(::grpc::ServerContext *context,
                        const ::Fam_Memory_Copy_Request *request,
                        ::Fam_Memory_Copy_Response *response) override;

    ::grpc::Status
    backup(::grpc::ServerContext *context,
           const ::Fam_Memory_Backup_Restore_Request *request,
           ::Fam_Memory_Backup_Restore_Response *response) override;

    ::grpc::Status
    restore(::grpc::ServerContext *context,
            const ::Fam_Memory_Backup_Restore_Request *request,
            ::Fam_Memory_Backup_Restore_Response *response) override;

    ::grpc::Status
    get_backup_info(::grpc::ServerContext *context,
                    const ::Fam_Memory_Backup_Info_Request *request,
                    ::Fam_Memory_Backup_Info_Response *response) override;
    ::grpc::Status
    list_backup(::grpc::ServerContext *context,
                const ::Fam_Memory_Backup_List_Request *request,
                ::Fam_Memory_Backup_List_Response *response) override;

    ::grpc::Status
    delete_backup(::grpc::ServerContext *context,
                  const ::Fam_Memory_Backup_List_Request *request,
                  ::Fam_Memory_Backup_List_Response *response) override;

    ::grpc::Status
    acquire_CAS_lock(::grpc::ServerContext *context,
                     const ::Fam_Memory_Service_Request *request,
                     ::Fam_Memory_Service_Response *response) override;

    ::grpc::Status
    release_CAS_lock(::grpc::ServerContext *context,
                     const ::Fam_Memory_Service_Request *request,
                     ::Fam_Memory_Service_Response *response) override;

    ::grpc::Status
    get_local_pointer(::grpc::ServerContext *context,
                      const ::Fam_Memory_Service_Request *request,
                      ::Fam_Memory_Service_Response *response) override;

    ::grpc::Status get_key(::grpc::ServerContext *context,
                           const ::Fam_Memory_Service_Request *request,
                           ::Fam_Memory_Service_Response *response) override;

    ::grpc::Status get_atomic(::grpc::ServerContext *context,
                              const ::Fam_Memory_Atomic_Get_Request *request,
                              ::Fam_Memory_Atomic_Response *response) override;

    ::grpc::Status put_atomic(::grpc::ServerContext *context,
                              const ::Fam_Memory_Atomic_Put_Request *request,
                              ::Fam_Memory_Atomic_Response *response) override;

    ::grpc::Status scatter_strided_atomic(
        ::grpc::ServerContext *context,
        const ::Fam_Memory_Atomic_SG_Strided_Request *request,
        ::Fam_Memory_Atomic_Response *response) override;

    ::grpc::Status
    gather_strided_atomic(::grpc::ServerContext *context,
                          const ::Fam_Memory_Atomic_SG_Strided_Request *request,
                          ::Fam_Memory_Atomic_Response *response) override;

    ::grpc::Status scatter_indexed_atomic(
        ::grpc::ServerContext *context,
        const ::Fam_Memory_Atomic_SG_Indexed_Request *request,
        ::Fam_Memory_Atomic_Response *response) override;

    ::grpc::Status
    gather_indexed_atomic(::grpc::ServerContext *context,
                          const ::Fam_Memory_Atomic_SG_Indexed_Request *request,
                          ::Fam_Memory_Atomic_Response *response) override;

  protected:
    int numClients;
    Fam_Memory_Service_Rpc::Service *service;
    std::unique_ptr<Server> server;
    Fam_Memory_Service_Direct *memoryService;
};

} // namespace openfam
#endif
