/*
 * fam_ops_libfabric.cpp
 * Copyright (c) 2019-2021 Hewlett Packard Enterprise Development, LP. All
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

#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <future>

#include "common/fam_internal.h"
#include "common/fam_libfabric.h"
#include "common/fam_memserver_profile.h"
#include "common/fam_ops.h"
#include "common/fam_ops_libfabric.h"
#include "fam/fam.h"
#include "fam/fam_exception.h"

using namespace std;
using namespace chrono;

namespace openfam {

Fam_Ops_Libfabric::~Fam_Ops_Libfabric() {

    delete contexts;
    delete defContexts;
    delete fiAddrs;
    delete memServerAddrs;
    delete fiMemsrvMap;
    delete fiMrs;
    free(service);
    free(provider);
    free(serverAddrName);
    free(if_device);
}

Fam_Ops_Libfabric::Fam_Ops_Libfabric(bool source, const char *libfabricProvider,
                                     const char *if_device_str,
                                     Fam_Thread_Model famTM,
                                     Fam_Allocator_Client *famAlloc,
                                     Fam_Context_Model famCM) {
    std::ostringstream message;
    memoryServerName = NULL;
    service = NULL;
    provider = strdup(libfabricProvider);
    if_device = strdup(if_device_str);
    isSource = source;
    famThreadModel = famTM;
    famContextModel = famCM;
    famAllocator = famAlloc;

    fiAddrs = new std::vector<fi_addr_t>();
    memServerAddrs = new std::map<uint64_t, std::pair<void *, size_t>>();
    fiMemsrvMap = new std::map<uint64_t, fi_addr_t>();
    fiMrs = new std::map<uint64_t, Fam_Region_Map_t *>();
    contexts = new std::map<uint64_t, Fam_Context *>();
    defContexts = new std::map<uint64_t, Fam_Context *>();

    fi = NULL;
    fabric = NULL;
    eq = NULL;
    domain = NULL;
    av = NULL;
    serverAddrNameLen = 0;
    serverAddrName = NULL;
    ctxId = FAM_DEFAULT_CTX_ID;
    nextCtxId = ctxId + 1;

    numMemoryNodes = 0;
    if (!isSource && famAllocator == NULL) {
        message << "Fam Invalid Option Fam_Alloctor: NULL value specified"
                << famContextModel;
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }
}

Fam_Ops_Libfabric::Fam_Ops_Libfabric(bool source, const char *libfabricProvider,
                                     const char *if_device_str,
                                     Fam_Thread_Model famTM,
                                     Fam_Allocator_Client *famAlloc,
                                     Fam_Context_Model famCM,
                                     const char *memServerName,
                                     const char *libfabricPort) {
    std::ostringstream message;
    memoryServerName = strdup(memServerName);
    service = strdup(libfabricPort);
    provider = strdup(libfabricProvider);
    if_device = strdup(if_device_str);
    isSource = source;
    famThreadModel = famTM;
    famContextModel = famCM;
    famAllocator = famAlloc;

    fiAddrs = new std::vector<fi_addr_t>();
    memServerAddrs = new std::map<uint64_t, std::pair<void *, size_t>>();
    fiMemsrvMap = new std::map<uint64_t, fi_addr_t>();
    fiMrs = new std::map<uint64_t, Fam_Region_Map_t *>();
    contexts = new std::map<uint64_t, Fam_Context *>();
    defContexts = new std::map<uint64_t, Fam_Context *>();

    fi = NULL;
    fabric = NULL;
    eq = NULL;
    domain = NULL;
    av = NULL;
    serverAddrNameLen = 0;
    serverAddrName = NULL;
    ctxId = FAM_DEFAULT_CTX_ID;
    nextCtxId = ctxId + 1;

    numMemoryNodes = 0;
    if (!isSource && famAllocator == NULL) {
        message << "Fam Invalid Option Fam_Alloctor: NULL value specified"
                << famContextModel;
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }
}

Fam_Ops_Libfabric::Fam_Ops_Libfabric(Fam_Ops_Libfabric *famOps) {

    memoryServerName = famOps->memoryServerName;
    service = famOps->service;
    provider = famOps->provider;
    if_device = famOps->if_device;
    isSource = famOps->isSource;
    famThreadModel = famOps->famThreadModel;
    famContextModel = famOps->famContextModel;
    famAllocator = famOps->famAllocator;

    fiAddrs = famOps->fiAddrs;
    memServerAddrs = famOps->memServerAddrs;
    fiMemsrvMap = famOps->fiMemsrvMap;
    fiMrs = famOps->fiMrs;
    contexts = famOps->contexts;
    defContexts = famOps->defContexts;
    ctxLock = famOps->ctxLock;

    fi = famOps->fi;
    fabric = famOps->fabric;
    eq = famOps->eq;
    domain = famOps->domain;
    av = famOps->av;
    serverAddrNameLen = famOps->serverAddrNameLen;
    serverAddrName = famOps->serverAddrName;
    ctxId = famOps->get_next_ctxId(1);
    nextCtxId = ctxId + 1;

    numMemoryNodes = famOps->numMemoryNodes;
    fabric_iov_limit = famOps->fabric_iov_limit;
    fabric_max_msg_size = famOps->fabric_max_msg_size;
}

int Fam_Ops_Libfabric::initialize() {
    std::ostringstream message;
    int ret = 0;

    // Initialize the mutex lock
    (void)pthread_rwlock_init(&fiMrLock, NULL);

    // Initialize the mutex lock
    (void)pthread_rwlock_init(&fiMemsrvAddrLock, NULL);

    // Initialize the mutex lock
    (void)pthread_mutex_init(&ctxLock, NULL);

    if ((ret = fabric_initialize(memoryServerName, service, isSource, provider,
                                 if_device, &fi, &fabric, &eq, &domain,
                                 famThreadModel)) < 0) {
        message << "Fam libfabric fabric_initialize failed: "
                << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    // Initialize address vector
    if (fi->ep_attr->type == FI_EP_RDM) {
        if ((ret = fabric_initialize_av(fi, domain, eq, &av)) < 0) {
            message << "Fam libfabric fabric_initialize_av failed: "
                    << fabric_strerror(ret);
            THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
        }
    }

    if (!isSource) {
        populate_address_vector();
    } else {
        // This is memory server. Populate the serverAddrName and
        // serverAddrNameLen from libfabric
        Fam_Context *tmpCtx = new Fam_Context(fi, domain, famThreadModel);
        ret = fabric_enable_bind_ep(fi, av, eq, tmpCtx->get_ep());
        if (ret < 0) {
            message << "Fam libfabric fabric_enable_bind_ep failed: "
                    << fabric_strerror(ret);
            THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
        }

        serverAddrNameLen = 0;
        ret = fabric_getname_len(tmpCtx->get_ep(), &serverAddrNameLen);
        if (serverAddrNameLen <= 0) {
            message << "Fam libfabric fabric_getname_len failed: "
                    << fabric_strerror(ret);
            THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
        }
        serverAddrName = calloc(1, serverAddrNameLen);
        ret = fabric_getname(tmpCtx->get_ep(), serverAddrName,
                             &serverAddrNameLen);
        if (ret < 0) {
            message << "Fam libfabric fabric_getname failed: "
                    << fabric_strerror(ret);
            THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
        }

        // Save this context to defContexts on memoryserver
        defContexts->insert({FAM_DEFAULT_CTX_ID, tmpCtx});
    }
    if (fi->ep_attr->max_msg_size > 0) {
        fabric_max_msg_size = fi->ep_attr->max_msg_size;
    } else {
        message << "Unexpected FAM libfabric error: Fabric Info max message size 0";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    fabric_iov_limit = fi->tx_attr->rma_iov_limit;

    return 0;
}

void Fam_Ops_Libfabric::populate_address_vector(void *memServerInfoBuffer,
                                                size_t memServerInfoSize,
                                                uint64_t numMemNodes,
                                                uint64_t myId) {
    // Insert the memory server address into address vector
    // Only if it is not source
    std::ostringstream message;
    numMemoryNodes = numMemNodes;
    int ret = 0;
    if (!memServerInfoBuffer) {
        numMemoryNodes = famAllocator->get_num_memory_servers();
        if (numMemoryNodes == 0) {
            message << "Libfabric initialize: memory server name not specified";
            THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
        }
        ret = famAllocator->get_memserverinfo_size(&memServerInfoSize);
        if (ret < 0) {
            message << "Fam allocator get_memserverinfo_size failed";
            THROW_ERRNO_MSG(Fam_Allocator_Exception, FAM_ERR_ALLOCATOR,
                            message.str().c_str());
        }
        if (memServerInfoSize) {
            memServerInfoBuffer = calloc(1, memServerInfoSize);
            ret = famAllocator->get_memserverinfo(memServerInfoBuffer);

            if (ret < 0) {
                message << "Fam Allocator get_memserverinfo failed";
                THROW_ERRNO_MSG(Fam_Allocator_Exception, FAM_ERR_ALLOCATOR,
                                message.str().c_str());
            }
        }
    }
    size_t bufPtr = 0;
    uint64_t nodeId;
    size_t addrSize;
    void *nodeAddr;
    uint64_t fiAddrsSize = fiAddrs->size();

    while (bufPtr < memServerInfoSize) {
        memcpy(&nodeId, ((char *)memServerInfoBuffer + bufPtr),
               sizeof(uint64_t));
        bufPtr += sizeof(uint64_t);
        memcpy(&addrSize, ((char *)memServerInfoBuffer + bufPtr),
               sizeof(size_t));
        bufPtr += sizeof(size_t);
        nodeAddr = calloc(1, addrSize);
        memcpy(nodeAddr, ((char *)memServerInfoBuffer + bufPtr), addrSize);
        bufPtr += addrSize;
        // Save memory server address in memServerAddrs map

        // In each memory server skip the address vector insertion part for its
        // own address
        if (isSource && (myId == nodeId))
            continue;
        memServerAddrs->insert({nodeId, std::make_pair(nodeAddr, addrSize)});

        std::vector<fi_addr_t> tmpAddrV;
        ret = fabric_insert_av((char *)nodeAddr, av, &tmpAddrV);
        if (ret < 0) {
            message << "Fam libfabric fabric_insert_av failed: "
                    << fabric_strerror(ret);
            THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
        }

        // Place the fi_addr_t at nodeId index of fiAddrs vector.
        if (nodeId >= fiAddrsSize) {
            // Increase the size of fiAddrs vector to accomodate
            // nodeId larger than the current size.
            fiAddrs->resize(nodeId + 512, FI_ADDR_UNSPEC);
            fiAddrsSize = fiAddrs->size();
        }
        fiAddrs->at(nodeId) = tmpAddrV[0];
    }
    // Initialize defaultCtx
    if (famContextModel == FAM_CONTEXT_DEFAULT) {
        Fam_Context *defaultCtx = new Fam_Context(fi, domain, famThreadModel);
        defContexts->insert({FAM_DEFAULT_CTX_ID, defaultCtx});
        set_context(defaultCtx);
        ret = fabric_enable_bind_ep(fi, av, eq, defaultCtx->get_ep());
        if (ret < 0) {
            message << "Fam libfabric fabric_enable_bind_ep failed: "
                    << fabric_strerror(ret);
            THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
        }
    }
}

Fam_Context *Fam_Ops_Libfabric::get_context(Fam_Descriptor *descriptor) {
    std::ostringstream message;
    // Case - FAM_CONTEXT_DEFAULT
    if (famContextModel == FAM_CONTEXT_DEFAULT) {
        return get_context();
    } else {
        message << "Fam Invalid Option FAM_CONTEXT_MODEL: " << famContextModel;
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }
}

void Fam_Ops_Libfabric::finalize() {
    fabric_finalize();
    if (fiMrs != NULL) {
        for (auto mr : *fiMrs) {
            Fam_Region_Map_t *fiRegionMap = mr.second;
            for (auto dmr : *(fiRegionMap->fiRegionMrs)) {
                fi_close(&(dmr.second->fid));
            }
            fiRegionMap->fiRegionMrs->clear();
            free(fiRegionMap);
        }
        fiMrs->clear();
    }

    if (contexts != NULL) {
        for (auto fam_ctx : *contexts) {
            delete fam_ctx.second;
        }
        contexts->clear();
    }

    if (defContexts != NULL) {
        for (auto fam_ctx : *defContexts) {
            delete fam_ctx.second;
        }
        defContexts->clear();
    }

    if (fi) {
        fi_freeinfo(fi);
        fi = NULL;
    }

    if (fabric) {
        fi_close(&fabric->fid);
        fabric = NULL;
    }

    if (eq) {
        fi_close(&eq->fid);
        eq = NULL;
    }

    if (domain) {
        fi_close(&domain->fid);
        domain = NULL;
    }

    if (av) {
        fi_close(&av->fid);
        av = NULL;
    }
}

int Fam_Ops_Libfabric::put_blocking(void *local, Fam_Descriptor *descriptor,
                                    uint64_t offset, uint64_t nbytes) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    int ret = 0;
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block, else issue a single IO
    // to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        uint64_t currentLocal = (uint64_t)local;
        uint64_t currentOffset = offset;
        uint64_t currentNbytes = nbytes;
        uint64_t pending_nbytes;
        while (currentNbytes > 0) {
            if (currentNbytes > fabric_max_msg_size) {
                pending_nbytes = currentNbytes - fabric_max_msg_size;
                currentNbytes = fabric_max_msg_size;
            } else {
                pending_nbytes = 0;
            }
            // Issue an IO
            fi_context *ctx =
                fabric_write(keys[0], (void *)currentLocal, currentNbytes,
                             (uint64_t)(base_addr_list[0]) + currentOffset,
                             (*fiAddr)[memServerIds[0]], famCtx, true);
            // wait for IO to complete
            famCtx->aquire_RDLock();
            try {
                ret = fabric_completion_wait(famCtx, ctx, 0);
                delete ctx;
            } catch (...) {
                famCtx->inc_num_tx_fail_cnt(1l);
                // Release Fam_Context read lock
                famCtx->release_lock();
                throw;
            }
            famCtx->release_lock();
            currentNbytes = pending_nbytes;
            if (currentNbytes > 0) {
                currentOffset = currentOffset + fabric_max_msg_size;
                currentLocal = currentLocal + fabric_max_msg_size;
            }
        }
        return ret;
    }

    // Vector to store fi_context pointer of each IO
    std::vector<struct fi_context *> fiCtxVector;

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    uint64_t chunkSize;
    uint64_t nBytesWritten = 0;
    /*
     * If the offset is displaced from a starting position of a block
     * issue a seperate IO for that chunk of data
     */
    uint64_t firstBlockSize = interleaveSize - displacement;
    if (firstBlockSize < interleaveSize) {
        // Calculate the chunk of size for each IO
        if (nbytes < firstBlockSize)
            chunkSize = nbytes;
        else
            chunkSize = firstBlockSize;
        // Issue an IO
        fi_context *ctx = fabric_write(
            keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
            (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr +
                displacement,
            (*fiAddr)[memServerIds[currentServerIndex]],
            get_context(descriptor), true);
        // store the fi_context pointer to ensure the completion latter.
        fiCtxVector.push_back(ctx);
        // go to next server for next block of data
        currentServerIndex++;
        // If last memory server is reached roll back to first server and
        // incement the interleave block by one
        if (currentServerIndex == usedMemsrvCnt) {
            currentServerIndex = 0;
            // Increment a block everytime cricles through used servers.
            currentFamPtr += interleaveSize;
        }
        // Increment the number of bytes written
        nBytesWritten += chunkSize;
        // Increment the local buffer pointer by number of bytes written
        currentLocalPtr += chunkSize;
    }
    // Loop until the requested number of bytes are issued through IO
    while (nBytesWritten < nbytes) {
        // Calculate the chunk of size for each IO
        //(For last IO the chunk size may not be same as inetrleave size)
        if ((nbytes - nBytesWritten) < interleaveSize)
            chunkSize = nbytes - nBytesWritten;
        else
            chunkSize = interleaveSize;
        // Issue an IO
        fi_context *ctx = fabric_write(
            keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
            (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
            (*fiAddr)[memServerIds[currentServerIndex]],
            get_context(descriptor), true);
        // store the fi_context pointer to ensure the completion latter.
        fiCtxVector.push_back(ctx);
        // go to next server for next block of data
        currentServerIndex++;
        // If last memory server is reached roll back to first server and
        // incement the interleave block by one
        if (currentServerIndex == usedMemsrvCnt) {
            currentServerIndex = 0;
            currentFamPtr += interleaveSize;
        }
        // Increment the number of bytes written
        currentLocalPtr += chunkSize;
        // Increment the local buffer pointer by number of bytes written
        nBytesWritten += chunkSize;
    }
    /*
     * Iterate over the vector of fi_context to ensure the completion of all IOs
     */
    if (!fiCtxVector.empty()) {
        famCtx->aquire_RDLock();
        for (auto ctx : fiCtxVector) {
            try {
                ret = fabric_completion_wait(famCtx, ctx, 0);
                delete ctx;
            } catch (...) {
                famCtx->inc_num_tx_fail_cnt(1l);
                // Release Fam_Context read lock
                famCtx->release_lock();
                throw;
            }
        }
        famCtx->release_lock();
    }
    return ret;
}

int Fam_Ops_Libfabric::get_blocking(void *local, Fam_Descriptor *descriptor,
                                    uint64_t offset, uint64_t nbytes) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    int ret = 0;
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block using the given offset,
    // else issue a single IO to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        uint64_t currentLocal = (uint64_t)local;
        uint64_t currentOffset = offset;
        uint64_t currentNbytes = nbytes;
        uint64_t pending_nbytes;
        while (currentNbytes > 0) {
            if (currentNbytes > fabric_max_msg_size) {
                pending_nbytes = currentNbytes - fabric_max_msg_size;
                currentNbytes = fabric_max_msg_size;
            } else {
                pending_nbytes = 0;
            }
            // Issue an IO
            fi_context *ctx =
                fabric_read(keys[0], (void *)currentLocal, currentNbytes,
                            (uint64_t)(base_addr_list[0]) + currentOffset,
                            (*fiAddr)[memServerIds[0]], famCtx, true);

            // wait for IO to complete
            famCtx->aquire_RDLock();
            try {
                ret = fabric_completion_wait(famCtx, ctx, 0);
                delete ctx;
            } catch (...) {
                famCtx->inc_num_rx_fail_cnt(1l);
                // Release Fam_Context read lock
                famCtx->release_lock();
                throw;
            }
            famCtx->release_lock();
            currentNbytes = pending_nbytes;
            if (currentNbytes > 0) {
                currentOffset = currentOffset + fabric_max_msg_size;
                currentLocal = currentLocal + fabric_max_msg_size;
            }
        }
        return ret;
    }
    // Vector to store fi_context pointer of each IO
    std::vector<struct fi_context *> fiCtxVector;

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;
    uint64_t chunkSize;
    uint64_t nBytesRead = 0;
    /*
     * If the offset is displaced from a starting position of a block
     * issue a seperate IO for that chunk of data
     */
    uint64_t firstBlockSize = interleaveSize - displacement;
    if (firstBlockSize < interleaveSize) {
        // Calculate the chunk of size for each IO
        if (nbytes < firstBlockSize)
            chunkSize = nbytes;
        else
            chunkSize = firstBlockSize;

        // Issue an IO
        fi_context *ctx = fabric_read(
            keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
            (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr +
                displacement,
            (*fiAddr)[memServerIds[currentServerIndex]],
            get_context(descriptor), true);
        // store the fi_context pointer to ensure the completion latter.
        fiCtxVector.push_back(ctx);
        // go to next server for next block of data
        currentServerIndex++;
        // If last memory server is reached roll back to first server and
        // incement the interleave block by one
        if (currentServerIndex == usedMemsrvCnt) {
            currentServerIndex = 0;
            // Increment a block everytime cricles through used servers.
            currentFamPtr += interleaveSize;
        }
        // Increment the number of bytes read
        nBytesRead += chunkSize;
        // Increment the local buffer pointer by number of bytes read
        currentLocalPtr += chunkSize;
    }
    // Loop until the requested number of bytes are issued through IO
    while (nBytesRead < nbytes) {
        // Calculate the chunk of size for each IO
        //(For last IO the chunk size may not be same as inetrleave size)
        if ((nbytes - nBytesRead) < interleaveSize)
            chunkSize = nbytes - nBytesRead;
        else
            chunkSize = interleaveSize;
        // Issue an IO
        fi_context *ctx = fabric_read(
            keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
            (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
            (*fiAddr)[memServerIds[currentServerIndex]],
            get_context(descriptor), true);
        // store the fi_context pointer to ensure the completion latter.
        fiCtxVector.push_back(ctx);
        // go to next server for next block of data
        currentServerIndex++;
        // If last memory server is reached roll back to first server and
        // incement the interleave block by one
        if (currentServerIndex == usedMemsrvCnt) {
            currentServerIndex = 0;
            currentFamPtr += interleaveSize;
        }
        // Increment the number of bytes read
        currentLocalPtr += chunkSize;
        // Increment the local buffer pointer by number of bytes read
        nBytesRead += chunkSize;
    }
    /*
     * Iterate over the vector of fi_context to ensure the completion of all IOs
     */
    if (!fiCtxVector.empty()) {
        famCtx->aquire_RDLock();
        for (auto ctx : fiCtxVector) {
            try {
                ret = fabric_completion_wait(famCtx, ctx, 0);
                delete ctx;
            } catch (...) {
                famCtx->inc_num_rx_fail_cnt(1l);
                // Release Fam_Context read lock
                famCtx->release_lock();
                throw;
            }
        }
        famCtx->release_lock();
    }
    return ret;
}

int Fam_Ops_Libfabric::scatter_blocking(void *local, Fam_Descriptor *descriptor,
                                        uint64_t nElements,
                                        uint64_t firstElement, uint64_t stride,
                                        uint64_t elementSize) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    int ret = 0;
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block, else issue a single IO
    // to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        if (elementSize > fabric_max_msg_size) {
            std::ostringstream message;
            message << "IO size is larger than maximum supported IO size by "
                       "provider";
            THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_LIBFABRIC,
                            message.str().c_str());
        }
        // Issue an IO
        fi_context *ctx = fabric_scatter_stride(
            keys[0], (void *)local, elementSize, firstElement, nElements,
            stride, (*fiAddr)[memServerIds[0]], famCtx, fabric_iov_limit,
            (uint64_t)(base_addr_list[0]), true);
        // wait for IO to complete
        famCtx->aquire_RDLock();
        try {
            ret = fabric_completion_wait(famCtx, ctx, 0);
            delete ctx;
        } catch (...) {
            famCtx->inc_num_tx_fail_cnt(1l);
            // Release Fam_Context read lock
            famCtx->release_lock();
            throw;
        }
        famCtx->release_lock();
        return ret;
    }

    // Vector to store fi_context pointer of each IO
    std::vector<struct fi_context *> fiCtxVector;

    // Initialize the offset to first element position
    uint64_t offset = firstElement * elementSize;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    for (int i = 0; i < (int)nElements; i++) {
        // Current memory server Id index
        uint64_t currentServerIndex =
            ((offset / interleaveSize) % usedMemsrvCnt);
        // Current remote location in FAM
        uint64_t currentFamPtr =
            (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
            interleaveSize;
        // Displacement from the starting position of the interleave block
        uint64_t displacement = offset % interleaveSize;
        uint64_t chunkSize;
        uint64_t nBytesWritten = 0;
        /*
         * If the offset is displaced from a starting position of a block
         * issue a seperate IO for that chunk of data
         */
        uint64_t firstBlockSize = interleaveSize - displacement;
        if (firstBlockSize < interleaveSize) {
            if (elementSize < firstBlockSize)
                chunkSize = elementSize;
            else
                chunkSize = firstBlockSize;
            // Issue an IO
            fi_context *ctx = fabric_write(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr +
                    displacement,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), true);
            // store the fi_context pointer to ensure the completion latter.
            fiCtxVector.push_back(ctx);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                // Increment a block everytime cricles through used servers.
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes written
            nBytesWritten += chunkSize;
            // Increment the local buffer pointer by number of bytes written
            currentLocalPtr += chunkSize;
        }
        // Loop until the requested number of bytes are issued through IO
        while (nBytesWritten < elementSize) {
            // Calculate the chunk of size for each IO
            //(For last IO the chunk size may not be same as inetrleave size)
            if ((elementSize - nBytesWritten) < interleaveSize)
                chunkSize = elementSize - nBytesWritten;
            else
                chunkSize = interleaveSize;
            // Issue an IO
            fi_context *ctx = fabric_write(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), true);
            // store the fi_context pointer to ensure the completion latter.
            fiCtxVector.push_back(ctx);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes written
            currentLocalPtr += chunkSize;
            // Increment the local buffer pointer by number of bytes written
            nBytesWritten += chunkSize;
        }
        offset += (stride * elementSize);
    }
    /*
     * Iterate over the vector of fi_context to ensure the completion of all IOs
     */
    if (!fiCtxVector.empty()) {
        famCtx->aquire_RDLock();
        for (auto ctx : fiCtxVector) {
            try {
                ret = fabric_completion_wait(famCtx, ctx, 0);
                delete ctx;
            } catch (...) {
                famCtx->inc_num_tx_fail_cnt(1l);
                // Release Fam_Context read lock
                famCtx->release_lock();
                throw;
            }
        }
        famCtx->release_lock();
    }
    return ret;
}

