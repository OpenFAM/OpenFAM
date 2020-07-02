/*
 * fam_cis_client.cpp
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

#include "cis/fam_cis_client.h"

namespace openfam {

Fam_CIS_Client::Fam_CIS_Client(MemServerMap servers, uint64_t port) {
    std::ostringstream message;

    /** Creating a channel and stub **/
    for (auto obj = servers.begin(); obj != servers.end(); ++obj) {
        string name = obj->second + ":" + std::to_string(port);
        service stub = Fam_CIS_Rpc::NewStub(
            grpc::CreateChannel(name, ::grpc::InsecureChannelCredentials()));
        if (!stub) {
            message << "Fam Grpc Initialialization failed: stub is null";
            throw Fam_Allocator_Exception(FAM_ERR_GRPC, message.str().c_str());
        }

        Fam_Request req;
        Fam_Start_Response res;

        ::grpc::ClientContext ctx;
        /** sending a start signal to server **/
        ::grpc::Status status = stub->signal_start(&ctx, req, &res);
        if (!status.ok()) {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }

        size_t FabricAddrSize = res.addrnamelen();
        char *fabricAddr = (char *)calloc(1, FabricAddrSize);

        uint32_t lastBytes = 0;
        int lastBytesCount = (int)(FabricAddrSize % sizeof(uint32_t));
        int readCount = res.addrname_size();

        if (lastBytesCount > 0)
            readCount -= 1;

        for (int ndx = 0; ndx < readCount; ndx++) {
            *((uint32_t *)fabricAddr + ndx) = res.addrname(ndx);
        }

        if (lastBytesCount > 0) {
            lastBytes = res.addrname(readCount);
            memcpy(((uint32_t *)fabricAddr + readCount), &lastBytes,
                   lastBytesCount);
        }
        Fam_CIS_Server_Info *server = new Fam_CIS_Server_Info();
        server->stub = std::move(stub);
        server->memServerFabricAddrSize = FabricAddrSize;
        server->memServerFabricAddr = fabricAddr;
        ::grpc::CompletionQueue *cq = new ::grpc::CompletionQueue();
        server->cq = cq;
        serversInfo.insert(std::make_pair(obj->first, server));
    }
}

Fam_CIS_Client::~Fam_CIS_Client() {
    Fam_Request req;
    Fam_Response res;

    ::grpc::ClientContext ctx;

    for (auto obj = serversInfo.begin(); obj != serversInfo.end(); ++obj) {
        Fam_CIS_Server_Info *server = get_server_info(obj->first);

        ::grpc::Status status =
            server->stub->signal_termination(&ctx, req, &res);

        delete obj->second;
    }
}

Fam_CIS_Server_Info *Fam_CIS_Client::get_server_info(uint64_t memoryServerId) {
    auto obj = serversInfo.find(memoryServerId);
    if (obj == serversInfo.end()) {
        throw Fam_Allocator_Exception(FAM_ERR_RPC_STUB_NOTFOUND,
                                      "RPC stub not found");
    }
    return obj->second;
}

void Fam_CIS_Client::reset_profile(uint64_t memoryServerId) {
#ifdef MEMSERVER_PROFILE
    Fam_Request req;
    Fam_Response res;

    ::grpc::ClientContext ctx;

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);
    ::grpc::Status status = server->stub->reset_profile(&ctx, req, &res);
#endif
}

void Fam_CIS_Client::generate_profile(uint64_t memoryServerId) {
#ifdef MEMSERVER_PROFILE
    Fam_Request req;
    Fam_Response res;

    ::grpc::ClientContext ctx;

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);
    ::grpc::Status status = server->stub->generate_profile(&ctx, req, &res);
#endif
}

Fam_Region_Item_Info
Fam_CIS_Client::create_region(string name, size_t nbytes, mode_t permission,
                              Fam_Redundancy_Level redundancyLevel,
                              uint64_t memoryServerId, uint32_t uid,
                              uint32_t gid) {
    Fam_Region_Request req;
    Fam_Region_Response res;
    ::grpc::ClientContext ctx;

    req.set_name(name);
    req.set_size(nbytes);
    req.set_perm(permission);
    req.set_uid(uid);
    req.set_gid(gid);
    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);
    ::grpc::Status status = server->stub->create_region(&ctx, req, &res);
    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            Fam_Region_Item_Info info;
            info.regionId =
                res.regionid() | (memoryServerId << MEMSERVERID_SHIFT);
            info.offset = res.offset();
            return info;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

