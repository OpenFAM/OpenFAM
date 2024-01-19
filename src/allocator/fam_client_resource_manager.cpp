/*
 * fam_client_resource_manager.cpp
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

#include "allocator/fam_client_resource_manager.h"

using namespace std;
namespace openfam {

Fam_Client_Resource_Manager::Fam_Client_Resource_Manager(Fam_CIS *famCIS) {
    this->famCIS = famCIS;
    famResourceTable =
        new std::unordered_map<uint64_t, Fam_Client_Resource *>();
    famClientResourceGarbageQ =
        new boost::lockfree::queue<Fam_Client_Resource *>(MAX_GARBAGE_ENTRY);

    pthread_rwlock_init(&famResourceTableLock, NULL);
}

Fam_Client_Resource_Manager::~Fam_Client_Resource_Manager() {
    while (!famClientResourceGarbageQ->empty()) {
        Fam_Client_Resource *famResource = NULL;
        famClientResourceGarbageQ->pop(famResource);
        if (famResource)
            delete famResource;
    }
}

/*
 * This function creates a new Fam_Client_Resource structure or returns an
 * existing valid Fam_Client_Resource structure for a requested region. Also, if
 * there is a invalid stale entry, it is removed and a fresh entry is added.
 */
Fam_Client_Resource *Fam_Client_Resource_Manager::find_or_create_resource(
    uint64_t regionId, Fam_Permission_Level permissionLevel, uint32_t uid,
    uint32_t gid) {
    Fam_Client_Resource *famResourceOld;

    // Start by taking a readlock on region table
    pthread_rwlock_rdlock(&famResourceTableLock);
    auto regionObj = famResourceTable->find(regionId);
    if (regionObj != famResourceTable->end()) {
        famResourceOld = regionObj->second;
        uint64_t readValue = ATOMIC_READ(&famResourceOld->statusAndRefcount);
        uint64_t status = GET_STATUS(readValue);
        // If the existing entry is valid, return it.
        if (status != RELEASED) {
            pthread_rwlock_unlock(&famResourceTableLock);
            return famResourceOld;
        }
    }

    pthread_rwlock_unlock(&famResourceTableLock);

    // Create a new Fam_Client_Resource entry for requested region.
    Fam_Client_Resource *famResourceTemp = new Fam_Client_Resource();
    // When a new wntry is added, it is introduced in INACTIVE state with
    // reference count 0.
    famResourceTemp->statusAndRefcount = CONCAT_STATUS_REFCNT(INACTIVE, 0);
    pthread_rwlock_init(&famResourceTemp->famRegionLock, NULL);

    // Acquire write lock on resource table
    pthread_rwlock_wrlock(&famResourceTableLock);
    // Make sure no other thread has already added the entry
    regionObj = famResourceTable->find(regionId);
    if (regionObj == famResourceTable->end()) {
        famResourceTable->insert({regionId, famResourceTemp});
        pthread_rwlock_unlock(&famResourceTableLock);
        return famResourceTemp;
    } else {
        // If there exist an entry, check its status.
        famResourceOld = regionObj->second;
        uint64_t readValue = ATOMIC_READ(&famResourceOld->statusAndRefcount);
        uint64_t status = GET_STATUS(readValue);
        // If the status is RELEASED, replace the stale entry with new entry
        if (status == RELEASED) {
            famResourceTable->erase(regionObj);
            famClientResourceGarbageQ->push(famResourceOld);
            famResourceTable->insert({regionId, famResourceTemp});
            pthread_rwlock_unlock(&famResourceTableLock);
            return famResourceTemp;
        }
        pthread_rwlock_unlock(&famResourceTableLock);
        delete famResourceTemp;
        return famResourceOld;
    }
}

/*
 * This function look for the entry for requested region
 */
Fam_Client_Resource *
Fam_Client_Resource_Manager::find_resource(uint64_t regionId) {
    Fam_Client_Resource *famResource = NULL;
    // Start by taking a readlock on resource table
    pthread_rwlock_rdlock(&famResourceTableLock);
    auto regionObj = famResourceTable->find(regionId);
    if (regionObj != famResourceTable->end()) {
        famResource = regionObj->second;
    }
    // Release lock on resource table
    pthread_rwlock_unlock(&famResourceTableLock);
    return famResource;
}

/*
 * This function open the resources of the region in memory server
 */