int Fam_Ops_Libfabric::gather_blocking(void *local, Fam_Descriptor *descriptor,
                                       uint64_t nElements,
                                       uint64_t firstElement, uint64_t stride,
                                       uint64_t elementSize) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    int ret = 0;
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block, else issue a single IO
    // to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        if (elementSize > fabric_max_msg_size) {
            std::ostringstream message;
            message << "IO size is larger than maximum supported IO size by "
                       "provider";
            THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_LIBFABRIC,
                            message.str().c_str());
        }
        // Issue an IO
        fi_context *ctx = fabric_gather_stride(
            keys[0], (void *)local, elementSize, firstElement, nElements,
            stride, (*fiAddr)[memServerIds[0]], famCtx, fabric_iov_limit,
            (uint64_t)(base_addr_list[0]), true);
        // wait for IO to complete
        famCtx->aquire_RDLock();
        try {
            ret = fabric_completion_wait(famCtx, ctx, 0);
            delete ctx;
        } catch (...) {
            famCtx->inc_num_rx_fail_cnt(1l);
            // Release Fam_Context read lock
            famCtx->release_lock();
            throw;
        }
        famCtx->release_lock();
        return ret;
    }

    // Vector to store fi_context pointer of each IO
    std::vector<struct fi_context *> fiCtxVector;

    // Initialize the offset to first element position
    uint64_t offset = firstElement * elementSize;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    for (int i = 0; i < (int)nElements; i++) {
        // Current memory server Id index
        uint64_t currentServerIndex =
            ((offset / interleaveSize) % usedMemsrvCnt);
        // Current remote location in FAM
        uint64_t currentFamPtr =
            (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
            interleaveSize;
        // Displacement from the starting position of the interleave block
        uint64_t displacement = offset % interleaveSize;
        uint64_t chunkSize;
        uint64_t nBytesRead = 0;
        /*
         * If the offset is displaced from a starting position of a block
         * issue a seperate IO for that chunk of data
         */
        uint64_t firstBlockSize = interleaveSize - displacement;
        if (firstBlockSize < interleaveSize) {
            if (elementSize < firstBlockSize)
                chunkSize = elementSize;
            else
                chunkSize = firstBlockSize;
            // Issue an IO
            fi_context *ctx = fabric_read(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr +
                    displacement,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), true);
            // store the fi_context pointer to ensure the completion latter.
            fiCtxVector.push_back(ctx);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                // Increment a block everytime cricles through used servers.
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes read
            nBytesRead += chunkSize;
            // Increment the local buffer pointer by number of bytes read
            currentLocalPtr += chunkSize;
        }
        // Loop until the requested number of bytes are issued through IO
        while (nBytesRead < elementSize) {
            // Calculate the chunk of size for each IO
            //(For last IO the chunk size may not be same as inetrleave size)
            if ((elementSize - nBytesRead) < interleaveSize)
                chunkSize = elementSize - nBytesRead;
            else
                chunkSize = interleaveSize;
            // Issue an IO
            fi_context *ctx = fabric_read(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), true);
            // store the fi_context pointer to ensure the completion latter.
            fiCtxVector.push_back(ctx);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes read
            currentLocalPtr += chunkSize;
            // Increment the local buffer pointer by number of bytes read
            nBytesRead += chunkSize;
        }
        offset += (stride * elementSize);
    }
    /*
     * Iterate over the vector of fi_context to ensure the completion of all IOs
     */
    if (!fiCtxVector.empty()) {
        famCtx->aquire_RDLock();
        for (auto ctx : fiCtxVector) {
            try {
                ret = fabric_completion_wait(famCtx, ctx, 0);
                delete ctx;
            } catch (...) {
                famCtx->inc_num_rx_fail_cnt(1l);
                // Release Fam_Context read lock
                famCtx->release_lock();
                throw;
            }
        }
        famCtx->release_lock();
    }
    return ret;
}

