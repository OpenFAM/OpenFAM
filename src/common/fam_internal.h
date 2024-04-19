/*
 * fam_internal.h
 * Copyright (c) 2017, 2018, 2023 Hewlett Packard Enterprise Development, LP.
 * All rights reserved. Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the following conditions
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
 * Created Oct 22, 2017, by Sharad Singhal
 * Modified Nov 1, 2017, by Sharad Singhal, added C API
 * Modified Nov 2, 2017, by Sharad Singhal based on discussion with Kim Keeton
 * Modified Nov 3, 2017, by Kim Keeton based on discussion with Sharad Singhal
 * Modified Dec 10, 2017, by Sharad Singhal to C11. Initial distribution for
 * comments Modified Dec 13, 2017, by Sharad Singhal to update to OpenFAM-API
 * version 1.01 Modified Feb 20, 2018, by Sharad Singhal to update to
 * OpenFAM-API version 1.02 Modified Feb 23, 2018, by Kim Keeton to update to
 * OpenFAM-API version 1.03 Modified Apr 24, 2018, by Sharad Singhal to update
 * to OpenFAM-API version 1.04 Modified Oct 5, 2018, by Sharad Singhal to
 * include C++ definitions Modifed Oct 22, 2018 by Sharad Singhal to separate
 * out C and C11 definitions
 *
 * Work in progress, UNSTABLE
 * Uses _Generic and 128-bit integer types, tested under gcc 6.3.0. May require
 * “-std=c11” compiler flag if you are using the generic API as documented in
 * OpenFAM-API-v104.
 *
 * Programming conventions used in the API:
 * APIs are defined using underscore separated words. Types start with initial
 * capitals, while names of function calls start with lowercase letters.
 * Variable and parameter names are camelCase with lower case initial letter.
 * All APIs have the prefix "Fam_" or "fam_" depending on whether the name
 * represents a type or a function.
 *
 * Where multiple methods representing same function with different data types
 * in the signature exist, generics are used to map the functions to the same
 * name.
 *
 * Where different types are involved (e.g. in atomics) and the method signature
 * is not sufficient to separate out the function calls, we follow the SHMEM
 * convention of adding _TYPE as a suffix for the C API.
 *
 */
#ifndef FAM_INTERNAL_H_
#define FAM_INTERNAL_H_

#include <iostream>
#include <map>
#include <stdint.h> // needed for uint64_t etc.
#include <string>
#include <sys/stat.h> // needed for mode_t
#include <unordered_map>

#include "radixtree/kvs.h"
#include "radixtree/radix_tree.h"

#include "fam/fam.h"
#include "fam/fam_exception.h"
#include "nvmm/epoch_manager.h"
#include "nvmm/memory_manager.h"
#include <nvmm/fam.h>
#include <nvmm/global_ptr.h>

#include <grpcpp/grpcpp.h>

using namespace radixtree;
using namespace nvmm;
using namespace std;

#ifdef __cplusplus
/** C++ Header
 *  The header is defined as a single interface containing all desired methods
 */
