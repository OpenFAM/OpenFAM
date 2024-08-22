/*
 * fam_cis_thallium_rpc_structures.h
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
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>

using namespace std;

#ifndef FAM_CIS_THALLIUM_RPC_STRUCTURES_H
#define FAM_CIS_THALLIUM_RPC_STRUCTURES_H

#define DECL_GETTER_SETTER(name)                                               \
    void set_##name(auto value) { this->name = value; }                        \
    auto get_##name() { return this->name; }

#define DECL_VECTOR_GETTER_SETTER(name)                                        \
    void set_##name(auto *value, int size) {                                   \
        for (int i = 0; i < size; i++) {                                       \
            this->name.push_back(value[i]);                                    \
        }                                                                      \
    }                                                                          \
                                                                               \
    void set_##name(auto value) { this->name = value; }                        \
    auto get_##name() { return this->name; }

class Fam_CIS_Thallium_Request {
  private:
    uint64_t memserver_id;
    uint64_t regionid;
    uint64_t offset;
    uint32_t uid;
    uint32_t gid;
    uint64_t perm;
    string name;
    uint64_t size;
    uint32_t redundancylevel;
    uint32_t memorytype;
    uint32_t interleaveenable;
    uint32_t permissionlevel;
    string regionname;
    uint64_t key;
    uint64_t base;
    bool dup;
    bool register_memory;
    uint64_t src_region_id;
    uint64_t dest_region_id;
    uint64_t src_offset;
    uint64_t dest_offset;
    uint64_t src_copy_start;
    uint64_t dest_copy_start;
    uint64_t copy_size;
    std::vector<uint64_t> src_keys;
    std::vector<uint64_t> src_base_addr;
    uint64_t first_src_memsrv_id;
    uint64_t first_dest_memsrv_id;
    string addr;
    uint32_t addrlen;
    string bname;
    uint32_t mode;
    string diname;
    uint64_t srcoffset;
    uint64_t dstoffset;
    uint64_t nstring;
    string nodeaddr;
    uint32_t nodeaddrsize;
    uint64_t srcbaseaddr;
    string data;
    uint64_t nelements;
    uint64_t firstelement;
    uint64_t stride;
    uint64_t elementsize;
    string elementindex;
    uint64_t nbytes;
    uint64_t src_used_memsrv_cnt;
    std::vector<uint64_t> memsrv_list;
    uint32_t allocpolicy;
    uint32_t hostingnode;

  public:
    Fam_CIS_Thallium_Request() {}

    DECL_GETTER_SETTER(memserver_id)
    DECL_GETTER_SETTER(regionid)
    DECL_GETTER_SETTER(offset)
    DECL_GETTER_SETTER(uid)
    DECL_GETTER_SETTER(gid)
    DECL_GETTER_SETTER(perm)
    DECL_GETTER_SETTER(name)
    DECL_GETTER_SETTER(size)
    DECL_GETTER_SETTER(redundancylevel)
    DECL_GETTER_SETTER(memorytype)
    DECL_GETTER_SETTER(interleaveenable)
    DECL_GETTER_SETTER(permissionlevel)
    DECL_GETTER_SETTER(regionname)
    DECL_GETTER_SETTER(key)
    DECL_GETTER_SETTER(base)
    DECL_GETTER_SETTER(dup)
    DECL_GETTER_SETTER(register_memory)
    DECL_GETTER_SETTER(src_region_id)
    DECL_GETTER_SETTER(dest_region_id)
    DECL_GETTER_SETTER(src_offset)
    DECL_GETTER_SETTER(dest_offset)
    DECL_GETTER_SETTER(src_copy_start)
    DECL_GETTER_SETTER(dest_copy_start)
    DECL_GETTER_SETTER(copy_size)
    DECL_VECTOR_GETTER_SETTER(src_keys)
    DECL_VECTOR_GETTER_SETTER(src_base_addr)
    DECL_GETTER_SETTER(first_src_memsrv_id)
    DECL_GETTER_SETTER(first_dest_memsrv_id)
    DECL_GETTER_SETTER(addr)
    DECL_GETTER_SETTER(addrlen)
    DECL_GETTER_SETTER(bname)
    DECL_GETTER_SETTER(mode)
    DECL_GETTER_SETTER(diname)
    DECL_GETTER_SETTER(srcoffset)
    DECL_GETTER_SETTER(dstoffset)
    DECL_GETTER_SETTER(nstring)
    DECL_VECTOR_GETTER_SETTER(nodeaddr)
    DECL_GETTER_SETTER(nodeaddrsize)
    DECL_GETTER_SETTER(srcbaseaddr)
    DECL_VECTOR_GETTER_SETTER(data)
    DECL_GETTER_SETTER(nelements)
    DECL_GETTER_SETTER(firstelement)
    DECL_GETTER_SETTER(stride)
    DECL_GETTER_SETTER(elementsize)
    DECL_VECTOR_GETTER_SETTER(elementindex)
    DECL_GETTER_SETTER(nbytes)
    DECL_GETTER_SETTER(src_used_memsrv_cnt)
    DECL_VECTOR_GETTER_SETTER(memsrv_list)
    DECL_GETTER_SETTER(allocpolicy)
    DECL_GETTER_SETTER(hostingnode)

    template <typename A>
    friend void serialize(A &ar, Fam_CIS_Thallium_Request &m) {
        ar &m.memserver_id;
        ar &m.regionid;
        ar &m.offset;
        ar &m.uid;
        ar &m.gid;
        ar &m.perm;
        ar &m.name;
        ar &m.size;
        ar &m.redundancylevel;
        ar &m.memorytype;
        ar &m.interleaveenable;
        ar &m.permissionlevel;
        ar &m.regionname;
        ar &m.key;
        ar &m.base;
        ar &m.dup;
        ar &m.register_memory;
        ar &m.src_region_id;
        ar &m.dest_region_id;
        ar &m.src_offset;
        ar &m.dest_offset;
        ar &m.src_copy_start;
        ar &m.dest_copy_start;
        ar &m.copy_size;
        ar &m.src_keys;
        ar &m.src_base_addr;
        ar &m.first_src_memsrv_id;
        ar &m.first_dest_memsrv_id;
        ar &m.addr;
        ar &m.addrlen;
        ar &m.bname;
        ar &m.mode;
        ar &m.diname;
        ar &m.srcoffset;
        ar &m.dstoffset;
        ar &m.nstring;
        ar &m.nodeaddr;
        ar &m.nodeaddrsize;
        ar &m.srcbaseaddr;
        ar &m.data;
        ar &m.nelements;
        ar &m.firstelement;
        ar &m.stride;
        ar &m.elementsize;
        ar &m.elementindex;
        ar &m.nbytes;
        ar &m.src_used_memsrv_cnt;
        ar &m.memsrv_list;
        ar &m.allocpolicy;
        ar &m.hostingnode;
    }
};

class Fam_CIS_Thallium_Response {
  private:
    uint64_t num_memory_server;
    uint32_t errorcode;
    string errormsg;
    std::vector<uint32_t> addrname;
    uint64_t addrnamelen;
    uint64_t memserverinfo_size;
    string memserverinfo;
    uint64_t regionid;
    uint64_t offset;
    uint64_t size;
    string name;
    uint64_t maxnamelen;
    uint64_t perm;
    uint64_t memserver_id;
    uint32_t redundancylevel;
    uint32_t memorytype;
    uint32_t interleaveenable;
    uint64_t interleavesize;
    uint32_t uid;
    uint32_t gid;
    std::vector<uint64_t> memsrv_list;
    uint32_t permissionlevel;
    bool region_registration_status;
    uint64_t key;
    uint64_t base;
    std::vector<uint64_t> keys;
    std::vector<uint64_t> offsets;
    std::vector<uint64_t> base_addr_list;
    std::vector<uint64_t> memsrv_cnt;
    std::vector<uint32_t> extent_idx_list;
    uint64_t interleave_size;
    uint64_t permission_level;
    bool item_registration_status;
    uint32_t mode;
    string diname;
    string contents;
    bool status;
    uint32_t allocpolicy;

  public:
    Fam_CIS_Thallium_Response() {}

    class Region_Key_Map {

      public:
        uint64_t memserver_id;
        std::vector<uint64_t> region_keys;
        std::vector<uint64_t> region_base;
        DECL_GETTER_SETTER(memserver_id)
        DECL_VECTOR_GETTER_SETTER(region_keys)
        DECL_VECTOR_GETTER_SETTER(region_base)

        template <typename A> friend void serialize(A &ar, Region_Key_Map &p) {
            ar &p.memserver_id;
            ar &p.region_keys;
            ar &p.region_base;
        }
    };
    std::vector<Region_Key_Map> region_key_map;

    DECL_GETTER_SETTER(num_memory_server)
    DECL_GETTER_SETTER(errorcode)
    DECL_GETTER_SETTER(errormsg)
    DECL_VECTOR_GETTER_SETTER(addrname)
    DECL_GETTER_SETTER(addrnamelen)
    DECL_GETTER_SETTER(memserverinfo_size)
    DECL_VECTOR_GETTER_SETTER(memserverinfo)
    DECL_GETTER_SETTER(regionid)
    DECL_GETTER_SETTER(offset)
    DECL_GETTER_SETTER(size)
    DECL_GETTER_SETTER(name)
    DECL_GETTER_SETTER(maxnamelen)
    DECL_GETTER_SETTER(perm)
    DECL_GETTER_SETTER(memserver_id)
    DECL_GETTER_SETTER(redundancylevel)
    DECL_GETTER_SETTER(memorytype)
    DECL_GETTER_SETTER(interleaveenable)
    DECL_GETTER_SETTER(interleavesize)
    DECL_GETTER_SETTER(uid)
    DECL_GETTER_SETTER(gid)
    DECL_VECTOR_GETTER_SETTER(memsrv_list)
    DECL_GETTER_SETTER(permissionlevel)
    DECL_GETTER_SETTER(region_key_map)
    DECL_GETTER_SETTER(region_registration_status)
    DECL_GETTER_SETTER(key)
    DECL_GETTER_SETTER(base)
    DECL_VECTOR_GETTER_SETTER(keys)
    DECL_VECTOR_GETTER_SETTER(offsets)
    DECL_VECTOR_GETTER_SETTER(base_addr_list)
    DECL_VECTOR_GETTER_SETTER(memsrv_cnt)
    DECL_VECTOR_GETTER_SETTER(extent_idx_list)
    DECL_GETTER_SETTER(interleave_size)
    DECL_GETTER_SETTER(permission_level)
    DECL_GETTER_SETTER(item_registration_status)
    DECL_GETTER_SETTER(mode)
    DECL_GETTER_SETTER(diname)
    DECL_GETTER_SETTER(contents)
    DECL_GETTER_SETTER(status)
    DECL_GETTER_SETTER(allocpolicy)

    template <typename A>
    friend void serialize(A &ar, Fam_CIS_Thallium_Response &p) {
        ar &p.num_memory_server;
        ar &p.errorcode;
        ar &p.errormsg;
        ar &p.addrname;
        ar &p.addrnamelen;
        ar &p.memserverinfo_size;
        ar &p.memserverinfo;
        ar &p.regionid;
        ar &p.offset;
        ar &p.size;
        ar &p.name;
        ar &p.maxnamelen;
        ar &p.perm;
        ar &p.memserver_id;
        ar &p.redundancylevel;
        ar &p.memorytype;
        ar &p.interleaveenable;
        ar &p.interleavesize;
        ar &p.uid;
        ar &p.gid;
        ar &p.memsrv_list;
        ar &p.permissionlevel;
        ar &p.region_key_map;
        ar &p.region_registration_status;
        ar &p.key;
        ar &p.base;
        ar &p.keys;
        ar &p.offsets;
        ar &p.base_addr_list;
        ar &p.memsrv_cnt;
        ar &p.extent_idx_list;
        ar &p.interleave_size;
        ar &p.permission_level;
        ar &p.item_registration_status;
        ar &p.mode;
        ar &p.diname;
        ar &p.contents;
        ar &p.status;
        ar &p.allocpolicy;
    }
};
#endif