int Fam_Ops_Libfabric::scatter_blocking(void *local, Fam_Descriptor *descriptor,
                                        uint64_t nElements,
                                        uint64_t *elementIndex,
                                        uint64_t elementSize) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    int ret = 0;
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block, else issue a single IO
    // to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        if (elementSize > fabric_max_msg_size) {
            std::ostringstream message;
            message << "IO size is larger than maximum supported IO size by "
                       "provider";
            THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_LIBFABRIC,
                            message.str().c_str());
        }
        // Issue an IO
        fi_context *ctx = fabric_scatter_index(
            keys[0], (void *)local, elementSize, elementIndex, nElements,
            (*fiAddr)[memServerIds[0]], famCtx, fabric_iov_limit,
            (uint64_t)(base_addr_list[0]), true);
        // wait for IO to complete
        famCtx->aquire_RDLock();
        try {
            ret = fabric_completion_wait(famCtx, ctx, 0);
            delete ctx;
        } catch (...) {
            famCtx->inc_num_tx_fail_cnt(1l);
            // Release Fam_Context read lock
            famCtx->release_lock();
            throw;
        }
        famCtx->release_lock();
        return ret;
    }

    // Vector to store fi_context pointer of each IO
    std::vector<struct fi_context *> fiCtxVector;

    // iterate through the indeces provided by user
    uint64_t offset;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    for (int i = 0; i < (int)nElements; i++) {
        offset = elementIndex[i] * elementSize;
        // Current memory server Id index
        uint64_t currentServerIndex =
            ((offset / interleaveSize) % usedMemsrvCnt);
        // Current remote location in FAM
        uint64_t currentFamPtr =
            (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
            interleaveSize;
        // Displacement from the starting position of the interleave block
        uint64_t displacement = offset % interleaveSize;
        uint64_t chunkSize;
        uint64_t nBytesWritten = 0;
        /*
         * If the offset is displaced from a starting position of a block
         * issue a seperate IO for that chunk of data
         */
        uint64_t firstBlockSize = interleaveSize - displacement;
        if (firstBlockSize < interleaveSize) {
            if (elementSize < firstBlockSize)
                chunkSize = elementSize;
            else
                chunkSize = firstBlockSize;
            // Issue an IO
            fi_context *ctx = fabric_write(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr +
                    displacement,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), true);
            // store the fi_context pointer to ensure the completion latter.
            fiCtxVector.push_back(ctx);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                // Increment a block everytime cricles through used servers.
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes written
            nBytesWritten += chunkSize;
            // Increment the local buffer pointer by number of bytes written
            currentLocalPtr += chunkSize;
        }
        // Loop until the requested number of bytes are issued through IO
        while (nBytesWritten < elementSize) {
            // Calculate the chunk of size for each IO
            //(For last IO the chunk size may not be same as inetrleave size)
            if ((elementSize - nBytesWritten) < interleaveSize)
                chunkSize = elementSize - nBytesWritten;
            else
                chunkSize = interleaveSize;
            // Issue an IO
            fi_context *ctx = fabric_write(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), true);
            // store the fi_context pointer to ensure the completion latter.
            fiCtxVector.push_back(ctx);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes written
            currentLocalPtr += chunkSize;
            // Increment the local buffer pointer by number of bytes written
            nBytesWritten += chunkSize;
        }
    }
    /*
     * Iterate over the vector of fi_context to ensure the completion of all IOs
     */
    if (!fiCtxVector.empty()) {
        famCtx->aquire_RDLock();
        for (auto ctx : fiCtxVector) {
            try {
                ret = fabric_completion_wait(famCtx, ctx, 0);
                delete ctx;
            } catch (...) {
                famCtx->inc_num_tx_fail_cnt(1l);
                // Release Fam_Context read lock
                famCtx->release_lock();
                throw;
            }
        }
        famCtx->release_lock();
    }
    return ret;
}

