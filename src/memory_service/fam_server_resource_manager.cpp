/*
 * fam_server_resource_manager.cpp
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

#include "fam_server_resource_manager.h"
#include <boost/atomic.hpp>

#include <iomanip>
#include <string.h>
#include <unistd.h>

using namespace std;
namespace openfam {

Fam_Server_Resource_Manager::Fam_Server_Resource_Manager(
    Memserver_Allocator *allocator, bool enableResourceRelease,
    bool isSharedMemory, Fam_Ops_Libfabric *famOps) {
    ostringstream message;

    this->allocator = allocator;
    famResourceTable = new std::map<uint64_t, Fam_Server_Resource *>();
    famServerResourceGarbageQ =
        new boost::lockfree::queue<Fam_Server_Resource *>(MAX_GARBAGE_ENTRY);

    pthread_rwlock_init(&famResourceTableLock, NULL);

    this->isSharedMemory = isSharedMemory;
    this->enableResourceRelease = enableResourceRelease;
    this->famOps = famOps;

    if (!isSharedMemory) {
        fenceMr = 0;
        register_fence_memory();
    }
    isBaseRequire = false;
    if (famOps) {
        if (strncmp(famOps->get_provider(), "verbs", 5) == 0)
            isBaseRequire = true;
    }
}

Fam_Server_Resource_Manager::~Fam_Server_Resource_Manager() {
    if (!isSharedMemory) {
        unregister_fence_memory();
    }
    while (!famServerResourceGarbageQ->empty()) {
        Fam_Server_Resource *famResource = NULL;
        famServerResourceGarbageQ->pop(famResource);
        if (famResource)
            delete famResource;
    }
}

void Fam_Server_Resource_Manager::reset_profile() { fabric_reset_profile(); }

void Fam_Server_Resource_Manager::dump_profile() { fabric_dump_profile(); }

uint64_t Fam_Server_Resource_Manager::generate_access_key(uint64_t regionId,
                                                          uint64_t dataitemId,
                                                          bool permission) {
    uint64_t key = 0;
    key |= (regionId & REGIONID_MASK) << REGIONID_SHIFT;
    key |= (dataitemId & DATAITEMID_MASK) << DATAITEMID_SHIFT;
    key |= permission;
    return key;
}

void Fam_Server_Resource_Manager::register_fence_memory() {
    ostringstream message;
    message << "Error while registering fence memory : ";
    int ret;
    fid_mr *mr = 0;
    uint64_t key = FAM_FENCE_KEY;
    void *localPointer;
    size_t len = (size_t)sysconf(_SC_PAGESIZE);
    int fd = -1;

    localPointer = mmap(NULL, len, PROT_WRITE, MAP_SHARED | MAP_ANON, fd, 0);
    if (localPointer == MAP_FAILED) {
    }

    // register the memory location with libfabric
    if (fenceMr == 0) {
        ret =
            fabric_register_mr(localPointer, len, &key, famOps->get_domain(),
                               1, mr);
        if (ret < 0) {
            message << "failed to register with fabric";
            throw Memory_Service_Exception(FENCE_REG_FAILED,
                                           message.str().c_str());
        }
        fenceMr = mr;
    }
}

/*
 * This function creates a new Fam_Server_Resource structure or returns an
 * existing valid Fam_Server_Resource structure for a requested region. Also, if
 * there is a invalid stale entry, it is removed and a fresh entry is added.
 */
