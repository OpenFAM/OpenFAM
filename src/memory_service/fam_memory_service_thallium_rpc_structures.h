/*
 * fam_memory_service_thallium_rpc_structures.h
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

#include <iostream>
#include <string>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>

using namespace std;

#ifndef FAM_MEMORY_SERVICE_THALLIUM_RPC_STRUCTURES_H
#define FAM_MEMORY_SERVICE_THALLIUM_RPC_STRUCTURES_H

#define DECL_GETTER_SETTER(name)                                               \
    void set_##name(auto value) { this->name = value; }                        \
    auto get_##name() { return this->name; }

#define DECL_VECTOR_GETTER_SETTER(name)                                        \
    void set_##name(auto *value, int size) {                                   \
        for (int i = 0; i < size; i++) {                                       \
            this->name.push_back(value[i]);                                    \
        }                                                                      \
    }                                                                          \
    auto get_##name() { return this->name; }

class Fam_Memory_Service_Thallium_Request {
  private:
    uint64_t region_id;
    uint64_t offset;
    bool rw_flag;
    uint64_t size;
    uint64_t src_region_id;
    uint64_t dest_region_id;
    std::vector<uint64_t> src_offsets;
    std::vector<uint64_t> src_keys;
    std::vector<uint64_t> src_ids;
    std::vector<uint64_t> src_base_addr_list;
    uint64_t dest_offset;
    uint64_t src_copy_start;
    uint64_t src_copy_end;
    uint64_t src_used_memsrv_cnt;
    uint64_t dest_used_memsrv_cnt;
    uint64_t src_interleave_size;
    uint64_t dest_interleave_size;
    uint64_t key;
    std::vector<uint64_t> addr;
    uint32_t addr_len;
    uint64_t memserver_id;
    string bname;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    string diname;
    uint64_t chunk_size;
    uint64_t used_memserver_cnt;
    uint64_t file_start_pos;
    bool write_metadata;
    uint64_t item_size;
    uint64_t srcoffset;
    uint64_t dstoffset;
    uint64_t nbytes;
    string nodeaddr;
    uint32_t nodeaddrsize;
    uint64_t src_base_addr;
    string data;
    uint64_t nelements;
    string elementindex;
    uint64_t elementsize;
    uint64_t firstelement;
    uint64_t stride;
    uint64_t memserverinfo_size;
    string memserverinfo;
    uint64_t num_memservers;
    bool unregister_memory;

  public:
    Fam_Memory_Service_Thallium_Request() {}

    DECL_GETTER_SETTER(region_id)
    DECL_GETTER_SETTER(offset)
    DECL_GETTER_SETTER(rw_flag)
    DECL_GETTER_SETTER(size)
    DECL_GETTER_SETTER(src_region_id)
    DECL_GETTER_SETTER(dest_region_id)
    DECL_VECTOR_GETTER_SETTER(src_offsets)
    DECL_VECTOR_GETTER_SETTER(src_keys)
    DECL_VECTOR_GETTER_SETTER(src_ids)
    DECL_VECTOR_GETTER_SETTER(src_base_addr_list)
    DECL_GETTER_SETTER(dest_offset)
    DECL_GETTER_SETTER(src_copy_start)
    DECL_GETTER_SETTER(src_copy_end)
    DECL_GETTER_SETTER(src_used_memsrv_cnt)
    DECL_GETTER_SETTER(dest_used_memsrv_cnt)
    DECL_GETTER_SETTER(src_interleave_size)
    DECL_GETTER_SETTER(dest_interleave_size)
    DECL_GETTER_SETTER(key)
    DECL_VECTOR_GETTER_SETTER(addr)
    DECL_GETTER_SETTER(addr_len)
    DECL_GETTER_SETTER(memserver_id)
    DECL_GETTER_SETTER(bname)
    DECL_GETTER_SETTER(uid)
    DECL_GETTER_SETTER(gid)
    DECL_GETTER_SETTER(mode)
    DECL_GETTER_SETTER(diname)
    DECL_GETTER_SETTER(chunk_size)
    DECL_GETTER_SETTER(used_memserver_cnt)
    DECL_GETTER_SETTER(file_start_pos)
    DECL_GETTER_SETTER(write_metadata)
    DECL_GETTER_SETTER(item_size)
    DECL_GETTER_SETTER(srcoffset)
    DECL_GETTER_SETTER(dstoffset)
    DECL_GETTER_SETTER(nbytes)
    DECL_VECTOR_GETTER_SETTER(nodeaddr)
    DECL_GETTER_SETTER(nodeaddrsize)
    DECL_GETTER_SETTER(src_base_addr)
    DECL_VECTOR_GETTER_SETTER(data)
    DECL_GETTER_SETTER(nelements)
    DECL_VECTOR_GETTER_SETTER(elementindex)
    DECL_GETTER_SETTER(elementsize)
    DECL_GETTER_SETTER(firstelement)
    DECL_GETTER_SETTER(stride)
    DECL_GETTER_SETTER(memserverinfo_size)
    DECL_VECTOR_GETTER_SETTER(memserverinfo)
    DECL_GETTER_SETTER(num_memservers)
    DECL_GETTER_SETTER(unregister_memory)

    template <typename A>
    friend void serialize(A &ar, Fam_Memory_Service_Thallium_Request &m) {
        ar &m.region_id;
        ar &m.offset;
        ar &m.rw_flag;
        ar &m.size;
        ar &m.src_region_id;
        ar &m.dest_region_id;
        ar &m.src_offsets;
        ar &m.src_keys;
        ar &m.src_ids;
        ar &m.src_base_addr_list;
        ar &m.dest_offset;
        ar &m.src_copy_start;
        ar &m.src_copy_end;
        ar &m.src_used_memsrv_cnt;
        ar &m.dest_used_memsrv_cnt;
        ar &m.src_interleave_size;
        ar &m.dest_interleave_size;
        ar &m.key;
        ar &m.addr;
        ar &m.addr_len;
        ar &m.memserver_id;
        ar &m.bname;
        ar &m.uid;
        ar &m.gid;
        ar &m.mode;
        ar &m.diname;
        ar &m.chunk_size;
        ar &m.used_memserver_cnt;
        ar &m.file_start_pos;
        ar &m.write_metadata;
        ar &m.item_size;
        ar &m.srcoffset;
        ar &m.dstoffset;
        ar &m.nbytes;
        ar &m.nodeaddr;
        ar &m.nodeaddrsize;
        ar &m.src_base_addr;
        ar &m.data;
        ar &m.nelements;
        ar &m.elementindex;
        ar &m.elementsize;
        ar &m.firstelement;
        ar &m.stride;
        ar &m.memserverinfo_size;
        ar &m.memserverinfo;
        ar &m.num_memservers;
        ar &m.unregister_memory;
    }
};

class Fam_Memory_Service_Thallium_Response {
  private:
    std::vector<uint32_t> addrname;
    uint64_t addrnamelen;
    uint32_t memorytype;
    uint64_t region_id;
    uint64_t offset;
    uint64_t key;
    uint64_t base;
    uint64_t size;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    string name;
    string contents;
    std::vector<uint64_t> region_keys;
    std::vector<uint64_t> region_base;
    uint64_t resource_status;

  public:
    Fam_Memory_Service_Thallium_Response() {}

    uint32_t errorcode;
    string errormsg;
    bool status;
    DECL_GETTER_SETTER(errorcode)
    DECL_GETTER_SETTER(errormsg)
    DECL_GETTER_SETTER(status)
    DECL_VECTOR_GETTER_SETTER(addrname)
    DECL_GETTER_SETTER(addrnamelen)
    DECL_GETTER_SETTER(memorytype)
    DECL_GETTER_SETTER(region_id)
    DECL_GETTER_SETTER(offset)
    DECL_GETTER_SETTER(key)
    DECL_GETTER_SETTER(base)
    DECL_GETTER_SETTER(size)
    DECL_GETTER_SETTER(uid)
    DECL_GETTER_SETTER(gid)
    DECL_GETTER_SETTER(mode)
    DECL_GETTER_SETTER(name)
    DECL_GETTER_SETTER(contents)
    DECL_GETTER_SETTER(region_keys)
    DECL_GETTER_SETTER(region_base)
    DECL_GETTER_SETTER(resource_status)

    template <typename A>
    friend void serialize(A &ar, Fam_Memory_Service_Thallium_Response &p) {
        ar &p.errorcode;
        ar &p.errormsg;
        ar &p.status;
        ar &p.addrname;
        ar &p.addrnamelen;
        ar &p.memorytype;
        ar &p.region_id;
        ar &p.offset;
        ar &p.key;
        ar &p.base;
        ar &p.size;
        ar &p.uid;
        ar &p.gid;
        ar &p.mode;
        ar &p.name;
        ar &p.contents;
        ar &p.region_keys;
        ar &p.region_base;
        ar &p.resource_status;
    }
};

#endif
