/*
 * fam_memory_service_client.cpp
 * Copyright (c) 2020-2021 Hewlett Packard Enterprise Development, LP. All
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

#include "fam_memory_service_client.h"
#include "common/atomic_queue.h"
#include "common/fam_memserver_profile.h"

#include <boost/atomic.hpp>

#include <chrono>
#include <iomanip>
#include <string.h>
#include <unistd.h>

using namespace std;
using namespace chrono;

namespace openfam {
MEMSERVER_PROFILE_START(MEMORY_SERVICE_CLIENT)
#ifdef MEMSERVER_PROFILE
#define MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()                              \
    {                                                                          \
        Profile_Time start = MEMORY_SERVICE_CLIENT_get_time();

#define MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(apiIdx)                          \
    Profile_Time end = MEMORY_SERVICE_CLIENT_get_time();                       \
    Profile_Time total =                                                       \
        MEMORY_SERVICE_CLIENT_time_diff_nanoseconds(start, end);               \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(MEMORY_SERVICE_CLIENT, prof_##apiIdx,   \
                                       total)                                  \
    }
#define MEMORY_SERVICE_CLIENT_PROFILE_DUMP()                                   \
    memory_service_client_profile_dump()
#else
#define MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
#define MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(apiIdx)
#define MEMORY_SERVICE_CLIENT_PROFILE_DUMP()
#endif

void memory_service_client_profile_dump() {
    MEMSERVER_PROFILE_END(MEMORY_SERVICE_CLIENT);
    MEMSERVER_DUMP_PROFILE_BANNER(MEMORY_SERVICE_CLIENT)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(MEMORY_SERVICE_CLIENT, name, prof_##name)
#include "memory_service/memory_service_client_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(MEMORY_SERVICE_CLIENT, prof_##name)
#include "memory_service/memory_service_client_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(MEMORY_SERVICE_CLIENT)
}

Fam_Memory_Service_Client::Fam_Memory_Service_Client(const char *name,
                                                     uint64_t port) {
    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_CLIENT)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_CLIENT)
    std::ostringstream message;
    std::string name_s(name);
    /** Creating a channel and stub **/
    name_s += ":" + std::to_string(port);
    this->stub = Fam_Memory_Service_Rpc::NewStub(
        grpc::CreateChannel(name_s, ::grpc::InsecureChannelCredentials()));
    if (!stub) {
        message << "Fam Grpc Initialialization failed: stub is null";
        throw Memory_Service_Exception(FAM_ERR_RPC, message.str().c_str());
    }

    Fam_Memory_Service_General_Request req;
    Fam_Memory_Service_Start_Response res;

    ::grpc::ClientContext ctx;
    /** sending a start signal to server **/
    ::grpc::Status status = stub->signal_start(&ctx, req, &res);
    if (!status.ok()) {
        throw Memory_Service_Exception(FAM_ERR_RPC,
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
    memServerFabricAddrSize = FabricAddrSize;
    memServerFabricAddr = fabricAddr;
}

Fam_Memory_Service_Client::~Fam_Memory_Service_Client() {
    Fam_Memory_Service_General_Request req;
    Fam_Memory_Service_General_Response res;

    ::grpc::ClientContext ctx;

    ::grpc::Status status = stub->signal_termination(&ctx, req, &res);
}

void Fam_Memory_Service_Client::reset_profile() {
    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_CLIENT)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_CLIENT)
#ifdef MEMSERVER_PROFILE
    Fam_Memory_Service_General_Request req;
    Fam_Memory_Service_General_Response res;

    ::grpc::ClientContext ctx;

    ::grpc::Status status = stub->reset_profile(&ctx, req, &res);
#endif
}

void Fam_Memory_Service_Client::dump_profile() {
    MEMORY_SERVICE_CLIENT_PROFILE_DUMP();
#ifdef MEMSERVER_PROFILE
    Fam_Memory_Service_General_Request req;
    Fam_Memory_Service_General_Response res;

    ::grpc::ClientContext ctx;

    ::grpc::Status status = stub->dump_profile(&ctx, req, &res);
#endif
}

void Fam_Memory_Service_Client::create_region(uint64_t regionId,
                                              size_t nbytes) {

    Fam_Memory_Service_Request req;
    Fam_Memory_Service_Response res;
    ::grpc::ClientContext ctx;
    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_region_id(regionId);
    req.set_size(nbytes);

    ::grpc::Status status = stub->create_region(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)

    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_create_region);
}

void Fam_Memory_Service_Client::destroy_region(uint64_t regionId) {
    Fam_Memory_Service_Request req;
    Fam_Memory_Service_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_region_id(regionId);

    ::grpc::Status status = stub->destroy_region(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_destroy_region);
}

void Fam_Memory_Service_Client::resize_region(uint64_t regionId,
                                              size_t nbytes) {
    Fam_Memory_Service_Request req;
    Fam_Memory_Service_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_region_id(regionId);
    req.set_size(nbytes);

    ::grpc::Status status = stub->resize_region(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_resize_region);
}

