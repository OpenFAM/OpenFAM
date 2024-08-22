/*
 * fam_descriptor.cpp
 * Copyright (c) 2019, 2023 Hewlett Packard Enterprise Development, LP. All
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
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "common/fam_internal.h"
#include "fam/fam.h"

using namespace std;
using namespace openfam;
/*
 * Internal implementation of Fam_Descriptor
 */
class Fam_Descriptor::FamDescriptorImpl_ {
  public:
    FamDescriptorImpl_(Fam_Global_Descriptor globalDesc, uint64_t itemSize) {
        gDescriptor = globalDesc;
        // key = FAM_KEY_UNINITIALIZED;
        keys = NULL;
        context = NULL;
        base_addr_list = NULL;
        desc_update_status = DESC_UNINITIALIZED;
        size = itemSize;
        interleaveSize = 0;
        perm = 0;
        name = NULL;
        memserver_ids = NULL;
        used_memsrv_cnt = 0;
        uid = 0;
        gid = 0;
        permissionLevel = PERMISSION_LEVEL_DEFAULT;
    }

    FamDescriptorImpl_(Fam_Global_Descriptor globalDesc) {
        gDescriptor = globalDesc;
        // key = FAM_KEY_UNINITIALIZED;
        keys = NULL;
        context = NULL;
        base_addr_list = NULL;
        desc_update_status = DESC_UNINITIALIZED;
        size = 0;
        interleaveSize = 0;
        perm = 0;
        name = NULL;
        memserver_ids = NULL;
        used_memsrv_cnt = 0;
        uid = 0;
        gid = 0;
        permissionLevel = PERMISSION_LEVEL_DEFAULT;
    }

    FamDescriptorImpl_() {
        gDescriptor = {FAM_INVALID_REGION, 0};
        // key = FAM_KEY_UNINITIALIZED;
        keys = NULL;
        context = NULL;
        base_addr_list = NULL;
        desc_update_status = DESC_UNINITIALIZED;
        size = 0;
        interleaveSize = 0;
        perm = 0;
        name = NULL;
        memserver_ids = NULL;
        used_memsrv_cnt = 0;
        uid = 0;
        gid = 0;
        permissionLevel = PERMISSION_LEVEL_DEFAULT;
    }

    ~FamDescriptorImpl_() {
        gDescriptor = {FAM_INVALID_REGION, 0};
        context = NULL;
        base_addr_list = NULL;
        desc_update_status = DESC_INVALID;
        size = 0;
        interleaveSize = 0;
        perm = 0;
        if (name)
            free(name);
        if (memserver_ids)
            free(memserver_ids);
        if (keys)
            free(keys);
        if (base_addr_list)
            free(base_addr_list);
        used_memsrv_cnt = 0;
        uid = 0;
        gid = 0;
        permissionLevel = PERMISSION_LEVEL_DEFAULT;
    }

    Fam_Global_Descriptor get_global_descriptor() { return this->gDescriptor; }

    void bind_keys(uint64_t *tempKeys, uint64_t cnt) {
        if (!keys) {
            keys = (uint64_t *)malloc(cnt * sizeof(uint64_t));
            memcpy(keys, tempKeys, sizeof(uint64_t) * cnt);
        }
        //      key = check_permissions_get_key(gDescriptor.regionID,
        //      gDescriptor.offset);
    }

    uint64_t *get_keys() { return keys; }

    void set_context(void *ctx) { context = ctx; }

    void *get_context() { return context; }

    void set_base_address_list(uint64_t *addressList, uint64_t cnt) {
        base_addr_list = (uint64_t *)malloc(cnt * sizeof(uint64_t));
        memcpy(base_addr_list, addressList, sizeof(uint64_t) * cnt);
    }

    uint64_t *get_base_address_list() { return base_addr_list; }

    void set_desc_status(int update_status) {
        desc_update_status = update_status;
    }
    int get_desc_status() { return desc_update_status; }

    void set_interleave_size(uint64_t interleaveSize_) {
        interleaveSize = interleaveSize_;
    }

    uint64_t get_interleave_size() { return interleaveSize; }

    void set_size(uint64_t itemSize) {
        if (size == 0)
            size = itemSize;
    }

    uint64_t get_size() { return size; }

    void set_perm(mode_t itemPerm) {
        if (perm == 0)
            perm = itemPerm;
    }

    mode_t get_perm() { return perm; }

