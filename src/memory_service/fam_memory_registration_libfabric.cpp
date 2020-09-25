/*
 * fam_memory_registration_libfabric.cpp
 * Copyright (c) 2019-2020 Hewlett Packard Enterprise Development, LP. All
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

#include "fam_memory_registration_libfabric.h"
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
MEMSERVER_PROFILE_START(MEMORY_REG_FABRIC)
#ifdef MEMSERVER_PROFILE
#define MEMORY_REG_FABRIC_PROFILE_START_OPS()                                  \
    {                                                                          \
        Profile_Time start = MEMORY_REG_FABRIC_get_time();

#define MEMORY_REG_FABRIC_PROFILE_END_OPS(apiIdx)                              \
    Profile_Time end = MEMORY_REG_FABRIC_get_time();                           \
    Profile_Time total = MEMORY_REG_FABRIC_time_diff_nanoseconds(start, end);  \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(MEMORY_REG_FABRIC, prof_##apiIdx,       \
                                       total)                                  \
    }
#define MEMORY_REG_FABRIC_PROFILE_DUMP() memory_reg_fabric_profile_dump()
#else
#define MEMORY_REG_FABRIC_PROFILE_START_OPS()
#define MEMORY_REG_FABRIC_PROFILE_END_OPS(apiIdx)
#define MEMORY_REG_FABRIC_PROFILE_DUMP()
#endif

void memory_reg_fabric_profile_dump() {
    MEMSERVER_PROFILE_END(MEMORY_REG_FABRIC);
    MEMSERVER_DUMP_PROFILE_BANNER(MEMORY_REG_FABRIC)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(MEMORY_REG_FABRIC, name, prof_##name)
#include "memory_service/memory_reg_fabric_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(MEMORY_REG_FABRIC, prof_##name)
#include "memory_service/memory_reg_fabric_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(MEMORY_REG_FABRIC)
}

void Fam_Memory_Registration_Libfabric::progress_thread() {
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        while (1) {
            if (!haltProgress)
                famOps->quiet();
            else
                break;
        }
    }
}

Fam_Memory_Registration_Libfabric::Fam_Memory_Registration_Libfabric(
    const char *name, const char *service, const char *provider) {
    MEMSERVER_PROFILE_INIT(MEMORY_REG_FABRIC)
    MEMSERVER_PROFILE_START_TIME(MEMORY_REG_FABRIC)
    ostringstream message;

    fiMrs = NULL;
    fenceMr = 0;

    famOps = new Fam_Ops_Libfabric(true, provider, FAM_THREAD_MULTIPLE, NULL,
                                   FAM_CONTEXT_DEFAULT, name, service);
    int ret = famOps->initialize();
    if (ret < 0) {
        message << "famOps initialization failed";
        throw Memory_Service_Exception(OPS_INIT_FAILED, message.str().c_str());
    }
    struct fi_info *fi = famOps->get_fi();
    if (fi->domain_attr->control_progress == FI_PROGRESS_MANUAL ||
        fi->domain_attr->data_progress == FI_PROGRESS_MANUAL) {
        libfabricProgressMode = FI_PROGRESS_MANUAL;
    }

    register_fence_memory();

    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        haltProgress = false;
        progressThread =
            std::thread(&Fam_Memory_Registration_Libfabric::progress_thread, this);
    }

    if (strncmp(famOps->get_provider(), "verbs", 5) == 0)
        isBaseRequire = true;
    else
        isBaseRequire = false;
    famOpsLibfabricQ = famOps;
}

Fam_Memory_Registration_Libfabric::~Fam_Memory_Registration_Libfabric() {
    deregister_fence_memory();

    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        haltProgress = true;
        progressThread.join();
    }

    famOps->finalize();
    delete famOps;
}

void Fam_Memory_Registration_Libfabric::reset_profile() {
    MEMSERVER_PROFILE_INIT(MEMORY_REG_FABRIC)
    MEMSERVER_PROFILE_START_TIME(MEMORY_REG_FABRIC)
    fabric_reset_profile();
}

void Fam_Memory_Registration_Libfabric::dump_profile() {
    MEMORY_REG_FABRIC_PROFILE_DUMP();
    fabric_dump_profile();
}

uint64_t Fam_Memory_Registration_Libfabric::generate_access_key(uint64_t regionId,
                                                            uint64_t dataitemId,
                                                            bool permission) {
    uint64_t key = 0;
    MEMORY_REG_FABRIC_PROFILE_START_OPS()
    key |= (regionId & REGIONID_MASK) << REGIONID_SHIFT;
    key |= (dataitemId & DATAITEMID_MASK) << DATAITEMID_SHIFT;
    key |= permission;
    MEMORY_REG_FABRIC_PROFILE_END_OPS(generate_access_key);
    return key;
}

void Fam_Memory_Registration_Libfabric::register_fence_memory() {
    MEMORY_REG_FABRIC_PROFILE_START_OPS()
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
        ret = fabric_register_mr(localPointer, len, &key, famOps->get_domain(),
                                 1, mr);
        if (ret < 0) {
            message << "failed to register with fabric";
            throw Memory_Service_Exception(FENCE_REG_FAILED,
                                           message.str().c_str());
        }
        fenceMr = mr;
    }
    MEMORY_REG_FABRIC_PROFILE_END_OPS(register_fence_memory);
}

uint64_t Fam_Memory_Registration_Libfabric::register_memory(uint64_t regionId,
                                                        uint64_t offset,
                                                        void *base,
                                                        uint64_t size,
                                                        bool rwFlag) {
    uint64_t key = 0;
    MEMORY_REG_FABRIC_PROFILE_START_OPS()
    ostringstream message;
    message << "Error while registering memory : ";
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    if (fiMrs == NULL)
        fiMrs = famOps->get_fiMrs();
    fid_mr *mr = 0;
    int ret = 0;
    uint64_t mrkey = 0;
    Fam_Region_Map_t *fiRegionMap = NULL;
    Fam_Region_Map_t *fiRegionMapDiscard = NULL;

    void *localPointer = base;

    key = mrkey = generate_access_key(regionId, dataitemId, rwFlag);

    // register the data item with required permission with libfabric
    // Start by taking a readlock on fiMrs
    pthread_rwlock_rdlock(famOps->get_mr_lock());
    auto regionMrObj = fiMrs->find(regionId);
    if (regionMrObj == fiMrs->end()) {
        // Create a RegionMap
        fiRegionMap = (Fam_Region_Map_t *)calloc(1, sizeof(Fam_Region_Map_t));
        fiRegionMap->regionId = regionId;
        fiRegionMap->fiRegionMrs = new std::map<uint64_t, fid_mr *>();
        pthread_rwlock_init(&fiRegionMap->fiRegionLock, NULL);
        // RegionMap not found, release read lock, take a write lock
        pthread_rwlock_unlock(famOps->get_mr_lock());
        pthread_rwlock_wrlock(famOps->get_mr_lock());
        // Check again if regionMap added by another thread.
        regionMrObj = fiMrs->find(regionId);
        if (regionMrObj == fiMrs->end()) {
            // Add the fam region map into fiMrs
            fiMrs->insert({regionId, fiRegionMap});
        } else {
            // Region Map already added by another thread,
            // discard the one created here.
            fiRegionMapDiscard = fiRegionMap;
            fiRegionMap = regionMrObj->second;
        }
    } else {
        fiRegionMap = regionMrObj->second;
    }

    // Take a writelock on fiRegionMap
    pthread_rwlock_wrlock(&fiRegionMap->fiRegionLock);
    // Release lock on fiMrs
    pthread_rwlock_unlock(famOps->get_mr_lock());

    // Delete the discarded region map here.
    if (fiRegionMapDiscard != NULL) {
        delete fiRegionMapDiscard->fiRegionMrs;
        free(fiRegionMapDiscard);
    }

    auto mrObj = fiRegionMap->fiRegionMrs->find(key);
    if (mrObj == fiRegionMap->fiRegionMrs->end()) {
        ret = fabric_register_mr(localPointer, size, &mrkey,
                                 famOps->get_domain(), rwFlag, mr);
        if (ret < 0) {
            pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
            message << "failed to register with fabric";
            throw Memory_Service_Exception(ITEM_REGISTRATION_FAILED,
                                           message.str().c_str());
        }

        fiRegionMap->fiRegionMrs->insert({key, mr});
    } else {
        mrkey = fi_mr_key(mrObj->second);
    }
    // Always return mrkey, which might be different than key.
    key = mrkey;

    pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
    MEMORY_REG_FABRIC_PROFILE_END_OPS(register_memory);
    return key;
}

void Fam_Memory_Registration_Libfabric::deregister_fence_memory() {
    MEMORY_REG_FABRIC_PROFILE_START_OPS()
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
    MEMORY_REG_FABRIC_PROFILE_END_OPS(deregister_fence_memory);
}

void Fam_Memory_Registration_Libfabric::deregister_memory(uint64_t regionId,
                                                      uint64_t offset) {
    MEMORY_REG_FABRIC_PROFILE_START_OPS()
    ostringstream message;
    message << "Error while deregistering memory : ";

    int ret = 0;
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    uint64_t rKey = generate_access_key(regionId, dataitemId, 0);
    uint64_t rwKey = generate_access_key(regionId, dataitemId, 1);
    Fam_Region_Map_t *fiRegionMap;

    if (fiMrs == NULL)
        fiMrs = famOps->get_fiMrs();

    // Take read lock on fiMrs
    pthread_rwlock_rdlock(famOps->get_mr_lock());
    auto regionMrObj = fiMrs->find(regionId);
    if (regionMrObj == fiMrs->end()) {
        pthread_rwlock_unlock(famOps->get_mr_lock());
        return;
    } else {
        fiRegionMap = regionMrObj->second;
    }

    // Take a writelock on fiRegionMap
    pthread_rwlock_wrlock(&fiRegionMap->fiRegionLock);
    // Release lock on fiMrs
    pthread_rwlock_unlock(famOps->get_mr_lock());

    auto rMr = fiRegionMap->fiRegionMrs->find(rKey);
    auto rwMr = fiRegionMap->fiRegionMrs->find(rwKey);
    if (rMr != fiRegionMap->fiRegionMrs->end()) {
        ret = fabric_deregister_mr(rMr->second);
        if (ret < 0) {
            pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
            message << "failed to deregister with fabric";
            throw Memory_Service_Exception(ITEM_DEREGISTRATION_FAILED,
                                           message.str().c_str());
        }
        fiRegionMap->fiRegionMrs->erase(rMr);
    }

    if (rwMr != fiRegionMap->fiRegionMrs->end()) {
        ret = fabric_deregister_mr(rwMr->second);
        if (ret < 0) {
            pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
            message << "failed to deregister with fabric";
            throw Memory_Service_Exception(ITEM_DEREGISTRATION_FAILED,
                                           message.str().c_str());
        }
        fiRegionMap->fiRegionMrs->erase(rwMr);
    }

    pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
    MEMORY_REG_FABRIC_PROFILE_END_OPS(deregister_memory);
}

void Fam_Memory_Registration_Libfabric::deregister_region_memory(
    uint64_t regionId) {
    MEMORY_REG_FABRIC_PROFILE_START_OPS()
    int ret = 0;
    Fam_Region_Map_t *fiRegionMap;

    if (fiMrs == NULL)
        fiMrs = famOps->get_fiMrs();

    // Take write lock on fiMrs
    pthread_rwlock_wrlock(famOps->get_mr_lock());

    auto regionMrObj = fiMrs->find(regionId);
    if (regionMrObj == fiMrs->end()) {
        pthread_rwlock_unlock(famOps->get_mr_lock());
        return;
    } else {
        fiRegionMap = regionMrObj->second;
    }

    // Take a writelock on fiRegionMap
    pthread_rwlock_wrlock(&fiRegionMap->fiRegionLock);
    // Remove region map from fiMrs
    fiMrs->erase(regionMrObj);
    // Release lock on fiMrs
    pthread_rwlock_unlock(famOps->get_mr_lock());

    // Unregister all dataItem memory from region map
    for (auto mr : *(fiRegionMap->fiRegionMrs)) {
        ret = fabric_deregister_mr(mr.second);
        if (ret < 0) {
            cout << "destroy region<" << fiRegionMap->regionId
                 << ">: memory deregister failed with errno(" << ret << ")"
                 << endl;
        }
    }

    pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
    fiRegionMap->fiRegionMrs->clear();
    delete fiRegionMap->fiRegionMrs;
    free(fiRegionMap);
    MEMORY_REG_FABRIC_PROFILE_END_OPS(deregister_region_memory);
}

size_t Fam_Memory_Registration_Libfabric::get_addr_size() {
    size_t addrSize;
    MEMORY_REG_FABRIC_PROFILE_START_OPS()
    addrSize = famOps->get_addr_size();
    MEMORY_REG_FABRIC_PROFILE_END_OPS(mem_reg_get_addr_size);
    return addrSize;
}

void *Fam_Memory_Registration_Libfabric::get_addr() {
    void *addr;
    MEMORY_REG_FABRIC_PROFILE_START_OPS()
    addr = famOps->get_addr();
    MEMORY_REG_FABRIC_PROFILE_END_OPS(mem_reg_get_addr);
    return addr;
}

bool Fam_Memory_Registration_Libfabric::is_base_require() {
    return isBaseRequire;
}

} // namespace openfam