int Fam_Ops_Libfabric::gather_blocking(void *local, Fam_Descriptor *descriptor,
                                       uint64_t nElements,
                                       uint64_t *elementIndex,
                                       uint64_t elementSize) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    int ret = 0;
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block, else issue a single IO
    // to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        if (elementSize > fabric_max_msg_size) {
            std::ostringstream message;
            message << "IO size is larger than maximum supported IO size by "
                       "provider";
            THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_LIBFABRIC,
                            message.str().c_str());
        }
        // Issue an IO
        fi_context *ctx = fabric_gather_index(
            keys[0], (void *)local, elementSize, elementIndex, nElements,
            (*fiAddr)[memServerIds[0]], famCtx, fabric_iov_limit,
            (uint64_t)(base_addr_list[0]), true);
        // wait for IO to complete
        famCtx->aquire_RDLock();
        try {
            ret = fabric_completion_wait(famCtx, ctx, 0);
            delete ctx;
        } catch (...) {
            famCtx->inc_num_rx_fail_cnt(1l);
            // Release Fam_Context read lock
            famCtx->release_lock();
            throw;
        }
        famCtx->release_lock();
        return ret;
    }

    // Vector to store fi_context pointer of each IO
    std::vector<struct fi_context *> fiCtxVector;

    // iterate through the indeces provided by user
    uint64_t offset;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    for (int i = 0; i < (int)nElements; i++) {
        offset = elementIndex[i] * elementSize;
        // Current memory server Id index
        uint64_t currentServerIndex =
            ((offset / interleaveSize) % usedMemsrvCnt);
        // Current remote location in FAM
        uint64_t currentFamPtr =
            (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
            interleaveSize;
        // Displacement from the starting position of the interleave block
        uint64_t displacement = offset % interleaveSize;
        uint64_t chunkSize;
        uint64_t nBytesRead = 0;
        /*
         * If the offset is displaced from a starting position of a block
         * issue a seperate IO for that chunk of data
         */
        uint64_t firstBlockSize = interleaveSize - displacement;
        if (firstBlockSize < interleaveSize) {
            if (elementSize < firstBlockSize)
                chunkSize = elementSize;
            else
                chunkSize = firstBlockSize;
            // Issue IO
            fi_context *ctx = fabric_read(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr +
                    displacement,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), true);
            // store the fi_context pointer to ensure the completion latter.
            fiCtxVector.push_back(ctx);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                // Increment a block everytime cricles through used servers.
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes read
            nBytesRead += chunkSize;
            // Increment the local buffer pointer by number of bytes read
            currentLocalPtr += chunkSize;
        }
        // Loop until the requested number of bytes are issued through IO
        while (nBytesRead < elementSize) {
            // Calculate the chunk of size for each IO
            //(For last IO the chunk size may not be same as inetrleave size)
            if ((elementSize - nBytesRead) < interleaveSize)
                chunkSize = elementSize - nBytesRead;
            else
                chunkSize = interleaveSize;
            // Issue an IO
            fi_context *ctx = fabric_read(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), true);
            // store the fi_context pointer to ensure the completion latter.
            fiCtxVector.push_back(ctx);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes read
            currentLocalPtr += chunkSize;
            // Increment the local buffer pointer by number of bytes read
            nBytesRead += chunkSize;
        }
    }
    /*
     * Iterate over the vector of fi_context to ensure the completion of all IOs
     */
    if (!fiCtxVector.empty()) {
        famCtx->aquire_RDLock();
        for (auto ctx : fiCtxVector) {
            try {
                ret = fabric_completion_wait(famCtx, ctx, 0);
                delete ctx;
            } catch (...) {
                famCtx->inc_num_rx_fail_cnt(1l);
                // Release Fam_Context read lock
                famCtx->release_lock();
                throw;
            }
        }
        famCtx->release_lock();
    }
    return ret;
}

void Fam_Ops_Libfabric::put_nonblocking(void *local, Fam_Descriptor *descriptor,
                                        uint64_t offset, uint64_t nbytes) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block, else issue a single IO
    // to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        uint64_t currentLocal = (uint64_t)local;
        uint64_t currentOffset = offset;
        uint64_t currentNbytes = nbytes;
        uint64_t pending_nbytes;
        while (currentNbytes > 0) {
            if (currentNbytes > fabric_max_msg_size) {
                pending_nbytes = currentNbytes - fabric_max_msg_size;
                currentNbytes = fabric_max_msg_size;
            } else {
                pending_nbytes = 0;
            }
            // Issue an IO
            fabric_write(keys[0], (void *)currentLocal, currentNbytes,
                         (uint64_t)(base_addr_list[0]) + currentOffset,
                         (*fiAddr)[memServerIds[0]], famCtx, false);
            currentNbytes = pending_nbytes;
            if (currentNbytes > 0) {
                currentOffset = currentOffset + fabric_max_msg_size;
                currentLocal = currentLocal + fabric_max_msg_size;
            }
        }

        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    uint64_t chunkSize;
    uint64_t nBytesWritten = 0;
    /*
     * If the offset is displaced from a starting position of a block
     * issue a seperate IO for that chunk of data
     */
    uint64_t firstBlockSize = interleaveSize - displacement;
    if (firstBlockSize < interleaveSize) {
        // Calculate the chunk of size for each IO
        if (nbytes < firstBlockSize)
            chunkSize = nbytes;
        else
            chunkSize = firstBlockSize;
        // Issue an IO
        fabric_write(keys[currentServerIndex], (void *)currentLocalPtr,
                     chunkSize,
                     (uint64_t)(base_addr_list[currentServerIndex]) +
                         currentFamPtr + displacement,
                     (*fiAddr)[memServerIds[currentServerIndex]],
                     get_context(descriptor), false);
        // go to next server for next block of data
        currentServerIndex++;
        // If last memory server is reached roll back to first server and
        // incement the interleave block by one
        if (currentServerIndex == usedMemsrvCnt) {
            currentServerIndex = 0;
            // Increment a block everytime cricles through used servers.
            currentFamPtr += interleaveSize;
        }
        // Increment the number of bytes written
        nBytesWritten += chunkSize;
        // Increment the local buffer pointer by number of bytes written
        currentLocalPtr += chunkSize;
    }
    // Loop until the requested number of bytes are issued through IO
    while (nBytesWritten < nbytes) {
        // Calculate the chunk of size for each IO
        //(For last IO the chunk size may not be same as inetrleave size)
        if ((nbytes - nBytesWritten) < interleaveSize)
            chunkSize = nbytes - nBytesWritten;
        else
            chunkSize = interleaveSize;
        // Issue an IO
        fabric_write(
            keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
            (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
            (*fiAddr)[memServerIds[currentServerIndex]],
            get_context(descriptor), false);
        // go to next server for next block of data
        currentServerIndex++;
        // If last memory server is reached roll back to first server and
        // incement the interleave block by one
        if (currentServerIndex == usedMemsrvCnt) {
            currentServerIndex = 0;
            currentFamPtr += interleaveSize;
        }
        // Increment the number of bytes written
        currentLocalPtr += chunkSize;
        // Increment the local buffer pointer by number of bytes written
        nBytesWritten += chunkSize;
    }
    return;
}

