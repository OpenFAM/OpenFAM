/*
 * fam_memory_service_direct.cpp
 * Copyright (c) 2020-2023 Hewlett Packard Enterprise Development, LP. All
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
#include "fam_memory_service_direct.h"
#include "common/atomic_queue.h"
#include "common/fam_config_info.h"
#include "common/fam_internal.h"
#include "common/fam_memserver_profile.h"
#include <thread>

#include <boost/atomic.hpp>

#include <chrono>
#include <iomanip>
#include <string.h>
#include <unistd.h>

using namespace std;
using namespace chrono;
namespace openfam {
MEMSERVER_PROFILE_START(MEMORY_SERVICE_DIRECT)
#ifdef MEMSERVER_PROFILE
#define MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()                              \
    {                                                                          \
        Profile_Time start = MEMORY_SERVICE_DIRECT_get_time();

#define MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(apiIdx)                          \
    Profile_Time end = MEMORY_SERVICE_DIRECT_get_time();                       \
    Profile_Time total =                                                       \
        MEMORY_SERVICE_DIRECT_time_diff_nanoseconds(start, end);               \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(MEMORY_SERVICE_DIRECT, prof_##apiIdx,   \
                                       total)                                  \
    }
#define MEMORY_SERVICE_DIRECT_PROFILE_DUMP()                                   \
    memory_service_direct_profile_dump()
#else
#define MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
#define MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(apiIdx)
#define MEMORY_SERVICE_DIRECT_PROFILE_DUMP()
#endif

void memory_service_direct_profile_dump() {
    MEMSERVER_PROFILE_END(MEMORY_SERVICE_DIRECT);
    MEMSERVER_DUMP_PROFILE_BANNER(MEMORY_SERVICE_DIRECT)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(MEMORY_SERVICE_DIRECT, name, prof_##name)
#include "memory_service/memory_service_direct_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(MEMORY_SERVICE_DIRECT, prof_##name)
#include "memory_service/memory_service_direct_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(MEMORY_SERVICE_DIRECT)
}

Fam_Memory_Service_Direct::Fam_Memory_Service_Direct(uint64_t memserver_id,
                                                     bool isSharedMemory) {
    memory_server_id = memserver_id;
    ostringstream message;
    // Look for options information from config file.
    // Use config file options only if NULL is passed.
    std::string config_file_path;
    configFileParams config_options;

    // Check for config file in or in path mentioned
    // by OPENFAM_ROOT environment variable or in /opt/OpenFAM.
    try {
        config_file_path =
            find_config_file(strdup("fam_memoryserver_config.yaml"));
    } catch (Fam_InvalidOption_Exception &e) {

        // If the config_file is not present, then ignore the exception.
    }
    // Get the configuration info from the configruation file.
    if (!config_file_path.empty()) {
        config_options = get_config_info(config_file_path);
    } else {
        message << "No config file present in OPENFAM_ROOT path.";
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }

    set_rpc_framework_protocol(config_options);

    std::string memType = config_options["Memservers:memory_type"];
    fam_path = (char *)strdup(config_options["Memservers:fam_path"].c_str());
    // default case??
    if (strcmp(memType.c_str(), "volatile") == 0) {
        memServermemType = VOLATILE;
        (void)memServermemType;
    } else if (strcmp(memType.c_str(), "persistent") == 0) {
        memServermemType = PERSISTENT;
        (void)memServermemType;
    } else {
        message << "memory_type option in the config file is invalid.";
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }

    (void)memory_server_id;
    libfabricPort = (char *)strdup(config_options["Memservers:libfabric_port"].c_str());

    rpc_interface = (char *)strdup(config_options["Memservers:rpc_interface"].c_str());
    std::string addr = rpc_interface.substr(0, rpc_interface.find(':'));
    if_device = (char *)strdup(config_options["Memservers:if_device"].c_str());
    libfabricProvider = (char *)strdup(config_options["provider"].c_str());

    int num_delayed_free_Threads =
        atoi(config_options["delayed_free_threads"].c_str());

    allocator =
        new Memserver_Allocator(num_delayed_free_Threads, fam_path.c_str());
    fam_backup_path = config_options["fam_backup_path"];
    struct stat info;
    if (stat(fam_backup_path.c_str(), &info) == -1) {
        if ((errno == ENOENT) || (errno == ENOTDIR)) {
            mkdir(fam_backup_path.c_str(),
                  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        }
    }

    if (strcmp(config_options["resource_release"].c_str(), "enable") == 0) {
        enableResourceRelease = true;
    } else {
        enableResourceRelease = false;
    }

    this->isSharedMemory = isSharedMemory;
    if (isSharedMemory) {
        famResourceManager =
            new Fam_Server_Resource_Manager(allocator, enableResourceRelease);
        isBaseRequire = true;
    } else {
        fabric_initialize(addr.c_str(), libfabricPort.c_str(),
                          libfabricProvider.c_str(), if_device.c_str());
        famResourceManager = new Fam_Server_Resource_Manager(
            allocator, enableResourceRelease, false, famOps);
    }

    for (int i = 0; i < CAS_LOCK_CNT; i++) {
        (void)pthread_mutex_init(&casLock[i], NULL);
    }
    char *end;
    numAtomicThreads = atoi(config_options["ATL_threads"].c_str());
    if (numAtomicThreads > MAX_ATOMIC_THREADS)
        numAtomicThreads = MAX_ATOMIC_THREADS;
    memoryPerThread =
        1024 * 1024 *
        strtoul(config_options["ATL_data_size"].c_str(), &end, 10);
    queueCapacity = atoi(config_options["ATL_queue_size"].c_str());
    init_atomic_queue();
}

Fam_Memory_Service_Direct::~Fam_Memory_Service_Direct() {
    allocator->memserver_allocator_finalize();
    for (int i = 0; i < numAtomicThreads; i++)
        pthread_join(atid[i], NULL);
    for (int i = 0; i < CAS_LOCK_CNT; i++) {
        (void)pthread_mutex_destroy(&casLock[i]);
    }

    if (!isSharedMemory) {
        fabric_finalize();
    }

    delete allocator;
    delete famResourceManager;
}

void Fam_Memory_Service_Direct::fabric_initialize(const char *name,
                                                  const char *service,
                                                  const char *provider,
                                                  const char *if_device) {
    ostringstream message;
    famOps =
        new Fam_Ops_Libfabric(true, provider, if_device, FAM_THREAD_MULTIPLE,
                              NULL, FAM_CONTEXT_DEFAULT, name, service);
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

    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        haltProgress = false;
        progressThread =
            std::thread(&Fam_Memory_Service_Direct::progress_thread, this);
    }

    if (strncmp(famOps->get_provider(), "verbs", 5) == 0)
        isBaseRequire = true;
    else
        isBaseRequire = false;
    famOpsLibfabricQ = famOps;
}

void Fam_Memory_Service_Direct::fabric_finalize() {
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        haltProgress = true;
        progressThread.join();
    }

    famOps->finalize();
    delete famOps;
}

void Fam_Memory_Service_Direct::progress_thread() {
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        while (1) {
            if (!haltProgress)
                famOps->check_progress();
            else
                break;
        }
    }
}

void Fam_Memory_Service_Direct::reset_profile() {

    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_DIRECT)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_DIRECT)
    allocator->reset_profile();
    return;
}

void Fam_Memory_Service_Direct::dump_profile() {
    MEMORY_SERVICE_DIRECT_PROFILE_DUMP();
    allocator->dump_profile();
}

void Fam_Memory_Service_Direct::update_memserver_addrlist(
    void *memServerInfoBuffer, size_t memServerInfoSize,
    uint64_t memoryServerCount) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    // This function is used only in memory server model
    if (isSharedMemory)
        return;
    famOps->populate_address_vector(memServerInfoBuffer, memServerInfoSize,
                                    memoryServerCount, memory_server_id);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_update_memserver_addrlist);
}

void Fam_Memory_Service_Direct::create_region(uint64_t regionId,
                                              size_t nbytes) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()

    allocator->create_region(regionId, nbytes);

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_create_region);
}

void Fam_Memory_Service_Direct::destroy_region(uint64_t regionId,
                                               uint64_t *resourceStatus) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()

    allocator->destroy_region(regionId);
    if (!isSharedMemory) {
        if (!enableResourceRelease) {
            Fam_Server_Resource *famResource =
                famResourceManager->find_resource(regionId);
            if (famResource) {
                famResourceManager->unregister_region_memory(famResource);
                famResource->statusAndRefcount =
                    CONCAT_STATUS_REFCNT(RELEASED, 0);
            }
            *resourceStatus = RELEASED;
        } else {
            Fam_Server_Resource *famResource =
                famResourceManager->find_resource(regionId);
            if (famResource) {
                if (famResource->permissionLevel == DATAITEM) {
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
                    uint64_t readValue =
                        ATOMIC_READ(&famResource->statusAndRefcount);
                    *resourceStatus = GET_STATUS(readValue);
                    ATOMIC_WRITE(&famResource->destroyed, true);
#else
                    famResourceManager->unregister_region_memory(famResource);
                    famResource->statusAndRefcount =
                        CONCAT_STATUS_REFCNT(RELEASED, 0);
                    *resourceStatus = RELEASED;
#endif
                } else {
                    uint64_t readValue =
                        ATOMIC_READ(&famResource->statusAndRefcount);
                    *resourceStatus = GET_STATUS(readValue);
                }
            } else {
                *resourceStatus = RELEASED;
            }
        }
    }
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_destroy_region);
}

void Fam_Memory_Service_Direct::resize_region(uint64_t regionId,
                                              size_t nbytes) {

    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()

    int newExtentIdx;
    allocator->resize_region(regionId, nbytes, &newExtentIdx);

    // Register the newly added extent if region registration is enabled for the
    // region
    if (!isSharedMemory) {
        Fam_Server_Resource *famResource =
            famResourceManager->find_resource(regionId);
        if (famResource) {
            if (famResource->permissionLevel == REGION) {
                // get the updated extent details
                Fam_Region_Extents_t extents;
                allocator->get_region_extents(regionId, &extents);
                famResourceManager->register_memory(
                    regionId, newExtentIdx - 1,
                    extents.addrList[newExtentIdx - 1],
                    extents.sizes[newExtentIdx - 1], famResource->accessType,
                    famResource);
            }
        }
    }
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_resize_region);
}

Fam_Region_Item_Info Fam_Memory_Service_Direct::allocate(uint64_t regionId,
                                                         size_t nbytes) {
    Fam_Region_Item_Info info;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()

    info.offset = allocator->allocate(regionId, nbytes);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_allocate);
    return info;
}

void Fam_Memory_Service_Direct::deallocate(uint64_t regionId, uint64_t offset) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    if (!isSharedMemory) {
        Fam_Server_Resource *famResource =
            famResourceManager->find_resource(regionId);
        if (famResource) {
            if (enableResourceRelease) {
                if (famResource->permissionLevel == DATAITEM) {
                    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
#ifdef ENABLE_RESOURCE_RELEASE_ITEM_PERM
                    /*
                     * If ENABLE_RESOURCE_RELEASE_ITEM_PERM is defined, resource
                     * relinquishmeet is enabled for region with DATAITEM level
                     * permission. In that case, the deallocation is delayed
                     * until the region is closed. This is done to avoid offset
                     * being reused because, offset is part of registration key.
                     */
                    famResourceManager->mark_deallocated(regionId, dataitemId,
                                                         famResource);
                    return;
