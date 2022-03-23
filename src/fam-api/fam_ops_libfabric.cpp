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
#include "common/fam_ops.h"
#include "common/fam_ops_libfabric.h"
#include "fam/fam.h"
#include "fam/fam_exception.h"

using namespace std;

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
}

Fam_Ops_Libfabric::Fam_Ops_Libfabric(bool source, const char *libfabricProvider,
                                     const char *if_device,
                                     Fam_Thread_Model famTM,
                                     Fam_Allocator_Client *famAlloc,
                                     Fam_Context_Model famCM) {
    std::ostringstream message;
    memoryServerName = NULL;
    service = NULL;
    provider = strdup(libfabricProvider);
    if_device = strdup(if_device);
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
        return ret;
    }
    // Initialize address vector
    if (fi->ep_attr->type == FI_EP_RDM) {
        if ((ret = fabric_initialize_av(fi, domain, eq, &av)) < 0) {
            return ret;
        }
    }

    // Insert the memory server address into address vector
    // Only if it is not source
    if (!isSource) {
        numMemoryNodes = famAllocator->get_num_memory_servers();
        if (numMemoryNodes == 0) {
            message << "Libfabric initialize: memory server name not specified";
            THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
        }
        size_t memServerInfoSize = 0;
        ret = famAllocator->get_memserverinfo_size(&memServerInfoSize);
        if (ret < 0) {
            message << "Fam allocator get_memserverinfo_size failed";
            THROW_ERRNO_MSG(Fam_Allocator_Exception, FAM_ERR_ALLOCATOR,
                            message.str().c_str());
        }

        if (memServerInfoSize) {
            void *memServerInfoBuffer = calloc(1, memServerInfoSize);
            ret = famAllocator->get_memserverinfo(memServerInfoBuffer);

            if (ret < 0) {
                message << "Fam Allocator get_memserverinfo failed";
                THROW_ERRNO_MSG(Fam_Allocator_Exception, FAM_ERR_ALLOCATOR,
                                message.str().c_str());
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
                memcpy(nodeAddr, ((char *)memServerInfoBuffer + bufPtr),
                       addrSize);
                bufPtr += addrSize;
                // Save memory server address in memServerAddrs map
                memServerAddrs->insert(
                    {nodeId, std::make_pair(nodeAddr, addrSize)});

                std::vector<fi_addr_t> tmpAddrV;
                ret = fabric_insert_av((char *)nodeAddr, av, &tmpAddrV);

                if (ret < 0) {
                    // TODO: Log error
                    return ret;
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
                Fam_Context *defaultCtx =
                    new Fam_Context(fi, domain, famThreadModel);
                defContexts->insert({FAM_DEFAULT_CTX_ID, defaultCtx});
                ret = fabric_enable_bind_ep(fi, av, eq, defaultCtx->get_ep());
                if (ret < 0) {
                    // TODO: Log error
                    return ret;
                }
            }
        }
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
    fabric_iov_limit = fi->tx_attr->rma_iov_limit;

    return 0;
}

Fam_Context *Fam_Ops_Libfabric::get_context(Fam_Descriptor *descriptor) {
    std::ostringstream message;
    // Case - FAM_CONTEXT_DEFAULT
    if (famContextModel == FAM_CONTEXT_DEFAULT) {
        return get_defaultCtx(get_context_id());
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
    std::ostringstream message;
    // Write data into memory region with this key
    uint64_t key;
    key = descriptor->get_key();
    offset += (uint64_t)descriptor->get_base_address();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int ret = fabric_write(key, local, nbytes, offset, (*fiAddr)[nodeId],
                           get_context(descriptor));
    return ret;
}

int Fam_Ops_Libfabric::get_blocking(void *local, Fam_Descriptor *descriptor,
                                    uint64_t offset, uint64_t nbytes) {
    std::ostringstream message;
    // Write data into memory region with this key
    uint64_t key;
    key = descriptor->get_key();
    offset += (uint64_t)descriptor->get_base_address();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int ret = fabric_read(key, local, nbytes, offset, (*fiAddr)[nodeId],
                          get_context(descriptor));

    return ret;
}

int Fam_Ops_Libfabric::gather_blocking(void *local, Fam_Descriptor *descriptor,
                                       uint64_t nElements,
                                       uint64_t firstElement, uint64_t stride,
                                       uint64_t elementSize) {

    uint64_t key;

    key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int ret = fabric_gather_stride_blocking(
        key, local, elementSize, firstElement, nElements, stride,
        (*fiAddr)[nodeId], get_context(descriptor), fabric_iov_limit,
        (uint64_t)descriptor->get_base_address());
    return ret;
}

int Fam_Ops_Libfabric::gather_blocking(void *local, Fam_Descriptor *descriptor,
                                       uint64_t nElements,
                                       uint64_t *elementIndex,
                                       uint64_t elementSize) {
    uint64_t key;

    key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int ret = fabric_gather_index_blocking(
        key, local, elementSize, elementIndex, nElements, (*fiAddr)[nodeId],
        get_context(descriptor), fabric_iov_limit,
        (uint64_t)descriptor->get_base_address());
    return ret;
}

int Fam_Ops_Libfabric::scatter_blocking(void *local, Fam_Descriptor *descriptor,
                                        uint64_t nElements,
                                        uint64_t firstElement, uint64_t stride,
                                        uint64_t elementSize) {

    uint64_t key;

    key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int ret = fabric_scatter_stride_blocking(
        key, local, elementSize, firstElement, nElements, stride,
        (*fiAddr)[nodeId], get_context(descriptor), fabric_iov_limit,
        (uint64_t)descriptor->get_base_address());
    return ret;
}

int Fam_Ops_Libfabric::scatter_blocking(void *local, Fam_Descriptor *descriptor,
                                        uint64_t nElements,
                                        uint64_t *elementIndex,
                                        uint64_t elementSize) {
    uint64_t key;

    key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int ret = fabric_scatter_index_blocking(
        key, local, elementSize, elementIndex, nElements, (*fiAddr)[nodeId],
        get_context(descriptor), fabric_iov_limit,
        (uint64_t)descriptor->get_base_address());
    return ret;
}

void Fam_Ops_Libfabric::put_nonblocking(void *local, Fam_Descriptor *descriptor,
                                        uint64_t offset, uint64_t nbytes) {

    uint64_t key;

    key = descriptor->get_key();
    offset += (uint64_t)descriptor->get_base_address();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_write_nonblocking(key, local, nbytes, offset, (*fiAddr)[nodeId],
                             get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::get_nonblocking(void *local, Fam_Descriptor *descriptor,
                                        uint64_t offset, uint64_t nbytes) {
    uint64_t key;

    key = descriptor->get_key();
    offset += (uint64_t)descriptor->get_base_address();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_read_nonblocking(key, local, nbytes, offset, (*fiAddr)[nodeId],
                            get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::gather_nonblocking(
    void *local, Fam_Descriptor *descriptor, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize) {

    uint64_t key;

    key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_gather_stride_nonblocking(key, local, elementSize, firstElement,
                                     nElements, stride, (*fiAddr)[nodeId],
                                     get_context(descriptor), fabric_iov_limit,
                                     (uint64_t)descriptor->get_base_address());
    return;
}

void Fam_Ops_Libfabric::gather_nonblocking(void *local,
                                           Fam_Descriptor *descriptor,
                                           uint64_t nElements,
                                           uint64_t *elementIndex,
                                           uint64_t elementSize) {
    uint64_t key;

    key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_gather_index_nonblocking(key, local, elementSize, elementIndex,
                                    nElements, (*fiAddr)[nodeId],
                                    get_context(descriptor), fabric_iov_limit,
                                    (uint64_t)descriptor->get_base_address());
    return;
}

void Fam_Ops_Libfabric::scatter_nonblocking(
    void *local, Fam_Descriptor *descriptor, uint64_t nElements,
    uint64_t firstElement, uint64_t stride, uint64_t elementSize) {

    uint64_t key;

    key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_scatter_stride_nonblocking(key, local, elementSize, firstElement,
                                      nElements, stride, (*fiAddr)[nodeId],
                                      get_context(descriptor), fabric_iov_limit,
                                      (uint64_t)descriptor->get_base_address());
    return;
}

void Fam_Ops_Libfabric::scatter_nonblocking(void *local,
                                            Fam_Descriptor *descriptor,
                                            uint64_t nElements,
                                            uint64_t *elementIndex,
                                            uint64_t elementSize) {
    uint64_t key;

    key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_scatter_index_nonblocking(key, local, elementSize, elementIndex,
                                     nElements, (*fiAddr)[nodeId],
                                     get_context(descriptor), fabric_iov_limit,
                                     (uint64_t)descriptor->get_base_address());
    return;
}

// Note : In case of copy operation across memoryserver this API is blocking
// and no need to wait on copy.
void *Fam_Ops_Libfabric::copy(Fam_Descriptor *src, uint64_t srcOffset,
                              Fam_Descriptor *dest, uint64_t destOffset,
                              uint64_t nbytes) {
    // Perform actual copy operation at the destination memory server
    // Send additional information to destination:
    // source addr len, source addr

    std::pair<void *, size_t> srcMemSrv;
    auto obj = memServerAddrs->find(src->get_memserver_id());
    if (obj == memServerAddrs->end())
        THROW_ERR_MSG(Fam_Datapath_Exception, "memserver not found");
    else
        srcMemSrv = obj->second;

    return famAllocator->copy(src, srcOffset, (const char *)srcMemSrv.first,
                              (uint32_t)srcMemSrv.second, dest, destOffset,
                              nbytes);
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
        quiet_context(get_defaultCtx(get_context_id()));
        return;
    }
}

uint64_t Fam_Ops_Libfabric::progress_context() {
    uint64_t pending = 0;
    for (auto fam_ctx : *defContexts) {
        pending += fabric_progress(fam_ctx.second);
    }
    return pending;
}

uint64_t Fam_Ops_Libfabric::progress() { return progress_context(); }
void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   int32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_ATOMIC_WRITE, FI_INT32,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   int64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_ATOMIC_WRITE, FI_INT64,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_ATOMIC_WRITE, FI_UINT32,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_ATOMIC_WRITE, FI_UINT64,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   float value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_ATOMIC_WRITE, FI_FLOAT,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   double value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_ATOMIC_WRITE, FI_DOUBLE,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   int32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_SUM, FI_INT32,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   int64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_SUM, FI_INT64,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_SUM, FI_UINT32,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_SUM, FI_UINT64,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   float value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_SUM, FI_FLOAT,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   double value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_SUM, FI_DOUBLE,
                  (*fiAddr)[nodeId], get_context(descriptor));
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
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MIN, FI_INT32,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   int64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MIN, FI_INT64,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MIN, FI_UINT32,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MIN, FI_UINT64,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   float value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MIN, FI_FLOAT,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   double value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MIN, FI_DOUBLE,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   int32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MAX, FI_INT32,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   int64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MAX, FI_INT64,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MAX, FI_UINT32,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MAX, FI_UINT64,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   float value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MAX, FI_FLOAT,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   double value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_MAX, FI_DOUBLE,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_and(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_BAND, FI_UINT32,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_and(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_BAND, FI_UINT64,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_or(Fam_Descriptor *descriptor, uint64_t offset,
                                  uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_BOR, FI_UINT32,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_or(Fam_Descriptor *descriptor, uint64_t offset,
                                  uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_BOR, FI_UINT64,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_xor(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_BXOR, FI_UINT32,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

void Fam_Ops_Libfabric::atomic_xor(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    fabric_atomic(key, (void *)&value, offset, FI_BXOR, FI_UINT64,
                  (*fiAddr)[nodeId], get_context(descriptor));
    return;
}

int32_t Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                                int32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset,
                        FI_ATOMIC_WRITE, FI_INT32, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return old;
}

int64_t Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                                int64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset,
                        FI_ATOMIC_WRITE, FI_INT64, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                                 uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset,
                        FI_ATOMIC_WRITE, FI_UINT32, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                                 uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset,
                        FI_ATOMIC_WRITE, FI_UINT64, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return old;
}

float Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                              float value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    float old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset,
                        FI_ATOMIC_WRITE, FI_FLOAT, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return old;
}

double Fam_Ops_Libfabric::swap(Fam_Descriptor *descriptor, uint64_t offset,
                               double value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    double old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset,
                        FI_ATOMIC_WRITE, FI_DOUBLE, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return old;
}

int32_t Fam_Ops_Libfabric::compare_swap(Fam_Descriptor *descriptor,
                                        uint64_t offset, int32_t oldValue,
                                        int32_t newValue) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t old;
    fabric_compare_atomic(key, (void *)&oldValue, (void *)&old,
                          (void *)&newValue, offset, FI_CSWAP, FI_INT32,
                          (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

int64_t Fam_Ops_Libfabric::compare_swap(Fam_Descriptor *descriptor,
                                        uint64_t offset, int64_t oldValue,
                                        int64_t newValue) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t old;
    fabric_compare_atomic(key, (void *)&oldValue, (void *)&old,
                          (void *)&newValue, offset, FI_CSWAP, FI_INT64,
                          (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::compare_swap(Fam_Descriptor *descriptor,
                                         uint64_t offset, uint32_t oldValue,
                                         uint32_t newValue) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    fabric_compare_atomic(key, (void *)&oldValue, (void *)&old,
                          (void *)&newValue, offset, FI_CSWAP, FI_UINT32,
                          (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::compare_swap(Fam_Descriptor *descriptor,
                                         uint64_t offset, uint64_t oldValue,
                                         uint64_t newValue) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    fabric_compare_atomic(key, (void *)&oldValue, (void *)&old,
                          (void *)&newValue, offset, FI_CSWAP, FI_UINT64,
                          (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

int128_t Fam_Ops_Libfabric::compare_swap(Fam_Descriptor *descriptor,
                                         uint64_t offset, int128_t oldValue,
                                         int128_t newValue) {

    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int128_t local;

    famAllocator->acquire_CAS_lock(descriptor);
    try {
        fabric_read(key, &local, sizeof(int128_t), offset, (*fiAddr)[nodeId],
                    get_context(descriptor));
    } catch (...) {
        famAllocator->release_CAS_lock(descriptor);
        throw;
    }

    if (local == oldValue) {
        try {
            fabric_write(key, &newValue, sizeof(int128_t), offset,
                         (*fiAddr)[nodeId], get_context(descriptor));
        } catch (...) {
            famAllocator->release_CAS_lock(descriptor);
            throw;
        }
    }
    famAllocator->release_CAS_lock(descriptor);
    return local;
}

int32_t Fam_Ops_Libfabric::atomic_fetch_int32(Fam_Descriptor *descriptor,
                                              uint64_t offset) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t result;
    fabric_fetch_atomic(key, (void *)&result, (void *)&result, offset,
                        FI_ATOMIC_READ, FI_INT32, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return result;
}

int64_t Fam_Ops_Libfabric::atomic_fetch_int64(Fam_Descriptor *descriptor,
                                              uint64_t offset) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t result;
    fabric_fetch_atomic(key, (void *)&result, (void *)&result, offset,
                        FI_ATOMIC_READ, FI_INT64, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return result;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_uint32(Fam_Descriptor *descriptor,
                                                uint64_t offset) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t result;
    fabric_fetch_atomic(key, (void *)&result, (void *)&result, offset,
                        FI_ATOMIC_READ, FI_UINT32, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return result;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_uint64(Fam_Descriptor *descriptor,
                                                uint64_t offset) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t result;
    fabric_fetch_atomic(key, (void *)&result, (void *)&result, offset,
                        FI_ATOMIC_READ, FI_UINT64, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return result;
}

float Fam_Ops_Libfabric::atomic_fetch_float(Fam_Descriptor *descriptor,
                                            uint64_t offset) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    float result;
    fabric_fetch_atomic(key, (void *)&result, (void *)&result, offset,
                        FI_ATOMIC_READ, FI_FLOAT, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return result;
}

double Fam_Ops_Libfabric::atomic_fetch_double(Fam_Descriptor *descriptor,
                                              uint64_t offset) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    double result;
    fabric_fetch_atomic(key, (void *)&result, (void *)&result, offset,
                        FI_ATOMIC_READ, FI_DOUBLE, (*fiAddr)[nodeId],
                        get_context(descriptor));
    return result;
}

int32_t Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                            uint64_t offset, int32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_SUM,
                        FI_INT32, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

int64_t Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                            uint64_t offset, int64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_SUM,
                        FI_INT64, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_SUM,
                        FI_UINT32, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_SUM,
                        FI_UINT64, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

float Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                          uint64_t offset, float value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    float old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_SUM,
                        FI_FLOAT, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

double Fam_Ops_Libfabric::atomic_fetch_add(Fam_Descriptor *descriptor,
                                           uint64_t offset, double value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    double old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_SUM,
                        FI_DOUBLE, (*fiAddr)[nodeId], get_context(descriptor));
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
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MIN,
                        FI_INT32, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

int64_t Fam_Ops_Libfabric::atomic_fetch_min(Fam_Descriptor *descriptor,
                                            uint64_t offset, int64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MIN,
                        FI_INT64, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_min(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MIN,
                        FI_UINT32, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_min(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MIN,
                        FI_UINT64, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

float Fam_Ops_Libfabric::atomic_fetch_min(Fam_Descriptor *descriptor,
                                          uint64_t offset, float value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    float old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MIN,
                        FI_FLOAT, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

double Fam_Ops_Libfabric::atomic_fetch_min(Fam_Descriptor *descriptor,
                                           uint64_t offset, double value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    double old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MIN,
                        FI_DOUBLE, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

int32_t Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                            uint64_t offset, int32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int32_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MAX,
                        FI_INT32, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

int64_t Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                            uint64_t offset, int64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    int64_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MAX,
                        FI_INT64, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MAX,
                        FI_UINT32, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MAX,
                        FI_UINT64, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

float Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                          uint64_t offset, float value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    float old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MAX,
                        FI_FLOAT, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

double Fam_Ops_Libfabric::atomic_fetch_max(Fam_Descriptor *descriptor,
                                           uint64_t offset, double value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    double old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_MAX,
                        FI_DOUBLE, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_and(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_BAND,
                        FI_UINT32, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_and(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_BAND,
                        FI_UINT64, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_or(Fam_Descriptor *descriptor,
                                            uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_BOR,
                        FI_UINT32, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_or(Fam_Descriptor *descriptor,
                                            uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_BOR,
                        FI_UINT64, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint32_t Fam_Ops_Libfabric::atomic_fetch_xor(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint32_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint32_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_BXOR,
                        FI_UINT32, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

uint64_t Fam_Ops_Libfabric::atomic_fetch_xor(Fam_Descriptor *descriptor,
                                             uint64_t offset, uint64_t value) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    uint64_t old;
    fabric_fetch_atomic(key, (void *)&value, (void *)&old, offset, FI_BXOR,
                        FI_UINT64, (*fiAddr)[nodeId], get_context(descriptor));
    return old;
}

void Fam_Ops_Libfabric::abort(int status) FAM_OPS_UNIMPLEMENTED(void__);

void Fam_Ops_Libfabric::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                                   int128_t value) {
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    famAllocator->acquire_CAS_lock(descriptor);
    try {
        fabric_write(key, &value, sizeof(int128_t), offset, (*fiAddr)[nodeId],
                     get_context(descriptor));
    } catch (...) {
        famAllocator->release_CAS_lock(descriptor);
        throw;
    }
    famAllocator->release_CAS_lock(descriptor);
}

int128_t Fam_Ops_Libfabric::atomic_fetch_int128(Fam_Descriptor *descriptor,
                                                uint64_t offset) {
    uint64_t key = descriptor->get_key();
    uint64_t nodeId = descriptor->get_memserver_id();
    offset += (uint64_t)descriptor->get_base_address();

    int128_t local;
    std::vector<fi_addr_t> *fiAddr = get_fiAddrs();
    famAllocator->acquire_CAS_lock(descriptor);
    try {
        fabric_read(key, &local, sizeof(int128_t), offset, (*fiAddr)[nodeId],
                    get_context(descriptor));
    } catch (...) {
        famAllocator->release_CAS_lock(descriptor);
        throw;
    }
    famAllocator->release_CAS_lock(descriptor);
    return local;
}

void Fam_Ops_Libfabric::context_open(uint64_t contextId) {
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