Fam_Server_Resource *Fam_Server_Resource_Manager::find_or_create_resource(
    uint64_t regionId, Fam_Permission_Level permissionLevel, bool accessType) {
    Fam_Server_Resource *famResourceOld;

    // Start by taking a readlock on region table
    pthread_rwlock_rdlock(&famResourceTableLock);
    auto regionObj = famResourceTable->find(regionId);
    if (regionObj != famResourceTable->end()) {
        famResourceOld = regionObj->second;
        // If resource relinquishment is not enabled, no need to check the
        // status
        if (!enableResourceRelease) {
            pthread_rwlock_unlock(&famResourceTableLock);
            return famResourceOld;
        }

        // In case if resource relinquishment is enabled, check the status.
        // If status is RELEASED, the stale entry needs to be replaced with the
        // fresh entry
        uint64_t readValue = ATOMIC_READ(&famResourceOld->statusAndRefcount);
        uint64_t status = GET_STATUS(readValue);
        if (status != RELEASED) {
            pthread_rwlock_unlock(&famResourceTableLock);
            return famResourceOld;
        }
    }

    // Create a new Fam_Server_Resource entry
    Fam_Server_Resource *famResourceTemp = new Fam_Server_Resource();
    // When a new entry is added, it is introduced in INACTIVE state with
    // reference count 0.
    famResourceTemp->statusAndRefcount = CONCAT_STATUS_REFCNT(INACTIVE, 0);
    famResourceTemp->permissionLevel = permissionLevel;
    famResourceTemp->accessType = accessType;
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
    famResourceTemp->destroyed = false;
#endif
    famResourceTemp->famRegistrationTable =
        new std::map<uint64_t, Fam_Memory_Registration *>();
    pthread_rwlock_init(&famResourceTemp->famRegionLock, NULL);
    pthread_rwlock_unlock(&famResourceTableLock);
    pthread_rwlock_wrlock(&famResourceTableLock);
    regionObj = famResourceTable->find(regionId);
    if (regionObj == famResourceTable->end()) {
        // Add the fam region entry to the table
        famResourceTable->insert({regionId, famResourceTemp});
        pthread_rwlock_unlock(&famResourceTableLock);
        return famResourceTemp;
    } else {
        // If there exist an entry, check its status.
        famResourceOld = regionObj->second;
        // If resource relinquishment is not enabled, no need to check the
        // status
        if (!enableResourceRelease) {
            pthread_rwlock_unlock(&famResourceTableLock);
            return famResourceOld;
        }

        // In case if resource relinquishment is enabled, check the status
        uint64_t readValue = ATOMIC_READ(&famResourceOld->statusAndRefcount);
        uint64_t status = GET_STATUS(readValue);
        if (status == RELEASED) {
            // If the status is RELEASED, replace the stale entry with new
            // entry
            famResourceTable->erase(regionObj);
            famServerResourceGarbageQ->push(famResourceOld);
            famResourceTable->insert({regionId, famResourceTemp});
            pthread_rwlock_unlock(&famResourceTableLock);
            return famResourceTemp;
        }
        pthread_rwlock_unlock(&famResourceTableLock);
        delete famResourceTemp->famRegistrationTable;
        delete famResourceTemp;
        return famResourceOld;
    }
}

/*
 * This function register region memory
 */
void Fam_Server_Resource_Manager::register_region_memory(
    uint64_t regionId, bool accessType, Fam_Server_Resource *famResource) {
    /*
     * Create a new entry or get the existing entry for requested region
     */
    if (!famResource) {
        famResource = find_or_create_resource(regionId, REGION, accessType);
    }

    /*
     * Get all region extent
     */
    Fam_Region_Extents_t extents;
    allocator->get_region_extents(regionId, &extents);
    /*
     * Register each extent
     */
    for (int i = 0; i < extents.numExtents; i++) {
        register_memory(regionId, i, extents.addrList[i], extents.sizes[i],
                        accessType, famResource);
    }
}

/*
 * This function get region memory information
 */
Fam_Region_Memory Fam_Server_Resource_Manager::get_region_memory_info(
    uint64_t regionId, bool accessType, Fam_Server_Resource *famResource) {
    /*
     * Create a new entry or get the existing entry for requested region
     */
    if (!famResource) {
        famResource = find_or_create_resource(regionId, REGION, accessType);
    }

    /*
     * Get all region extent
     */
    Fam_Region_Extents_t extents;
    allocator->get_region_extents(regionId, &extents);
    Fam_Region_Memory regionMemory;
    uint64_t key, base = 0;
    /*
     * Register each extent
     */
    for (int i = 0; i < extents.numExtents; i++) {
        key = register_memory(regionId, i, extents.addrList[i],
                              extents.sizes[i], accessType, famResource);
        if (isBaseRequire) {
            base = (uint64_t)extents.addrList[i];
        }
        regionMemory.keys.push_back(key);
        regionMemory.base.push_back(base);
    }
    return regionMemory;
}

/*
 * This function get data item memory information
 */