void Fam_Ops_Libfabric::get_nonblocking(void *local, Fam_Descriptor *descriptor,
                                        uint64_t offset, uint64_t nbytes) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block using the given offset,
    // else issue a single IO to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        uint64_t currentLocal = (uint64_t)local;
        uint64_t currentOffset = offset;
        uint64_t currentNbytes = nbytes;
        uint64_t pending_nbytes;
        while (currentNbytes > 0) {
            if (currentNbytes > fabric_max_msg_size) {
                pending_nbytes = currentNbytes - fabric_max_msg_size;
                currentNbytes = fabric_max_msg_size;
            } else {
                pending_nbytes = 0;
            }

            // Issue an IO
            fabric_read(keys[0], (void *)currentLocal, currentNbytes,
                        (uint64_t)(base_addr_list[0]) + currentOffset,
                        (*fiAddr)[memServerIds[0]], famCtx, false);
            currentNbytes = pending_nbytes;
            if (currentNbytes > 0) {
                currentOffset = currentOffset + fabric_max_msg_size;
                currentLocal = currentLocal + fabric_max_msg_size;
            }
        }

        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;
    uint64_t chunkSize;
    uint64_t nBytesRead = 0;
    /*
     * If the offset is displaced from a starting position of a block
     * issue a seperate IO for that chunk of data
     */
    uint64_t firstBlockSize = interleaveSize - displacement;
    if (firstBlockSize < interleaveSize) {
        // Calculate the chunk of size for each IO
        if (nbytes < firstBlockSize)
            chunkSize = nbytes;
        else
            chunkSize = firstBlockSize;

        // Issue an IO
        fabric_read(keys[currentServerIndex], (void *)currentLocalPtr,
                    chunkSize,
                    (uint64_t)(base_addr_list[currentServerIndex]) +
                        currentFamPtr + displacement,
                    (*fiAddr)[memServerIds[currentServerIndex]],
                    get_context(descriptor), false);
        // go to next server for next block of data
        currentServerIndex++;
        // If last memory server is reached roll back to first server and
        // incement the interleave block by one
        if (currentServerIndex == usedMemsrvCnt) {
            currentServerIndex = 0;
            // Increment a block everytime cricles through used servers.
            currentFamPtr += interleaveSize;
        }
        // Increment the number of bytes read
        nBytesRead += chunkSize;
        // Increment the local buffer pointer by number of bytes read
        currentLocalPtr += chunkSize;
    }
    // Loop until the requested number of bytes are issued through IO
    while (nBytesRead < nbytes) {
        // Calculate the chunk of size for each IO
        //(For last IO the chunk size may not be same as inetrleave size)
        if ((nbytes - nBytesRead) < interleaveSize)
            chunkSize = nbytes - nBytesRead;
        else
            chunkSize = interleaveSize;
        // Issue an IO
        fabric_read(
            keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
            (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
            (*fiAddr)[memServerIds[currentServerIndex]],
            get_context(descriptor), false);
        // go to next server for next block of data
        currentServerIndex++;
        // If last memory server is reached roll back to first server and
        // incement the interleave block by one
        if (currentServerIndex == usedMemsrvCnt) {
            currentServerIndex = 0;
            currentFamPtr += interleaveSize;
        }
        // Increment the number of bytes read
        currentLocalPtr += chunkSize;
        // Increment the local buffer pointer by number of bytes read
        nBytesRead += chunkSize;
    }
    return;
}

void Fam_Ops_Libfabric::scatter_nonblocking(
    void *local, Fam_Descriptor *descriptor, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block, else issue a single IO
    // to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        if (elementSize > fabric_max_msg_size) {
            std::ostringstream message;
            message << "IO size is larger than maximum supported IO size by "
                       "provider";
            THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_LIBFABRIC,
                            message.str().c_str());
        }
        // Issue an IO
        fabric_scatter_stride(keys[0], (void *)local, elementSize, firstElement,
                              nElements, stride, (*fiAddr)[memServerIds[0]],
                              famCtx, fabric_iov_limit,
                              (uint64_t)(base_addr_list[0]), false);
        return;
    }

    // Initialize the offset to first element position
    uint64_t offset = firstElement * elementSize;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    for (int i = 0; i < (int)nElements; i++) {
        // Current memory server Id index
        uint64_t currentServerIndex =
            ((offset / interleaveSize) % usedMemsrvCnt);
        // Current remote location in FAM
        uint64_t currentFamPtr =
            (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
            interleaveSize;
        // Displacement from the starting position of the interleave block
        uint64_t displacement = offset % interleaveSize;
        uint64_t chunkSize;
        uint64_t nBytesWritten = 0;
        /*
         * If the offset is displaced from a starting position of a block
         * issue a seperate IO for that chunk of data
         */
        uint64_t firstBlockSize = interleaveSize - displacement;
        if (firstBlockSize < interleaveSize) {
            if (elementSize < firstBlockSize)
                chunkSize = elementSize;
            else
                chunkSize = firstBlockSize;
            // Issue an IO
            fabric_write(keys[currentServerIndex], (void *)currentLocalPtr,
                         chunkSize,
                         (uint64_t)(base_addr_list[currentServerIndex]) +
                             currentFamPtr + displacement,
                         (*fiAddr)[memServerIds[currentServerIndex]],
                         get_context(descriptor), false);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                // Increment a block everytime cricles through used servers.
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes written
            nBytesWritten += chunkSize;
            // Increment the local buffer pointer by number of bytes written
            currentLocalPtr += chunkSize;
        }
        // Loop until the requested number of bytes are issued through IO
        while (nBytesWritten < elementSize) {
            // Calculate the chunk of size for each IO
            //(For last IO the chunk size may not be same as inetrleave size)
            if ((elementSize - nBytesWritten) < interleaveSize)
                chunkSize = elementSize - nBytesWritten;
            else
                chunkSize = interleaveSize;
            // Issue an IO
            fabric_write(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), false);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes written
            currentLocalPtr += chunkSize;
            // Increment the local buffer pointer by number of bytes written
            nBytesWritten += chunkSize;
        }
        offset += (stride * elementSize);
    }
    return;
}

void Fam_Ops_Libfabric::gather_nonblocking(
    void *local, Fam_Descriptor *descriptor, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block, else issue a single IO
    // to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        if (elementSize > fabric_max_msg_size) {
            std::ostringstream message;
            message << "IO size is larger than maximum supported IO size by "
                       "provider";
            THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_LIBFABRIC,
                            message.str().c_str());
        }
        // Issue an IO
        fabric_gather_stride(keys[0], (void *)local, elementSize, firstElement,
                             nElements, stride, (*fiAddr)[memServerIds[0]],
                             famCtx, fabric_iov_limit,
                             (uint64_t)(base_addr_list[0]), false);
        return;
    }

    // Initialize the offset to first element position
    uint64_t offset = firstElement * elementSize;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    for (int i = 0; i < (int)nElements; i++) {
        // Current memory server Id index
        uint64_t currentServerIndex =
            ((offset / interleaveSize) % usedMemsrvCnt);
        // Current remote location in FAM
        uint64_t currentFamPtr =
            (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
            interleaveSize;
        // Displacement from the starting position of the interleave block
        uint64_t displacement = offset % interleaveSize;
        uint64_t chunkSize;
        uint64_t nBytesRead = 0;
        /*
         * If the offset is displaced from a starting position of a block
         * issue a seperate IO for that chunk of data
         */
        uint64_t firstBlockSize = interleaveSize - displacement;
        if (firstBlockSize < interleaveSize) {
            if (elementSize < firstBlockSize)
                chunkSize = elementSize;
            else
                chunkSize = firstBlockSize;
            // Issue an IO
            fabric_read(keys[currentServerIndex], (void *)currentLocalPtr,
                        chunkSize,
                        (uint64_t)(base_addr_list[currentServerIndex]) +
                            currentFamPtr + displacement,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor), false);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                // Increment a block everytime cricles through used servers.
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes read
            nBytesRead += chunkSize;
            // Increment the local buffer pointer by number of bytes read
            currentLocalPtr += chunkSize;
        }
        // Loop until the requested number of bytes are issued through IO
        while (nBytesRead < elementSize) {
            // Calculate the chunk of size for each IO
            //(For last IO the chunk size may not be same as inetrleave size)
            if ((elementSize - nBytesRead) < interleaveSize)
                chunkSize = elementSize - nBytesRead;
            else
                chunkSize = interleaveSize;
            // Issue an IO
            fabric_read(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), false);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes read
            currentLocalPtr += chunkSize;
            // Increment the local buffer pointer by number of bytes read
            nBytesRead += chunkSize;
        }
        offset += (stride * elementSize);
    }
    return;
}

