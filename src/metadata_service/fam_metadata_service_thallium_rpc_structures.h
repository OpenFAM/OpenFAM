/*
 * fam_metadata_service_thallium_rpc_structures.h
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

#ifndef FAM_METADATA_SERVICE_THALLIUM_RPC_STRUCTURES_H
#define FAM_METADATA_SERVICE_THALLIUM_RPC_STRUCTURES_H

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

class Fam_Metadata_Thallium_Request {
  private:
    uint64_t key_region_id;
    uint64_t key_dataitem_id;
    string key_region_name;
    string key_dataitem_name;
    uint64_t region_id;
    string name;
    uint64_t offset;
    std::vector<uint64_t> offsets;
    uint32_t uid;
    uint32_t uid_in;
    uint32_t uid_meta;
    uint32_t gid;
    uint32_t gid_in;
    uint32_t gid_meta;
    uint64_t perm;
    uint64_t size;
    uint32_t user_policy;
    uint64_t memsrv_cnt;
    std::vector<uint64_t> memsrv_list;
    uint64_t memsrv_id;
    uint32_t op;
    uint32_t ops;
    uint32_t redundancylevel;
    uint32_t memorytype;
    uint32_t interleaveenable;
    uint64_t interleavesize;
    uint64_t interleave_size;
    uint32_t nmemserverspersistent;
    std::vector<uint64_t> memsrv_persistent_list;
    uint32_t nmemserversvolatile;
    std::vector<uint64_t> memsrv_volatile_list;
    bool check_name_id;
    uint64_t type_flag;
    uint64_t permission_level;

  public:
    Fam_Metadata_Thallium_Request() {}

    DECL_GETTER_SETTER(key_region_id)
    DECL_GETTER_SETTER(key_dataitem_id)
    DECL_GETTER_SETTER(key_region_name)
    DECL_GETTER_SETTER(key_dataitem_name)
    DECL_GETTER_SETTER(region_id)
    DECL_GETTER_SETTER(name)
    DECL_GETTER_SETTER(offset)
    DECL_VECTOR_GETTER_SETTER(offsets)
    DECL_GETTER_SETTER(uid)
    DECL_GETTER_SETTER(uid_in)
    DECL_GETTER_SETTER(uid_meta)
    DECL_GETTER_SETTER(gid)
    DECL_GETTER_SETTER(gid_in)
    DECL_GETTER_SETTER(gid_meta)
    DECL_GETTER_SETTER(perm)
    DECL_GETTER_SETTER(size)
    DECL_GETTER_SETTER(user_policy)
    DECL_GETTER_SETTER(memsrv_cnt)
    DECL_VECTOR_GETTER_SETTER(memsrv_list)
    DECL_GETTER_SETTER(memsrv_id)
    DECL_GETTER_SETTER(op)
    DECL_GETTER_SETTER(ops)
    DECL_GETTER_SETTER(redundancylevel)
    DECL_GETTER_SETTER(memorytype)
    DECL_GETTER_SETTER(interleaveenable)
    DECL_GETTER_SETTER(interleavesize)
    DECL_GETTER_SETTER(interleave_size)
    DECL_GETTER_SETTER(nmemserverspersistent)
    DECL_GETTER_SETTER(memsrv_persistent_list)
    DECL_GETTER_SETTER(nmemserversvolatile)
    DECL_GETTER_SETTER(memsrv_volatile_list)
    DECL_GETTER_SETTER(check_name_id)
    DECL_GETTER_SETTER(type_flag)
    DECL_GETTER_SETTER(permission_level)

    template <typename A>
    friend void serialize(A &ar, Fam_Metadata_Thallium_Request &m) {
        ar &m.key_region_id;
        ar &m.key_dataitem_id;
        ar &m.key_region_name;
        ar &m.key_dataitem_name;
        ar &m.region_id;
        ar &m.name;
        ar &m.offset;
        ar &m.offsets;
        ar &m.uid;
        ar &m.uid_in;
        ar &m.uid_meta;
        ar &m.gid;
        ar &m.gid_in;
        ar &m.gid_meta;
        ar &m.perm;
        ar &m.size;
        ar &m.user_policy;
        ar &m.memsrv_cnt;
        ar &m.memsrv_list;
        ar &m.memsrv_id;
        ar &m.op;
        ar &m.ops;
        ar &m.redundancylevel;
        ar &m.memorytype;
        ar &m.interleaveenable;
        ar &m.interleavesize;
        ar &m.interleave_size;
        ar &m.nmemserverspersistent;
        ar &m.memsrv_persistent_list;
        ar &m.nmemserversvolatile;
        ar &m.memsrv_volatile_list;
        ar &m.check_name_id;
        ar &m.type_flag;
        ar &m.permission_level;
    }
};

class Fam_Metadata_Thallium_Response {
  private:
    uint64_t region_id;
    string name;
    uint64_t offset;
    std::vector<uint64_t> offsets;
    uint32_t uid;
    uint32_t gid;
    uint64_t perm;
    uint64_t size;
    uint64_t maxkeylen;
    bool isfound;
    bool is_permitted;
    uint64_t memsrv_id;
    uint64_t interleave_size;
    std::vector<uint64_t> memsrv_list;
    uint64_t memsrv_cnt;
    uint32_t redundancylevel;
    uint32_t memorytype;
    uint32_t interleaveenable;
    uint64_t interleavesize;
    std::vector<uint32_t> addrname;
    uint64_t addrnamelen;
    uint64_t permission_level;
    uint64_t region_permission;

  public:
    Fam_Metadata_Thallium_Response() {}

    uint32_t errorcode;
    string errormsg;
    bool status;

    DECL_GETTER_SETTER(region_id)
    DECL_GETTER_SETTER(name)
    DECL_GETTER_SETTER(offset)
    DECL_VECTOR_GETTER_SETTER(offsets)
    DECL_GETTER_SETTER(uid)
    DECL_GETTER_SETTER(gid)
    DECL_GETTER_SETTER(perm)
    DECL_GETTER_SETTER(size)
    DECL_GETTER_SETTER(maxkeylen)
    DECL_GETTER_SETTER(isfound)
    DECL_GETTER_SETTER(is_permitted)
    DECL_GETTER_SETTER(errorcode)
    DECL_GETTER_SETTER(errormsg)
    DECL_GETTER_SETTER(status)
    DECL_GETTER_SETTER(memsrv_id)
    DECL_GETTER_SETTER(interleave_size)
    DECL_VECTOR_GETTER_SETTER(memsrv_list)
    DECL_GETTER_SETTER(memsrv_cnt)
    DECL_GETTER_SETTER(redundancylevel)
    DECL_GETTER_SETTER(memorytype)
    DECL_GETTER_SETTER(interleaveenable)
    DECL_GETTER_SETTER(interleavesize)
    DECL_VECTOR_GETTER_SETTER(addrname)
    DECL_GETTER_SETTER(addrnamelen)
    DECL_GETTER_SETTER(permission_level)
    DECL_GETTER_SETTER(region_permission)

    template <typename A>
    friend void serialize(A &ar, Fam_Metadata_Thallium_Response &p) {
        ar &p.region_id;
        ar &p.name;
        ar &p.offset;
        ar &p.offsets;
        ar &p.uid;
        ar &p.gid;
        ar &p.perm;
        ar &p.size;
        ar &p.maxkeylen;
        ar &p.isfound;
        ar &p.is_permitted;
        ar &p.errorcode;
        ar &p.errormsg;
        ar &p.status;
        ar &p.memsrv_id;
        ar &p.interleave_size;
        ar &p.memsrv_list;
        ar &p.memsrv_cnt;
        ar &p.redundancylevel;
        ar &p.memorytype;
        ar &p.interleaveenable;
        ar &p.interleavesize;
        ar &p.addrname;
        ar &p.addrnamelen;
        ar &p.permission_level;
        ar &p.region_permission;
    }
};
#endif
