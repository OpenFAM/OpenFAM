/*
 * fam_server_resource_manager.h
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

#ifndef FAM_SERVER_RESOURCE_MANAGER_H
#define FAM_SERVER_RESOURCE_MANAGER_H

#include <boost/lockfree/queue.hpp>
#include <iostream>
#include <map>
#include <thread>
#include <unistd.h>

#include "bitmap-manager/bitmap.h"
#include "common/fam_internal.h"
#include "common/fam_internal_exception.h"
#include "common/fam_libfabric.h"
#include "common/fam_ops_libfabric.h"
#include "common/fam_options.h"

#include <boost/atomic.hpp>

using namespace std;

namespace openfam {

class Fam_Ops_Libfabric;

// Structure to represent each registration within the region(region extent or
// dataitem)
typedef struct {
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
    std::atomic<bool> deallocated;
#endif
    fid_mr *mr;
} Fam_Memory_Registration;

// Structure to manage resource on server side
typedef struct {
    std::atomic<uint64_t> statusAndRefcount;
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
    std::atomic<bool> destroyed;
#endif
    bool accessType;
    Fam_Permission_Level permissionLevel;
    std::map<uint64_t, Fam_Memory_Registration *> *famRegistrationTable;
    pthread_rwlock_t famRegionLock;
} Fam_Server_Resource;

class Fam_Server_Resource_Manager {
  public:
    Fam_Server_Resource_Manager(Memserver_Allocator *allocator,
                                bool enableResourceRelease,
                                bool isSharedMemory = true,
                                Fam_Ops_Libfabric *famOps = NULL);
    ~Fam_Server_Resource_Manager();
    void reset_profile();
    void dump_profile();
    Fam_Server_Resource *find_resource(uint64_t regionId);
    Fam_Server_Resource *open_resource(uint64_t regionId,
                                       Fam_Permission_Level permissionLevel,
                                       bool accessType, int flag);
    uint64_t close_resource(uint64_t regionId);
    uint64_t register_memory(uint64_t regionId, uint64_t registrationId,
                             void *base, uint64_t size, bool rwFlag,
                             Fam_Server_Resource *famResource);
    void unregister_memory(uint64_t regionId, uint64_t registrationId,
                           Fam_Server_Resource *famResource);
    void register_region_memory(uint64_t regionId, bool accessType,
                                Fam_Server_Resource *famResource = NULL);
    void unregister_region_memory(Fam_Server_Resource *famResource);
    Fam_Region_Memory
    get_region_memory_info(uint64_t regionId, bool accessType,
                           Fam_Server_Resource *famResource = NULL);
    Fam_Dataitem_Memory
    get_dataitem_memory_info(uint64_t regionId, uint64_t offset, uint64_t size,
                             bool accessType,
                             Fam_Server_Resource *famResource = NULL);
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
    void mark_deallocated(uint64_t regionId, uint64_t registrationId,
                          Fam_Server_Resource *famResource);
#endif
  private:
    Fam_Server_Resource *
    find_or_create_resource(uint64_t regionId,
                            Fam_Permission_Level permissionLevel,
                            bool accessType);
    uint64_t generate_access_key(uint64_t regionId, uint64_t dataitemId,
                                 bool permission);
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
    uint64_t get_offset_from_key(uint64_t key);
    uint64_t get_region_id_from_key(uint64_t key);
#endif
    void register_fence_memory();

    void unregister_fence_memory();

    void init_key_bitmap();

    uint64_t get_key_from_bitmap();

    Fam_Ops_Libfabric *famOps;
    Memserver_Allocator *allocator;
    pthread_rwlock_t famResourceTableLock;
    std::map<uint64_t, Fam_Server_Resource *> *famResourceTable;
    boost::lockfree::queue<Fam_Server_Resource *> *famServerResourceGarbageQ;
    fid_mr *fenceMr;
    bitmap *keyMap;
    bool isSharedMemory;
    bool isBaseRequire;
    bool enableResourceRelease;
};
} // namespace openfam
#endif
