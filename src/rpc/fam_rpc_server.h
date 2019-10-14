/*
 * fam_rpc_server.h
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
#include <unistd.h>

#define ADDR_SIZE 20

using namespace std;

namespace openfam {

typedef Fam_Rpc::WithAsyncMethod_copy<Fam_Rpc_Service_Impl> sType;

class Fam_Rpc_Server {
  public:
    Fam_Rpc_Server(uint64_t rpcPort, char *name, char *libfabricPort,
                   char *provider)
        : serverAddress(name), port(rpcPort) {
        allocator = new Memserver_Allocator();
        service = new sType();
        service->rpc_service_initialize(name, libfabricPort, provider,
                                        allocator);
    }

    ~Fam_Rpc_Server() { delete service; }

    void rpc_server_finalize() { service->rpc_service_finalize(); }

    void run() {
        char address[ADDR_SIZE + sizeof(uint64_t)];
        sprintf(address, "%s:%lu", serverAddress, port);
        std::string serverAddress(address);

        ::grpc::ServerBuilder builder;
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(serverAddress,
                                 grpc::InsecureServerCredentials());
        // Register "service" as the instance through which we'll communicate
        // with clients. In this case it corresponds to an *synchronous*
        // service.
        builder.RegisterService(service);

        // Add completion queue
        cq = builder.AddCompletionQueue();

        // Finally assemble the server.
        server = builder.BuildAndStart();
#if defined(FAM_DEBUG)
        cout << "Server listening on " << serverAddress << endl;
#endif

        // Spawn a seperate thread to handle asynchronous service
        std::thread async_service_handler(&Fam_Rpc_Server::HandleRpcs<sType>,
                                          this, service, cq.get(), allocator);

        server->Wait();
        async_service_handler.join();
    }

  private:
    Memserver_Allocator *allocator;
    class CallData {
      public:
        // Take in the "service" instance (in this case representing an
        // asynchronous server) and the completion queue "cq" used for
        // asynchronous communication with the gRPC runtime.
        CallData(sType *service, ServerCompletionQueue *cq,
                 Memserver_Allocator *memAlloc)
            : ret(0), service(service), cq(cq), responder(&ctx),
              status(CREATE) {
            allocator = memAlloc;
            // Invoke the serving logic right away.
            Proceed();
        }

        void Proceed() {
            if (status == CREATE) {
                // Make this instance progress to the PROCESS state.
                status = PROCESS;
                // As part of the initial CREATE state, we *request* that the
                // system start processing copy requests. In this request,
                // "this" acts are the tag uniquely identifying the request (so
                // that different CallData instances can serve different
                // requests concurrently), in this case the memory address of
                // this CallData instance.
                service->Requestcopy(&ctx, &request, &responder, cq, cq, this);
            } else if (status == PROCESS) {
                // Spawn a new CallData instance to serve new clients while we
                // process the one for this CallData. The instance will
                // deallocate itself as part of its FINISH state.

                new CallData(service, cq, allocator);
                // copy the data from source dataitem to target dataitem
                grpcStatus = Status::OK;
                try {
                    ret = allocator->copy(
                        request.regionid(), request.srcoffset(),
                        request.srccopystart(), request.destoffset(),
                        request.destcopystart(), request.uid(), request.gid(),
                        (size_t)request.copysize());
                } catch (Memserver_Exception &e) {
                    response.set_errorcode(e.fam_error());
                    response.set_errormsg(e.fam_error_msg());
                    grpcStatus = Status::OK;
                }

                // And we are done! Let the gRPC runtime know we've finished,
                // using the memory address of this instance as the uniquely
                // identifying tag for the event.
                status = FINISH;
                responder.Finish(response, grpcStatus, this);
            } else {
                GPR_ASSERT(status == FINISH);
                // Once in the FINISH state, deallocate ourselves (CallData).
                delete this;
            }
        }

      private:
        int ret;
        ostringstream errString;
        // The means of communication with the gRPC runtime for an asynchronous
        // server.
        sType *service;
        // The producer-consumer queue where for asynchronous server
        // notifications.
        ServerCompletionQueue *cq;
        // Context for the rpc, allowing to tweak aspects of it such as the use
        // of compression, authentication, as well as to send metadata back to
        // the client.
        ServerContext ctx;

        Memserver_Allocator *allocator;
        // What we get from the client.
        Fam_Copy_Request request;
        // What we send back to the client.
        Fam_Copy_Response response;

        Status grpcStatus;
        // The means to get back to the client.
        ServerAsyncResponseWriter<Fam_Copy_Response> responder;

        // Let's implement a tiny state machine with the following states.
        enum CallStatus { CREATE, PROCESS, FINISH };
        CallStatus status; // The current serving state.
    };

    // This can be run in multiple threads if needed.
    template <class Service>
    void HandleRpcs(Service *service, ServerCompletionQueue *cq,
                    Memserver_Allocator *allocator) {
        // Spawn a new CallData instance to serve new clients.
        new CallData(service, cq, allocator);
        void *tag; // uniquely identifies a request.
        bool ok;
        while (true) {
            // Block waiting to read the next event from the completion queue.
            // The event is uniquely identified by its tag, which in this case
            // is the memory address of a CallData instance. The return value of
            // Next should always be checked. This return value tells us whether
            // there is any kind of event or cq is shutting down.
            GPR_ASSERT(cq->Next(&tag, &ok));
            GPR_ASSERT(ok);
            static_cast<CallData *>(tag)->Proceed();
        }
    }
    char *serverAddress;
    uint64_t port;
    // Fam_Rpc_Service_Impl* sericeImpl;
    std::unique_ptr<ServerCompletionQueue> cq;
    sType *service;
    std::unique_ptr<Server> server;
};

} // namespace openfam