    void set_name(char *itemName) {
        if (name == NULL) {
            name = (char *)malloc(RadixTree::MAX_KEY_LEN);
            memcpy(name, itemName, RadixTree::MAX_KEY_LEN);
        }
    }

    char *get_name() { return name; }

    void set_used_memsrv_cnt(uint64_t cnt) { used_memsrv_cnt = cnt; }

    uint64_t get_used_memsrv_cnt() { return used_memsrv_cnt; }

    void set_memserver_ids(uint64_t *ids) {
        memserver_ids = (uint64_t *)malloc(used_memsrv_cnt * sizeof(uint64_t));
        memcpy(memserver_ids, ids, used_memsrv_cnt * sizeof(uint64_t));
    }

    uint64_t *get_memserver_ids() { return memserver_ids; }

    uint64_t get_first_memserver_id() {
        return (gDescriptor.regionId) >> MEMSERVERID_SHIFT;
    }

    void set_uid(uint32_t uid_) { uid = uid_; }

    uint32_t get_uid() { return uid; }

    void set_gid(uint32_t gid_) { gid = gid_; }

    uint32_t get_gid() { return gid; }

    void set_permissionLevel(Fam_Permission_Level permissionLevel_) {
        permissionLevel = permissionLevel_;
    }

    Fam_Permission_Level get_permissionLevel() { return permissionLevel; }

  private:
    Fam_Global_Descriptor gDescriptor;
    /* libfabric access key*/
    uint64_t *keys;
    void *context;
    uint64_t *base_addr_list;
    int desc_update_status;
    mode_t perm;
    uint32_t uid;
    uint32_t gid;
    char *name;
    uint64_t interleaveSize;
    uint64_t size;
    uint64_t *memserver_ids;
    uint64_t used_memsrv_cnt;
    Fam_Permission_Level permissionLevel;
};

Fam_Descriptor::Fam_Descriptor(Fam_Global_Descriptor gDescriptor,
                               uint64_t itemSize) {
    fdimpl_ = new FamDescriptorImpl_(gDescriptor, itemSize);
}

Fam_Descriptor::Fam_Descriptor(Fam_Global_Descriptor gDescriptor) {
    fdimpl_ = new FamDescriptorImpl_(gDescriptor);
}

Fam_Descriptor::Fam_Descriptor() { fdimpl_ = new FamDescriptorImpl_(); }

Fam_Descriptor::~Fam_Descriptor() { delete fdimpl_; }

Fam_Global_Descriptor Fam_Descriptor::get_global_descriptor() {
    return fdimpl_->get_global_descriptor();
}

void Fam_Descriptor::bind_keys(uint64_t *tempkeys, uint64_t cnt) {
    fdimpl_->bind_keys(tempkeys, cnt);
}

uint64_t *Fam_Descriptor::get_keys() { return fdimpl_->get_keys(); }

void Fam_Descriptor::set_context(void *ctx) { fdimpl_->set_context(ctx); }

void *Fam_Descriptor::get_context() { return fdimpl_->get_context(); }

void Fam_Descriptor::set_base_address_list(uint64_t *addressList,
                                           uint64_t cnt) {
    fdimpl_->set_base_address_list(addressList, cnt);
}

uint64_t *Fam_Descriptor::get_base_address_list() {
    return fdimpl_->get_base_address_list();
}

void Fam_Descriptor::set_desc_status(int desc_update_status) {
    return fdimpl_->set_desc_status(desc_update_status);
}
int Fam_Descriptor::get_desc_status() { return fdimpl_->get_desc_status(); }

void Fam_Descriptor::set_interleave_size(uint64_t interleaveSize_) {
    return fdimpl_->set_interleave_size(interleaveSize_);
}

uint64_t Fam_Descriptor::get_interleave_size() {
    return fdimpl_->get_interleave_size();
}

void Fam_Descriptor::set_size(uint64_t itemSize) {
    return fdimpl_->set_size(itemSize);
}

uint64_t Fam_Descriptor::get_size() { return fdimpl_->get_size(); }

void Fam_Descriptor::set_perm(mode_t regionPerm) {
    fdimpl_->set_perm(regionPerm);
}

void Fam_Descriptor::set_used_memsrv_cnt(uint64_t cnt) {
    return fdimpl_->set_used_memsrv_cnt(cnt);
}

uint64_t Fam_Descriptor::get_used_memsrv_cnt() {
    return fdimpl_->get_used_memsrv_cnt();
}

void Fam_Descriptor::set_memserver_ids(uint64_t *ids) {
    return fdimpl_->set_memserver_ids(ids);
}