Fam_Dataitem_Memory Fam_Server_Resource_Manager::get_dataitem_memory_info(
    uint64_t regionId, uint64_t offset, uint64_t size, bool accessType,
    Fam_Server_Resource *famResource) {
    Fam_Dataitem_Memory dataitemMemory;
    if (isSharedMemory) {
        if (accessType)
            dataitemMemory.key = FAM_WRITE_KEY_SHM | FAM_READ_KEY_SHM;
        else
            dataitemMemory.key = FAM_READ_KEY_SHM;
        dataitemMemory.base =
            (uint64_t)allocator->get_local_pointer(regionId, offset);
        return dataitemMemory;
    }

    /*
     * Create a new entry or get the existing entry for requested region
     */
    if (!famResource) {
        famResource = find_or_create_resource(regionId, DATAITEM, accessType);
    }

    uint64_t key, base = 0;
    /*
     * Register the dataitem memory
     */
    void *localPtr = allocator->get_local_pointer(regionId, offset);
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    key = register_memory(regionId, dataitemId, localPtr, size, accessType,
                          famResource);
    if (isBaseRequire) {
        base = (uint64_t)localPtr;
    }
    dataitemMemory.key = key;
    dataitemMemory.base = base;
    return dataitemMemory;
}
/*
 * This function look for the entry for requested region
 */
Fam_Server_Resource *
Fam_Server_Resource_Manager::find_resource(uint64_t regionId) {
    Fam_Server_Resource *famResource = NULL;
    // Start by taking a readlock on region table
    pthread_rwlock_rdlock(&famResourceTableLock);
    auto regionObj = famResourceTable->find(regionId);
    if (regionObj != famResourceTable->end()) {
        famResource = regionObj->second;
    }
    // Release lock on region table
    pthread_rwlock_unlock(&famResourceTableLock);
    return famResource;
}

/*
 * This function open the resource of a given region and helps to keep track of
 * the references to the region The reference count is incremented in each call
 * for an ACTIVE RESOURCE.
 */
Fam_Server_Resource *
Fam_Server_Resource_Manager::open_resource(uint64_t regionId,
                                           Fam_Permission_Level permissionLevel,
                                           bool accessType, int flag) {
    /*
     * Create a new entry or get the existing entry for requested region
     */
    Fam_Server_Resource *famResource =
        find_or_create_resource(regionId, permissionLevel, accessType);

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
         * to 0, It is the responsibility of the responsibility of the thread
         * which successfully changed the status from INACTIVE to BUSY to
         * register the memory.
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
                    register_region_memory(regionId, famResource);
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
            // TODO:Add sleep to avoid continous poling
            continue;
        case RELEASED:
            famResource =
                find_or_create_resource(regionId, permissionLevel, accessType);
            continue;
        }
    }
}

/*
 * This function closes the region resource. If the resource is in ACTIVE state
 * it reduces the reference count by 1. In all other case it only returns the
 * current status. Suppose the reference count reach 0, status is changed to
 * RELEASED. Also, if status is successfully changed to RELEASED, all memory
 * registration under the region are released.
 */
uint64_t Fam_Server_Resource_Manager::close_resource(uint64_t regionId) {
    ostringstream message;

    /*
     * Look for the existing resource entry
     */
    Fam_Server_Resource *famResource = find_resource(regionId);
    if (!famResource) {
        message << "Requested region resource is not found in resource table";
        throw Memory_Service_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

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
                // registration is released immediately.
                // So the status is changed to RELEASED.
                // In future, the release operation is delayed. A seperate
                // thread will be running to free up the resource which will
                // release the registration later in time.
                newValue = CONCAT_STATUS_REFCNT(BUSY, 0);
            } else {
                newValue = CONCAT_STATUS_REFCNT(ACTIVE, refCount);
            }
        } else if (status == BUSY) {
            continue;
        } else {
            // TODO: In case of any other value of status, exception can be
            // thrown.
            return status;
        }
    } while (
        !ATOMIC_CAS(&famResource->statusAndRefcount, &readValue, newValue));

    // Get the updated status
    status = GET_STATUS(newValue);
    refCount = GET_REFCNT(newValue);
    if (status == BUSY) {
        CHANGE_STATUS_REFCNT(&famResource->statusAndRefcount, RELEASED, 0);
        unregister_region_memory(famResource);
        return RELEASED;
    }
    return status;
}

