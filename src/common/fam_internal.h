/*
 * fam_internal.h
 * Copyright (c) 2017, 2018 Hewlett Packard Enterprise Development, LP. All
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

#include "radixtree/kvs.h"
#include "radixtree/radix_tree.h"

#include "nvmm/epoch_manager.h"
#include "nvmm/memory_manager.h"
#include "fam/fam_exception.h"
#include <nvmm/fam.h>

using namespace famradixtree;
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

#define FAM_KEY_UNINITIALIZED ((uint64_t)-1)
#define FAM_KEY_INVALID ((uint64_t)-2)
#define FAM_FENCE_KEY ((uint64_t)-4)
#define INVALID_OFFSET ((uint64_t)-1)
#define FAM_INVALID_REGION ((uint64_t)-1)
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

using CISServerMap = std::map<uint64_t, std::string>;

/**
 *  DataItem Metadata descriptor
 */
typedef struct {
    uint64_t regionId;
    uint64_t offset;
    uint64_t key;
    uint64_t size;
    mode_t perm;
    void *base;
    char name[RadixTree::MAX_KEY_LEN];
    ;
    size_t maxNameLen;
    // char *name;
} Fam_Region_Item_Info;

inline CISServerMap parse_memserver_list(std::string memServer,
                                         std::string delimiter1,
                                         std::string delimiter2) {
    CISServerMap memoryServerList;
    uint64_t prev1 = 0, pos1 = 0;
    do {
        pos1 = memServer.find(delimiter1, prev1);
        if (pos1 == string::npos)
            pos1 = memServer.length();
        std::string token = memServer.substr(prev1, pos1 - prev1);
        if (!token.empty()) {
            uint64_t prev2 = 0, pos2 = 0, count = 0, nodeid = 0;
            do {
                pos2 = token.find(delimiter2, prev2);
                if (pos2 == string::npos)
                    pos2 = token.length();
                std::string token2 = token.substr(prev2, pos2 - prev2);
                if (!token2.empty()) {
                    if (count % 2 == 0) {
                        nodeid = stoull(token2);
                    } else {
                        memoryServerList.insert({nodeid, token2});
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

/*
 * These derived exception classes are currently being used
 * internally by OpenFAM. All the OpenFAM api return only
 * Fam_Exception objects.
 *
 */
template <typename T> void log_msg(std::ostringstream &message, T const &f) {
    message << f;
}

template <typename T, typename... Args>
void log_msg(std::ostringstream &message, T const &f, Args const &... args) {
    message << f << ":";
    log_msg(message, args...);
}

#define ERROR_MSG(message, ...)                                                \
    log_msg(message, __func__, __LINE__, __VA_ARGS__)

class Fam_InvalidOption_Exception : public Fam_Exception {
  public:
    Fam_InvalidOption_Exception();
    Fam_InvalidOption_Exception(const char *msg);
};

class Fam_Permission_Exception : public Fam_Exception {
  public:
    Fam_Permission_Exception();
    Fam_Permission_Exception(const char *msg);
};

class Fam_Timeout_Exception : public Fam_Exception {
  public:
    Fam_Timeout_Exception();
    Fam_Timeout_Exception(const char *msg);
};

class Fam_Datapath_Exception : public Fam_Exception {
  public:
    Fam_Datapath_Exception();
    Fam_Datapath_Exception(const char *msg);
    Fam_Datapath_Exception(enum Fam_Error fErr, const char *msg);
};

class Fam_Allocator_Exception : public Fam_Exception {
  public:
    Fam_Allocator_Exception();
    Fam_Allocator_Exception(enum Fam_Error fErr, const char *msg);
};

class Fam_Pmi_Exception : public Fam_Exception {
  public:
    Fam_Pmi_Exception();
    Fam_Pmi_Exception(const char *msg);
};

class Fam_Unimplemented_Exception : public Fam_Exception {
  public:
    Fam_Unimplemented_Exception();
    Fam_Unimplemented_Exception(const char *msg);
};

} // namespace openfam

#endif /* end of C/C11 Headers */

#endif /* end of FAM__INTERNAL_H_ */
