/*
 * fam_memory_service_thallium_server.h
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

#ifndef FAM_MEMORY_SERVICE_THALLIUM_SERVER_H
#define FAM_MEMORY_SERVICE_THALLIUM_SERVER_H

#include <iostream>
#include <map>
#include <thallium/serialization/stl/string.hpp>
#include <thread>
#include <unistd.h>

#include "fam_memory_service_thallium_rpc_structures.h"
#include "memory_service/fam_memory_service_direct.h"

namespace tl = thallium;
namespace openfam {

using namespace std;
using namespace nvmm;
using namespace metadata;

class Fam_Memory_Service_Thallium_Server
    : public tl::provider<Fam_Memory_Service_Thallium_Server> {
  public:
    Fam_Memory_Service_Thallium_Server(
        Fam_Memory_Service_Direct *direct_metadataService_,
        const tl::engine &myEngine);

    ~Fam_Memory_Service_Thallium_Server();

    void run();

    void reset_profile();

    void dump_profile();

    void signal_start(const tl::request &req,
                      Fam_Memory_Service_Thallium_Request memRequest);

    void signal_termination(const tl::request &req,
                            Fam_Memory_Service_Thallium_Request memRequest);

    void create_region(const tl::request &req,
                       Fam_Memory_Service_Thallium_Request memRequest);

    void destroy_region(const tl::request &req,
                        Fam_Memory_Service_Thallium_Request memRequest);

    void resize_region(const tl::request &req,
                       Fam_Memory_Service_Thallium_Request memRequest);

    void allocate(const tl::request &req,
                  Fam_Memory_Service_Thallium_Request memRequest);

    void deallocate(const tl::request &req,
                    Fam_Memory_Service_Thallium_Request memRequest);

    void copy(const tl::request &req,
              Fam_Memory_Service_Thallium_Request memRequest);

    void backup(const tl::request &req,
                Fam_Memory_Service_Thallium_Request memRequest);

    void restore(const tl::request &req,
                 Fam_Memory_Service_Thallium_Request memRequest);

    void get_backup_info(const tl::request &req,
                         Fam_Memory_Service_Thallium_Request memRequest);

    void list_backup(const tl::request &req,
                     Fam_Memory_Service_Thallium_Request memRequest);

    void delete_backup(const tl::request &req,
                       Fam_Memory_Service_Thallium_Request memRequest);

    void acquire_CAS_lock(const tl::request &req,
                          Fam_Memory_Service_Thallium_Request memRequest);

    void release_CAS_lock(const tl::request &req,
                          Fam_Memory_Service_Thallium_Request memRequest);

    void get_local_pointer(const tl::request &req,
                           Fam_Memory_Service_Thallium_Request memRequest);

    void open_region_with_registration(
        const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest);

    void open_region_without_registration(
        const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest);

    void close_region(const tl::request &req,
                      Fam_Memory_Service_Thallium_Request memRequest);

    void register_region_memory(const tl::request &req,
                                Fam_Memory_Service_Thallium_Request memRequest);

    void get_region_memory(const tl::request &req,
                           Fam_Memory_Service_Thallium_Request memRequest);

    void get_dataitem_memory(const tl::request &req,
                             Fam_Memory_Service_Thallium_Request memRequest);

    void get_atomic(const tl::request &req,
                    Fam_Memory_Service_Thallium_Request memRequest);

    void put_atomic(const tl::request &req,
                    Fam_Memory_Service_Thallium_Request memRequest);

    void scatter_strided_atomic(const tl::request &req,
                                Fam_Memory_Service_Thallium_Request memRequest);

    void gather_strided_atomic(const tl::request &req,
                               Fam_Memory_Service_Thallium_Request memRequest);

    void scatter_indexed_atomic(const tl::request &req,
                                Fam_Memory_Service_Thallium_Request memRequest);

    void gather_indexed_atomic(const tl::request &req,
                               Fam_Memory_Service_Thallium_Request memRequest);

    void
    update_memserver_addrlist(const tl::request &req,
                              Fam_Memory_Service_Thallium_Request memRequest);

    void create_region_failure_cleanup(
        const tl::request &req, Fam_Memory_Service_Thallium_Request memRequest);

    void grpc_server();

  protected:
    int numClients;
    Fam_Memory_Service_Rpc::Service *service;
    std::unique_ptr<Server> server;
    Fam_Memory_Service_Direct *memoryService;

  private:
    Fam_Memory_Service_Direct *direct_memoryService;
    tl::engine myEngine;
};

} // namespace openfam
#endif
