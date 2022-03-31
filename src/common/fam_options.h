/*
 * fam_options.h
 * Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All rights
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
#ifndef FAM_OPTIONS_H
#define FAM_OPTIONS_H
#include <iostream>
#include <map>
#include <string.h>
#include <vector>

using namespace std;

/**
 * Enumeration defining system wide options supported by this OpenFAM
 * implementation.
 */
typedef enum {
    /** Version informaiton of this OpenFAM implementation */
    VERSION = 0,
    /** Default Region Name to be used by OpenFAM */
    DEFAULT_REGION_NAME,
    /** CIS server to be used by OpenFAM */
    CIS_SERVER,
    /** Port to be used by OpenFam grpc memory allocator */
    GRPC_PORT,
    /** Libfabric provider to be used by OpenFam libfabric datapath operations;
        "sockets" by default */
    LIBFABRIC_PROVIDER,
    /** Fam thread model */
    FAM_THREAD_MODEL,
    /** CIS interface to be used, default is RPC, Support Direct also */
    CIS_INTERFACE_TYPE,
    /** OpenFAM model to be used; default is memory_server, Other option is
       shared_memory */
    OPENFAM_MODEL,
    /** Fam Context model */
    FAM_CONTEXT_MODEL,
    /** Total count of PEs */
    PE_COUNT,
    /** Id/Rank of PEs */
    PE_ID,
    /** Fam Runtime selection */
    RUNTIME,
    /**Number of consumer threads in case of shared memory model**/
    NUM_CONSUMER,
    FAM_DEFAULT_MEMORY_TYPE,
    /** Interface device used in the pe side for communication */
    IF_DEVICE,
    /** END of Option keys */
    END_OPT = -1
} Fam_Option_Key;

/**
 * FAM_THREAD_MODEL supproted options values
 */
#define FAM_THREAD_SERIALIZE_STR "FAM_THREAD_SERIALIZE"
#define FAM_THREAD_MULTIPLE_STR "FAM_THREAD_MULTIPLE"

#define FAM_CONTEXT_DEFAULT_STR "FAM_CONTEXT_DEFAULT"
#define FAM_CONTEXT_REGION_STR "FAM_CONTEXT_REGION"

#define FAM_OPTIONS_SHM_STR "shared_memory"
#define FAM_OPTIONS_MEMSERV_STR "memory_server"

#define FAM_OPTIONS_DIRECT_STR "direct"
#define FAM_OPTIONS_RPC_STR "rpc"

#define FAM_OPTIONS_RUNTIME_PMIX_STR "PMIX"
#define FAM_OPTIONS_RUNTIME_PMI2_STR "PMI2"
#define FAM_OPTIONS_RUNTIME_NONE_STR "NONE"

typedef enum {
    /** For single threaded applicaiton */
    FAM_THREAD_SERIALIZE = 1,
    FAM_THREAD_MULTIPLE
} Fam_Thread_Model;

typedef enum {
    /** For single threaded applicaiton */
    FAM_CONTEXT_DEFAULT = 1,
    FAM_CONTEXT_REGION
} Fam_Context_Model;

#endif
