/*
 * fam_cis_thallium_server.h
 * Copyright (c) 2023 Hewlett Packard Enterprise Development, LP. All
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

#ifndef FAM_CIS_THALLIUM_SERVER_H
#define FAM_CIS_THALLIUM_SERVER_H

#include <iostream>
#include <map>
#include <thread>
#include <unistd.h>

#include "cis/fam_cis_direct.h"
#include "fam_cis_thallium_rpc_structures.h"
#include <thallium/serialization/stl/string.hpp>

#include "common/fam_internal.h"
#include "common/fam_internal_exception.h"
#include "common/fam_libfabric.h"
#include "common/fam_ops_libfabric.h"
#include "common/fam_options.h"

#include <boost/atomic.hpp>
#include <nvmm/heap.h>

namespace tl = thallium;
namespace openfam {

#define CAS_LOCK_CNT 128
#define LOCKHASH(offset) (offset >> 7) % CAS_LOCK_CNT

using namespace std;
using namespace nvmm;
using namespace metadata;

class Fam_CIS_Thallium_Server : public tl::provider<Fam_CIS_Thallium_Server> {
  public:
    Fam_CIS_Thallium_Server(Fam_CIS_Direct *direct_CIS_,
                            const tl::engine &myEngine);

    ~Fam_CIS_Thallium_Server();

    ::grpc::Status signal_start(::grpc::ServerContext *context,
                                const ::Fam_Request *request,
                                ::Fam_Response *response);

    ::grpc::Status signal_termination(::grpc::ServerContext *context,
                                      const ::Fam_Request *request,
                                      ::Fam_Response *response);

    void run();

    void reset_profile();

    void dump_profile();

    // void generate_profile(const tl::request& req, Fam_CIS_Thallium_Request
    // cisRequest);
    void generate_profile();

    void get_num_memory_servers(const tl::request &req,
                                Fam_CIS_Thallium_Request cisRequest);

    void create_region(const tl::request &req,
                       Fam_CIS_Thallium_Request cisRequest);

    void destroy_region(const tl::request &req,
                        Fam_CIS_Thallium_Request cisRequest);

    void resize_region(const tl::request &req,
                       Fam_CIS_Thallium_Request cisRequest);

    void allocate(const tl::request &req, Fam_CIS_Thallium_Request cisRequest);

    void deallocate(const tl::request &req,
                    Fam_CIS_Thallium_Request cisRequest);

    void change_region_permission(const tl::request &req,
                                  Fam_CIS_Thallium_Request cisRequest);

    void change_dataitem_permission(const tl::request &req,
                                    Fam_CIS_Thallium_Request cisRequest);

    void lookup_region(const tl::request &req,
                       Fam_CIS_Thallium_Request cisRequest);

    void lookup(const tl::request &req, Fam_CIS_Thallium_Request cisRequest);

    void check_permission_get_region_info(const tl::request &req,
                                          Fam_CIS_Thallium_Request cisRequest);

    void check_permission_get_item_info(const tl::request &req,
                                        Fam_CIS_Thallium_Request cisRequest);

    void get_stat_info(const tl::request &req,
                       Fam_CIS_Thallium_Request cisRequest);

    void copy(const tl::request &req, Fam_CIS_Thallium_Request cisRequest);

    void backup(const tl::request &req, Fam_CIS_Thallium_Request cisRequest);

    void restore(const tl::request &req, Fam_CIS_Thallium_Request cisRequest);

    void acquire_CAS_lock(const tl::request &req,
                          Fam_CIS_Thallium_Request cisRequest);

    void release_CAS_lock(const tl::request &req,
                          Fam_CIS_Thallium_Request cisRequest);

    void get_backup_info(const tl::request &req,
                         Fam_CIS_Thallium_Request cisRequest);

    void list_backup(const tl::request &req,
                     Fam_CIS_Thallium_Request cisRequest);

    void delete_backup(const tl::request &req,
                       Fam_CIS_Thallium_Request cisRequest);

    void get_memserverinfo_size(const tl::request &req,
                                Fam_CIS_Thallium_Request cisRequest);

    void get_memserverinfo(const tl::request &req,
                           Fam_CIS_Thallium_Request cisRequest);

    void get_atomic(const tl::request &req,
                    Fam_CIS_Thallium_Request cisRequest);

    void put_atomic(const tl::request &req,
                    Fam_CIS_Thallium_Request cisRequest);

    void scatter_strided_atomic(const tl::request &req,
                                Fam_CIS_Thallium_Request cisRequest);

    void gather_strided_atomic(const tl::request &req,
                               Fam_CIS_Thallium_Request cisRequest);

    void scatter_indexed_atomic(const tl::request &req,
                                Fam_CIS_Thallium_Request cisRequest);

    void gather_indexed_atomic(const tl::request &req,
                               Fam_CIS_Thallium_Request cisRequest);

    void get_region_memory(const tl::request &req,
                           Fam_CIS_Thallium_Request cisRequest);

    void open_region_with_registration(const tl::request &req,
                                       Fam_CIS_Thallium_Request cisRequest);

    void open_region_without_registration(const tl::request &req,
                                          Fam_CIS_Thallium_Request cisRequest);

    void close_region(const tl::request &req,
                      Fam_CIS_Thallium_Request cisRequest);

    void grpc_server();

  protected:
    int numClients;
    tl::engine myEngine;
    Fam_CIS_Direct *famCIS, *direct_CIS;
};

} // namespace openfam
#endif