uint64_t *Fam_Descriptor::get_memserver_ids() {
    return fdimpl_->get_memserver_ids();
}

uint64_t Fam_Descriptor::get_first_memserver_id() {
    return fdimpl_->get_first_memserver_id();
}

mode_t Fam_Descriptor::get_perm() { return fdimpl_->get_perm(); }

void Fam_Descriptor::set_name(char *itemName) { fdimpl_->set_name(itemName); }

char *Fam_Descriptor::get_name() { return fdimpl_->get_name(); }

void Fam_Descriptor::set_uid(uint32_t uid_) { fdimpl_->set_uid(uid_); }
uint32_t Fam_Descriptor::get_uid() { return fdimpl_->get_uid(); }

void Fam_Descriptor::set_gid(uint32_t gid_) { fdimpl_->set_gid(gid_); }
uint32_t Fam_Descriptor::get_gid() { return fdimpl_->get_gid(); }

void Fam_Descriptor::set_permissionLevel(
    Fam_Permission_Level permissionLevel_) {
    fdimpl_->set_permissionLevel(permissionLevel_);
}

Fam_Permission_Level Fam_Descriptor::get_permissionLevel() {
    return fdimpl_->get_permissionLevel();
}

/*
 * Internal implementation of Fam_Region_Descriptor
 */
class Fam_Region_Descriptor::FamRegionDescriptorImpl_ {
  public:
    FamRegionDescriptorImpl_(Fam_Global_Descriptor globalDesc,
                             uint64_t regionSize) {
        gDescriptor = globalDesc;
        context = NULL;
        desc_update_status = DESC_UNINITIALIZED;
        size = regionSize;
        perm = 0;
        name = NULL;
        permissionLevel = PERMISSION_LEVEL_DEFAULT;
    }

    FamRegionDescriptorImpl_() {
        gDescriptor = {FAM_INVALID_REGION, 0};
        context = NULL;
        desc_update_status = DESC_UNINITIALIZED;
        size = 0;
        perm = 0;
        name = NULL;
        permissionLevel = PERMISSION_LEVEL_DEFAULT;
    }

    FamRegionDescriptorImpl_(Fam_Global_Descriptor globalDesc) {
        gDescriptor = globalDesc;
        context = NULL;
        desc_update_status = DESC_UNINITIALIZED;
        size = 0;
        perm = 0;
        name = NULL;
        permissionLevel = PERMISSION_LEVEL_DEFAULT;
    }

    ~FamRegionDescriptorImpl_() {
        gDescriptor = {FAM_INVALID_REGION, 0};
        context = NULL;
        desc_update_status = DESC_INVALID;
        size = 0;
        perm = 0;
        name = NULL;
        permissionLevel = PERMISSION_LEVEL_DEFAULT;
    }

    Fam_Global_Descriptor get_global_descriptor() { return this->gDescriptor; }

    void set_context(void *ctx) { context = ctx; }

    void *get_context() { return context; }

    void set_desc_status(int update_status) {
        desc_update_status = update_status;
    }
    int get_desc_status() { return desc_update_status; }

    void set_size(uint64_t regionSize) {
        if (size == 0)
            size = regionSize;
    }

    uint64_t get_size() { return size; }

    uint64_t get_memserver_id() {
        return (gDescriptor.regionId) >> MEMSERVERID_SHIFT;
    }

    void set_perm(mode_t itemPerm) {
        if (perm == 0)
            perm = itemPerm;
    }

    mode_t get_perm() { return perm; }

    void set_name(char *itemName) {
        if (name == NULL) {
            name = (char *)malloc(RadixTree::MAX_KEY_LEN);
            memcpy(name, itemName, RadixTree::MAX_KEY_LEN);
        }
    }

    char *get_name() { return name; }

    void set_redundancyLevel(Fam_Redundancy_Level reg_redundancyLevel) {
        redundancyLevel = reg_redundancyLevel;
    }

    void set_memoryType(Fam_Memory_Type reg_memoryType) {
        memoryType = reg_memoryType;
    }

    void set_interleaveEnable(Fam_Interleave_Enable reg_interleaveEnable) {
        interleaveEnable = reg_interleaveEnable;
    }

    void set_permissionLevel(Fam_Permission_Level reg_permissionLevel) {
        permissionLevel = reg_permissionLevel;
    }

    void set_allocationPolicy(Fam_Allocation_Policy reg_allocationPolicy) {
        allocationPolicy = reg_allocationPolicy;
    }

