/*
 * fam_descriptor.cpp
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
        key = FAM_KEY_UNINITIALIZED;
        context = NULL;
        base = NULL;
        desc_update_status = DESC_UNINITIALIZED;
        size = itemSize;
        perm = 0;
        name = NULL;
    }

    FamDescriptorImpl_(Fam_Global_Descriptor globalDesc) {
        gDescriptor = globalDesc;
        key = FAM_KEY_UNINITIALIZED;
        context = NULL;
        base = NULL;
        desc_update_status = DESC_UNINITIALIZED;
        size = 0;
        perm = 0;
        name = NULL;
    }

    FamDescriptorImpl_() {
        gDescriptor = {FAM_INVALID_REGION, 0};
        key = FAM_KEY_UNINITIALIZED;
        context = NULL;
        base = NULL;
        desc_update_status = DESC_UNINITIALIZED;
        size = 0;
        perm = 0;
        name = NULL;
    }

    ~FamDescriptorImpl_() {
        gDescriptor = {FAM_INVALID_REGION, 0};
        key = FAM_KEY_UNINITIALIZED;
        context = NULL;
        base = NULL;
        desc_update_status = DESC_INVALID;
        size = 0;
        perm = 0;
        name = NULL;
    }

    Fam_Global_Descriptor get_global_descriptor() { return this->gDescriptor; }

    void bind_key(uint64_t tempKey) {
        if (key == FAM_KEY_UNINITIALIZED)
            key = tempKey;
        //      key = check_permissions_get_key(gDescriptor.regionID,
        //      gDescriptor.offset);
    }

    uint64_t get_key() { return key; }

    void set_context(void *ctx) { context = ctx; }

    void *get_context() { return context; }

    void set_base_address(void *address) { base = address; }

    void *get_base_address() { return base; }

    void set_desc_status(int update_status) {
        desc_update_status = update_status;
    }
    int get_desc_status() { return desc_update_status; }

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
        if (name == NULL)
            name = itemName;
    }

    char *get_name() { return name; }

    uint64_t get_memserver_id() {
        return (gDescriptor.regionId) >> MEMSERVERID_SHIFT;
    }

  private:
    Fam_Global_Descriptor gDescriptor;
    /* libfabric access key*/
    uint64_t key;
    void *context;
    void *base;
    int desc_update_status;
    mode_t perm;
    char *name;
    uint64_t size;
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

void Fam_Descriptor::bind_key(uint64_t tempkey) { fdimpl_->bind_key(tempkey); }

uint64_t Fam_Descriptor::get_key() { return fdimpl_->get_key(); }

void Fam_Descriptor::set_context(void *ctx) { fdimpl_->set_context(ctx); }

void *Fam_Descriptor::get_context() { return fdimpl_->get_context(); }

void Fam_Descriptor::set_base_address(void *address) {
    fdimpl_->set_base_address(address);
}

void *Fam_Descriptor::get_base_address() { return fdimpl_->get_base_address(); }

void Fam_Descriptor::set_desc_status(int desc_update_status) {
    return fdimpl_->set_desc_status(desc_update_status);
}
int Fam_Descriptor::get_desc_status() { return fdimpl_->get_desc_status(); }

void Fam_Descriptor::set_size(uint64_t itemSize) {
    return fdimpl_->set_size(itemSize);
}

uint64_t Fam_Descriptor::get_size() { return fdimpl_->get_size(); }

uint64_t Fam_Descriptor::get_memserver_id() {
    return fdimpl_->get_memserver_id();
}
void Fam_Descriptor::set_perm(mode_t regionPerm) {
    fdimpl_->set_perm(regionPerm);
}

mode_t Fam_Descriptor::get_perm() { return fdimpl_->get_perm(); }

void Fam_Descriptor::set_name(char *itemName) { fdimpl_->set_name(itemName); }

char *Fam_Descriptor::get_name() { return fdimpl_->get_name(); }

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
    }

    FamRegionDescriptorImpl_() {
        gDescriptor = {FAM_INVALID_REGION, 0};
        context = NULL;
        desc_update_status = DESC_UNINITIALIZED;
        size = 0;
        perm = 0;
        name = NULL;
    }

    FamRegionDescriptorImpl_(Fam_Global_Descriptor globalDesc) {
        gDescriptor = globalDesc;
        context = NULL;
        desc_update_status = DESC_UNINITIALIZED;
        size = 0;
        perm = 0;
        name = NULL;
    }

    ~FamRegionDescriptorImpl_() {
        gDescriptor = {FAM_INVALID_REGION, 0};
        context = NULL;
        desc_update_status = DESC_INVALID;
        size = 0;
        perm = 0;
        name = NULL;
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
        if (name == 0)
            name = itemName;
    }

    char *get_name() { return name; }

  private:
    Fam_Global_Descriptor gDescriptor;
    void *context;
    int desc_update_status;
    char *name;
    uint64_t size;
    mode_t perm;
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
