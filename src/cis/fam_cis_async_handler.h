/*
 * fam_rpc_server.h
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
#ifndef FAM_CIS_ASYNC_HANDLER_H
#define FAM_CIS_ASYNC_HANDLER_H

#include "cis/fam_cis_server.h"
#include "common/fam_memserver_profile.h"

#include <boost/atomic.hpp>
#include <chrono>
#include <iomanip>
#include <string.h>
#include <unistd.h>

#define ADDR_SIZE 20

using namespace std;
using namespace chrono;

namespace openfam {
MEMSERVER_PROFILE_START(CIS_ASYNC)

#ifdef MEMSERVER_PROFILE
#define CIS_ASYNC_PROFILE_START_OPS()                                          \
    {                                                                          \
        Profile_Time start = CIS_ASYNC_get_time();

#define CIS_ASYNC_PROFILE_END_OPS(apiIdx)                                      \
    Profile_Time end = CIS_ASYNC_get_time();                                   \
    Profile_Time total = CIS_ASYNC_time_diff_nanoseconds(start, end);          \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(CIS_ASYNC, prof_##apiIdx, total)        \
    }
#define CIS_ASYNC_PROFILE_DUMP() cis_async_profile_end()
#else
#define CIS_ASYNC_PROFILE_START_OPS()
#define CIS_ASYNC_PROFILE_END_OPS(apiIdx)
#define CIS_ASYNC_PROFILE_DUMP()
#endif

void cis_async_profile_end() {
    MEMSERVER_PROFILE_END(CIS_ASYNC)
    MEMSERVER_DUMP_PROFILE_BANNER(CIS_ASYNC)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(CIS_ASYNC, name, prof_##name)
#include "cis/cis_async_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name) MEMSERVER_PROFILE_TOTAL(CIS_ASYNC, prof_##name)
#include "cis/cis_async_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(CIS_ASYNC)
}

typedef Fam_CIS_Rpc::WithAsyncMethod_restore<
    Fam_CIS_Rpc::WithAsyncMethod_backup<
        Fam_CIS_Rpc::WithAsyncMethod_copy<Fam_CIS_Server> > > sType;
enum RequestType {
    RT_COPY,
    RT_BACKUP,
    RT_RESTORE
};

class Fam_CIS_Async_Handler {
  public:
    Fam_CIS_Async_Handler(uint64_t rpcPort, char *name)
        : serverAddress(name), port(rpcPort) {
        famCIS = new Fam_CIS_Direct(name);
        service = new sType();
        MEMSERVER_PROFILE_INIT(CIS_ASYNC);
        MEMSERVER_PROFILE_START_TIME(CIS_ASYNC);
        service->cis_server_initialize(famCIS);
    }

    ~Fam_CIS_Async_Handler() { delete service; }

    void dump_profile() {
        CIS_ASYNC_PROFILE_DUMP();
    }

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
        bcq = builder.AddCompletionQueue();
        rcq = builder.AddCompletionQueue();

        // Finally assemble the server.
        server = builder.BuildAndStart();
#if defined(FAM_DEBUG)
        cout << "Server listening on " << serverAddress << endl;
#endif

        // Spawn a seperate thread to handle asynchronous service
        std::thread async_service_handler(
            &Fam_CIS_Async_Handler::HandleRpcs<sType>, this, service, cq.get(),
            famCIS, RT_COPY);

        std::thread async_service_handler_backup(
            &Fam_CIS_Async_Handler::HandleRpcs<sType>, this, service, bcq.get(),
            famCIS, RT_BACKUP);
        std::thread async_service_handler_restore(
            &Fam_CIS_Async_Handler::HandleRpcs<sType>, this, service, rcq.get(),
            famCIS, RT_RESTORE);

        server->Wait();
        async_service_handler.join();
        async_service_handler_backup.join();
        async_service_handler_restore.join();
    }

  private:
    Fam_CIS_Direct *famCIS;
    class CallData {
      public:
        // Take in the "service" instance (in this case representing an
        // asynchronous server) and the completion queue "cq" used for
        // asynchronous communication with the gRPC runtime.
        CallData(sType *service, ServerCompletionQueue *cq,
                 Fam_CIS_Direct *__famCIS, RequestType type)
            : ret(0), service(service), cq(cq), responder(&ctx),
              bresponder(&ctx), rresponder(&ctx), status(CREATE) {

            famCIS = __famCIS;
            // Invoke the serving logic right away.
            Proceed(type);
        }

        void Proceed(RequestType type) {
            if (status == CREATE) {
                CIS_ASYNC_PROFILE_START_OPS()
                // Make this instance progress to the PROCESS state.
                status = PROCESS;
                // As part of the initial CREATE state, we *request* that the
                // system start processing copy requests. In this request,
                // "this" acts are the tag uniquely identifying the request (so
                // that different CallData instances can serve different
                // requests concurrently), in this case the memory address of
                // this CallData instance.
                switch (type) {
                case RT_COPY:
                    service->Requestcopy(&ctx, &request, &responder, cq, cq,
                                         this);
                    CIS_ASYNC_PROFILE_END_OPS(copy_create);
                    break;
                case RT_BACKUP:
                    service->Requestbackup(&ctx, &brequest, &bresponder, cq, cq,
                                           this);
                    CIS_ASYNC_PROFILE_END_OPS(backup_create);
                    break;
                case RT_RESTORE:
                    service->Requestrestore(&ctx, &rrequest, &rresponder, cq,
                                            cq, this);
                    CIS_ASYNC_PROFILE_END_OPS(restore_create);
                    break;
                }
            } else if (status == PROCESS) {
                CIS_ASYNC_PROFILE_START_OPS()
                // Spawn a new CallData instance to serve new clients while we
                // process the one for this CallData. The instance will
                // deallocate itself as part of its FINISH state.

                new CallData(service, cq, famCIS, type);
                // copy the data from source dataitem to target dataitem
                grpcStatus = Status::OK;
                try {
                    void *waitObj;
                    switch (type) {
                    case RT_COPY:
                    *waitObj = famCIS->copy(
                        request.srcregionid(), request.srcoffset(),
                        request.srccopystart(), request.srckey(),
                        request.srcbaseaddr(), request.srcaddr().c_str(),
                        request.srcaddrlen(), request.destregionid(),
                        request.destoffset(), request.destcopystart(),
                        request.copysize(), request.src_memserver_id(),
                        request.dest_memserver_id(), request.uid(),
                        request.gid());
                        delete (Fam_Copy_Tag *)waitObj;
                        break;
                    case RT_BACKUP:
                        waitObj = famCIS->backup(
                            brequest.regionid(), brequest.offset(),
                            brequest.memserver_id(),
                            brequest.filename().c_str(), brequest.uid(),
                            brequest.gid(), brequest.size());
                        delete (Fam_Backup_Tag *)waitObj;

                        break;
                    case RT_RESTORE:
                        waitObj = famCIS->restore(
                            rrequest.regionid(), rrequest.offset(),
                            rrequest.memserver_id(),
                            rrequest.filename().c_str(), rrequest.uid(),
                            rrequest.gid(), rrequest.size());
                        delete (Fam_Restore_Tag *)waitObj;
                        break;
                    }
                } catch (Memory_Service_Exception &e) {
                    switch (type) {
                    case RT_COPY:
                        response.set_errorcode(e.fam_error());
                        response.set_errormsg(e.fam_error_msg());
                        break;

                    case RT_BACKUP:
                        bresponse.set_errorcode(e.fam_error());
                        bresponse.set_errormsg(e.fam_error_msg());
                        break;
                    case RT_RESTORE:
                        rresponse.set_errorcode(e.fam_error());
                        rresponse.set_errormsg(e.fam_error_msg());
                        break;
                    }
                    grpcStatus = Status::OK;
                }

                // And we are done! Let the gRPC runtime know we've finished,
                // using the memory address of this instance as the uniquely
                // identifying tag for the event.
                status = FINISH;
                switch (type) {
                case RT_COPY:
                    responder.Finish(response, grpcStatus, this);
                    CIS_ASYNC_PROFILE_END_OPS(copy_process);
                    break;
                case RT_BACKUP:
                    bresponder.Finish(bresponse, grpcStatus, this);
                    CIS_ASYNC_PROFILE_END_OPS(backup_process);
                    break;
                case RT_RESTORE:
                    rresponder.Finish(rresponse, grpcStatus, this);
                    CIS_ASYNC_PROFILE_END_OPS(restore_process);
                    break;
                }

            } else {
                CIS_ASYNC_PROFILE_START_OPS()
                GPR_ASSERT(status == FINISH);
                // Once in the FINISH state, deallocate ourselves (CallData).
                delete this;
                switch (type) {
                case RT_COPY:
                    CIS_ASYNC_PROFILE_END_OPS(copy_finish);
                    break;
                case RT_BACKUP:
                    CIS_ASYNC_PROFILE_END_OPS(backup_finish);
                    break;
                case RT_RESTORE:
                    CIS_ASYNC_PROFILE_END_OPS(restore_finish);
                    break;
                }
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

        Fam_CIS_Direct *famCIS;
        // What we get from the client.
        Fam_Copy_Request request;

        // What we send back to the client.
        Fam_Copy_Response response;

        // What we get from the client.
        Fam_Backup_Restore_Request brequest;

        // What we send back to the client.
        Fam_Backup_Restore_Response bresponse;

        // What we get from the client.
        Fam_Backup_Restore_Request rrequest;

        // What we send back to the client.
        Fam_Backup_Restore_Response rresponse;

        Status grpcStatus;
        // The means to get back to the client.
        ServerAsyncResponseWriter<Fam_Copy_Response> responder;
        ServerAsyncResponseWriter<Fam_Backup_Restore_Response> bresponder;
        ServerAsyncResponseWriter<Fam_Backup_Restore_Response> rresponder;

        // Let's implement a tiny state machine with the following states.
        enum CallStatus { CREATE, PROCESS, FINISH };
        CallStatus status; // The current serving state.
    };

    // This can be run in multiple threads if needed.
    template <class Service>
    void HandleRpcs(Service *service, ServerCompletionQueue *cq,
                    Fam_CIS_Direct *famCIS, RequestType type) {
        // Spawn a new CallData instance to serve new clients.
        new CallData(service, cq, famCIS, type);
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
            static_cast<CallData *>(tag)->Proceed(type);
        }
    }
    char *serverAddress;
    uint64_t port;
    // Fam_Rpc_Service_Impl* sericeImpl;
    std::unique_ptr<ServerCompletionQueue> cq;
    std::unique_ptr<ServerCompletionQueue> bcq;
    std::unique_ptr<ServerCompletionQueue> rcq;
    sType *service;
    std::unique_ptr<Server> server;
};

} // namespace openfam
#endif