/*
 * This function register chunk of a region. The chunk can be region extent in
 * case of REGION level permission or data item in case of DATAITEM
 * (&famResource->famRegionLock* permission
 */
uint64_t Fam_Server_Resource_Manager::register_memory(
    uint64_t regionId, uint64_t registrationId, void *base, uint64_t size,
    bool rwFlag, Fam_Server_Resource *famResource) {
    uint64_t key = 0;
    ostringstream message;
    message << "Error while registering memory : ";
    fid_mr *mr = 0;
    int ret = 0;
    uint64_t mrkey = 0;

    void *localPointer = base;

    key = mrkey = generate_access_key(regionId, registrationId, rwFlag);

    // Take read lock to check if registration already available
    pthread_rwlock_rdlock(&famResource->famRegionLock);

    auto mrObj = famResource->famRegistrationTable->find(key);

    if (mrObj != famResource->famRegistrationTable->end()) {
        Fam_Memory_Registration *famRegistration = mrObj->second;
        pthread_rwlock_unlock(&famResource->famRegionLock);
        mrkey = fi_mr_key(famRegistration->mr);
        return mrkey;
    }

    pthread_rwlock_unlock(&famResource->famRegionLock);
    // Take a writelock if memory is not regsitered already
    pthread_rwlock_wrlock(&famResource->famRegionLock);

    mrObj = famResource->famRegistrationTable->find(key);
    if (mrObj != famResource->famRegistrationTable->end()) {
        Fam_Memory_Registration *famRegistration = mrObj->second;
        pthread_rwlock_unlock(&famResource->famRegionLock);
        mrkey = fi_mr_key(famRegistration->mr);
        return mrkey;
    }

    ret = fabric_register_mr(localPointer, size, &mrkey, famOps->get_domain(),
                             rwFlag, mr);
    if (ret < 0) {
        pthread_rwlock_unlock(&famResource->famRegionLock);
        message << "failed to register with fabric";
        throw Memory_Service_Exception(REGISTRATION_FAILED,
                                       message.str().c_str());
    }
    Fam_Memory_Registration *famRegistration = new Fam_Memory_Registration();
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
    famRegistration->deallocated = false;
#endif
    famRegistration->mr = mr;
    famResource->famRegistrationTable->insert({key, famRegistration});

    pthread_rwlock_unlock(&famResource->famRegionLock);
    // Always return mrkey, which might be different than key.
    return mrkey;
}

void Fam_Server_Resource_Manager::unregister_fence_memory() {
    ostringstream message;
    message << "Error while deregistering fence memory : ";
    int ret = 0;
    if (fenceMr != 0) {
        ret = fabric_deregister_mr(fenceMr);
        if (ret < 0) {
            message << "failed to deregister with fabric";
            throw Memory_Service_Exception(FENCE_DEREG_FAILED,
                                           message.str().c_str());
        }
    }
    fenceMr = 0;
}

void Fam_Server_Resource_Manager::unregister_memory(
    uint64_t regionId, uint64_t registrationId,
    Fam_Server_Resource *famResource) {
    ostringstream message;
    message << "Error while deregistering memory : ";

    int ret = 0;
    uint64_t rKey = generate_access_key(regionId, registrationId, 0);
    uint64_t rwKey = generate_access_key(regionId, registrationId, 1);

    // Take a writelock on fiRegionMap
    pthread_rwlock_wrlock(&famResource->famRegionLock);

    auto rMr = famResource->famRegistrationTable->find(rKey);
    if (rMr != famResource->famRegistrationTable->end()) {
        Fam_Memory_Registration *famRegistration = rMr->second;
        ret = fabric_deregister_mr(famRegistration->mr);
        if (ret < 0) {
            pthread_rwlock_unlock(&famResource->famRegionLock);
            message << "failed to deregister with fabric";
            throw Memory_Service_Exception(ITEM_DEREGISTRATION_FAILED,
                                           message.str().c_str());
        }
        famResource->famRegistrationTable->erase(rMr);
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
        if (enableResourceRelease &&
            (famResource->permissionLevel == DATAITEM)) {
            if (ATOMIC_READ(&famRegistration->deallocated)) {
                uint64_t offset = get_offset_from_key(rKey);
                allocator->deallocate(regionId, offset);
            }
        }
#endif
    }

    auto rwMr = famResource->famRegistrationTable->find(rwKey);
    if (rwMr != famResource->famRegistrationTable->end()) {
        Fam_Memory_Registration *famRegistration = rwMr->second;
        ret = fabric_deregister_mr(famRegistration->mr);
        if (ret < 0) {
            pthread_rwlock_unlock(&famResource->famRegionLock);
            message << "failed to deregister with fabric";
            throw Memory_Service_Exception(ITEM_DEREGISTRATION_FAILED,
                                           message.str().c_str());
        }
        famResource->famRegistrationTable->erase(rwMr);
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
        if (enableResourceRelease &&
            (famResource->permissionLevel == DATAITEM)) {
            if (ATOMIC_READ(&famRegistration->deallocated)) {
                uint64_t offset = get_offset_from_key(rwKey);
                allocator->deallocate(regionId, offset);
            }
        }
#endif
    }

    pthread_rwlock_unlock(&famResource->famRegionLock);
}