#else
                    famResourceManager->unregister_memory(regionId, dataitemId,
                                                          famResource);
#endif
                }
            } else {
                if (famResource->permissionLevel == DATAITEM) {
                    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
                    famResourceManager->unregister_memory(regionId, dataitemId,
                                                          famResource);
                }
            }
        }
    }
    allocator->deallocate(regionId, offset);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_deallocate);

    return;
}

void *Fam_Memory_Service_Direct::get_local_pointer(uint64_t regionId,
                                                   uint64_t offset) {
    void *base;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    if (isBaseRequire)
        base = allocator->get_local_pointer(regionId, offset);
    else
        base = NULL;
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_local_pointer);
    return base;
}

void Fam_Memory_Service_Direct::copy(
    uint64_t srcRegionId, uint64_t *srcOffsets, uint64_t srcUsedMemsrvCnt,
    uint64_t srcCopyStart, uint64_t srcCopyEnd, uint64_t *srcKeys,
    uint64_t *srcBaseAddrList, uint64_t destRegionId, uint64_t destOffset,
    uint64_t destUsedMemsrvCnt, uint64_t *srcMemserverIds,
    uint64_t srcInterleaveSize, uint64_t destInterleaveSize, uint64_t size) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()

    // This function is used only in memory server model
    if (isSharedMemory)
        return;
    Fam_Context *famCtx = famOps->get_defaultCtx(uint64_t(0));

    std::vector<fi_addr_t> *fiAddr = famOps->get_fiAddrs();
    uint64_t destDisplacement = destOffset % destInterleaveSize;
    // Get the local pointer to destination FAM offset
    void *local = allocator->get_local_pointer(destRegionId, destOffset);
    uint64_t currentSrcOffset = srcCopyStart;
    // Vector to store fi_context pointer of each IO
    std::vector<struct fi_context *> fiCtxVector;
    uint64_t currentLocalPtr = (uint64_t)local;
    uint64_t localBufferSize;
    // Copy data to consecutive blocks till the end of the byte that needs to be
    // copied is reached
    while (currentSrcOffset < srcCopyEnd) {
        // If destination dataitem is present in only one memory server, the
        // local buffer size offered for copy should be equal to the total bytes
        // that needs to be copied, else if it is distributed across more than
        // one memory server the size of local buffer can be destination
        // dataitem inetreleave size or less than that based on the offset at
        // which data needs to be copied
        if (destUsedMemsrvCnt == 1) {
            localBufferSize = size;
        } else if (destDisplacement) {
            localBufferSize = destInterleaveSize - destDisplacement;
        } else {
            localBufferSize = destInterleaveSize;
        }
        // Check if the source dataitem is spread across more than one memory
        // server, if spread across multiple servers calculate the index of
        // first server, first block and the displacement within the block, else
        // issue a single IO to a memory server where that dataitem is located.
        if (srcUsedMemsrvCnt == 1) {
            // If both destination and source dataitems reside in same memory
            // server use memcpy to copy the data else pull data from remote
            // memory server
            if (memory_server_id == srcMemserverIds[0]) {
                void *srcLocalAddr = allocator->get_local_pointer(
                    srcRegionId, srcOffsets[0] + currentSrcOffset);
                memcpy(local, srcLocalAddr, localBufferSize);
            } else {
                // Issue an IO
                fi_context *ctx = fabric_read(
                    srcKeys[0], (void *)currentLocalPtr, localBufferSize,
                    (uint64_t)(srcBaseAddrList[0]) + currentSrcOffset,
                    (*fiAddr)[srcMemserverIds[0]], famCtx, true);
                // store the fi_context pointer to ensure the completion latter.
                fiCtxVector.push_back(ctx);
            }
            currentSrcOffset += (localBufferSize +
                                 (destUsedMemsrvCnt - 1) * destInterleaveSize);
            currentLocalPtr += localBufferSize;
            destDisplacement = 0;
            continue;
        }

        // Current src memory server id index
        uint64_t currentSrcServerIndex =
            ((currentSrcOffset / srcInterleaveSize) % srcUsedMemsrvCnt);
        // Current remote location in FAM
        uint64_t currentSrcFamPtr =
            (((currentSrcOffset / srcInterleaveSize) - currentSrcServerIndex) /
             srcUsedMemsrvCnt) *
            srcInterleaveSize;
        // Current Local pointer position
        // uint64_t currentLocalPtr = (uint64_t)local;
        // Displacement from the starting position of the interleave block
        uint64_t srcDisplacement = currentSrcOffset % srcInterleaveSize;

        uint64_t chunkSize;
        uint64_t nBytesRead = 0;
        /*
         * If the offset is displaced from a starting position of a block
         * issue a seperate IO for that chunk of data
         */
        uint64_t firstBlockSize = srcInterleaveSize - srcDisplacement;
        if (firstBlockSize < srcInterleaveSize) {
            // Calculate the chunk of size for each IO
            if (localBufferSize < firstBlockSize)
                chunkSize = localBufferSize;
            else
                chunkSize = firstBlockSize;
            // If both destination and source dataitems reside in same memory
            // server use memcpy to copy the data else pull data from remote
            // memory server
            if (memory_server_id == srcMemserverIds[currentSrcServerIndex]) {
                void *srcLocalAddr = allocator->get_local_pointer(
                    srcRegionId, srcOffsets[currentSrcServerIndex] +
                                     currentSrcFamPtr + srcDisplacement);
                memcpy((void *)currentLocalPtr, srcLocalAddr, chunkSize);
            } else {
                // Issue an IO
                fi_context *ctx = fabric_read(
                    srcKeys[currentSrcServerIndex], (void *)currentLocalPtr,
                    chunkSize,
                    (uint64_t)(srcBaseAddrList[currentSrcServerIndex]) +
                        currentSrcFamPtr + srcDisplacement,
                    (*fiAddr)[srcMemserverIds[currentSrcServerIndex]], famCtx,
                    true);
                // store the fi_context pointer to ensure the completion latter.
                fiCtxVector.push_back(ctx);
            }
            // go to next server for next block of data
            currentSrcServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentSrcServerIndex == srcUsedMemsrvCnt) {
                currentSrcServerIndex = 0;
                // Increment a block everytime cricles through used servers.
                currentSrcFamPtr += srcInterleaveSize;
            }
            // Increment the number of bytes written
            nBytesRead += chunkSize;
            // Increment the local buffer pointer by number of bytes written
            currentLocalPtr += chunkSize;
        }
        // Loop until the requested number of bytes are issued through IO
        while (nBytesRead < localBufferSize) {
            // Calculate the chunk of size for each IO
            //(For last IO the chunk size may not be same as inetrleave size)
            if ((localBufferSize - nBytesRead) < srcInterleaveSize)
                chunkSize = localBufferSize - nBytesRead;
            else
                chunkSize = srcInterleaveSize;
            // If both destination and source dataitems reside in same memory
            // server use memcpy to copy the data else pull data from remote
            // memory server
            if (memory_server_id == srcMemserverIds[currentSrcServerIndex]) {
                void *srcLocalAddr = allocator->get_local_pointer(
                    srcRegionId,
                    srcOffsets[currentSrcServerIndex] + currentSrcFamPtr);
                memcpy((void *)currentLocalPtr, srcLocalAddr, chunkSize);
            } else {
                // Issue an IO
                fi_context *ctx = fabric_read(
                    srcKeys[currentSrcServerIndex], (void *)currentLocalPtr,
                    chunkSize,
                    (uint64_t)(srcBaseAddrList[currentSrcServerIndex]) +
                        currentSrcFamPtr,
                    (*fiAddr)[srcMemserverIds[currentSrcServerIndex]], famCtx,
                    true);
                // store the fi_context pointer to ensure the completion latter.
                fiCtxVector.push_back(ctx);
            }
            // go to next server for next block of data
            currentSrcServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentSrcServerIndex == srcUsedMemsrvCnt) {
                currentSrcServerIndex = 0;
                currentSrcFamPtr += srcInterleaveSize;
            }
            // Increment the number of bytes written
            currentLocalPtr += chunkSize;
            // Increment the local buffer pointer by number of bytes written
            nBytesRead += chunkSize;
        }
        currentSrcOffset +=
            (localBufferSize + (destUsedMemsrvCnt - 1) * destInterleaveSize);
        destDisplacement = 0;
    }
    /*
     * Iterate over the vector of fi_context to ensure the completion of all IOs
     */
    if (!fiCtxVector.empty()) {
        famCtx->acquire_RDLock();
        for (auto ctx : fiCtxVector) {
            try {
                fabric_completion_wait(famCtx, ctx, 0);
            } catch (Fam_Exception &e) {
                famCtx->inc_num_rx_fail_cnt(1l);
                // Release Fam_Context read lock
                famCtx->release_lock();
                throw e;
            }
        }
        famCtx->release_lock();
    }

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_copy);
}

