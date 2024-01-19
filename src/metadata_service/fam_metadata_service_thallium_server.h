/*
 * fam_metadata_service_thallium_server.h
 * Copyright (c) 2023 Hewlett Packard Enterprise Development, LP. All rights
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
#ifndef FAM_METADATA_SERVICE_THALLIUM_SERVER_H
#define FAM_METADATA_SERVICE_THALLIUM_SERVER_H

#include "fam_metadata_service_direct.h"
#include "fam_metadata_service_thallium_rpc_structures.h"
#include <iostream>
#include <map>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thread>
#include <unistd.h>

namespace tl = thallium;
using namespace openfam;

namespace metadata {

class Fam_Metadata_Service_Thallium_Server
    : public tl::provider<Fam_Metadata_Service_Thallium_Server> {
  public:
    Fam_Metadata_Service_Thallium_Server(
        Fam_Metadata_Service_Direct *direct_metadataService_,
        const tl::engine &myEngine);

    ~Fam_Metadata_Service_Thallium_Server();

    void run();

    void reset_profile();

    void dump_profile();

    void metadata_insert_region(const tl::request &req,
                                Fam_Metadata_Thallium_Request metaRequest);

    void metadata_delete_region(const tl::request &req,
                                Fam_Metadata_Thallium_Request metaRequest);

    void metadata_find_region(const tl::request &req,
                              Fam_Metadata_Thallium_Request metaRequest);

    void metadata_modify_region(const tl::request &req,
                                Fam_Metadata_Thallium_Request metaRequest);

    void metadata_insert_dataitem(const tl::request &req,
                                  Fam_Metadata_Thallium_Request metaRequest);

    void metadata_delete_dataitem(const tl::request &req,
                                  Fam_Metadata_Thallium_Request metaRequest);

    void metadata_find_dataitem(const tl::request &req,
                                Fam_Metadata_Thallium_Request metaRequest);

    void metadata_modify_dataitem(const tl::request &req,
                                  Fam_Metadata_Thallium_Request metaRequest);

    void metadata_check_region_permissions(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest);

    void
    metadata_check_item_permissions(const tl::request &req,
                                    Fam_Metadata_Thallium_Request metaRequest);

    void metadata_maxkeylen(const tl::request &req,
                            Fam_Metadata_Thallium_Request metaRequest);

    void
    metadata_update_memoryserver(const tl::request &req,
                                 Fam_Metadata_Thallium_Request metaRequest);

    void metadata_reset_bitmap(const tl::request &req,
                               Fam_Metadata_Thallium_Request metaRequest);

    void metadata_validate_and_create_region(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest);

    void metadata_validate_and_destroy_region(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest);

    void metadata_validate_and_allocate_dataitem(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest);

    void metadata_validate_and_deallocate_dataitem(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest);

    void metadata_find_region_and_check_permissions(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest);

    void metadata_find_dataitem_and_check_permissions(
        const tl::request &req, Fam_Metadata_Thallium_Request metaRequest);

    void get_memory_server_list(const tl::request &req,
                                Fam_Metadata_Thallium_Request metaRequest);

    void grpc_server();

  private:
    Fam_Metadata_Service_Direct *metadataService, *direct_metadataService;
    tl::engine myEngine;
};

} // namespace metadata
#endif // namespace metadata
