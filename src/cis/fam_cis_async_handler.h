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

typedef Fam_CIS_Rpc::WithAsyncMethod_delete_backup<
    Fam_CIS_Rpc::WithAsyncMethod_restore<Fam_CIS_Rpc::WithAsyncMethod_backup<
        Fam_CIS_Rpc::WithAsyncMethod_copy<Fam_CIS_Server>>>>
    sType;
enum RequestType { RT_COPY, RT_BACKUP, RT_RESTORE, RT_DELETE_BACKUP };

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
        copycq = builder.AddCompletionQueue();
        backupcq = builder.AddCompletionQueue();
        restorecq = builder.AddCompletionQueue();
        delbackupcq = builder.AddCompletionQueue();
        // Finally assemble the server.
        server = builder.BuildAndStart();
#if defined(FAM_DEBUG)
        cout << "Server listening on " << serverAddress << endl;
#endif

        // Spawn a seperate thread to handle asynchronous service
        std::thread async_service_handler(
            &Fam_CIS_Async_Handler::HandleRpcs<sType>, this, service,
            copycq.get(), famCIS, RT_COPY);

        std::thread async_service_handler_backup(
            &Fam_CIS_Async_Handler::HandleRpcs<sType>, this, service,
            backupcq.get(), famCIS, RT_BACKUP);
        std::thread async_service_handler_restore(
            &Fam_CIS_Async_Handler::HandleRpcs<sType>, this, service,
            restorecq.get(), famCIS, RT_RESTORE);
        std::thread async_service_handler_delete_backup(
            &Fam_CIS_Async_Handler::HandleRpcs<sType>, this, service,
            delbackupcq.get(), famCIS, RT_DELETE_BACKUP);

        server->Wait();
        async_service_handler.join();
        async_service_handler_backup.join();
        async_service_handler_restore.join();
        async_service_handler_delete_backup.join();
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
            : ret(0), service(service), cq(cq), copyresponder(&ctx),
              backupresponder(&ctx), restoreresponder(&ctx),
              delbackupresponder(&ctx), status(CREATE) {

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
                    service->Requestcopy(&ctx, &copyrequest, &copyresponder, cq,
                                         cq, this);
                    CIS_ASYNC_PROFILE_END_OPS(copy_create);
                    break;
                case RT_BACKUP:
                    service->Requestbackup(&ctx, &backuprequest,
                                           &backupresponder, cq, cq, this);
                    CIS_ASYNC_PROFILE_END_OPS(backup_create);
                    break;
                case RT_RESTORE:
                    service->Requestrestore(&ctx, &restorerequest,
                                            &restoreresponder, cq, cq, this);
                    CIS_ASYNC_PROFILE_END_OPS(restore_create);
                    break;
                case RT_DELETE_BACKUP:
                    service->Requestdelete_backup(&ctx, &delbackuprequest,
                                                  &delbackupresponder, cq, cq,
                                                  this);
                    CIS_ASYNC_PROFILE_END_OPS(delete_backup_create);
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
                        waitObj = famCIS->copy(
                            copyrequest.srcregionid(), copyrequest.srcoffset(),
                            copyrequest.srccopystart(), copyrequest.srckey(),
                            copyrequest.srcbaseaddr(),
                            copyrequest.srcaddr().c_str(),
                            copyrequest.srcaddrlen(),
                            copyrequest.destregionid(),
                            copyrequest.destoffset(),
                            copyrequest.destcopystart(), copyrequest.copysize(),
                            copyrequest.src_memserver_id(),
                            copyrequest.dest_memserver_id(), copyrequest.uid(),
                            copyrequest.gid());
                        delete (Fam_Copy_Tag *)waitObj;
                        break;
                    case RT_BACKUP:
                        waitObj = famCIS->backup(
                            backuprequest.regionid(), backuprequest.offset(),
                            backuprequest.memserver_id(), backuprequest.bname(),
                            backuprequest.uid(), backuprequest.gid(),
                            backuprequest.size());
                        delete (Fam_Backup_Tag *)waitObj;

                        break;
                    case RT_RESTORE:
                        waitObj = famCIS->restore(
                            restorerequest.regionid(), restorerequest.offset(),
                            restorerequest.memserver_id(),
                            restorerequest.bname(), restorerequest.uid(),
                            restorerequest.gid());
                        delete (Fam_Restore_Tag *)waitObj;
                        break;
                    case RT_DELETE_BACKUP:
                        waitObj = famCIS->delete_backup(
                            delbackuprequest.bname().c_str(),
                            delbackuprequest.memserver_id(),
                            delbackuprequest.uid(), delbackuprequest.gid());
                        delete (Fam_Delete_Backup_Tag *)waitObj;

                        break;
                    }
                } catch (Memory_Service_Exception &e) {
                    switch (type) {
                    case RT_COPY:
                        copyresponse.set_errorcode(e.fam_error());
                        copyresponse.set_errormsg(e.fam_error_msg());
                        break;

                    case RT_BACKUP:
                        backupresponse.set_errorcode(e.fam_error());
                        backupresponse.set_errormsg(e.fam_error_msg());
                        break;
                    case RT_RESTORE:
                        restoreresponse.set_errorcode(e.fam_error());
                        restoreresponse.set_errormsg(e.fam_error_msg());
                        break;
                    case RT_DELETE_BACKUP:
                        delbackupresponse.set_errorcode(e.fam_error());
                        delbackupresponse.set_errormsg(e.fam_error_msg());
                        break;
                    }
                    grpcStatus = Status::OK;
                } catch (CIS_Exception &e) {
                    switch (type) {
                    case RT_COPY:
                        copyresponse.set_errorcode(e.fam_error());
                        copyresponse.set_errormsg(e.fam_error_msg());
                        break;

                    case RT_BACKUP:
                        backupresponse.set_errorcode(e.fam_error());
                        backupresponse.set_errormsg(e.fam_error_msg());
                        break;
                    case RT_RESTORE:
                        restoreresponse.set_errorcode(e.fam_error());
                        restoreresponse.set_errormsg(e.fam_error_msg());
                        break;
                    case RT_DELETE_BACKUP:
                        delbackupresponse.set_errorcode(e.fam_error());
                        delbackupresponse.set_errormsg(e.fam_error_msg());
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
                    copyresponder.Finish(copyresponse, grpcStatus, this);
                    CIS_ASYNC_PROFILE_END_OPS(copy_process);
                    break;
                case RT_BACKUP:
                    backupresponder.Finish(backupresponse, grpcStatus, this);
                    CIS_ASYNC_PROFILE_END_OPS(backup_process);
                    break;
                case RT_RESTORE:
                    restoreresponder.Finish(restoreresponse, grpcStatus, this);
                    CIS_ASYNC_PROFILE_END_OPS(restore_process);
                    break;
                case RT_DELETE_BACKUP:
                    delbackupresponder.Finish(delbackupresponse, grpcStatus,
                                              this);
                    CIS_ASYNC_PROFILE_END_OPS(delete_backup_process);
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
                case RT_DELETE_BACKUP:
                    CIS_ASYNC_PROFILE_END_OPS(delete_backup_finish);
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
        Fam_Copy_Request copyrequest;

        // What we send back to the client.
        Fam_Copy_Response copyresponse;

        // What we get from the client.
        Fam_Backup_Restore_Request backuprequest;

        // What we send back to the client.
        Fam_Backup_Restore_Response backupresponse;

        // What we get from the client.
        Fam_Backup_Restore_Request restorerequest;

        // What we send back to the client.
        Fam_Backup_Restore_Response restoreresponse;

        // What we get from the client.
        Fam_Backup_List_Request delbackuprequest;

        // What we send back to the client.
        Fam_Backup_List_Response delbackupresponse;

        Status grpcStatus;
        // The means to get back to the client.
        ServerAsyncResponseWriter<Fam_Copy_Response> copyresponder;
        ServerAsyncResponseWriter<Fam_Backup_Restore_Response> backupresponder;
        ServerAsyncResponseWriter<Fam_Backup_Restore_Response> restoreresponder;
        ServerAsyncResponseWriter<Fam_Backup_List_Response> delbackupresponder;

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
    std::unique_ptr<ServerCompletionQueue> copycq;
    std::unique_ptr<ServerCompletionQueue> backupcq;
    std::unique_ptr<ServerCompletionQueue> restorecq;
    std::unique_ptr<ServerCompletionQueue> delbackupcq;
    sType *service;
    std::unique_ptr<Server> server;
};

} // namespace openfam
#endif