void Fam_Memory_Service_Direct::backup(
    uint64_t srcRegionId, uint64_t srcOffset, uint64_t size, uint64_t chunkSize,
    uint64_t usedMemserverCnt, uint64_t fileStartPos, const string BackupName,
    uint32_t uid, uint32_t gid, mode_t mode, const string dataitemName,
    uint64_t itemSize, bool writeMetadata) {
    ostringstream message;
    struct stat info;
    if (stat(fam_backup_path.c_str(), &info) == -1) {
        message << "Fam_Backup_Path doesn't exist. " << endl;
        THROW_ERRNO_MSG(Memory_Service_Exception, BACKUP_PATH_NOT_EXIST,
                        message.str().c_str());
    }
    std::string BackupNamePath = fam_backup_path + "/" + BackupName;

    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    allocator->backup(srcRegionId, srcOffset, size, chunkSize, usedMemserverCnt,
                      fileStartPos, BackupNamePath, uid, gid, mode,
                      dataitemName, itemSize, writeMetadata);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_backup);
}

void Fam_Memory_Service_Direct::restore(uint64_t destRegionId,
                                        uint64_t destOffset, uint64_t size,
                                        uint64_t chunkSize,
                                        uint64_t usedMemserverCnt,
                                        uint64_t fileStartPos,
                                        string BackupName) {
    ostringstream message;
    struct stat info;
    if (stat(fam_backup_path.c_str(), &info) == -1) {
        message << "Fam_Backup_Path doesn't exist. " << endl;
        THROW_ERRNO_MSG(Memory_Service_Exception, BACKUP_PATH_NOT_EXIST,
                        message.str().c_str());
    }
    std::string BackupNamePath = fam_backup_path + "/" + BackupName;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    allocator->restore(destRegionId, destOffset, size, chunkSize,
                       usedMemserverCnt, fileStartPos, BackupNamePath);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_restore);
}

