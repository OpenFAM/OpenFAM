/*
 * fam_allocator_grpc.cpp
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

#include <iostream>
#include <stdint.h>   // needed
#include <sys/stat.h> // needed for mode_t

#include "allocator/fam_allocator_grpc.h"

using namespace std;

#define FAM_UNIMPLEMENTED_GRPC()                                               \
    {                                                                          \
        std::ostringstream message;                                            \
        message << __func__                                                    \
                << " is Not Yet Implemented for libfabric interface !!!";      \
        throw Fam_Unimplemented_Exception(message.str().c_str());              \
    }

namespace openfam {
Fam_Allocator_Grpc::Fam_Allocator_Grpc(MemServerMap name, uint64_t port) {
    if (name.size() == 0) {
        throw Fam_Allocator_Exception(FAM_ERR_RPC_CLIENT_NOTFOUND,
                                      "server name not found");
    }
    rpcClients = new RpcClientMap();

    for (auto obj = name.begin(); obj != name.end(); ++obj) {
        Fam_Rpc_Client *client =
            new Fam_Rpc_Client((obj->second).c_str(), port);
        rpcClients->insert({obj->first, client});
    }
}

Fam_Allocator_Grpc::~Fam_Allocator_Grpc() {
    if (rpcClients != NULL) {
        for (auto rpc_client : *rpcClients) {
            delete rpc_client.second;
        }
        rpcClients->clear();
    }
    delete rpcClients;
}

void Fam_Allocator_Grpc::allocator_initialize() {}

void Fam_Allocator_Grpc::allocator_finalize() {}

Fam_Rpc_Client *Fam_Allocator_Grpc::get_rpc_client(uint64_t memoryServerId) {
    auto obj = rpcClients->find(memoryServerId);
    if (obj == rpcClients->end()) {
        throw Fam_Allocator_Exception(FAM_ERR_RPC_CLIENT_NOTFOUND,
                                      "RPC client not found");
    }
    return obj->second;
}

Fam_Region_Descriptor *Fam_Allocator_Grpc::create_region(
    const char *name, uint64_t nbytes, mode_t permissions,
    Fam_Redundancy_Level redundancyLevel, uint64_t memoryServerId = 0) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(memoryServerId);
    return rpcClient->create_region(name, nbytes, permissions, redundancyLevel,
                                    memoryServerId);
}

void Fam_Allocator_Grpc::destroy_region(Fam_Region_Descriptor *descriptor) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(descriptor->get_memserver_id());
    return rpcClient->destroy_region(descriptor);
}

int Fam_Allocator_Grpc::resize_region(Fam_Region_Descriptor *descriptor,
                                      uint64_t nbytes) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(descriptor->get_memserver_id());
    return rpcClient->resize_region(descriptor, nbytes);
}

Fam_Descriptor *Fam_Allocator_Grpc::allocate(const char *name, uint64_t nbytes,
                                             mode_t accessPermissions,
                                             Fam_Region_Descriptor *region) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(region->get_memserver_id());
    return rpcClient->allocate(name, nbytes, accessPermissions, region);
}

void Fam_Allocator_Grpc::deallocate(Fam_Descriptor *descriptor) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(descriptor->get_memserver_id());
    return rpcClient->deallocate(descriptor);
}

int Fam_Allocator_Grpc::change_permission(Fam_Region_Descriptor *descriptor,
                                          mode_t accessPermissions) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(descriptor->get_memserver_id());
    return rpcClient->change_permission(descriptor, accessPermissions);
}

int Fam_Allocator_Grpc::change_permission(Fam_Descriptor *descriptor,
                                          mode_t accessPermissions) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(descriptor->get_memserver_id());
    return rpcClient->change_permission(descriptor, accessPermissions);
}

Fam_Region_Descriptor *
Fam_Allocator_Grpc::lookup_region(const char *name,
                                  uint64_t memoryServerId = 0) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(memoryServerId);
    return rpcClient->lookup_region(name, memoryServerId);
}

Fam_Descriptor *Fam_Allocator_Grpc::lookup(const char *itemName,
                                           const char *regionName,
                                           uint64_t memoryServerId = 0) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(memoryServerId);
    return rpcClient->lookup(itemName, regionName, memoryServerId);
}

Fam_Region_Item_Info Fam_Allocator_Grpc::check_permission_get_info(
    Fam_Region_Descriptor *descriptor) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(descriptor->get_memserver_id());
    return rpcClient->check_permission_get_info(descriptor);
}

Fam_Region_Item_Info
Fam_Allocator_Grpc::check_permission_get_info(Fam_Descriptor *descriptor) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(descriptor->get_memserver_id());
    return rpcClient->check_permission_get_info(descriptor);
}

Fam_Region_Item_Info
Fam_Allocator_Grpc::get_stat_info(Fam_Descriptor *descriptor) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(descriptor->get_memserver_id());
    return rpcClient->get_stat_info(descriptor);
}

void *Fam_Allocator_Grpc::copy(Fam_Descriptor *src, uint64_t srcOffset,
                               Fam_Descriptor *dest, uint64_t destOffset,
                               uint64_t nbytes) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(src->get_memserver_id());
    return rpcClient->copy(src, srcOffset, dest, destOffset, nbytes);
}

void Fam_Allocator_Grpc::wait_for_copy(void *waitObj) {
    uint64_t memoryServerId = ((Fam_Copy_Tag *)waitObj)->memServerId;
    Fam_Rpc_Client *rpcClient = get_rpc_client(memoryServerId);
    return rpcClient->wait_for_copy(waitObj);
}

void *Fam_Allocator_Grpc::fam_map(Fam_Descriptor *descriptor) {
    FAM_UNIMPLEMENTED_GRPC();
    return NULL;
}

void Fam_Allocator_Grpc::fam_unmap(void *local, Fam_Descriptor *descriptor) {
    FAM_UNIMPLEMENTED_GRPC();
    return;
}

void Fam_Allocator_Grpc::acquire_CAS_lock(Fam_Descriptor *descriptor) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(descriptor->get_memserver_id());
    return rpcClient->acquire_CAS_lock(descriptor);
}

void Fam_Allocator_Grpc::release_CAS_lock(Fam_Descriptor *descriptor) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(descriptor->get_memserver_id());
    return rpcClient->release_CAS_lock(descriptor);
}

int Fam_Allocator_Grpc::get_addr_size(size_t *addrSize,
                                      uint64_t memoryServerId = 0) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(memoryServerId);
    if ((*addrSize = rpcClient->get_addr_size()) <= 0)
        return -1;
    return 0;
}

int Fam_Allocator_Grpc::get_addr(void *addr, size_t addrSize,
                                 uint64_t memoryServerId = 0) {
    Fam_Rpc_Client *rpcClient = get_rpc_client(memoryServerId);
    size_t addrnamelen = rpcClient->get_addr_size();
    if (addrSize > addrnamelen)
        return -1;

    if (memcpy(addr, rpcClient->get_addr(), addrSize) == NULL)
        return -1;

    return 0;
}

} // namespace openfam