void Fam_Server_Resource_Manager::unregister_region_memory(
    Fam_Server_Resource *famResource) {
    int ret = 0;
    ostringstream message;
    // Take a writelock on famRegionMap
    pthread_rwlock_wrlock(&famResource->famRegionLock);
    // Unregister all dataItem memory from region table, only, if the above
    // condition is satisfied.
    for (auto registrationObj : *(famResource->famRegistrationTable)) {
        Fam_Memory_Registration *famRegistration = registrationObj.second;
        fid_mr *mr = famRegistration->mr;
        ret = fabric_deregister_mr(mr);
        if (ret < 0) {
            message << "Failed to unregister memory";
            throw Memory_Service_Exception(UNREGISTRATION_FAILED,
                                           message.str().c_str());
        }
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
        if (enableResourceRelease &&
            (famResource->permissionLevel == DATAITEM)) {
            if (!ATOMIC_READ(&famResource->destroyed)) {
                if (ATOMIC_READ(&famRegistration->deallocated)) {
                    uint64_t regionId =
                        get_region_id_from_key(registrationObj.first);
                    uint64_t offset =
                        get_offset_from_key(registrationObj.first);
                    allocator->deallocate(regionId, offset);
                }
            }
        }
#endif
        delete famRegistration;
    }

    pthread_rwlock_unlock(&famResource->famRegionLock);
    famResource->famRegistrationTable->clear();
}

#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
uint64_t Fam_Server_Resource_Manager::get_offset_from_key(uint64_t key) {
    uint64_t dataitemId = key >> DATAITEMID_SHIFT;
    dataitemId = dataitemId & DATAITEMID_MASK;
    uint64_t offset = dataitemId * MIN_OBJ_SIZE;
    return offset;
}

uint64_t Fam_Server_Resource_Manager::get_region_id_from_key(uint64_t key) {
    uint64_t regionId = key >> REGIONID_SHIFT;
    regionId = regionId & REGIONID_MASK;
    return regionId;
}

void Fam_Server_Resource_Manager::mark_deallocated(
    uint64_t regionId, uint64_t registrationId,
    Fam_Server_Resource *famResource) {
    if (famResource->permissionLevel == DATAITEM) {
        uint64_t rKey = generate_access_key(regionId, registrationId, 0);
        uint64_t rwKey = generate_access_key(regionId, registrationId, 1);

        // Take a writelock on fiRegionMap
        pthread_rwlock_rdlock(&famResource->famRegionLock);

        auto rMr = famResource->famRegistrationTable->find(rKey);
        if (rMr != famResource->famRegistrationTable->end()) {
            Fam_Memory_Registration *famRegistration = rMr->second;
            ATOMIC_WRITE(&famRegistration->deallocated, true);
        }

        auto rwMr = famResource->famRegistrationTable->find(rwKey);
        if (rwMr != famResource->famRegistrationTable->end()) {
            Fam_Memory_Registration *famRegistration = rwMr->second;
            ATOMIC_WRITE(&famRegistration->deallocated, true);
        }

        pthread_rwlock_unlock(&famResource->famRegionLock);
    }
}
#endif

} // namespace openfam