Fam_Backup_Info
Fam_Memory_Service_Direct::get_backup_info(std::string BackupName, uint32_t uid,
                                           uint32_t gid, uint32_t mode) {
    Fam_Backup_Info info;
    ostringstream message;
    struct stat sinfo;
    if (stat(fam_backup_path.c_str(), &sinfo) == -1) {
        message << "Fam_Backup_Path doesn't exist. " << endl;
        THROW_ERRNO_MSG(Memory_Service_Exception, BACKUP_PATH_NOT_EXIST,
                        message.str().c_str());
    }
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    std::string BackupNamePath = fam_backup_path + "/" + BackupName;
    info = allocator->get_backup_info(BackupNamePath, uid, gid, mode);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_backup_info);
    return info;
}

std::string Fam_Memory_Service_Direct::list_backup(std::string BackupName,
                                                   uint32_t uid, uint32_t gid,
                                                   mode_t mode) {
    ostringstream message;
    struct stat info;
    std::string BackupNamePath;
    if (stat(fam_backup_path.c_str(), &info) == -1) {
        message << "Fam_Backup_Path doesn't exist. " << endl;
        THROW_ERRNO_MSG(Memory_Service_Exception, BACKUP_PATH_NOT_EXIST,
                        message.str().c_str());
    }

    std::size_t found = BackupName.find("*");
    if (found != std::string::npos)
        BackupNamePath = fam_backup_path;
    else
        BackupNamePath = fam_backup_path + "/" + BackupName;
    return (allocator->list_backup(BackupNamePath, uid, gid, mode));
}

