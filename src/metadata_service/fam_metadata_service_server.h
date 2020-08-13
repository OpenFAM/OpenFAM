/*
 * fam_metadata_service_server.h
 * Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All rights
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

#ifndef FAM_METADATA_SERVICE_SERVER_H
#define FAM_METADATA_SERVICE_SERVER_H

#include <iostream>
#include <map>
#include <thread>
#include <unistd.h>

#include "grpcpp/grpcpp.h"

#include "fam_metadata_rpc.grpc.pb.h"
#include "fam_metadata_service_direct.h"

using namespace openfam;

namespace metadata {

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

class Fam_Metadata_Service_Server : public Fam_Metadata_Rpc::Service {
  public:
    Fam_Metadata_Service_Server(uint64_t rpcPort, char *name);

    ~Fam_Metadata_Service_Server();

    void run();

    ::grpc::Status signal_start(::grpc::ServerContext *context,
                                const ::Fam_Metadata_Gen_Request *request,
                                ::Fam_Metadata_Gen_Response *response) override;

    ::grpc::Status
    signal_termination(::grpc::ServerContext *context,
                       const ::Fam_Metadata_Gen_Request *request,
                       ::Fam_Metadata_Gen_Response *response) override;

    ::grpc::Status
    reset_profile(::grpc::ServerContext *context,
                  const ::Fam_Metadata_Gen_Request *request,
                  ::Fam_Metadata_Gen_Response *response) override;

    ::grpc::Status dump_profile(::grpc::ServerContext *context,
                                const ::Fam_Metadata_Gen_Request *request,
                                ::Fam_Metadata_Gen_Response *response) override;

    ::grpc::Status
    metadata_insert_region(::grpc::ServerContext *context,
                           const ::Fam_Metadata_Request *request,
                           ::Fam_Metadata_Response *response) override;

    ::grpc::Status
    metadata_delete_region(::grpc::ServerContext *context,
                           const ::Fam_Metadata_Request *request,
                           ::Fam_Metadata_Response *response) override;

    ::grpc::Status
    metadata_find_region(::grpc::ServerContext *context,
                         const ::Fam_Metadata_Request *request,
                         ::Fam_Metadata_Response *response) override;

    ::grpc::Status
    metadata_modify_region(::grpc::ServerContext *context,
                           const ::Fam_Metadata_Request *request,
                           ::Fam_Metadata_Response *response) override;

    ::grpc::Status
    metadata_insert_dataitem(::grpc::ServerContext *context,
                             const ::Fam_Metadata_Request *request,
                             ::Fam_Metadata_Response *response) override;

    ::grpc::Status
    metadata_delete_dataitem(::grpc::ServerContext *context,
                             const ::Fam_Metadata_Request *request,
                             ::Fam_Metadata_Response *response) override;

    ::grpc::Status
    metadata_find_dataitem(::grpc::ServerContext *context,
                           const ::Fam_Metadata_Request *request,
                           ::Fam_Metadata_Response *response) override;

    ::grpc::Status
    metadata_modify_dataitem(::grpc::ServerContext *context,
                             const ::Fam_Metadata_Request *request,
                             ::Fam_Metadata_Response *response) override;

    ::grpc::Status metadata_check_region_permissions(
        ::grpc::ServerContext *context, const ::Fam_Permission_Request *request,
        ::Fam_Permission_Response *response) override;

    ::grpc::Status metadata_check_item_permissions(
        ::grpc::ServerContext *context, const ::Fam_Permission_Request *request,
        ::Fam_Permission_Response *response) override;

    ::grpc::Status
    metadata_maxkeylen(::grpc::ServerContext *context,
                       const ::Fam_Metadata_Request *request,
                       ::Fam_Metadata_Response *response) override;

  private:
    char *serverAddress;
    uint64_t port;
    int numClients;
    Fam_Metadata_Rpc::Service *service;
    std::unique_ptr<Server> server;
    Fam_Metadata_Service_Direct *metadataService;
};

} // namespace metadata
#endif
