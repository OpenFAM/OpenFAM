/*
 * fam_cis_server.h
 * Copyright (c) 2019-2021,2023 Hewlett Packard Enterprise Development, LP. All
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

#ifndef FAM_CIS_SERVER_H
#define FAM_CIS_SERVER_H

#include <iostream>
#include <map>
#include <thread>
#include <unistd.h>

#include "grpcpp/grpcpp.h"

#include "cis/fam_cis_direct.h"
#include "cis/fam_cis_rpc.grpc.pb.h"

#include "common/fam_internal.h"
#include "common/fam_internal_exception.h"
#include "common/fam_libfabric.h"
#include "common/fam_ops_libfabric.h"
#include "common/fam_options.h"

#include <boost/atomic.hpp>
#include <nvmm/heap.h>

namespace openfam {

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

class Fam_CIS_Server : public Fam_CIS_Rpc::Service {
  public:
    Fam_CIS_Server() {}

    ~Fam_CIS_Server();

    void cis_server_initialize(Fam_CIS_Direct *famCIS);

    ::grpc::Status signal_start(::grpc::ServerContext *context,
                                const ::Fam_Request *request,
                                ::Fam_Response *response) override;

    ::grpc::Status signal_termination(::grpc::ServerContext *context,
                                      const ::Fam_Request *request,
                                      ::Fam_Response *response) override;

    ::grpc::Status reset_profile(::grpc::ServerContext *context,
                                 const ::Fam_Request *request,
                                 ::Fam_Response *response) override;

    ::grpc::Status generate_profile(::grpc::ServerContext *context,
                                    const ::Fam_Request *request,
                                    ::Fam_Response *response) override;

    ::grpc::Status get_num_memory_servers(::grpc::ServerContext *context,
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

    ::grpc::Status get_stat_info(::grpc::ServerContext *context,
                                 const ::Fam_Dataitem_Request *request,
                                 ::Fam_Dataitem_Response *response) override;

    ::grpc::Status copy(::grpc::ServerContext *context,
                        const ::Fam_Copy_Request *request,
                        ::Fam_Copy_Response *response) override;

    ::grpc::Status backup(::grpc::ServerContext *context,
                          const ::Fam_Backup_Restore_Request *request,
                          ::Fam_Backup_Restore_Response *response) override;

    ::grpc::Status restore(::grpc::ServerContext *context,
                           const ::Fam_Backup_Restore_Request *request,
                           ::Fam_Backup_Restore_Response *response) override;

    ::grpc::Status acquire_CAS_lock(::grpc::ServerContext *context,
                                    const ::Fam_Dataitem_Request *request,
                                    ::Fam_Dataitem_Response *response) override;

    ::grpc::Status release_CAS_lock(::grpc::ServerContext *context,
                                    const ::Fam_Dataitem_Request *request,
                                    ::Fam_Dataitem_Response *response) override;

    ::grpc::Status get_addr_size(grpc::ServerContext *context,
                                 const Fam_Address_Request *request,
                                 Fam_Address_Response *response) override;

    ::grpc::Status get_addr(grpc::ServerContext *context,
                            const Fam_Address_Request *request,
                            Fam_Address_Response *response) override;

    ::grpc::Status get_backup_info(grpc::ServerContext *context,
                                   const Fam_Backup_Info_Request *request,
                                   Fam_Backup_Info_Response *response) override;

    ::grpc::Status list_backup(grpc::ServerContext *context,
                               const Fam_Backup_List_Request *request,
                               Fam_Backup_List_Response *response) override;
    ::grpc::Status delete_backup(grpc::ServerContext *context,
                                 const Fam_Backup_List_Request *request,
                                 Fam_Backup_List_Response *response) override;

    ::grpc::Status
    get_memserverinfo_size(grpc::ServerContext *context,
                           const Fam_Request *request,
                           Fam_Memserverinfo_Response *response) override;

    ::grpc::Status
    get_memserverinfo(grpc::ServerContext *context, const Fam_Request *request,
                      Fam_Memserverinfo_Response *response) override;

    ::grpc::Status get_atomic(::grpc::ServerContext *context,
                              const ::Fam_Atomic_Get_Request *request,
                              ::Fam_Atomic_Response *response) override;

    ::grpc::Status put_atomic(::grpc::ServerContext *context,
                              const ::Fam_Atomic_Put_Request *request,
                              ::Fam_Atomic_Response *response) override;

    ::grpc::Status
    scatter_strided_atomic(::grpc::ServerContext *context,
                           const ::Fam_Atomic_SG_Strided_Request *request,
                           ::Fam_Atomic_Response *response) override;

    ::grpc::Status
    gather_strided_atomic(::grpc::ServerContext *context,
                          const ::Fam_Atomic_SG_Strided_Request *request,
                          ::Fam_Atomic_Response *response) override;

    ::grpc::Status
    scatter_indexed_atomic(::grpc::ServerContext *context,
                           const ::Fam_Atomic_SG_Indexed_Request *request,
                           ::Fam_Atomic_Response *response) override;

    ::grpc::Status
    gather_indexed_atomic(::grpc::ServerContext *context,
                          const ::Fam_Atomic_SG_Indexed_Request *request,
                          ::Fam_Atomic_Response *response) override;

    ::grpc::Status
    open_region_with_registration(::grpc::ServerContext *context,
                                  const ::Fam_Region_Request *request,
                                  ::Fam_Region_Response *response) override;

    ::grpc::Status
    open_region_without_registration(::grpc::ServerContext *context,
                                     const ::Fam_Region_Request *request,
                                     ::Fam_Region_Response *response) override;

    ::grpc::Status close_region(::grpc::ServerContext *context,
                                const ::Fam_Region_Request *request,
                                ::Fam_Region_Response *response) override;

    ::grpc::Status get_region_memory(::grpc::ServerContext *context,
                                     const ::Fam_Region_Request *request,
                                     ::Fam_Region_Response *response) override;

  protected:
    int numClients;
    Fam_CIS_Direct *famCIS;
};

} // namespace openfam
#endif