void Fam_Memory_Service_Direct::delete_backup(std::string BackupName) {
    ostringstream message;
    struct stat info;
    if (stat(fam_backup_path.c_str(), &info) == -1) {
        message << "Fam_Backup_Path doesn't exist. " << endl;
        THROW_ERRNO_MSG(Memory_Service_Exception, BACKUP_PATH_NOT_EXIST,
                        message.str().c_str());
    }
    std::string backupnamePath = fam_backup_path + "/" + BackupName;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    allocator->delete_backup(backupnamePath);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_delete_backup);
}

size_t Fam_Memory_Service_Direct::get_addr_size() {
    size_t addrSize;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    // This function is used only in memory server model
    if (isSharedMemory)
        return 0;
    addrSize = famOps->get_addr_size();
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_addr_size);
    return addrSize;
}

void *Fam_Memory_Service_Direct::get_addr() {
    void *addr;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    // This function is used only in memory server model
    if (isSharedMemory)
        return NULL;
    addr = famOps->get_addr();
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_addr);
    return addr;
}

Fam_Memory_Type Fam_Memory_Service_Direct::get_memtype() {
    Fam_Memory_Type memory_type;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    memory_type = memServermemType;
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_memtype);
    return memory_type;
}

std::string Fam_Memory_Service_Direct::get_rpcaddr() {
    std::string rpcaddr;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    rpcaddr = rpc_interface;
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_rpcaddr);
    return rpcaddr;
}

void Fam_Memory_Service_Direct::acquire_CAS_lock(uint64_t offset) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    int idx = LOCKHASH(offset);
    pthread_mutex_lock(&casLock[idx]);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_acquire_CAS_lock);
}

void Fam_Memory_Service_Direct::release_CAS_lock(uint64_t offset) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    int idx = LOCKHASH(offset);
    pthread_mutex_unlock(&casLock[idx]);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_release_CAS_lock);
}

/*
 * This function only register region memory.
 */
void Fam_Memory_Service_Direct::register_region_memory(uint64_t regionId,
                                                       bool accessType) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()

    if (enableResourceRelease) {
        /*
         * Open the resource associated with the region.
         * here FAM_INIT_ONLY is set, so that the reference count is set to zero
         * and not incremented. FAM_REGISTER_MEMORY flag is set to register the
         * region memory.
         */
        famResourceManager->open_resource(regionId, REGION, accessType,
                                          FAM_REGISTER_MEMORY | FAM_INIT_ONLY);
    } else {
        /*
         * If resource relinquishment is not enabled, register region memory but
         * reference counts are not maintained.
         */
        famResourceManager->register_region_memory(regionId, accessType);
    }

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_register_region_memory);
}

/*
 * This function open the region and register the region memory.
 */
Fam_Region_Memory
Fam_Memory_Service_Direct::open_region_with_registration(uint64_t regionId,
                                                         bool accessType) {
    Fam_Region_Memory regionMemory;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    /*
     * Open the resource associated with the region.
     */
    Fam_Server_Resource *famResource = famResourceManager->open_resource(
        regionId, REGION, accessType, FAM_REGISTER_MEMORY);

    /*
     * Get the region memory information
     */
    regionMemory = famResourceManager->get_region_memory_info(
        regionId, accessType, famResource);

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_region_memory);
    return regionMemory;
}

/*
 * This function open the region, but does not register region memory.
 */
void Fam_Memory_Service_Direct::open_region_without_registration(
    uint64_t regionId) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    /*
     * Open the resource associated with the region.
     */
    famResourceManager->open_resource(regionId, DATAITEM, true,
                                      FAM_RESOURCE_DEFAULT);

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_region_memory);
}

/*
 * This function closes the region. Internally it reduces the reference count.
 * If reference count reaches zero, the resources associated with the region are
 * released back to the system.
 */
uint64_t Fam_Memory_Service_Direct::close_region(uint64_t regionId) {
    uint64_t status;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    /*
     * close the resource
     */
    status = famResourceManager->close_resource(regionId);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_close_region)
    return status;
}

/*
 * This function get memory registration details of already opened region.
 */
Fam_Region_Memory
Fam_Memory_Service_Direct::get_region_memory(uint64_t regionId,
                                             bool accessType) {
    Fam_Region_Memory regionMemory;
    ostringstream message;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    /*
     * Get the region memory information
     */
    regionMemory =
        famResourceManager->get_region_memory_info(regionId, accessType);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_region_memory);
    return regionMemory;
}