void Fam_Client_Resource_Manager::open_region_remote(
    uint64_t regionId, Fam_Client_Resource *famResource,
    Fam_Permission_Level permissionLevel, uint32_t uid, uint32_t gid) {
    ostringstream message;
    // Get the region memory map from memory server
    Fam_Region_Memory_Map regionMemoryMap;
    std::vector<uint64_t> memserverIds;
    // If permission level of the region is set at region level, the entire
    // region memory is registered at once. The memory registration information
    // is fetched from the memory server when the region is referenced for the
    // first time and this information is stored in the form of
    // Fam_Region_Memory_Map locally.
    try {
        famCIS->open_region_with_registration(regionId, uid, gid, &memserverIds,
                                              &regionMemoryMap);
        famResource->memserverIds = memserverIds;
        famResource->memoryMap = regionMemoryMap;
    } catch (Fam_Exception &e) {
        // In case of exception change the status to RELEASED
        CHANGE_STATUS_REFCNT(&famResource->statusAndRefcount, RELEASED, 0);
        throw e;
    } catch (...) {
        CHANGE_STATUS_REFCNT(&famResource->statusAndRefcount, RELEASED, 0);
        message << "Failed to open the region in memory server";
        THROW_ERRNO_MSG(Fam_Allocator_Exception, FAM_ERR_RESOURCE,
                        message.str().c_str());
    }
}

void Fam_Client_Resource_Manager::open_region_remote(
    uint64_t regionId, Fam_Client_Resource *famResource) {
    ostringstream message;
    std::vector<uint64_t> memserverIds;
    try {
        famCIS->open_region_without_registration(regionId, &memserverIds);
        famResource->memserverIds = memserverIds;
    } catch (Fam_Exception &e) {
        CHANGE_STATUS_REFCNT(&famResource->statusAndRefcount, RELEASED, 0);
        throw e;
    } catch (...) {
        CHANGE_STATUS_REFCNT(&famResource->statusAndRefcount, RELEASED, 0);
        message << "Failed to open the region in memory server";
        THROW_ERRNO_MSG(Fam_Allocator_Exception, FAM_ERR_RESOURCE,
                        message.str().c_str());
    }
}

/*
 * This function open the resource of a given region and helps to keep track of
 * the references to the region The reference count is incremented in each call
 * for an ACTIVE RESOURCE.
 */
Fam_Client_Resource *Fam_Client_Resource_Manager::open_resource(
    uint64_t regionId, Fam_Permission_Level permissionLevel, uint32_t uid,
    uint32_t gid, int flag) {
    // Look for the already existing valid resource of the region, if not
    // present create a new resource structure
    Fam_Client_Resource *famResource =
        find_or_create_resource(regionId, permissionLevel, uid, gid);

    uint64_t newValue, readValue;
    uint64_t status = RELEASED;
    uint64_t refCount = 0;

    while (true) {
        // Atomically read the status and reference count field together as 64
        // bit value
        readValue = ATOMIC_READ(&famResource->statusAndRefcount);
        // Parse status value
        status = GET_STATUS(readValue);
        // Parse reference count
        refCount = GET_REFCNT(readValue);

        /* Checks for the current status, there are 4 cases to handle based on
         * it :
         * 1. ACTIVE   : Increments the reference count
         * 2. INACTIVE : Change the status to BUSY and set the reference count
         * to 0, It is the responsibility of the thread which successfully
         * changed the status from INACTIVE to BUSY to register the memory.
         * 3. BUSY : Wait until the status changes to ACTIVE/INACTIVE/RELEASED.
         * 4. RELEASED : Add a fresh entry for the requested region. The current
         * stale entry is pushed to garbage queue for cleanup.
         */
        switch (status) {
        case ACTIVE:
            refCount += 1;
            newValue = CONCAT_STATUS_REFCNT(ACTIVE, refCount);
            if (ATOMIC_CAS(&famResource->statusAndRefcount, &readValue,
                           newValue)) {
                return famResource;
            }
            continue;
        case INACTIVE:
            if (flag & FAM_REGISTER_MEMORY) {
                newValue = CONCAT_STATUS_REFCNT(BUSY, 0);
                if (ATOMIC_CAS(&famResource->statusAndRefcount, &readValue,
                               newValue)) {
                    open_region_remote(regionId, famResource, permissionLevel,
                                       uid, gid);
                    if (flag & FAM_INIT_ONLY) {
                        CHANGE_STATUS_REFCNT(&famResource->statusAndRefcount,
                                             ACTIVE, 0);
                    } else {
                        CHANGE_STATUS_REFCNT(&famResource->statusAndRefcount,
                                             ACTIVE, 1);
                    }
                    return famResource;
                }
            } else {
                open_region_remote(regionId, famResource);
                if (flag & FAM_INIT_ONLY) {
                    newValue = CONCAT_STATUS_REFCNT(ACTIVE, 0);
                } else {
                    newValue = CONCAT_STATUS_REFCNT(ACTIVE, 1);
                }
                if (ATOMIC_CAS(&famResource->statusAndRefcount, &readValue,
                               newValue)) {
                    return famResource;
                }
            }
            continue;
        case BUSY:
            // TODO:Add sleep to avoid continous polling
            continue;
        case RELEASED:
            famResource =
                find_or_create_resource(regionId, permissionLevel, uid, gid);
            continue;
        }
    }
}