namespace openfam {

/*
 * Following keys are used for shared memory datapath access
 */
#define FAM_READ_KEY_SHM ((uint64_t)0x1)
#define FAM_WRITE_KEY_SHM ((uint64_t)0x2)
#define FAM_RW_KEY_SHM (FAM_READ_KEY_SHM | FAM_WRITE_KEY_SHM)

#define FABRIC_KEY_START 4
#define FABRIC_MAX_KEY 65536
#define BYTE 8

#define FAM_KEY_UNINITIALIZED ((uint64_t)-1)
#define FAM_KEY_INVALID ((uint64_t)-2)
#define FAM_FENCE_KEY (FABRIC_KEY_START-1)
#define INVALID_OFFSET ((uint64_t)-1)
#define FAM_INVALID_REGION ((uint64_t)-1)
#define MAX_MEMORY_SERVERS_CNT 256
/*
 * Region id 5-15 are reserved for MODC
 * Region id 16-20 are reserved for OpenFAM
 * region id 16 is used for metadata service
 * fam_create_region() will allocate region id starting from 21
 */
#define MEMSERVER_REGIONID_START 21
#define REGIONID_BITS 14
#define MEMSERVERID_SHIFT REGIONID_BITS
#define REGIONID_MASK ((1UL << REGIONID_BITS) - 1)
#define REGIONID_SHIFT 34
#define DATAITEMID_BITS 33
#define DATAITEMID_MASK ((1UL << DATAITEMID_BITS) - 1)
#define DATAITEMID_SHIFT 1
#define RPC_PROTOCOL_CXI "ofi+cxi://"
#define RPC_PROTOCOL_VERBS "ofi+verbs://"
#define RPC_PROTOCOL_SOCKETS "ofi+sockets://"
#define RPC_PROTOCOL_TCP "ofi+tcp://"

#define ADDR_SIZE 20

#define FAM_UNIMPLEMENTED_MEMSRVMODEL()                                        \
    {                                                                          \
        std::ostringstream message;                                            \
        message << __func__                                                    \
                << " is Not Yet Implemented for memory server model !!!";      \
        throw Fam_Unimplemented_Exception(message.str().c_str());              \
    }

#define STATUS_CHECK(exception)                                                \
    {                                                                          \
        if (status.ok()) {                                                     \
            if (res.errorcode()) {                                             \
                throw exception((enum Fam_Error)res.errorcode(),               \
                                (res.errormsg()).c_str());                     \
            }                                                                  \
        } else {                                                               \
            throw exception(FAM_ERR_RPC, (status.error_message()).c_str());    \
        }                                                                      \
    }

#define RPC_STATUS_CHECK(exception, response)                                  \
    {                                                                          \
        if (!response.get_status()) {                                          \
            if (response.get_errorcode()) {                                    \
                throw exception((enum Fam_Error)response.get_errorcode(),      \
                                (response.get_errormsg()).c_str());            \
            }                                                                  \
        }                                                                      \
    }

#define FAM_DEFAULT_CTX_ID (uint64_t(0))
#define FAM_CTX_ID_UNINITIALIZED ((uint64_t)-1)

/*
 * Maximum entries in the garbage queue used for resource relinquishment
 */
#define MAX_GARBAGE_ENTRY 1024

/*
 * States to represent FAM resource
 *
 * ACTIVE   : Fam resource is active and used by one more client
 * INACTIVE : Fam resource is inactive, a new region resource is added in
 * INACTIVE state BUSY     : This state signifies that the current resource is
 * currently being updated by other client RELEASED : Resource is released back
 * to the system
 */
#define ACTIVE 1UL
#define INACTIVE 2UL
#define BUSY 3UL
#define RELEASED 4UL

/* Read the value atomically */
#define ATOMIC_READ(obj) std::atomic_load(obj)

/* Write the value atomically */
#define ATOMIC_WRITE(obj, value) std::atomic_store(obj, value)

/* Atomic compare swap operation */
#define ATOMIC_CAS(obj, expected, desired)                                     \
    std::atomic_compare_exchange_weak(obj, expected, desired)

/* Combines status and reference count fields */
#define CONCAT_STATUS_REFCNT(status, refcnt)                                   \
    ((uint64_t)status << 32) | ((uint64_t)refcnt & 0xffffffff)

#define REFCNT_BITS 32
#define STATUS_SHIFT REFCNT_BITS
#define REFCNT_MASK ((1UL << REFCNT_BITS) - 1)

/* Parse status from combined value */
#define GET_STATUS(obj) obj >> STATUS_SHIFT
/* Parse reference count from combined value */
#define GET_REFCNT(obj) obj &REFCNT_MASK

/*
 * Lock the region resource to synchronize resource update operation.
 * the resource state is changed to BUSY
 */
#define LOCK_REGION_RESOURCE(obj) lock_region_resource(obj)
inline bool lock_region_resource(std::atomic<uint64_t> *obj) {
    uint64_t readValue, newValue = 0;
    do {
        readValue = ATOMIC_READ(obj);
        uint64_t status = GET_STATUS(readValue);
        uint64_t refCount = GET_REFCNT(readValue);
        if (status == BUSY) {
            /*TODO: Add sleep here*/
            continue;
        }
        if (status != ACTIVE) {
            return false;
        }
        newValue = CONCAT_STATUS_REFCNT(BUSY, refCount);
    } while (!ATOMIC_CAS(obj, &readValue, newValue));
    return true;
}

/*
 * Unlock the region resource, change state back from BUSY to ACTIVE.
 */
#define UNLOCK_REGION_RESOURCE(obj) unlock_region_resource(obj)
inline void unlock_region_resource(std::atomic<uint64_t> *obj) {
    uint64_t readValue, newValue;
    do {
        readValue = ATOMIC_READ(obj);
        uint64_t status = GET_STATUS(readValue);
        uint64_t refCount = GET_REFCNT(readValue);
        if (status != BUSY) {
            return;
        }
        newValue = CONCAT_STATUS_REFCNT(ACTIVE, refCount);
    } while (!ATOMIC_CAS(obj, &readValue, newValue));
}

/*
 * Helper to change status field in combined value
 */
#define CHANGE_STATUS(obj, newStatus)                                          \
    {                                                                          \
        uint64_t readValue, newValue;                                          \
        do {                                                                   \
            readValue = ATOMIC_READ(obj);                                      \
            uint64_t status = GET_STATUS(readValue);                           \
            uint64_t refCount = GET_REFCNT(readValue);                         \
            if (status == RELEASED)                                            \
                return false;                                                  \
            if (status == BUSY)                                                \
                /*TODO: Add sleep here*/                                       \
                continue;                                                      \
            newValue = CONCAT_STATUS_REFCNT(status, refCount);                 \
        } while (!ATOMIC_CAS(obj, &readValue, newValue));                      \
        return true;                                                           \
    }

/*
 * Helper to change both status and reference count in combined value
 */
#define CHANGE_STATUS_REFCNT(obj, status, refCount)                            \
    {                                                                          \
        uint64_t readValue, newValue;                                          \
        do {                                                                   \
            readValue = ATOMIC_READ(obj);                                      \
            newValue = CONCAT_STATUS_REFCNT(status, refCount);                 \
        } while (!ATOMIC_CAS(obj, &readValue, newValue));                      \
    }

/*
 * Flags used while opening the resources
 */
typedef enum {
    FAM_RESOURCE_DEFAULT = 0,
    FAM_REGISTER_MEMORY = 1 << 0,
    FAM_INIT_ONLY = 2 << 1
} Fam_Resource_Flag;

using Server_Map = std::map<uint64_t, std::pair<std::string, uint64_t>>;

/*
 * This structure represent a region memory in FAM
 * keys : This is a vector of memory registration keys, each element corresponds
 * to a region
 * extent base : This is a vector of base address of a region
 * extent, each element corresponds to a region extent There is a one to one
 * mapping between key vector to base address vector.
 */
typedef struct {
    std::vector<uint64_t> keys;
    std::vector<uint64_t> base;
} Fam_Region_Memory;

using Fam_Region_Memory_Map = std::unordered_map<uint64_t, Fam_Region_Memory>;

/*
 * This structure represent a dataitem in FAM
 * key : Dataitem memory registration key
 * base : Dataitem base address
 */
typedef struct {
    uint64_t key;
    uint64_t base;
} Fam_Dataitem_Memory;

/**
 *  DataItem Metadata descriptor
 */
typedef struct {
    uint64_t regionId;
    uint64_t offset;
    uint64_t dataitemOffsets[MAX_MEMORY_SERVERS_CNT];
    uint64_t key;
    uint64_t dataitemKeys[MAX_MEMORY_SERVERS_CNT];
    uint64_t size;
    uint32_t uid;
    uint32_t gid;
    mode_t perm;
    Fam_Redundancy_Level redundancyLevel;
    Fam_Memory_Type memoryType;
    Fam_Interleave_Enable interleaveEnable;
    uint64_t base;
    uint64_t baseAddressList[MAX_MEMORY_SERVERS_CNT];
    char name[RadixTree::MAX_KEY_LEN];
    // uint64_t memoryServerId;
    uint64_t used_memsrv_cnt;
    uint64_t memoryServerIds[MAX_MEMORY_SERVERS_CNT];
    size_t maxNameLen;
    uint64_t interleaveSize;
    Fam_Permission_Level permissionLevel;
    Fam_Region_Memory_Map regionMemoryMap;
    bool itemRegistrationStatus;
} Fam_Region_Item_Info;

typedef struct {
    string bname; // Backup Name
    string dname; // Data Item Name
    int64_t size; // Backup Size
    uid_t uid;    // User ID of owner
    gid_t gid;    // Group ID of owner
    mode_t mode;  // File Type and Mode
    string backupTime;

} Fam_Backup_Info;

// Input string contains <node-id>:<ipaddr>:<grpc-port>,<node-id>:...
inline Server_Map parse_server_list(std::string memServer,
                                    std::string delimiter1,
                                    std::string delimiter2) {
    Server_Map memoryServerList;
    uint64_t prev1 = 0, pos1 = 0;
    do {
        pos1 = memServer.find(delimiter1, prev1);
        if (pos1 == string::npos)
            pos1 = memServer.length();
        std::string token = memServer.substr(prev1, pos1 - prev1);
        if (!token.empty()) {
            uint64_t prev2 = 0, pos2 = 0, count = 0, nodeid = 0, portid = 0;
            std::string ipaddr;
            do {
                pos2 = token.find(delimiter2, prev2);
                if (pos2 == string::npos)
                    pos2 = token.length();
                std::string token2 = token.substr(prev2, pos2 - prev2);
                if (!token2.empty()) {
                    if (count % 3 == 0) {
                        nodeid = stoull(token2);
                    } else if (count % 3 == 1) {
                        ipaddr = token2;
                    } else {
                        portid = stoull(token2);
                        memoryServerList.insert(
                            {nodeid, std::make_pair(ipaddr, portid)});
                    }
                    count++;
                }
                prev2 = pos2 + delimiter2.length();
            } while (pos2 < token.length() && prev2 < token.length());
        }
        prev1 = pos1 + delimiter1.length();
    } while (pos1 < memServer.length() && prev1 < memServer.length());

    return memoryServerList;
}

inline void openfam_persist(void *addr, uint64_t size) {
#ifdef USE_FAM_PERSIST
    fam_persist(addr, size);
#endif
}

inline void openfam_invalidate(void *addr, uint64_t size) {
#ifdef USE_FAM_INVALIDATE
    fam_invalidate(addr, size);
#endif
}

inline void decode_offset(uint64_t offset, int *extentIdx, uint64_t *startPos) {
    *extentIdx = (int)GlobalPtr(offset).GetShelfId().GetShelfIndex();
    *startPos = (uint64_t)GlobalPtr(offset).GetOffset();
}

inline string protocol_map(string provider) {
    std::map<std::string, int> providertypes;
    string protocol;
    providertypes["cxi"] = 1;
    providertypes["verbs"] = 2;
    providertypes["sockets"] = 3;
    providertypes["tcp"] = 4;
    int provider_num = providertypes[provider];

    switch (provider_num) {
    case 1:
        protocol = RPC_PROTOCOL_CXI;
        break;
    case 2:
        protocol = RPC_PROTOCOL_VERBS;
        break;
    case 3:
        protocol = RPC_PROTOCOL_SOCKETS;
        break;
    case 4:
        protocol = RPC_PROTOCOL_TCP;
        break;
    }
    return protocol;
}

} // namespace openfam

#endif /* end of C/C11 Headers */

#endif /* end of FAM__INTERNAL_H_ */