void Fam_Ops_Libfabric::scatter_nonblocking(void *local,
                                            Fam_Descriptor *descriptor,
                                            uint64_t nElements,
                                            uint64_t *elementIndex,
                                            uint64_t elementSize) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block, else issue a single IO
    // to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        if (elementSize > fabric_max_msg_size) {
            std::ostringstream message;
            message << "IO size is larger than maximum supported IO size by "
                       "provider";
            THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_LIBFABRIC,
                            message.str().c_str());
        }
        // Issue an IO
        fabric_scatter_index(keys[0], (void *)local, elementSize, elementIndex,
                             nElements, (*fiAddr)[memServerIds[0]], famCtx,
                             fabric_iov_limit, (uint64_t)(base_addr_list[0]),
                             false);
        return;
    }

    // iterate through the indeces provided by user
    uint64_t offset;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    for (int i = 0; i < (int)nElements; i++) {
        offset = elementIndex[i] * elementSize;
        // Current memory server Id index
        uint64_t currentServerIndex =
            ((offset / interleaveSize) % usedMemsrvCnt);
        // Current remote location in FAM
        uint64_t currentFamPtr =
            (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
            interleaveSize;
        // Displacement from the starting position of the interleave block
        uint64_t displacement = offset % interleaveSize;
        uint64_t chunkSize;
        uint64_t nBytesWritten = 0;
        /*
         * If the offset is displaced from a starting position of a block
         * issue a seperate IO for that chunk of data
         */
        uint64_t firstBlockSize = interleaveSize - displacement;
        if (firstBlockSize < interleaveSize) {
            if (elementSize < firstBlockSize)
                chunkSize = elementSize;
            else
                chunkSize = firstBlockSize;
            // Issue an IO
            fabric_write(keys[currentServerIndex], (void *)currentLocalPtr,
                         chunkSize,
                         (uint64_t)(base_addr_list[currentServerIndex]) +
                             currentFamPtr + displacement,
                         (*fiAddr)[memServerIds[currentServerIndex]],
                         get_context(descriptor), false);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                // Increment a block everytime cricles through used servers.
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes written
            nBytesWritten += chunkSize;
            // Increment the local buffer pointer by number of bytes written
            currentLocalPtr += chunkSize;
        }
        // Loop until the requested number of bytes are issued through IO
        while (nBytesWritten < elementSize) {
            // Calculate the chunk of size for each IO
            //(For last IO the chunk size may not be same as inetrleave size)
            if ((elementSize - nBytesWritten) < interleaveSize)
                chunkSize = elementSize - nBytesWritten;
            else
                chunkSize = interleaveSize;
            // Issue an IO
            fabric_write(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), false);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes written
            currentLocalPtr += chunkSize;
            // Increment the local buffer pointer by number of bytes written
            nBytesWritten += chunkSize;
        }
    }
    return;
}

void Fam_Ops_Libfabric::gather_nonblocking(void *local,
                                           Fam_Descriptor *descriptor,
                                           uint64_t nElements,
                                           uint64_t *elementIndex,
                                           uint64_t elementSize) {
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    Fam_Context *famCtx = get_context(descriptor);
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    // Check if the dataitem is spread across more than one memory server,
    // if spread across multiple servers calculate the index of first server,
    // first block and the displacement within the block, else issue a single IO
    // to a memory server where that dataitem is located.
    if (usedMemsrvCnt == 1) {
        if (elementSize > fabric_max_msg_size) {
            std::ostringstream message;
            message << "IO size is larger than maximum supported IO size by "
                       "provider";
            THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_LIBFABRIC,
                            message.str().c_str());
        }
        // Issue an IO
        fabric_gather_index(keys[0], (void *)local, elementSize, elementIndex,
                            nElements, (*fiAddr)[memServerIds[0]], famCtx,
                            fabric_iov_limit, (uint64_t)(base_addr_list[0]),
                            false);
        return;
    }

    // iterate through the indeces provided by user
    uint64_t offset;
    // Current Local pointer position
    uint64_t currentLocalPtr = (uint64_t)local;
    for (int i = 0; i < (int)nElements; i++) {
        offset = elementIndex[i] * elementSize;
        // Current memory server Id index
        uint64_t currentServerIndex =
            ((offset / interleaveSize) % usedMemsrvCnt);
        // Current remote location in FAM
        uint64_t currentFamPtr =
            (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
            interleaveSize;
        // Displacement from the starting position of the interleave block
        uint64_t displacement = offset % interleaveSize;
        uint64_t chunkSize;
        uint64_t nBytesRead = 0;
        /*
         * If the offset is displaced from a starting position of a block
         * issue a seperate IO for that chunk of data
         */
        uint64_t firstBlockSize = interleaveSize - displacement;
        if (firstBlockSize < interleaveSize) {
            if (elementSize < firstBlockSize)
                chunkSize = elementSize;
            else
                chunkSize = firstBlockSize;
            // Issue IO
            fabric_read(keys[currentServerIndex], (void *)currentLocalPtr,
                        chunkSize,
                        (uint64_t)(base_addr_list[currentServerIndex]) +
                            currentFamPtr + displacement,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor), false);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                // Increment a block everytime cricles through used servers.
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes read
            nBytesRead += chunkSize;
            // Increment the local buffer pointer by number of bytes read
            currentLocalPtr += chunkSize;
        }
        // Loop until the requested number of bytes are issued through IO
        while (nBytesRead < elementSize) {
            // Calculate the chunk of size for each IO
            //(For last IO the chunk size may not be same as inetrleave size)
            if ((elementSize - nBytesRead) < interleaveSize)
                chunkSize = elementSize - nBytesRead;
            else
                chunkSize = interleaveSize;
            // Issue an IO
            fabric_read(
                keys[currentServerIndex], (void *)currentLocalPtr, chunkSize,
                (uint64_t)(base_addr_list[currentServerIndex]) + currentFamPtr,
                (*fiAddr)[memServerIds[currentServerIndex]],
                get_context(descriptor), false);
            // go to next server for next block of data
            currentServerIndex++;
            // If last memory server is reached roll back to first server and
            // incement the interleave block by one
            if (currentServerIndex == usedMemsrvCnt) {
                currentServerIndex = 0;
                currentFamPtr += interleaveSize;
            }
            // Increment the number of bytes read
            currentLocalPtr += chunkSize;
            // Increment the local buffer pointer by number of bytes read
            nBytesRead += chunkSize;
        }
    }
    return;
}

void Fam_Ops_Libfabric::quiet_context(Fam_Context *context = NULL) {
    if (famContextModel == FAM_CONTEXT_DEFAULT && context == NULL) {
        std::list<std::shared_future<void>> resultList;
        int err = 0;
        std::string errmsg;
        int exception_caught = 0;

        for (auto context : *defContexts) {
            std::future<void> result =
                (std::async(std::launch::async, fabric_quiet, context.second));
            resultList.push_back(result.share());
        }
        for (auto result : resultList) {

            try {
                result.get();
            } catch (Fam_Exception &e) {
                err = e.fam_error();
                errmsg = e.fam_error_msg();
                exception_caught = 1;
            }
        }

        if (exception_caught == 1) {
            THROW_ERRNO_MSG(Fam_Datapath_Exception, get_fam_error(err), errmsg);
        }

    } else if (famContextModel == FAM_CONTEXT_DEFAULT && context != NULL) {
        fabric_quiet(context);
    }
    return;
}

void Fam_Ops_Libfabric::quiet(Fam_Region_Descriptor *descriptor) {
    if (famContextModel == FAM_CONTEXT_DEFAULT) {
        quiet_context(get_context());
        return;
    }
}

void Fam_Ops_Libfabric::fence(Fam_Region_Descriptor *descriptor) {
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();

    uint64_t nodeId = 0;
    if (famContextModel == FAM_CONTEXT_DEFAULT) {
        for (auto memServers : *memServerAddrs) {
            nodeId = memServers.first;
            fabric_fence((*fiAddr)[nodeId], get_context(NULL));
        }
    }
}

// Note : In case of copy operation across memoryserver this API is blocking
// and no need to wait on copy.
void *Fam_Ops_Libfabric::copy(Fam_Descriptor *src, uint64_t srcOffset,
                              Fam_Descriptor *dest, uint64_t destOffset,
                              uint64_t nbytes) {
    // Perform actual copy operation at the destination memory server

    return famAllocator->copy(src, srcOffset, dest, destOffset, nbytes);
}

void Fam_Ops_Libfabric::wait_for_copy(void *waitObj) {
    return famAllocator->wait_for_copy(waitObj);
}

void *Fam_Ops_Libfabric::backup(Fam_Descriptor *descriptor,
                                const char *BackupName) {

    return famAllocator->backup(descriptor, BackupName);
}

void *Fam_Ops_Libfabric::restore(const char *BackupName, Fam_Descriptor *dest) {

    return famAllocator->restore(dest, BackupName);
}

void Fam_Ops_Libfabric::wait_for_backup(void *waitObj) {
    return famAllocator->wait_for_backup(waitObj);
}
void Fam_Ops_Libfabric::wait_for_restore(void *waitObj) {
    return famAllocator->wait_for_restore(waitObj);
}

uint64_t Fam_Ops_Libfabric::progress_context() {
    uint64_t pending = 0;
    pending = fabric_progress(get_context());
    return pending;
}

uint64_t Fam_Ops_Libfabric::progress() { return progress_context(); }

