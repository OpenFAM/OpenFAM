/*
 * fam_memory_service_direct.cpp
 * Copyright (c) 2020-2021 Hewlett Packard Enterprise Development, LP. All
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

    std::string memType = config_options["Memservers:memory_type"];
    fam_path = config_options["Memservers:fam_path"];
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
    libfabricPort = config_options["Memservers:libfabric_port"];

    rpc_interface = config_options["Memservers:rpc_interface"];
    std::string addr = rpc_interface.substr(0, rpc_interface.find(':'));
    if_device = config_options["Memservers:if_device"];
    libfabricProvider = config_options["provider"];

    int num_delayed_free_Threads =
        atoi(config_options["delayed_free_threads"].c_str());

    allocator =
        new Memserver_Allocator(num_delayed_free_Threads, fam_path.c_str());
    fam_backup_path = config_options["fam_backup_path"];
    struct stat info;
    if (stat(fam_backup_path.c_str(), &info) == -1) {
        if ((errno == ENOENT) || (errno == ENOTDIR))
            mkdir(fam_backup_path.c_str(),
                  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    if (isSharedMemory) {
        memoryRegistration = new Fam_Memory_Registration_SHM();
    } else {
        memoryRegistration = new Fam_Memory_Registration_Libfabric(
            addr.c_str(), libfabricPort.c_str(), libfabricProvider.c_str());
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
    delete allocator;
    delete memoryRegistration;
}

void Fam_Memory_Service_Direct::reset_profile() {

    MEMSERVER_PROFILE_INIT(MEMORY_SERVICE_DIRECT)
    MEMSERVER_PROFILE_START_TIME(MEMORY_SERVICE_DIRECT)
    allocator->reset_profile();
    memoryRegistration->reset_profile();
    return;
}

void Fam_Memory_Service_Direct::dump_profile() {
    MEMORY_SERVICE_DIRECT_PROFILE_DUMP();
    allocator->dump_profile();
    memoryRegistration->dump_profile();
}

void Fam_Memory_Service_Direct::create_region(uint64_t regionId,
                                              size_t nbytes) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()

    allocator->create_region(regionId, nbytes);

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_create_region);
}

void Fam_Memory_Service_Direct::destroy_region(uint64_t regionId) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()

    allocator->destroy_region(regionId);

    memoryRegistration->deregister_region_memory(regionId);

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_destroy_region);
}

void Fam_Memory_Service_Direct::resize_region(uint64_t regionId,
                                              size_t nbytes) {

    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()

    allocator->resize_region(regionId, nbytes);

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_resize_region);
}

Fam_Region_Item_Info Fam_Memory_Service_Direct::allocate(uint64_t regionId,
                                                         size_t nbytes) {
    Fam_Region_Item_Info info;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()

    uint64_t offset = allocator->allocate(regionId, nbytes);
    void *base = allocator->get_local_pointer(regionId, offset);

    info.offset = offset;
    if (memoryRegistration->is_base_require()) {
        info.base = base;
    } else {
        info.base = NULL;
    }
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_allocate);
    return info;
}

void Fam_Memory_Service_Direct::deallocate(uint64_t regionId, uint64_t offset) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    ostringstream message;

    allocator->deallocate(regionId, offset);

    memoryRegistration->deregister_memory(regionId, offset);

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_deallocate);

    return;
}

void *Fam_Memory_Service_Direct::get_local_pointer(uint64_t regionId,
                                                   uint64_t offset) {
    void *base;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    if (memoryRegistration->is_base_require())
        base = allocator->get_local_pointer(regionId, offset);
    else
        base = NULL;
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_local_pointer);
    return base;
}

void Fam_Memory_Service_Direct::copy(uint64_t srcRegionId, uint64_t srcOffset,
                                     uint64_t srcKey, uint64_t srcCopyStart,
                                     uint64_t srcBaseAddr, const char *srcAddr,
                                     uint32_t srcAddrLen, uint64_t destRegionId,
                                     uint64_t destOffset, uint64_t size,
                                     uint64_t srcMemserverId,
                                     uint64_t destMemserverId) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    // srcOffset/destOffset - offset within the region, used only when src and
    // dest memory server are same.

    // srcCopyStart - offset within the src data Item from where data is copied
    // to dest. It is used to read source data item from source memory server
    // using fabric_read, i.e, when src and dest memory server are different.

    if (srcMemserverId == destMemserverId)
        allocator->copy(srcRegionId, srcOffset, destRegionId, destOffset, size);
    else {
        // Get memservermap
        ostringstream message;
        Fam_Ops_Libfabric *famOps =
            ((Fam_Memory_Registration_Libfabric *)memoryRegistration)
                ->get_famOps();
        std::map<uint64_t, fi_addr_t> *fiMemsrvMap = famOps->get_fiMemsrvMap();
        std::map<uint64_t, std::pair<void *, size_t>> *memServerAddrs =
            famOps->get_memServerAddrs();
        fi_addr_t srcFiAddr = FI_ADDR_UNSPEC;

        // Add or replace srcMemserver addr in the map
        std::pair<void *, size_t> srcMemSrv;

        // Start by taking a readlock on fiMemsrvMap
        pthread_rwlock_rdlock(famOps->get_memsrvaddr_lock());

        auto obj = memServerAddrs->find(srcMemserverId);
        if (obj != memServerAddrs->end()) {
            srcMemSrv = obj->second;
            if (strncmp((const char *)srcMemSrv.first, srcAddr, srcAddrLen) ==
                0) {
                // Get fabric addr from the map
                auto fiAddrObj = fiMemsrvMap->find(srcMemserverId);
                if (fiAddrObj != fiMemsrvMap->end())
                    srcFiAddr = fiAddrObj->second;
            }
        }

        // Release readlock on fiMemsrvMap
        pthread_rwlock_unlock(famOps->get_memsrvaddr_lock());

        // Register srcMemserver in the libfabric
        if (srcFiAddr == FI_ADDR_UNSPEC) {
            // Take a writelock on fiMemsrvMap
            pthread_rwlock_wrlock(famOps->get_memsrvaddr_lock());
            // Check if anyone other thread already registered the srcMemserver
            auto obj = memServerAddrs->find(srcMemserverId);
            auto fiAddrObj = fiMemsrvMap->find(srcMemserverId);
            if (obj != memServerAddrs->end()) {
                srcMemSrv = obj->second;
                if (strncmp((const char *)srcMemSrv.first, srcAddr,
                            srcAddrLen) == 0) {
                    // Get fabric addr from the map
                    if (fiAddrObj != fiMemsrvMap->end())
                        srcFiAddr = fiAddrObj->second;
                }
            }
            if (srcFiAddr == FI_ADDR_UNSPEC) {
                std::vector<fi_addr_t> fiAddrVector;
                if (fabric_insert_av(srcAddr, famOps->get_av(),
                                     &fiAddrVector) == -1) {
                    // Release writelock on fiMemsrvMap
                    pthread_rwlock_unlock(famOps->get_memsrvaddr_lock());
                    // raise an exception
                    message << "fabric_insert_av failed: libfabric error";
                    THROW_ERRNO_MSG(Memory_Service_Exception, LIBFABRIC_ERROR,
                                    message.str().c_str());
                }

                // save the registered address in the fiMemsrvMap
                srcFiAddr = fiAddrVector[0];
                // Make a copy of the srcAddr to save in the map.
                void *srcMemAddr = calloc(1, srcAddrLen);

                memcpy(srcMemAddr, srcAddr, srcAddrLen);
                if (obj != memServerAddrs->end()) {
                    // Free the existing src memory server address, before
                    // replacing with the new src addr.
                    free(srcMemSrv.first);
                    obj->second = std::make_pair(srcMemAddr, srcAddrLen);
                } else
                    memServerAddrs->insert(
                        {srcMemserverId,
                         std::make_pair(srcMemAddr, srcAddrLen)});
                if (fiAddrObj != fiMemsrvMap->end())
                    fiAddrObj->second = srcFiAddr;
                else
                    fiMemsrvMap->insert({srcMemserverId, srcFiAddr});
            }
            // Release writelock on fiMemsrvMap
            pthread_rwlock_unlock(famOps->get_memsrvaddr_lock());
        }

        // perform fabric_read (blocking) on the source data item
        // do mem copy - read directly to the destination location.
        void *destPtr = allocator->get_local_pointer(destRegionId, destOffset);
        if (fabric_read(srcKey, destPtr, size, (srcCopyStart + srcBaseAddr),
                        srcFiAddr, famOps->get_defaultCtx(uint64_t(0))) != 0) {
            // raise exception
            message << "fabric_read failed: libfabric error";
            THROW_ERRNO_MSG(Memory_Service_Exception, LIBFABRIC_ERROR,
                            message.str().c_str());
        }
    }

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_copy);
}

void Fam_Memory_Service_Direct::backup(uint64_t srcRegionId, uint64_t srcOffset,
                                       const string BackupName, uint64_t size,
                                       uint32_t uid, uint32_t gid, mode_t mode,
                                       const string dataitemName) {
    ostringstream message;
    std::string BackupNamePath = fam_backup_path + "/" + BackupName;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    allocator->backup(srcRegionId, srcOffset, BackupNamePath, size, uid, gid,
                      mode, dataitemName);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_backup);
}

void Fam_Memory_Service_Direct::restore(uint64_t destRegionId,
                                        uint64_t destOffset, string BackupName,
                                        uint64_t size) {
    ostringstream message;
    std::string BackupNamePath = fam_backup_path + "/" + BackupName;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    allocator->restore(destRegionId, destOffset, BackupNamePath, size);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_restore);
}

Fam_Backup_Info
Fam_Memory_Service_Direct::get_backup_info(std::string BackupName, uint32_t uid,
                                           uint32_t gid, uint32_t mode) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    std::string BackupNamePath = fam_backup_path + "/" + BackupName;
    Fam_Backup_Info info =
        allocator->get_backup_info(BackupNamePath, uid, gid, mode);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_backup_info);
    return info;
}

std::string Fam_Memory_Service_Direct::list_backup(std::string BackupName,
                                                   uint32_t uid, uint32_t gid,
                                                   mode_t mode) {

    std::string BackupNamePath;
    std::size_t found = BackupName.find("*");
    if (found != std::string::npos)
        BackupNamePath = fam_backup_path;
    else
        BackupNamePath = fam_backup_path + "/" + BackupName;
    return (allocator->list_backup(BackupNamePath, uid, gid, mode));
}

void Fam_Memory_Service_Direct::delete_backup(std::string BackupName) {
    std::string backupnamePath = fam_backup_path + "/" + BackupName;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    allocator->delete_backup(backupnamePath);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_delete_backup);
}

size_t Fam_Memory_Service_Direct::get_addr_size() {
    size_t addrSize;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    addrSize = memoryRegistration->get_addr_size();
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_addr_size);
    return addrSize;
}

void *Fam_Memory_Service_Direct::get_addr() {
    void *addr;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    addr = memoryRegistration->get_addr();
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

uint64_t Fam_Memory_Service_Direct::get_key(uint64_t regionId, uint64_t offset,
                                            uint64_t size, bool rwFlag) {
    uint64_t key;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    void *base = allocator->get_local_pointer(regionId, offset);
    key = memoryRegistration->register_memory(regionId, offset, base, size,
                                              rwFlag);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_get_key);
    return key;
}

/*
 * get_config_info - Obtain the required information from
 * fam_pe_config file. On Success, returns a map that has options updated from
 * from configuration file. Set default values if not found in config file.
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
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["provider"] = (char *)strdup("sockets");
        }

        try {
            options["ATL_threads"] =
                (char *)strdup((info->get_key_value("ATL_threads")).c_str());
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["ATL_threads"] = (char *)strdup("0");
        }

        try {
            options["ATL_queue_size"] =
                (char *)strdup((info->get_key_value("ATL_queue_size")).c_str());
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["ATL_queue_size"] = (char *)strdup("1000");
        }

        try {
            options["ATL_data_size"] =
                (char *)strdup((info->get_key_value("ATL_data_size")).c_str());
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["ATL_data_size"] = (char *)strdup("1073741824");
	}

        try {
            options["delayed_free_threads"] = (char *)strdup(
                (info->get_key_value("delayed_free_threads")).c_str());
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["delayed_free_threads"] = (char *)strdup("0");
        }
        try {
            options["fam_backup_path"] = (char *)strdup(
                (info->get_key_value("fam_backup_path")).c_str());
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["fam_backup_path"] = (char *)strdup("");
            cout << "Couldn't locate fam_backup_path" << endl;
        }
        try {
            options["Memservers:memory_type"] = (char *)strdup(
                (info->get_map_value("Memservers", memory_server_id,
                                     "memory_type"))
                    .c_str());
        } catch (Fam_InvalidOption_Exception e) {
            message << "memory_type option in the config file is invalid.";
            THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
        }
        try {
            options["Memservers:fam_path"] =
                (char *)strdup((info->get_map_value(
                                    "Memservers", memory_server_id, "fam_path"))
                                   .c_str());
        } catch (Fam_InvalidOption_Exception e) {
            message << "fam_path option in the config file is invalid.";
            THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
        }
        try {
            options["Memservers:libfabric_port"] = (char *)strdup(
                (info->get_map_value("Memservers", memory_server_id,
                                     "libfabric_port"))
                    .c_str());
        } catch (Fam_InvalidOption_Exception e) {
            message << "libfabric_port option in the config file is invalid.";
            THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
        }
        try {
            options["Memservers:rpc_interface"] = (char *)strdup(
                (info->get_map_value("Memservers", memory_server_id,
                                     "rpc_interface"))
                    .c_str());
        } catch (Fam_InvalidOption_Exception e) {
            message << "rpc_interface option in the config file is invalid.";
            THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
        }
        try {
            options["Memservers:if_device"] = (char *)strdup(
                (info->get_map_value("Memservers", memory_server_id,
                                     "if_device"))
                    .c_str());
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["Memservers:if_device"] = (char *)strdup("ib0");
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
    InpMsg.dstDataGdesc.offset = srcOffset;
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
    InpMsg.dstDataGdesc.offset = srcOffset;
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
    InpMsg.dstDataGdesc.offset = offset;
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
    InpMsg.dstDataGdesc.offset = offset;
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
    InpMsg.dstDataGdesc.offset = offset;
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
    InpMsg.dstDataGdesc.offset = offset;
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

} // namespace openfam