/*
 * This function closes the region resource. If the resource is in ACTIVE state
 * it reduces the reference count by 1. In all other case it only returns the
 * current status. Suppose the reference count reach 0, status is changed to
 * RELEASED, So that client can send request to memory server for closing the
 * region
 */
void Fam_Client_Resource_Manager::close_resource(uint64_t regionId) {
    // Lookup the resource in the resource table
    Fam_Client_Resource *famResource = find_resource(regionId);

    uint64_t readValue = 0, newValue = 0;
    uint64_t status, refCount;
    do {
        readValue = ATOMIC_READ(
            &famResource->statusAndRefcount); /* read current value */
        status = GET_STATUS(readValue);
        refCount = GET_REFCNT(readValue);
        if (status == ACTIVE) {
            if (refCount > 0)
                refCount -= 1;
            if (refCount == 0) {
                // TODO: Currently if reference count reaches 0, the memory
                // registration is released immediately. So the status is
                // changed to RELEASED. In future, the release operation is
                // delayed. A seperate thread will be running to free up the
                // resource which will release the registration later in time.
                newValue = CONCAT_STATUS_REFCNT(BUSY, 0);
            } else {
                newValue = CONCAT_STATUS_REFCNT(ACTIVE, refCount);
            }
        } else if (status == BUSY) {
            continue;
        } else {
            // TODO: In case of any other value of status, exception can be
            // thrown.
            return;
        }
    } while (
        !ATOMIC_CAS(&famResource->statusAndRefcount, &readValue, newValue));

    // Send the updated status
    status = GET_STATUS(newValue);

    /*
     * If the local resources are released(i.e. if all references to the region
     * are closed by the caller) then status is BUSY so, close the region in
     * memory server.
     */
    if (status == BUSY) {
        CHANGE_STATUS_REFCNT(&famResource->statusAndRefcount, RELEASED, 0);
        famCIS->close_region(regionId, famResource->memserverIds);
    }
    return;
}

/*
 * This function releases the resource associated with the region. The status is
 * changed to RELEASED.
 */
void Fam_Client_Resource_Manager::close_all_resources() {
    Fam_Resource_Table_Itr nextItr = famResourceTable->begin();
    /*
     * Acquire read lock on resource table maintained by local resource manager
     */
    pthread_rwlock_wrlock(&famResourceTableLock);
    /*
     * Iterate over the resource table and release all resources associated with
     * the region
     */
    for (auto it = famResourceTable->begin(); it != famResourceTable->end();
         it = nextItr) {
        /*
         * release_resource() in Fam_Client_Resource_Manager also erase the
         * element pointed current iterator hence, next iterator needs to be
         * stored before erase.
         */
        ++nextItr;
        uint64_t regionId = it->first;
        Fam_Client_Resource *famResource = it->second;
        std::vector<uint64_t> memserverIds = famResource->memserverIds;
        /*
         * Release the resource of current region
         */
        if (release_resource(regionId, famResource)) {
            /*
             * If resource is released by the calling thread, close region in
             * memory server
             */
            famCIS->close_region(regionId, memserverIds);
        }
    }
    /*
     * Release the lock on resource table
     */
    pthread_rwlock_unlock(&famResourceTableLock);
}

bool Fam_Client_Resource_Manager::release_resource(
    uint64_t regionId, Fam_Client_Resource *famResource) {
    uint64_t readValue, newValue;
    do {
        readValue = ATOMIC_READ(
            &famResource->statusAndRefcount); /* read current value */
        uint64_t status = GET_STATUS(readValue);
        /* Release only if the current status is ACTIVE */
        if (status != ACTIVE)
            return false;
        newValue = CONCAT_STATUS_REFCNT(RELEASED, 0);
    } while (
        !ATOMIC_CAS(&famResource->statusAndRefcount, &readValue, newValue));
    /* This function assumes that write lock is acquired on famResourceTable */
    famResourceTable->erase(regionId);
    /* If successfully changed the status to RELEASED, push the
     * Fam_Client_Resource structure to garbage queue */
    famClientResourceGarbageQ->push(famResource);
    return true;
}
} // namespace openfam