void Fam_Ops_Libfabric::check_progress(Fam_Region_Descriptor *descriptor) {
    if (famContextModel == FAM_CONTEXT_DEFAULT) {

        for (auto context : *defContexts) {
            Fam_Context *famCtx = context.second;
            uint64_t success = fi_cntr_read(famCtx->get_txCntr());
            success += fi_cntr_read(famCtx->get_rxCntr());
        }
    }
    return;
}

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   int32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_ATOMIC_WRITE,
                      FI_INT32, (*fiAddr)[memServerIds[0]],
                      get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_ATOMIC_WRITE, FI_INT32,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   int64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_ATOMIC_WRITE,
                      FI_INT64, (*fiAddr)[memServerIds[0]],
                      get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_ATOMIC_WRITE, FI_INT64,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_ATOMIC_WRITE,
                      FI_UINT32, (*fiAddr)[memServerIds[0]],
                      get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_ATOMIC_WRITE, FI_UINT32,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_ATOMIC_WRITE,
                      FI_UINT64, (*fiAddr)[memServerIds[0]],
                      get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_ATOMIC_WRITE, FI_UINT64,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   float value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_ATOMIC_WRITE,
                      FI_FLOAT, (*fiAddr)[memServerIds[0]],
                      get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(float) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_ATOMIC_WRITE, FI_FLOAT,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   double value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_ATOMIC_WRITE,
                      FI_DOUBLE, (*fiAddr)[memServerIds[0]],
                      get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(double) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_ATOMIC_WRITE, FI_DOUBLE,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   int32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_SUM, FI_INT32,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_SUM, FI_INT32, (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   int64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_SUM, FI_INT64,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_SUM, FI_INT64, (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_SUM, FI_UINT32,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_SUM, FI_UINT32,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_SUM, FI_UINT64,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_SUM, FI_UINT64,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   float value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_SUM, FI_FLOAT,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(float) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_SUM, FI_FLOAT, (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   double value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_SUM, FI_DOUBLE,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(double) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_SUM, FI_DOUBLE,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_subtract(Fam_Descriptor *descriptor,
                                        uint64_t offset, int32_t value) {
    atomic_add(descriptor, offset, -value);
    return;
}

void Fam_Ops_Libfabric::atomic_subtract(Fam_Descriptor *descriptor,
                                        uint64_t offset, int64_t value) {
    atomic_add(descriptor, offset, -value);
    return;
}

void Fam_Ops_Libfabric::atomic_subtract(Fam_Descriptor *descriptor,
                                        uint64_t offset, uint32_t value) {
    atomic_add(descriptor, offset, -value);
    return;
}

void Fam_Ops_Libfabric::atomic_subtract(Fam_Descriptor *descriptor,
                                        uint64_t offset, uint64_t value) {
    atomic_add(descriptor, offset, -value);
    return;
}

void Fam_Ops_Libfabric::atomic_subtract(Fam_Descriptor *descriptor,
                                        uint64_t offset, float value) {
    atomic_add(descriptor, offset, -value);
    return;
}

void Fam_Ops_Libfabric::atomic_subtract(Fam_Descriptor *descriptor,
                                        uint64_t offset, double value) {
    atomic_add(descriptor, offset, -value);
    return;
}

void Fam_Ops_Libfabric::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   int32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MIN, FI_INT32,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MIN, FI_INT32, (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   int64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MIN, FI_INT64,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MIN, FI_INT64, (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MIN, FI_UINT32,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MIN, FI_UINT32,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MIN, FI_UINT64,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MIN, FI_UINT64,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   float value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MIN, FI_FLOAT,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(float) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MIN, FI_FLOAT, (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   double value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MIN, FI_DOUBLE,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(double) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MIN, FI_DOUBLE,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   int32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MAX, FI_INT32,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MAX, FI_INT32, (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   int64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MAX, FI_INT64,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MAX, FI_INT64, (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MAX, FI_UINT32,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MAX, FI_UINT32,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MAX, FI_UINT64,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MAX, FI_UINT64,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   float value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MAX, FI_FLOAT,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(float) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MAX, FI_FLOAT, (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   double value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_MAX, FI_DOUBLE,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(double) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_MAX, FI_DOUBLE,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_and(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_BAND, FI_UINT32,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_BAND, FI_UINT32,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_and(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_BAND, FI_UINT64,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_BAND, FI_UINT64,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_or(Fam_Descriptor *descriptor, uint64_t offset,
                                  uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_BOR, FI_UINT32,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_BOR, FI_UINT32,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_or(Fam_Descriptor *descriptor, uint64_t offset,
                                  uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_BOR, FI_UINT64,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_BOR, FI_UINT64,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_xor(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_BXOR, FI_UINT32,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_BXOR, FI_UINT32,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_xor(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_atomic(keys[0], (void *)&value, offset, FI_BXOR, FI_UINT64,
                      (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_atomic(keys[currentServerIndex], (void *)&value, currentFamPtr,
                  FI_BXOR, FI_UINT64,
                  (*fiAddr)[memServerIds[currentServerIndex]],
                  get_context(descriptor));
    return;
}

int32_t Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                                int32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&value, (void *)&old, offset, FI_ATOMIC_WRITE,
            FI_INT32, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_ATOMIC_WRITE, FI_INT32,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

int64_t Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                                int64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&value, (void *)&old, offset, FI_ATOMIC_WRITE,
            FI_INT64, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_ATOMIC_WRITE, FI_INT64,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                                 uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&value, (void *)&old, offset, FI_ATOMIC_WRITE,
            FI_UINT32, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_ATOMIC_WRITE, FI_UINT32,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                                 uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&value, (void *)&old, offset, FI_ATOMIC_WRITE,
            FI_UINT64, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_ATOMIC_WRITE, FI_UINT64,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

float Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                              float value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    float old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&value, (void *)&old, offset, FI_ATOMIC_WRITE,
            FI_FLOAT, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(float) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_ATOMIC_WRITE, FI_FLOAT,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

double Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                               double value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    double old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&value, (void *)&old, offset, FI_ATOMIC_WRITE,
            FI_DOUBLE, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(double) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_ATOMIC_WRITE, FI_DOUBLE,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

int32_t Fam_Ops_Libfabric::compare_swap(Fam_Descriptor *descriptor,
                                        uint64_t offset, int32_t oldValue,
                                        int32_t newValue) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_compare_atomic(keys[0], (void *)&oldValue, (void *)&old,
                              (void *)&newValue, offset, FI_CSWAP, FI_INT32,
                              (*fiAddr)[memServerIds[0]],
                              get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_compare_atomic(
        keys[currentServerIndex], (void *)&oldValue, (void *)&old,
        (void *)&newValue, currentFamPtr, FI_CSWAP, FI_INT32,
        (*fiAddr)[memServerIds[currentServerIndex]], get_context(descriptor));
    return old;
}

int64_t Fam_Ops_Libfabric::compare_swap(Fam_Descriptor *descriptor,
                                        uint64_t offset, int64_t oldValue,
                                        int64_t newValue) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_compare_atomic(keys[0], (void *)&oldValue, (void *)&old,
                              (void *)&newValue, offset, FI_CSWAP, FI_INT64,
                              (*fiAddr)[memServerIds[0]],
                              get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_compare_atomic(
        keys[currentServerIndex], (void *)&oldValue, (void *)&old,
        (void *)&newValue, currentFamPtr, FI_CSWAP, FI_INT64,
        (*fiAddr)[memServerIds[currentServerIndex]], get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::compare_swap(Fam_Descriptor *descriptor,
                                         uint64_t offset, uint32_t oldValue,
                                         uint32_t newValue) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_compare_atomic(keys[0], (void *)&oldValue, (void *)&old,
                              (void *)&newValue, offset, FI_CSWAP, FI_UINT32,
                              (*fiAddr)[memServerIds[0]],
                              get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_compare_atomic(
        keys[currentServerIndex], (void *)&oldValue, (void *)&old,
        (void *)&newValue, currentFamPtr, FI_CSWAP, FI_UINT32,
        (*fiAddr)[memServerIds[currentServerIndex]], get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::compare_swap(Fam_Descriptor *descriptor,
                                         uint64_t offset, uint64_t oldValue,
                                         uint64_t newValue) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_compare_atomic(keys[0], (void *)&oldValue, (void *)&old,
                              (void *)&newValue, offset, FI_CSWAP, FI_UINT64,
                              (*fiAddr)[memServerIds[0]],
                              get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_compare_atomic(
        keys[currentServerIndex], (void *)&oldValue, (void *)&old,
        (void *)&newValue, currentFamPtr, FI_CSWAP, FI_UINT64,
        (*fiAddr)[memServerIds[currentServerIndex]], get_context(descriptor));
    return old;
}

int128_t Fam_Ops_Libfabric::compare_swap(Fam_Descriptor *descriptor,
                                         uint64_t offset, int128_t oldValue,
                                         int128_t newValue) {

    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int128_t local;

    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        famAllocator->acquire_CAS_lock(descriptor, memServerIds[0]);
        try {
            fabric_read(keys[0], &local, sizeof(int128_t), offset,
                        (*fiAddr)[memServerIds[0]], get_context(descriptor));
        } catch (...) {
            famAllocator->release_CAS_lock(descriptor, memServerIds[0]);
            throw;
        }

        if (local == oldValue) {
            try {
                fabric_write(keys[0], &newValue, sizeof(int128_t), offset,
                             (*fiAddr)[memServerIds[0]],
                             get_context(descriptor));
            } catch (...) {
                famAllocator->release_CAS_lock(descriptor, memServerIds[0]);
                throw;
            }
        }
        famAllocator->release_CAS_lock(descriptor, memServerIds[0]);
        return local;
    }
    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int128_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    famAllocator->acquire_CAS_lock(descriptor,
                                   memServerIds[currentServerIndex]);
    try {
        fabric_read(keys[currentServerIndex], &local, sizeof(int128_t),
                    currentFamPtr, (*fiAddr)[memServerIds[currentServerIndex]],
                    get_context(descriptor));
    } catch (...) {
        famAllocator->release_CAS_lock(descriptor,
                                       memServerIds[currentServerIndex]);
        throw;
    }

    if (local == oldValue) {
        try {
            fabric_write(keys[currentServerIndex], &newValue, sizeof(int128_t),
                         currentFamPtr,
                         (*fiAddr)[memServerIds[currentServerIndex]],
                         get_context(descriptor));
        } catch (...) {
            famAllocator->release_CAS_lock(descriptor,
                                           memServerIds[currentServerIndex]);
            throw;
        }
    }
    famAllocator->release_CAS_lock(descriptor,
                                   memServerIds[currentServerIndex]);
    return local;
}

int32_t Fam_Ops_Libfabric::atomic_fetch_int32(Fam_Descriptor *descriptor,
                                              uint64_t offset) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t result;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&result, (void *)&result, offset, FI_ATOMIC_READ,
            FI_INT32, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return result;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&result,
                        (void *)&result, currentFamPtr, FI_ATOMIC_READ,
                        FI_INT32, (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return result;
}

int64_t Fam_Ops_Libfabric::atomic_fetch_int64(Fam_Descriptor *descriptor,
                                              uint64_t offset) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t result;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&result, (void *)&result, offset, FI_ATOMIC_READ,
            FI_INT64, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return result;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&result,
                        (void *)&result, currentFamPtr, FI_ATOMIC_READ,
                        FI_INT64, (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return result;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_uint32(Fam_Descriptor *descriptor,
                                                uint64_t offset) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t result;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&result, (void *)&result, offset, FI_ATOMIC_READ,
            FI_UINT32, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return result;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&result,
                        (void *)&result, currentFamPtr, FI_ATOMIC_READ,
                        FI_UINT32, (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return result;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_uint64(Fam_Descriptor *descriptor,
                                                uint64_t offset) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t result;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&result, (void *)&result, offset, FI_ATOMIC_READ,
            FI_UINT64, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return result;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&result,
                        (void *)&result, currentFamPtr, FI_ATOMIC_READ,
                        FI_UINT64, (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return result;
}

float Fam_Ops_Libfabric::atomic_fetch_float(Fam_Descriptor *descriptor,
                                            uint64_t offset) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    float result;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&result, (void *)&result, offset, FI_ATOMIC_READ,
            FI_FLOAT, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return result;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(float) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&result,
                        (void *)&result, currentFamPtr, FI_ATOMIC_READ,
                        FI_FLOAT, (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return result;
}

double Fam_Ops_Libfabric::atomic_fetch_double(Fam_Descriptor *descriptor,
                                              uint64_t offset) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    double result;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(
            keys[0], (void *)&result, (void *)&result, offset, FI_ATOMIC_READ,
            FI_DOUBLE, (*fiAddr)[memServerIds[0]], get_context(descriptor));
        return result;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(double) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&result,
                        (void *)&result, currentFamPtr, FI_ATOMIC_READ,
                        FI_DOUBLE, (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return result;
}

int32_t Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                            uint64_t offset, int32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_SUM, FI_INT32, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_SUM, FI_INT32,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

int64_t Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                            uint64_t offset, int64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_SUM, FI_INT64, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_SUM, FI_INT64,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_SUM, FI_UINT32, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_SUM, FI_UINT32,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_SUM, FI_UINT64, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_SUM, FI_UINT64,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

float Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                          uint64_t offset, float value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    float old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_SUM, FI_FLOAT, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(float) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_SUM, FI_FLOAT,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

double Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                           uint64_t offset, double value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    double old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_SUM, FI_DOUBLE, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(double) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_SUM, FI_DOUBLE,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

int32_t Fam_Ops_Libfabric::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                                 uint64_t offset,
                                                 int32_t value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

int64_t Fam_Ops_Libfabric::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                                 uint64_t offset,
                                                 int64_t value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                                  uint64_t offset,
                                                  uint32_t value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                                  uint64_t offset,
                                                  uint64_t value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

float Fam_Ops_Libfabric::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                               uint64_t offset, float value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

double Fam_Ops_Libfabric::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                                uint64_t offset, double value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

int32_t Fam_Ops_Libfabric::atomic_fetch_min(Fam_Descriptor *descriptor,
                                            uint64_t offset, int32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MIN, FI_INT32, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MIN, FI_INT32,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

int64_t Fam_Ops_Libfabric::atomic_fetch_min(Fam_Descriptor *descriptor,
                                            uint64_t offset, int64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MIN, FI_INT64, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MIN, FI_INT64,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_min(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MIN, FI_UINT32, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MIN, FI_UINT32,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_min(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MIN, FI_UINT64, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MIN, FI_UINT64,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

float Fam_Ops_Libfabric::atomic_fetch_min(Fam_Descriptor *descriptor,
                                          uint64_t offset, float value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    float old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MIN, FI_FLOAT, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(float) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MIN, FI_FLOAT,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

double Fam_Ops_Libfabric::atomic_fetch_min(Fam_Descriptor *descriptor,
                                           uint64_t offset, double value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    double old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MIN, FI_DOUBLE, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(double) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MIN, FI_DOUBLE,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

int32_t Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                            uint64_t offset, int32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MAX, FI_INT32, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MAX, FI_INT32,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

int64_t Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                            uint64_t offset, int64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MAX, FI_INT64, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MAX, FI_INT64,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MAX, FI_UINT32, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MAX, FI_UINT32,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MAX, FI_UINT64, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MAX, FI_UINT64,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

float Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                          uint64_t offset, float value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    float old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MAX, FI_FLOAT, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(float) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MAX, FI_FLOAT,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

double Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                           uint64_t offset, double value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    double old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_MAX, FI_DOUBLE, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(double) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_MAX, FI_DOUBLE,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_and(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_BAND, FI_UINT32, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_BAND, FI_UINT32,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_and(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_BAND, FI_UINT64, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_BAND, FI_UINT64,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_or(Fam_Descriptor *descriptor,
                                            uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_BOR, FI_UINT32, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_BOR, FI_UINT32,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_or(Fam_Descriptor *descriptor,
                                            uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_BOR, FI_UINT64, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_BOR, FI_UINT64,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_xor(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_BXOR, FI_UINT32, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint32_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_BXOR, FI_UINT32,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_xor(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        fabric_fetch_atomic(keys[0], (void *)&value, (void *)&old, offset,
                            FI_BXOR, FI_UINT64, (*fiAddr)[memServerIds[0]],
                            get_context(descriptor));
        return old;
    }

    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(uint64_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    fabric_fetch_atomic(keys[currentServerIndex], (void *)&value, (void *)&old,
                        currentFamPtr, FI_BXOR, FI_UINT64,
                        (*fiAddr)[memServerIds[currentServerIndex]],
                        get_context(descriptor));
    return old;
}

void Fam_Ops_Libfabric::abort(int status) FAM_OPS_UNIMPLEMENTED(void__);

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   int128_t value) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        famAllocator->acquire_CAS_lock(descriptor, memServerIds[0]);
        try {
            fabric_write(keys[0], &value, sizeof(int128_t), offset,
                         (*fiAddr)[memServerIds[0]], get_context(descriptor),
                         false);
        } catch (...) {
            famAllocator->release_CAS_lock(descriptor, memServerIds[0]);
            throw;
        }
        famAllocator->release_CAS_lock(descriptor, memServerIds[0]);
        return;
    }
    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int128_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    famAllocator->acquire_CAS_lock(descriptor,
                                   memServerIds[currentServerIndex]);
    try {
        fabric_write(keys[currentServerIndex], &value, sizeof(int128_t),
                     currentFamPtr, (*fiAddr)[memServerIds[currentServerIndex]],
                     get_context(descriptor), false);
    } catch (...) {
        famAllocator->release_CAS_lock(descriptor,
                                       memServerIds[currentServerIndex]);
        throw;
    }
    famAllocator->release_CAS_lock(descriptor,
                                   memServerIds[currentServerIndex]);
}

int128_t Fam_Ops_Libfabric::atomic_fetch_int128(Fam_Descriptor *descriptor,
                                                uint64_t offset) {
    std::ostringstream message;
    uint64_t *memServerIds = descriptor->get_memserver_ids();
    size_t interleaveSize = descriptor->get_interleave_size();
    uint64_t *keys = descriptor->get_keys();
    void **base_addr_list = descriptor->get_base_address_list();
    uint64_t usedMemsrvCnt = descriptor->get_used_memsrv_cnt();

    int128_t local;
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    if (usedMemsrvCnt == 1) {
        offset += (uint64_t)base_addr_list[0];
        famAllocator->acquire_CAS_lock(descriptor, memServerIds[0]);
        try {
            fabric_read(keys[0], &local, sizeof(int128_t), offset,
                        (*fiAddr)[memServerIds[0]], get_context(descriptor));
        } catch (...) {
            famAllocator->release_CAS_lock(descriptor, memServerIds[0]);
            throw;
        }
        famAllocator->release_CAS_lock(descriptor, memServerIds[0]);
        return local;
    }
    // Current memory server Id index
    uint64_t currentServerIndex = ((offset / interleaveSize) % usedMemsrvCnt);
    // Current remote location in FAM
    uint64_t currentFamPtr =
        (((offset / interleaveSize) - currentServerIndex) / usedMemsrvCnt) *
        interleaveSize;
    // Displacement from the starting position of the interleave block
    uint64_t displacement = offset % interleaveSize;

    if (displacement + sizeof(int128_t) > interleaveSize) {
        message << "Atmoic operation can not be performed, size of the value "
                   "goes beyond interleave block";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    currentFamPtr += (uint64_t)base_addr_list[currentServerIndex];
    // Add displacement to remote FAM pointer
    currentFamPtr += displacement;

    famAllocator->acquire_CAS_lock(descriptor,
                                   memServerIds[currentServerIndex]);
    try {
        fabric_read(keys[currentServerIndex], &local, sizeof(int128_t),
                    currentFamPtr, (*fiAddr)[memServerIds[currentServerIndex]],
                    get_context(descriptor));
    } catch (...) {
        famAllocator->release_CAS_lock(descriptor,
                                       memServerIds[currentServerIndex]);
        throw;
    }
    famAllocator->release_CAS_lock(descriptor,
                                   memServerIds[currentServerIndex]);
    return local;
}

void Fam_Ops_Libfabric::context_open(uint64_t contextId, Fam_Ops *famOpsObj) {
    // Create a new fam_context
    std::ostringstream message;
    Fam_Context *ctx = new Fam_Context(fi, domain, famThreadModel);
    int ret = fabric_enable_bind_ep(fi, av, eq, ctx->get_ep());
    if (ret < 0) {
        message << "Fam libfabric fabric_enable_bind_ep failed: "
                << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    // ctx mutex lock
    (void)pthread_mutex_lock(&ctxLock);
    // Add it in the context map with unique contextId
    ((Fam_Ops_Libfabric *)famOpsObj)->set_context(ctx);
    defContexts->insert({contextId, ctx});
    // ctx mutex unlock
    (void)pthread_mutex_unlock(&ctxLock);
    return;
}

void Fam_Ops_Libfabric::context_close(uint64_t contextId) {
    // ctx mutex lock
    (void)pthread_mutex_lock(&ctxLock);
    // Remove context from defContexts map
    auto obj = defContexts->find(contextId);
    if (obj == defContexts->end()) {
        // ctx mutex unlock
        (void)pthread_mutex_unlock(&ctxLock);
        THROW_ERR_MSG(Fam_Datapath_Exception, "Context not found");
    } else {
        // Delete context : Need to validate this task.
        delete obj->second;
        // Remove item from map
        defContexts->erase(obj);
        // ctx mutex unlock
        (void)pthread_mutex_unlock(&ctxLock);
    }
    return;
}
} // namespace openfam