Fam_Region_Item_Info Fam_Memory_Service_Client::allocate(uint64_t regionId,
                                                         size_t nbytes) {
    Fam_Memory_Service_Request req;
    Fam_Memory_Service_Response res;
    ::grpc::ClientContext ctx;

    Fam_Region_Item_Info info;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_region_id(regionId);
    req.set_size(nbytes);

    ::grpc::Status status = stub->allocate(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)

    info.offset = res.offset();
    info.base = (void *)res.base();
    info.key = res.key();
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_allocate);
    return info;
}

void Fam_Memory_Service_Client::deallocate(uint64_t regionId, uint64_t offset) {
    Fam_Memory_Service_Request req;
    Fam_Memory_Service_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_region_id(regionId);
    req.set_offset(offset);

    ::grpc::Status status = stub->deallocate(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_deallocate);
}

void Fam_Memory_Service_Client::copy(uint64_t srcRegionId, uint64_t srcOffset,
                                     uint64_t srcKey, uint64_t srcCopyStart,
                                     uint64_t srcBaseAddr, const char *srcAddr,
                                     uint32_t srcAddrLen, uint64_t destRegionId,
                                     uint64_t destOffset, uint64_t size,
                                     uint64_t srcMemserverId,
                                     uint64_t destMemserverId) {
    Fam_Memory_Copy_Request req;
    Fam_Memory_Copy_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_src_region_id(srcRegionId);
    req.set_dest_region_id(destRegionId);
    req.set_src_offset(srcOffset);
    req.set_src_key(srcKey);
    req.set_src_copy_start(srcCopyStart);
    req.set_src_base_addr(srcBaseAddr);
    req.set_src_addr(srcAddr, srcAddrLen);
    req.set_src_addr_len(srcAddrLen);
    req.set_dest_offset(destOffset);
    req.set_size(size);
    req.set_src_memserver_id(srcMemserverId);
    req.set_dest_memserver_id(destMemserverId);

    ::grpc::Status status = stub->copy(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_copy);
}

void Fam_Memory_Service_Client::backup(uint64_t srcRegionId, uint64_t srcOffset,
                                       string outputFile, uint64_t size) {
    Fam_Memory_Backup_Restore_Request req;
    Fam_Memory_Backup_Restore_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_region_id(srcRegionId);
    req.set_offset(srcOffset);
    req.set_filename(outputFile);
    req.set_size(size);

    ::grpc::Status status = stub->backup(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_backup);
}
void Fam_Memory_Service_Client::restore(uint64_t destRegionId,
                                        uint64_t destOffset, string inputFile,
                                        uint64_t size) {
    Fam_Memory_Backup_Restore_Request req;
    Fam_Memory_Backup_Restore_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_region_id(destRegionId);
    req.set_offset(destOffset);
    req.set_filename(inputFile);
    req.set_size(size);

    ::grpc::Status status = stub->restore(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_restore);
}

void Fam_Memory_Service_Client::acquire_CAS_lock(uint64_t offset) {
    Fam_Memory_Service_Request req;
    Fam_Memory_Service_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_offset(offset);

    ::grpc::Status status = stub->acquire_CAS_lock(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_acquire_CAS_lock);
}

void Fam_Memory_Service_Client::release_CAS_lock(uint64_t offset) {
    Fam_Memory_Service_Request req;
    Fam_Memory_Service_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_offset(offset);

    ::grpc::Status status = stub->release_CAS_lock(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_release_CAS_lock);
}

size_t Fam_Memory_Service_Client::get_addr_size() {
    return memServerFabricAddrSize;
}

void *Fam_Memory_Service_Client::get_addr() { return memServerFabricAddr; }

Fam_Backup_Info
Fam_Memory_Service_Client::get_backup_info(std::string inputFile) {

    Fam_Memory_Backup_Info_Request req;
    Fam_Memory_Backup_Info_Response res;
    ::grpc::ClientContext ctx;
    std::string filename = std::string(inputFile);
    req.set_filename(filename);
    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()

    ::grpc::Status status = stub->get_backup_info(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_get_backup_info);

    Fam_Backup_Info info;
    info.size = res.size();
    // info.name = (char *)strdup(res.name());
    info.name = (char *)(res.name().c_str());
    info.uid = res.uid();
    info.gid = res.gid();
    info.mode = res.mode();

    return info;
}
void *Fam_Memory_Service_Client::get_local_pointer(uint64_t regionId,
                                                   uint64_t offset) {
    Fam_Memory_Service_Request req;
    Fam_Memory_Service_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_region_id(regionId);
    req.set_offset(offset);

    ::grpc::Status status = stub->get_local_pointer(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_get_local_pointer);

    return (void *)res.base();
}

uint64_t Fam_Memory_Service_Client::get_key(uint64_t regionId, uint64_t offset,
                                            uint64_t size, bool rwFlag) {
    Fam_Memory_Service_Request req;
    Fam_Memory_Service_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_region_id(regionId);
    req.set_offset(offset);
    req.set_size(size);
    req.set_rw_flag(rwFlag);

    ::grpc::Status status = stub->get_key(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)

    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_get_key);
    return res.key();
}

