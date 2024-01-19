/*
 * fam_client_resource_manager.h
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

#ifndef FAM_CLIENT_RESOURCE_MANAGER_H
#define FAM_CLIENT_RESOURCE_MANAGER_H

#include <boost/lockfree/queue.hpp>
#include <iostream>
#include <map>
#include <thread>
#include <unistd.h>

#include "cis/fam_cis.h"
#include "common/fam_internal.h"
#include "common/fam_internal_exception.h"
#include "common/fam_options.h"

using namespace std;

namespace openfam {
typedef struct {
    // An 64 bit atomic value to store status and reference count
    std::atomic<uint64_t> statusAndRefcount;
    // A vector of memory server IDs across which the region is spread.
    std::vector<uint64_t> memserverIds;
    // A memory map of a region(memory server ID to Fam_Region_Memory strcture)
    Fam_Region_Memory_Map memoryMap;
    // Region lock
    pthread_rwlock_t famRegionLock;
} Fam_Client_Resource;

using Fam_Resource_Table_Itr =
    std::unordered_map<uint64_t, Fam_Client_Resource *>::iterator;
class Fam_Client_Resource_Manager {
  public:
    Fam_Client_Resource_Manager(Fam_CIS *famCIS);
    ~Fam_Client_Resource_Manager();
    Fam_Client_Resource *find_resource(uint64_t regionId);
    Fam_Client_Resource *open_resource(uint64_t regionId,
                                       Fam_Permission_Level permissionLevel,
                                       uint32_t uid, uint32_t gid, int flag);
    void close_resource(uint64_t regionId);
    void close_all_resources();

  private:
    bool release_resource(uint64_t regionId, Fam_Client_Resource *famResource);
    Fam_Client_Resource *
    find_or_create_resource(uint64_t regionId,
                            Fam_Permission_Level permissionLevel, uint32_t uid,
                            uint32_t gid);
    void open_region_remote(uint64_t regionId, Fam_Client_Resource *famResource,
                            Fam_Permission_Level permissionLevel, uint32_t uid,
                            uint32_t gid);
    void open_region_remote(uint64_t regionId,
                            Fam_Client_Resource *famResource);
    pthread_rwlock_t famResourceTableLock;
    // A clientresource table is a map between region ID to client side resource
    // structure Fam_Client_Resource.
    std::unordered_map<uint64_t, Fam_Client_Resource *> *famResourceTable;
    // A garbage queue prevents crashes while refering stale resource entries
    boost::lockfree::queue<Fam_Client_Resource *> *famClientResourceGarbageQ;
    Fam_CIS *famCIS;
};
} // namespace openfam
#endif