    Fam_Redundancy_Level get_redundancyLevel() { return redundancyLevel; }

    Fam_Memory_Type get_memoryType() { return memoryType; }

    Fam_Interleave_Enable get_interleaveEnable() { return interleaveEnable; }

    Fam_Permission_Level get_permissionLevel() { return permissionLevel; }

    Fam_Allocation_Policy get_allocationPolicy() { return allocationPolicy; }

  private:
    Fam_Global_Descriptor gDescriptor;
    void *context;
    int desc_update_status;
    char *name;
    uint64_t size;
    mode_t perm;
    Fam_Redundancy_Level redundancyLevel;
    Fam_Memory_Type memoryType;
    Fam_Interleave_Enable interleaveEnable;
    Fam_Permission_Level permissionLevel;
    Fam_Allocation_Policy allocationPolicy;
};

Fam_Region_Descriptor::Fam_Region_Descriptor(Fam_Global_Descriptor gDescriptor,
                                             uint64_t regionSize) {
    frdimpl_ = new FamRegionDescriptorImpl_(gDescriptor, regionSize);
}

Fam_Region_Descriptor::Fam_Region_Descriptor(
    Fam_Global_Descriptor gDescriptor) {
    frdimpl_ = new FamRegionDescriptorImpl_(gDescriptor);
}

Fam_Region_Descriptor::Fam_Region_Descriptor() {
    frdimpl_ = new FamRegionDescriptorImpl_();
}

Fam_Region_Descriptor::~Fam_Region_Descriptor() { delete frdimpl_; }

Fam_Global_Descriptor Fam_Region_Descriptor::get_global_descriptor() {
    return frdimpl_->get_global_descriptor();
}

void Fam_Region_Descriptor::set_context(void *ctx) {
    frdimpl_->set_context(ctx);
}

void *Fam_Region_Descriptor::get_context() { return frdimpl_->get_context(); }

void Fam_Region_Descriptor::set_desc_status(int desc_update_status) {
    frdimpl_->set_desc_status(desc_update_status);
}
int Fam_Region_Descriptor::get_desc_status() {
    return frdimpl_->get_desc_status();
}

void Fam_Region_Descriptor::set_size(uint64_t regionSize) {
    frdimpl_->set_size(regionSize);
}

uint64_t Fam_Region_Descriptor::get_size() { return frdimpl_->get_size(); }

void Fam_Region_Descriptor::set_perm(mode_t regionPerm) {
    frdimpl_->set_perm(regionPerm);
}

mode_t Fam_Region_Descriptor::get_perm() { return frdimpl_->get_perm(); }

void Fam_Region_Descriptor::set_name(char *regionName) {
    frdimpl_->set_name(regionName);
}

char *Fam_Region_Descriptor::get_name() { return frdimpl_->get_name(); }

uint64_t Fam_Region_Descriptor::get_memserver_id() {
    return frdimpl_->get_memserver_id();
}

void Fam_Region_Descriptor::set_redundancyLevel(
    Fam_Redundancy_Level redundancyLevel) {
    frdimpl_->set_redundancyLevel(redundancyLevel);
}
void Fam_Region_Descriptor::set_memoryType(Fam_Memory_Type memoryType) {
    frdimpl_->set_memoryType(memoryType);
}
void Fam_Region_Descriptor::set_interleaveEnable(
    Fam_Interleave_Enable interleaveEnable) {
    frdimpl_->set_interleaveEnable(interleaveEnable);
}

void Fam_Region_Descriptor::set_permissionLevel(
    Fam_Permission_Level permissionLevel) {
    frdimpl_->set_permissionLevel(permissionLevel);
}

void Fam_Region_Descriptor::set_allocationPolicy(
    Fam_Allocation_Policy allocationPolicy) {
    frdimpl_->set_allocationPolicy(allocationPolicy);
}

Fam_Redundancy_Level Fam_Region_Descriptor::get_redundancyLevel() {
    return frdimpl_->get_redundancyLevel();
}
Fam_Memory_Type Fam_Region_Descriptor::get_memoryType() {
    return frdimpl_->get_memoryType();
}
Fam_Interleave_Enable Fam_Region_Descriptor::get_interleaveEnable() {
    return frdimpl_->get_interleaveEnable();
}

Fam_Permission_Level Fam_Region_Descriptor::get_permissionLevel() {
    return frdimpl_->get_permissionLevel();
}

Fam_Allocation_Policy Fam_Region_Descriptor::get_allocationPolicy() {
    return frdimpl_->get_allocationPolicy();
}