void Fam_Memory_Service_Client::get_atomic(uint64_t regionId,
                                           uint64_t srcOffset,
                                           uint64_t dstOffset, uint64_t nbytes,
                                           uint64_t key, uint64_t srcBaseAddr,
                                           const char *nodeAddr,
                                           uint32_t nodeAddrSize) {
    Fam_Memory_Atomic_Get_Request req;
    Fam_Memory_Atomic_Response res;
    ::grpc::ClientContext ctx;
    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_regionid(regionId & REGIONID_MASK);
    req.set_srcoffset(srcOffset);
    req.set_dstoffset(dstOffset);
    req.set_nbytes(nbytes);
    req.set_key(key);
    req.set_src_base_addr(srcBaseAddr);
    req.set_nodeaddr(nodeAddr, nodeAddrSize);
    req.set_nodeaddrsize(nodeAddrSize);
    ::grpc::Status status = stub->get_atomic(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_get_atomic);
}

void Fam_Memory_Service_Client::put_atomic(
    uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset, uint64_t nbytes,
    uint64_t key, uint64_t srcBaseAddr, const char *nodeAddr,
    uint32_t nodeAddrSize, const char *data) {
    Fam_Memory_Atomic_Put_Request req;
    Fam_Memory_Atomic_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_regionid(regionId & REGIONID_MASK);
    req.set_srcoffset(srcOffset);
    req.set_dstoffset(dstOffset);
    req.set_nbytes(nbytes);
    req.set_key(key);
    req.set_src_base_addr(srcBaseAddr);
    req.set_nodeaddr(nodeAddr, nodeAddrSize);
    req.set_nodeaddrsize(nodeAddrSize);
    if (nbytes <= MAX_DATA_IN_MSG)
        req.set_data(data, nbytes);

    ::grpc::Status status = stub->put_atomic(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_put_atomic);
}

void Fam_Memory_Service_Client::scatter_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    Fam_Memory_Atomic_SG_Strided_Request req;
    Fam_Memory_Atomic_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_regionid(regionId & REGIONID_MASK);
    req.set_offset(offset);
    req.set_nelements(nElements);
    req.set_firstelement(firstElement);
    req.set_stride(stride);
    req.set_elementsize(elementSize);
    req.set_key(key);
    req.set_src_base_addr(srcBaseAddr);
    req.set_nodeaddr(nodeAddr, nodeAddrSize);
    req.set_nodeaddrsize(nodeAddrSize);

    ::grpc::Status status = stub->scatter_strided_atomic(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_scatter_strided_atomic);
}

void Fam_Memory_Service_Client::gather_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    Fam_Memory_Atomic_SG_Strided_Request req;
    Fam_Memory_Atomic_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_regionid(regionId & REGIONID_MASK);
    req.set_offset(offset);
    req.set_nelements(nElements);
    req.set_firstelement(firstElement);
    req.set_stride(stride);
    req.set_elementsize(elementSize);
    req.set_key(key);
    req.set_src_base_addr(srcBaseAddr);
    req.set_nodeaddr(nodeAddr, nodeAddrSize);
    req.set_nodeaddrsize(nodeAddrSize);

    ::grpc::Status status = stub->gather_strided_atomic(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_gather_strided_atomic);
}

void Fam_Memory_Service_Client::scatter_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    Fam_Memory_Atomic_SG_Indexed_Request req;
    Fam_Memory_Atomic_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_regionid(regionId & REGIONID_MASK);
    req.set_offset(offset);
    req.set_nelements(nElements);
    req.set_elementindex(elementIndex, strlen((char *)elementIndex));
    req.set_elementsize(elementSize);
    req.set_key(key);
    req.set_src_base_addr(srcBaseAddr);
    req.set_nodeaddr(nodeAddr, nodeAddrSize);
    req.set_nodeaddrsize(nodeAddrSize);

    ::grpc::Status status = stub->scatter_indexed_atomic(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_scatter_indexed_atomic);
}

void Fam_Memory_Service_Client::gather_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    Fam_Memory_Atomic_SG_Indexed_Request req;
    Fam_Memory_Atomic_Response res;
    ::grpc::ClientContext ctx;

    MEMORY_SERVICE_CLIENT_PROFILE_START_OPS()
    req.set_regionid(regionId & REGIONID_MASK);
    req.set_offset(offset);
    req.set_nelements(nElements);
    req.set_elementindex(elementIndex, strlen((char *)elementIndex));
    req.set_elementsize(elementSize);
    req.set_key(key);
    req.set_src_base_addr(srcBaseAddr);
    req.set_nodeaddr(nodeAddr, nodeAddrSize);
    req.set_nodeaddrsize(nodeAddrSize);

    ::grpc::Status status = stub->gather_indexed_atomic(&ctx, req, &res);

    STATUS_CHECK(Memory_Service_Exception)
    MEMORY_SERVICE_CLIENT_PROFILE_END_OPS(mem_client_gather_indexed_atomic);
}

} // namespace openfam