/*
 * This function does memory registration of dataitem.
 * Here dataitem ID is used as registration ID.
 */
Fam_Dataitem_Memory Fam_Memory_Service_Direct::get_dataitem_memory(
    uint64_t regionId, uint64_t offset, uint64_t size, bool accessType) {
    Fam_Dataitem_Memory dataitemMemory;
    ostringstream message;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    /*
     * Get the data item memory information
     */
    dataitemMemory = famResourceManager->get_dataitem_memory_info(
        regionId, offset, size, accessType);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_dataitem);
    return dataitemMemory;
}

/*
 * get_config_info - Obtain the required information from
 * fam_pe_config file. On Success, returns a map that has options updated
 * from from configuration file. Set default values if not found in config
 * file.
 */
configFileParams
Fam_Memory_Service_Direct::get_config_info(std::string filename) {
    configFileParams options;
    ostringstream message;

    config_info *info = NULL;
    if (filename.find("fam_memoryserver_config.yaml") != std::string::npos) {
        info = new yaml_config_info(filename);
        try {
            options["provider"] =
                (char *)strdup((info->get_key_value("provider")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["provider"] = (char *)strdup("sockets");
        }

        try {
            options["ATL_threads"] =
                (char *)strdup((info->get_key_value("ATL_threads")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["ATL_threads"] = (char *)strdup("0");
        }

        try {
            options["ATL_queue_size"] =
                (char *)strdup((info->get_key_value("ATL_queue_size")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["ATL_queue_size"] = (char *)strdup("1000");
        }

        try {
            options["ATL_data_size"] =
                (char *)strdup((info->get_key_value("ATL_data_size")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["ATL_data_size"] = (char *)strdup("1073741824");
        }

        try {
            options["delayed_free_threads"] = (char *)strdup(
                (info->get_key_value("delayed_free_threads")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["delayed_free_threads"] = (char *)strdup("0");
        }
        try {
            options["fam_backup_path"] = (char *)strdup(
                (info->get_key_value("fam_backup_path")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["fam_backup_path"] = (char *)strdup("");
        }
        try {
            options["resource_release"] = (char *)strdup(
                (info->get_key_value("resource_release")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["resource_release"] = (char *)strdup("enable");
        }
        try {
            options["Memservers:memory_type"] = (char *)strdup(
                (info->get_map_value("Memservers", memory_server_id,
                                     "memory_type"))
                    .c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            message << "memory_type option in the config file is invalid.";
            THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
        }
        try {
            options["Memservers:fam_path"] =
                (char *)strdup((info->get_map_value(
                                    "Memservers", memory_server_id, "fam_path"))
                                   .c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            message << "fam_path option in the config file is invalid.";
            THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
        }
        try {
            options["Memservers:libfabric_port"] = (char *)strdup(
                (info->get_map_value("Memservers", memory_server_id,
                                     "libfabric_port"))
                    .c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            message << "libfabric_port option in the config file is invalid.";
            THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
        }
        try {
            options["Memservers:rpc_interface"] = (char *)strdup(
                (info->get_map_value("Memservers", memory_server_id,
                                     "rpc_interface"))
                    .c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            message << "rpc_interface option in the config file is invalid.";
            THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
        }
        try {
            options["Memservers:if_device"] = (char *)strdup(
                (info->get_map_value("Memservers", memory_server_id,
                                     "if_device"))
                    .c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["Memservers:if_device"] = (char *)strdup("");
        }
        try {
            options["rpc_framework_type"] = (char *)strdup(
                (info->get_key_value("rpc_framework_type")).c_str());
        } catch (Fam_InvalidOption_Exception &e) {
            // If parameter is not present, then set the default.
            options["rpc_framework_type"] = (char *)strdup("grpc");
        }
    }
    return options;
}

void Fam_Memory_Service_Direct::init_atomic_queue() {
    allocator->create_ATL_root(memoryPerThread * numAtomicThreads);
    pthread_rwlock_init(&fiAddrLock, NULL);
    for (int i = 0; i < numAtomicThreads; i++) {
        int ret = atomicQ[i].create(allocator, i);
        if (ret) {
            cout << "Couldn't create ATL Queue " << i << endl;
            cout << "Disabling ATL" << endl;
            numAtomicThreads = 0;
            for (int j = 0; j < i; j++)
                pthread_cancel(atid[j]);
            break;
        }
        atomicTInfo[i].allocator = allocator;
        atomicTInfo[i].qId = i;
        ret = pthread_create(&(atid[i]), NULL, &process_queue,
                             (void *)&atomicTInfo[i]);
        if (ret) {
            cout << "Couldn't create processing thread for queue " << i
                 << " status = " << ret << endl;
            cout << "Disabling ATL" << endl;
            numAtomicThreads = 0;
            for (int j = 0; j < i; j++)
                pthread_cancel(atid[j]);
            break;
        }
    }
}

void Fam_Memory_Service_Direct::get_atomic(uint64_t regionId,
                                           uint64_t srcOffset,
                                           uint64_t dstOffset, uint64_t nbytes,
                                           uint64_t key, uint64_t srcBaseAddr,
                                           const char *nodeAddr,
                                           uint32_t nodeAddrSize) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    void *inpData = NULL;
    if (numAtomicThreads <= 0) {
        message << "Atomic Transfer Library is not enabled";
        throw Memory_Service_Exception(ATL_NOT_ENABLED, message.str().c_str());
    }
    string hashStr = "";
    hash<string> mystdhash;
    hashStr = hashStr + std::to_string(regionId);
    hashStr = hashStr + std::to_string(srcOffset);
    uint64_t qId = mystdhash(hashStr) % numAtomicThreads;
    atomicMsg InpMsg;
    memset(&InpMsg, 0, sizeof(atomicMsg));
    InpMsg.nodeAddrSize = nodeAddrSize;
    memcpy(&InpMsg.nodeAddr, nodeAddr, nodeAddrSize);
    InpMsg.dstDataGdesc.regionId = regionId;
    // InpMsg.dstDataGdesc.offset = srcOffset;
    InpMsg.offset = dstOffset;
    InpMsg.key = key;
    InpMsg.srcBaseAddr = srcBaseAddr;
    InpMsg.size = nbytes;
    InpMsg.flag |= ATOMIC_READ;

    int ret = atomicQ[qId].push(&InpMsg, inpData);
    if (ret) {
        if (ret == ATL_QUEUE_FULL) {
            message << "Atomic queue Full - Failed to insert";
            throw Memory_Service_Exception(ATL_QUEUE_FULL,
                                           message.str().c_str());
        } else {
            message << "error inserting into Queue";
            throw Memory_Service_Exception(ATL_QUEUE_INSERT_ERROR,
                                           message.str().c_str());
        }
    }
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_atomic)
}

void Fam_Memory_Service_Direct::put_atomic(
    uint64_t regionId, uint64_t srcOffset, uint64_t dstOffset, uint64_t nbytes,
    uint64_t key, uint64_t srcBaseAddr, const char *nodeAddr,
    uint32_t nodeAddrSize, const char *data) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    string hashStr = "";
    if (numAtomicThreads <= 0) {
        message << "Atomic Transfer Library is not enabled";
        throw Memory_Service_Exception(ATL_NOT_ENABLED, message.str().c_str());
    }
    hash<string> mystdhash;
    hashStr = hashStr + std::to_string(regionId);
    hashStr = hashStr + std::to_string(srcOffset);
    uint64_t qId = mystdhash(hashStr) % numAtomicThreads;
    atomicMsg InpMsg;
    memset(&InpMsg, 0, sizeof(atomicMsg));
    InpMsg.nodeAddrSize = nodeAddrSize;
    memcpy(&InpMsg.nodeAddr, nodeAddr, nodeAddrSize);
    InpMsg.dstDataGdesc.regionId = regionId;
    // InpMsg.dstDataGdesc.offset = srcOffset;
    InpMsg.offset = dstOffset;
    InpMsg.key = key;
    InpMsg.srcBaseAddr = srcBaseAddr;
    InpMsg.size = nbytes;
    InpMsg.flag |= ATOMIC_WRITE;
    if ((nbytes > 0) && (nbytes < MAX_DATA_IN_MSG)) {
        InpMsg.flag |= ATOMIC_CONTAIN_DATA;
    }

    int ret = atomicQ[qId].push(&InpMsg, data);
    if (ret) {
        if (ret == ATL_QUEUE_FULL) {
            message << "Atomic queue Full - Failed to insert";
            throw Memory_Service_Exception(ATL_QUEUE_FULL,
                                           message.str().c_str());
        } else {
            message << "Error inserting into Queue";
            throw Memory_Service_Exception(ATL_QUEUE_INSERT_ERROR,
                                           message.str().c_str());
        }
    }

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_put_atomic)
}

void Fam_Memory_Service_Direct::scatter_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    void *inpData = NULL;
    string hashStr = "";
    if (numAtomicThreads <= 0) {
        message << "Atomic Transfer Library is not enabled";
        throw Memory_Service_Exception(ATL_NOT_ENABLED, message.str().c_str());
    }
    hash<string> mystdhash;
    hashStr = hashStr + std::to_string(regionId);
    hashStr = hashStr + std::to_string(offset);
    uint64_t qId = mystdhash(hashStr) % numAtomicThreads;
    atomicMsg InpMsg;
    memset(&InpMsg, 0, sizeof(atomicMsg));
    InpMsg.nodeAddrSize = nodeAddrSize;
    memcpy(&InpMsg.nodeAddr, nodeAddr, nodeAddrSize);
    InpMsg.dstDataGdesc.regionId = regionId;
    // InpMsg.dstDataGdesc.offset = offset;
    InpMsg.snElements = nElements;
    InpMsg.firstElement = firstElement;
    InpMsg.stride = stride;
    InpMsg.selementSize = elementSize;
    InpMsg.key = key;
    InpMsg.srcBaseAddr = srcBaseAddr;
    InpMsg.flag |= ATOMIC_SCATTER_STRIDE;

    int ret = atomicQ[qId].push(&InpMsg, inpData);
    if (ret) {
        if (ret == ATL_QUEUE_FULL) {
            message << "Atomic queue Full - Failed to insert";
            throw Memory_Service_Exception(ATL_QUEUE_FULL,
                                           message.str().c_str());
        } else {
            message << "Error inserting into Queue";
            throw Memory_Service_Exception(ATL_QUEUE_INSERT_ERROR,
                                           message.str().c_str());
        }
    }
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_scatter_strided_atomic)
}

void Fam_Memory_Service_Direct::gather_strided_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    void *inpData = NULL;
    string hashStr = "";
    if (numAtomicThreads <= 0) {
        message << "Atomic Transfer Library is not enabled";
        throw Memory_Service_Exception(ATL_NOT_ENABLED, message.str().c_str());
    }
    hash<string> mystdhash;
    hashStr = hashStr + std::to_string(regionId);
    hashStr = hashStr + std::to_string(offset);
    uint64_t qId = mystdhash(hashStr) % numAtomicThreads;
    atomicMsg InpMsg;
    memset(&InpMsg, 0, sizeof(atomicMsg));
    InpMsg.nodeAddrSize = nodeAddrSize;
    memcpy(&InpMsg.nodeAddr, nodeAddr, nodeAddrSize);
    InpMsg.dstDataGdesc.regionId = regionId;
    // InpMsg.dstDataGdesc.offset = offset;
    InpMsg.snElements = nElements;
    InpMsg.firstElement = firstElement;
    InpMsg.stride = stride;
    InpMsg.selementSize = elementSize;
    InpMsg.key = key;
    InpMsg.srcBaseAddr = srcBaseAddr;
    InpMsg.flag |= ATOMIC_GATHER_STRIDE;

    int ret = atomicQ[qId].push(&InpMsg, inpData);
    if (ret) {
        if (ret == ATL_QUEUE_FULL) {
            message << "Atomic queue Full - Failed to insert";
            throw Memory_Service_Exception(ATL_QUEUE_FULL,
                                           message.str().c_str());
        } else {
            message << "Error inserting into Queue";
            throw Memory_Service_Exception(ATL_QUEUE_INSERT_ERROR,
                                           message.str().c_str());
        }
    }
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_gather_strided_atomic)
}

void Fam_Memory_Service_Direct::scatter_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    string hashStr = "";
    if (numAtomicThreads <= 0) {
        message << "Atomic Transfer Library is not enabled";
        throw Memory_Service_Exception(ATL_NOT_ENABLED, message.str().c_str());
    }
    hash<string> mystdhash;
    hashStr = hashStr + std::to_string(regionId);
    hashStr = hashStr + std::to_string(offset);
    uint64_t qId = mystdhash(hashStr) % numAtomicThreads;
    atomicMsg InpMsg;
    memset(&InpMsg, 0, sizeof(atomicMsg));
    InpMsg.nodeAddrSize = nodeAddrSize;
    memcpy(&InpMsg.nodeAddr, nodeAddr, nodeAddrSize);
    InpMsg.dstDataGdesc.regionId = regionId;
    // InpMsg.dstDataGdesc.offset = offset;
    InpMsg.inElements = nElements;
    InpMsg.ielementSize = elementSize;
    InpMsg.key = key;
    InpMsg.srcBaseAddr = srcBaseAddr;
    InpMsg.flag |= ATOMIC_SCATTER_INDEX;

    int ret = atomicQ[qId].push(&InpMsg, elementIndex);
    if (ret) {
        if (ret == ATL_QUEUE_FULL) {
            message << "Atomic queue Full - Failed to insert";
            throw Memory_Service_Exception(ATL_QUEUE_FULL,
                                           message.str().c_str());
        } else {
            message << "Error inserting into Queue";
            throw Memory_Service_Exception(ATL_QUEUE_INSERT_ERROR,
                                           message.str().c_str());
        }
    }
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_scatter_indexed_atomic)
}

void Fam_Memory_Service_Direct::gather_indexed_atomic(
    uint64_t regionId, uint64_t offset, uint64_t nElements,
    const void *elementIndex, uint64_t elementSize, uint64_t key,
    uint64_t srcBaseAddr, const char *nodeAddr, uint32_t nodeAddrSize) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    ostringstream message;
    string hashStr = "";
    if (numAtomicThreads <= 0) {
        message << "Atomic Transfer Library is not enabled";
        throw Memory_Service_Exception(ATL_NOT_ENABLED, message.str().c_str());
    }
    hash<string> mystdhash;
    hashStr = hashStr + std::to_string(regionId);
    hashStr = hashStr + std::to_string(offset);
    uint64_t qId = mystdhash(hashStr) % numAtomicThreads;
    atomicMsg InpMsg;
    memset(&InpMsg, 0, sizeof(atomicMsg));
    InpMsg.nodeAddrSize = nodeAddrSize;
    memcpy(&InpMsg.nodeAddr, nodeAddr, nodeAddrSize);
    InpMsg.dstDataGdesc.regionId = regionId;
    // InpMsg.dstDataGdesc.offset = offset;
    InpMsg.inElements = nElements;
    InpMsg.ielementSize = elementSize;
    InpMsg.key = key;
    InpMsg.srcBaseAddr = srcBaseAddr;
    InpMsg.flag |= ATOMIC_GATHER_INDEX;

    int ret = atomicQ[qId].push(&InpMsg, elementIndex);
    if (ret) {
        if (ret == ATL_QUEUE_FULL) {
            message << "Atomic queue Full - Failed to insert";
            throw Memory_Service_Exception(ATL_QUEUE_FULL,
                                           message.str().c_str());
        } else {
            message << "Error inserting into Queue";
            throw Memory_Service_Exception(ATL_QUEUE_INSERT_ERROR,
                                           message.str().c_str());
        }
    }
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_gather_indexed_atomic)
}

// get and set controlpath address functions
string Fam_Memory_Service_Direct::get_controlpath_addr() {
    return controlpath_addr;
}

void Fam_Memory_Service_Direct::set_controlpath_addr(string addr) {
    controlpath_addr = addr;
}

// get and set rpc_framework and rpc_protocol type
void Fam_Memory_Service_Direct::set_rpc_framework_protocol(
    configFileParams file_options) {
    rpc_framework_type = file_options["rpc_framework_type"];
    rpc_protocol_type = protocol_map(file_options["provider"]);
}

string Fam_Memory_Service_Direct::get_rpc_framework_type() {
    return rpc_framework_type;
}

string Fam_Memory_Service_Direct::get_rpc_protocol_type() {
    return rpc_protocol_type;
}

void Fam_Memory_Service_Direct::create_region_failure_cleanup(
    uint64_t regionId) {
    // Destroy the region
    allocator->destroy_region(regionId);

    if (!isSharedMemory) {
        // close the resources opened for the region
        famResourceManager->close_resource(regionId);
    }
}

} // namespace openfam