void Fam_CIS_Client::destroy_region(uint64_t regionId, uint64_t memoryServerId,
                                    uint32_t uid, uint32_t gid) {
    Fam_Region_Request req;
    Fam_Region_Response res;
    ::grpc::ClientContext ctx;

    req.set_regionid(regionId & REGIONID_MASK);
    req.set_uid(uid);
    req.set_gid(gid);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status = server->stub->destroy_region(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            return;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

void Fam_CIS_Client::resize_region(uint64_t regionId, size_t nbytes,
                                   uint64_t memoryServerId, uint32_t uid,
                                   uint32_t gid) {
    Fam_Region_Request req;
    Fam_Region_Response res;
    ::grpc::ClientContext ctx;

    req.set_regionid(regionId & REGIONID_MASK);
    req.set_size(nbytes);
    req.set_uid(uid);
    req.set_gid(gid);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status = server->stub->resize_region(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            return;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

Fam_Region_Item_Info Fam_CIS_Client::allocate(string name, size_t nbytes,
                                              mode_t permission,
                                              uint64_t regionId,
                                              uint64_t memoryServerId,
                                              uint32_t uid, uint32_t gid) {
    Fam_Dataitem_Request req;
    Fam_Dataitem_Response res;
    ::grpc::ClientContext ctx;

    req.set_name(name);
    req.set_regionid(regionId & REGIONID_MASK);
    req.set_size(nbytes);
    req.set_perm(permission);
    req.set_uid(uid);
    req.set_gid(gid);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    uint64_t nodeId = memoryServerId;

    ::grpc::Status status = server->stub->allocate(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            Fam_Region_Item_Info info;
            info.regionId = res.regionid() | (nodeId << MEMSERVERID_SHIFT);
            info.offset = res.offset();
            info.key = res.key();
            info.base = (void *)res.base();
            return info;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

void Fam_CIS_Client::deallocate(uint64_t regionId, uint64_t offset,
                                uint64_t memoryServerId, uint32_t uid,
                                uint32_t gid) {
    Fam_Dataitem_Request req;
    Fam_Dataitem_Response res;
    ::grpc::ClientContext ctx;

    req.set_regionid(regionId & REGIONID_MASK);
    req.set_offset(offset);
    req.set_uid(uid);
    req.set_gid(gid);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status = server->stub->deallocate(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            return;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

void Fam_CIS_Client::change_region_permission(uint64_t regionId,
                                              mode_t permission,
                                              uint64_t memoryServerId,
                                              uint32_t uid, uint32_t gid) {
    Fam_Region_Request req;
    Fam_Region_Response res;
    ::grpc::ClientContext ctx;

    req.set_regionid(regionId & REGIONID_MASK);
    req.set_perm((uint64_t)permission);
    req.set_uid(uid);
    req.set_gid(gid);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status =
        server->stub->change_region_permission(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            return;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

void Fam_CIS_Client::change_dataitem_permission(uint64_t regionId,
                                                uint64_t offset,
                                                mode_t permission,
                                                uint64_t memoryServerId,
                                                uint32_t uid, uint32_t gid) {
    Fam_Dataitem_Request req;
    Fam_Dataitem_Response res;
    ::grpc::ClientContext ctx;

    req.set_regionid(regionId & REGIONID_MASK);
    req.set_offset(offset);
    req.set_perm((uint64_t)permission);
    req.set_uid(uid);
    req.set_gid(gid);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status =
        server->stub->change_dataitem_permission(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            return;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

Fam_Region_Item_Info Fam_CIS_Client::lookup_region(string name,
                                                   uint64_t memoryServerId,
                                                   uint32_t uid, uint32_t gid) {
    Fam_Region_Request req;
    Fam_Region_Response res;
    ::grpc::ClientContext ctx;

    req.set_name(name);
    req.set_uid(uid);
    req.set_gid(gid);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status = server->stub->lookup_region(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            Fam_Region_Item_Info info;
            info.regionId =
                res.regionid() | (memoryServerId << MEMSERVERID_SHIFT);
            info.offset = res.offset();
            info.size = res.size();
            info.perm = (mode_t)res.perm();
            info.name = (char *)(res.name()).c_str();
            return info;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

Fam_Region_Item_Info Fam_CIS_Client::lookup(string itemName, string regionName,
                                            uint64_t memoryServerId,
                                            uint32_t uid, uint32_t gid) {
    Fam_Dataitem_Request req;
    Fam_Dataitem_Response res;
    ::grpc::ClientContext ctx;

    req.set_name(itemName);
    req.set_regionname(regionName);
    req.set_uid(uid);
    req.set_gid(gid);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status = server->stub->lookup(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            Fam_Region_Item_Info info;
            info.regionId =
                res.regionid() | (memoryServerId << MEMSERVERID_SHIFT);
            info.offset = res.offset();
            info.key = FAM_KEY_UNINITIALIZED;
            info.size = res.size();
            info.perm = (mode_t)res.perm();
            info.name = (char *)(res.name()).c_str();
            return info;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

Fam_Region_Item_Info Fam_CIS_Client::check_permission_get_region_info(
    uint64_t regionId, uint64_t memoryServerId, uint32_t uid, uint32_t gid) {
    Fam_Region_Request req;
    Fam_Region_Response res;
    ::grpc::ClientContext ctx;

    req.set_regionid(regionId & REGIONID_MASK);
    req.set_gid(gid);
    req.set_uid(uid);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status =
        server->stub->check_permission_get_region_info(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            Fam_Region_Item_Info info;
            info.size = res.size();
            info.perm = (mode_t)res.perm();
            info.name = (char *)(res.name()).c_str();
            return info;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

Fam_Region_Item_Info Fam_CIS_Client::check_permission_get_item_info(
    uint64_t regionId, uint64_t offset, uint64_t memoryServerId, uint32_t uid,
    uint32_t gid) {
    Fam_Dataitem_Request req;
    Fam_Dataitem_Response res;
    ::grpc::ClientContext ctx;

    req.set_regionid(regionId & REGIONID_MASK);
    req.set_offset(offset);
    req.set_gid(gid);
    req.set_uid(uid);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status =
        server->stub->check_permission_get_item_info(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            Fam_Region_Item_Info info;
            info.key = res.key();
            info.base = (void *)res.base();
            info.size = res.size();
            info.perm = (mode_t)res.perm();
            info.name = (char *)(res.name()).c_str();
            return info;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

Fam_Region_Item_Info Fam_CIS_Client::get_stat_info(uint64_t regionId,
                                                   uint64_t offset,
                                                   uint64_t memoryServerId,
                                                   uint32_t uid, uint32_t gid) {
    Fam_Dataitem_Request req;
    Fam_Dataitem_Response res;
    ::grpc::ClientContext ctx;

    req.set_regionid(regionId & REGIONID_MASK);
    req.set_offset(offset);
    req.set_gid(gid);
    req.set_uid(uid);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status = server->stub->get_stat_info(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            Fam_Region_Item_Info info;
            info.size = res.size();
            info.perm = (mode_t)res.perm();
            info.name = (char *)(res.name()).c_str();
            return info;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}
void *Fam_CIS_Client::copy(uint64_t srcRegionId, uint64_t srcOffset,
                           uint64_t srcCopyStart, uint64_t destRegionId,
                           uint64_t destOffset, uint64_t destCopyStar,
                           uint64_t nbytes, uint64_t memoryServerId,
                           uint32_t uid, uint32_t gid) {
    Fam_Copy_Request copyReq;
    Fam_Copy_Response copyRes;
    ::grpc::ClientContext ctx;

    copyReq.set_srcregionid(srcRegionId & REGIONID_MASK);
    copyReq.set_destregionid(destRegionId & REGIONID_MASK);
    copyReq.set_srcoffset(srcOffset);
    copyReq.set_destoffset(destOffset);
    copyReq.set_srccopystart(srcCopyStart);
    copyReq.set_destcopystart(destCopyStar);
    copyReq.set_gid(gid);
    copyReq.set_uid(uid);
    copyReq.set_copysize(nbytes);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    Fam_Copy_Tag *tag = new Fam_Copy_Tag();

    tag->isCompleted = false;
    tag->memServerId = memoryServerId;

    ::grpc::CompletionQueue *cq = server->cq;

    tag->responseReader =
        server->stub->PrepareAsynccopy(&tag->ctx, copyReq, cq);

    // StartCall initiates the RPC call
    tag->responseReader->StartCall();

    tag->responseReader->Finish(&tag->res, &tag->status, (void *)tag);

    return (void *)tag;
}

void Fam_CIS_Client::wait_for_copy(void *waitObj) {
    void *got_tag;
    bool ok = false;
    Fam_Copy_Tag *tagIn, *tagCompleted;
    ::grpc::Status status;
    Fam_Copy_Response res;

    tagIn = static_cast<Fam_Copy_Tag *>(waitObj);

    Fam_CIS_Server_Info *server = get_server_info(tagIn->memServerId);
    ::grpc::CompletionQueue *cq = server->cq;

    if (!tagIn) {
        throw Fam_Allocator_Exception(FAM_ERR_INVALID, "Copy tag is null");
    }

    if (tagIn->isCompleted) {
        if (tagIn->status.ok()) {
            if (tagIn->res.errorcode()) {
                res = tagIn->res;
                delete tagIn;
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              res.errormsg().c_str());
            } else {
                delete tagIn;
                return;
            }
        } else {
            status = tagIn->status;
            delete tagIn;
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    } else {

        do {
            GPR_ASSERT(cq->Next(&got_tag, &ok));
            // The tag is the memory location of Fam_Copy_Tag object
            tagCompleted = static_cast<Fam_Copy_Tag *>(got_tag);
            if (!tagCompleted) {
                throw Fam_Allocator_Exception(FAM_ERR_INVALID,
                                              "Copy tag is null");
            }
            tagCompleted->isCompleted = true;
            // Verify that the request was completed successfully. Note
            // that "ok" corresponds solely to the request for updates
            // introduced by Finish().
            GPR_ASSERT(ok);
        } while (tagIn != tagCompleted);

        if (tagCompleted->status.ok()) {
            if (tagCompleted->res.errorcode()) {
                res = tagCompleted->res;
                delete tagCompleted;
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              res.errormsg().c_str());
            } else {
                delete tagCompleted;
                return;
            }
        } else {
            status = tagCompleted->status;
            delete tagCompleted;
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }
}

void *Fam_CIS_Client::fam_map(uint64_t regionId, uint64_t offset,
                              uint64_t memoryServerId, uint32_t uid,
                              uint32_t gid) {
    FAM_UNIMPLEMENTED_GRPC()
    return NULL;
}

void Fam_CIS_Client::fam_unmap(void *local, uint64_t regionId, uint64_t offset,
                               uint64_t memoryServerId, uint32_t uid,
                               uint32_t gid) {
    FAM_UNIMPLEMENTED_GRPC()
}

void Fam_CIS_Client::acquire_CAS_lock(uint64_t offset,
                                      uint64_t memoryServerId) {
    Fam_Dataitem_Request req;
    Fam_Dataitem_Response res;
    ::grpc::ClientContext ctx;

    req.set_offset(offset);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status = server->stub->acquire_CAS_lock(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            return;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

void Fam_CIS_Client::release_CAS_lock(uint64_t offset,
                                      uint64_t memoryServerId) {
    Fam_Dataitem_Request req;
    Fam_Dataitem_Response res;
    ::grpc::ClientContext ctx;

    req.set_offset(offset);

    Fam_CIS_Server_Info *server = get_server_info(memoryServerId);

    ::grpc::Status status = server->stub->release_CAS_lock(&ctx, req, &res);

    if (status.ok()) {
        if (res.errorcode()) {
            throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                          (res.errormsg()).c_str());
        } else {
            return;
        }
    } else {
        throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                      (status.error_message()).c_str());
    }
}

int Fam_CIS_Client::get_addr_size(size_t *addrSize, uint64_t memoryServerId) {
    auto obj = serversInfo.find(memoryServerId);
    if (obj == serversInfo.end()) {
        throw Fam_Allocator_Exception(FAM_ERR_RPC_STUB_NOTFOUND,
                                      "RPC stub not found");
    }
    Fam_CIS_Server_Info *server = obj->second;
    *addrSize = server->memServerFabricAddrSize;
    if (*addrSize <= 0)
        return -1;
    return 0;
}

int Fam_CIS_Client::get_addr(void *addr, size_t addrSize,
                             uint64_t memoryServerId) {
    auto obj = serversInfo.find(memoryServerId);
    if (obj == serversInfo.end()) {
        throw Fam_Allocator_Exception(FAM_ERR_RPC_STUB_NOTFOUND,
                                      "RPC stub not found");
    }
    Fam_CIS_Server_Info *server = obj->second;

    if (addrSize > server->memServerFabricAddrSize)
        return -1;

    if (memcpy(addr, server->memServerFabricAddr, addrSize) == NULL)
        return -1;

    return 0;
}

} // namespace openfam
