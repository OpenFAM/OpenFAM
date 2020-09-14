/*
 * fam_memory_service_direct.cpp
 * Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All rights
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
#include "fam_memory_service_direct.h"
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

Fam_Memory_Service_Direct::Fam_Memory_Service_Direct(
    const char *name, const char *libfabricPort = NULL,
    const char *libfabricProvider = NULL) {

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
    }

    libfabricPort =
        (libfabricPort == NULL ? config_options["libfabric_port"].c_str()
                               : libfabricPort);

    libfabricProvider =
        (libfabricProvider == NULL ? config_options["provider"].c_str()
                                   : libfabricProvider);

    allocator = new Memserver_Allocator();
#ifdef SHM
    memoryRegistration = new Fam_Memory_Registration_SHM();
#else
    memoryRegistration = new Fam_Memory_Registration_Libfabric(name, libfabricPort,
                                                           libfabricProvider);
#endif
    for (int i = 0; i < CAS_LOCK_CNT; i++) {
        (void)pthread_mutex_init(&casLock[i], NULL);
    }
}

Fam_Memory_Service_Direct::~Fam_Memory_Service_Direct() {
    allocator->memserver_allocator_finalize();
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

uint64_t Fam_Memory_Service_Direct::create_region(size_t nbytes) {
    uint64_t regionId;
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()

    regionId = allocator->create_region(nbytes);

    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_create_region);
    return regionId;
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
                                     uint64_t destRegionId, uint64_t destOffset,
                                     uint64_t size) {
    MEMORY_SERVICE_DIRECT_PROFILE_START_OPS()
    allocator->copy(srcRegionId, srcOffset, destRegionId, destOffset, size);
    MEMORY_SERVICE_DIRECT_PROFILE_END_OPS(mem_direct_copy);
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
            options["libfabric_port"] =
                (char *)strdup((info->get_key_value("libfabric_port")).c_str());
        } catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["libfabric_port"] = (char *)strdup("7500");
        }
    }
    return options;
}

} // namespace openfam
