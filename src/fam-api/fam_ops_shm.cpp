/*
 * fam_ops_shm.cpp
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

#include "common/fam_ops_shm.h"
#include "common/fam_config_info.h"
#include "common/fam_context.h"
#include "common/fam_internal.h"
#include "common/fam_memserver_profile.h"
#include "common/fam_ops.h"
#include "common/fam_util_atomic.h"
#include "fam/fam.h"
#include "fam/fam_exception.h"
#include "memory_service/fam_memory_service.h"
#include "memory_service/fam_memory_service_client.h"
#include "memory_service/fam_memory_service_direct.h"
#include "nvmm/nvmm_fam_atomic.h"
#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;
namespace openfam {
Fam_Ops_SHM::Fam_Ops_SHM(Fam_Thread_Model famTM, Fam_Context_Model famCM,
                         Fam_Allocator_Client *famAlloc, uint64_t numConsumer) {
    asyncQHandler = new Fam_Async_QHandler(numConsumer);
    famThreadModel = famTM;
    famContextModel = famCM;
    famAllocator = famAlloc;
    contexts = new std::map<uint64_t, Fam_Context *>();
}

Fam_Ops_SHM::~Fam_Ops_SHM() { finalize(); }

int Fam_Ops_SHM::initialize() {
    // Initialize the mutex lock
    if (famContextModel == FAM_CONTEXT_REGION)
        (void)pthread_mutex_init(&ctxLock, NULL);

    // Initialize defaultCtx
    if (famContextModel == FAM_CONTEXT_DEFAULT) {
        defaultCtx = new Fam_Context(famThreadModel);
        contexts->insert({0, defaultCtx});
    }

    return 0;
}

void Fam_Ops_SHM::finalize() {
    if (contexts != NULL) {
        for (auto fam_ctx : *contexts) {
            delete fam_ctx.second;
        }
        contexts->clear();
    }
}

Fam_Context *Fam_Ops_SHM::get_context(Fam_Descriptor *descriptor) {

    std::ostringstream message;
    // Case - FAM_CONTEXT_DEFAULT
    if (famContextModel == FAM_CONTEXT_DEFAULT) {
        return get_defaultCtx();
    } else if (famContextModel == FAM_CONTEXT_REGION) {
        // Case - FAM_CONTEXT_REGION
        Fam_Context *ctx = (Fam_Context *)descriptor->get_context();
        if (ctx)
            return ctx;

        Fam_Global_Descriptor global = descriptor->get_global_descriptor();
        uint64_t regionId = global.regionId;

        // ctx mutex lock
        (void)pthread_mutex_lock(&ctxLock);

        auto ctxObj = contexts->find(regionId);
        if (ctxObj == contexts->end()) {
            ctx = new Fam_Context(famThreadModel);
            contexts->insert({regionId, ctx});
        } else {
            ctx = ctxObj->second;
        }
        descriptor->set_context(ctx);

        // ctx mutex unlock
        (void)pthread_mutex_unlock(&ctxLock);
        return ctx;
    } else {
        message << "Fam Invalid Option FAM_CONTEXT_MODEL: " << famContextModel;
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
    }
}

int Fam_Ops_SHM::put_blocking(void *local, Fam_Descriptor *descriptor,
                              uint64_t offset, uint64_t nbytes) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + nbytes) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    void *dest = (void *)((uint64_t)base + offset);

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    memcpy(dest, local, nbytes);
    openfam_persist(dest, nbytes);

    // Release Fam_Context read lock
    famCtx->release_lock();

    return FAM_SUCCESS;
}

int Fam_Ops_SHM::get_blocking(void *local, Fam_Descriptor *descriptor,
                              uint64_t offset, uint64_t nbytes) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + nbytes) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_READ_KEY_SHM) != FAM_READ_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to read from dataitem");
    }

    void *src = (void *)((uint64_t)base + offset);

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    openfam_invalidate(src, nbytes);
    memcpy(local, src, nbytes);

    // Release Fam_Context read lock
    famCtx->release_lock();

    return FAM_SUCCESS;
}

int Fam_Ops_SHM::gather_blocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t nElements, uint64_t firstElement,
                                 uint64_t stride, uint64_t elementSize) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    void *src;
    void *dest;

    if (((firstElement * elementSize) > size) ||
        ((firstElement * elementSize) + elementSize * stride * nElements) >
            size) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_READ_KEY_SHM) != FAM_READ_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to read from dataitem");
    }

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    for (uint64_t i = 0; i < nElements; i++) {
        src = (void *)((uint64_t)base + ((firstElement * elementSize) +
                                         elementSize * stride * i));
        dest = (void *)((uint64_t)local + (i * elementSize));
        openfam_invalidate(src, elementSize);
        memcpy(dest, src, elementSize);
    }

    // Release Fam_Context read lock
    famCtx->release_lock();

    return FAM_SUCCESS;
}

int Fam_Ops_SHM::gather_blocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t nElements, uint64_t *elementIndex,
                                 uint64_t elementSize) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    void *src;
    void *dest;

    uint64_t maxOffset = elementIndex[0];
    for (uint64_t i = 0; i < nElements; i++) {
        if (maxOffset < elementIndex[i])
            maxOffset = elementIndex[i];
    }

    if ((maxOffset > size) || ((maxOffset + elementSize) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_READ_KEY_SHM) != FAM_READ_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to read from dataitem");
    }

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    for (uint64_t i = 0; i < nElements; i++) {
        src = (void *)((uint64_t)base + (elementIndex[i] * elementSize));
        dest = (void *)((uint64_t)local + (i * elementSize));
        openfam_invalidate(src, elementSize);
        memcpy(dest, src, elementSize);
    }

    // Release Fam_Context read lock
    famCtx->release_lock();

    return FAM_SUCCESS;
}

int Fam_Ops_SHM::scatter_blocking(void *local, Fam_Descriptor *descriptor,
                                  uint64_t nElements, uint64_t firstElement,
                                  uint64_t stride, uint64_t elementSize) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    void *src;
    void *dest;

    if (((firstElement * elementSize) > size) ||
        ((firstElement * elementSize) + elementSize * stride * nElements) >
            size) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    for (uint64_t i = 0; i < nElements; i++) {
        src = (void *)((uint64_t)local + (i * elementSize));
        dest = (void *)((uint64_t)base + ((firstElement * elementSize) +
                                          elementSize * stride * i));
        memcpy(dest, src, elementSize);
        openfam_persist(dest, elementSize);
    }

    // Release Fam_Context read lock
    famCtx->release_lock();

    return FAM_SUCCESS;
}

int Fam_Ops_SHM::scatter_blocking(void *local, Fam_Descriptor *descriptor,
                                  uint64_t nElements, uint64_t *elementIndex,
                                  uint64_t elementSize) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    void *src;
    void *dest;

    uint64_t maxOffset = elementIndex[0];
    for (uint64_t i = 0; i < nElements; i++) {
        if (maxOffset < elementIndex[i])
            maxOffset = elementIndex[i];
    }

    if ((maxOffset > size) || ((maxOffset + elementSize) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    for (uint64_t i = 0; i < nElements; i++) {
        src = (void *)((uint64_t)local + (i * elementSize));
        dest = (void *)((uint64_t)base + elementIndex[i] * elementSize);
        memcpy(dest, src, elementSize);
        openfam_persist(dest, elementSize);
    }

    // Release Fam_Context read lock
    famCtx->release_lock();

    return FAM_SUCCESS;
}

void Fam_Ops_SHM::put_nonblocking(void *local, Fam_Descriptor *descriptor,
                                  uint64_t offset, uint64_t nbytes) {
    void *base = descriptor->get_base_address();
    uint64_t itemSize = descriptor->get_size();
    uint64_t key = descriptor->get_key();
    uint64_t upperBound = offset + nbytes;

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    void *dest = (void *)((uint64_t)base + offset);
    Fam_Ops_Info opsInfo = {WRITE,      local, dest,     nbytes, offset,
                            upperBound, key,   itemSize, NULL};
    asyncQHandler->initiate_operation(opsInfo);
    famCtx->inc_num_tx_ops();

    // Release Fam_Context read lock
    famCtx->release_lock();

    return;
}

void Fam_Ops_SHM::get_nonblocking(void *local, Fam_Descriptor *descriptor,
                                  uint64_t offset, uint64_t nbytes) {
    void *base = descriptor->get_base_address();
    uint64_t itemSize = descriptor->get_size();
    uint64_t key = descriptor->get_key();
    uint64_t upperBound = offset + nbytes;

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    void *src = (void *)((uint64_t)base + offset);

    Fam_Ops_Info opsInfo = {READ,       src, local,    nbytes, offset,
                            upperBound, key, itemSize, NULL};
    asyncQHandler->initiate_operation(opsInfo);
    famCtx->inc_num_rx_ops();

    // Release Fam_Context read lock
    famCtx->release_lock();

    return;
}

void Fam_Ops_SHM::gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                     uint64_t nElements, uint64_t firstElement,
                                     uint64_t stride, uint64_t elementSize) {
    void *base = descriptor->get_base_address();
    uint64_t itemSize = descriptor->get_size();
    uint64_t key = descriptor->get_key();
    uint64_t offset = firstElement * elementSize;
    uint64_t upperBound =
        (firstElement * elementSize) + elementSize * stride * nElements;
    void *src;
    void *dest;

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    for (uint64_t i = 0; i < nElements; i++) {
        src = (void *)((uint64_t)base + ((firstElement * elementSize) +
                                         elementSize * stride * i));
        dest = (void *)((uint64_t)local + (i * elementSize));
        Fam_Ops_Info opsInfo = {READ,       src, dest,     elementSize, offset,
                                upperBound, key, itemSize, NULL};
        asyncQHandler->initiate_operation(opsInfo);
        famCtx->inc_num_rx_ops();
    }

    // Release Fam_Context read lock
    famCtx->release_lock();

    return;
}

void Fam_Ops_SHM::gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                     uint64_t nElements, uint64_t *elementIndex,
                                     uint64_t elementSize) {
    void *base = descriptor->get_base_address();
    uint64_t itemSize = descriptor->get_size();
    uint64_t key = descriptor->get_key();
    uint64_t upperBound;

    void *src;
    void *dest;

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    for (uint64_t i = 0; i < nElements; i++) {
        src = (void *)((uint64_t)base + (elementIndex[i] * elementSize));
        dest = (void *)((uint64_t)local + (i * elementSize));
        upperBound = elementIndex[i] + elementSize;
        Fam_Ops_Info opsInfo = {
            READ,       src, dest,     elementSize, elementIndex[i],
            upperBound, key, itemSize, NULL};
        asyncQHandler->initiate_operation(opsInfo);
        famCtx->inc_num_rx_ops();
    }

    // Release Fam_Context read lock
    famCtx->release_lock();

    return;
}

void Fam_Ops_SHM::scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                                      uint64_t nElements, uint64_t firstElement,
                                      uint64_t stride, uint64_t elementSize) {
    void *base = descriptor->get_base_address();
    uint64_t itemSize = descriptor->get_size();
    uint64_t key = descriptor->get_key();
    uint64_t offset = firstElement * elementSize;
    uint64_t upperBound =
        (firstElement * elementSize) + (elementSize * stride * nElements);
    void *src;
    void *dest;

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    for (uint64_t i = 0; i < nElements; i++) {
        src = (void *)((uint64_t)local + (i * elementSize));
        dest = (void *)((uint64_t)base + ((firstElement * elementSize) +
                                          elementSize * stride * i));
        Fam_Ops_Info opsInfo = {WRITE,      src, dest,     elementSize, offset,
                                upperBound, key, itemSize, NULL};
        asyncQHandler->initiate_operation(opsInfo);
        famCtx->inc_num_tx_ops();
    }

    // Release Fam_Context read lock
    famCtx->release_lock();

    return;
}
void Fam_Ops_SHM::scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                                      uint64_t nElements,
                                      uint64_t *elementIndex,
                                      uint64_t elementSize) {
    void *base = descriptor->get_base_address();
    uint64_t itemSize = descriptor->get_size();
    uint64_t key = descriptor->get_key();
    uint64_t upperBound;
    void *src;
    void *dest;

    Fam_Context *famCtx = get_context(descriptor);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    for (uint64_t i = 0; i < nElements; i++) {
        src = (void *)((uint64_t)local + (i * elementSize));
        dest = (void *)((uint64_t)base + elementIndex[i] * elementSize);
        upperBound = elementIndex[i] + elementSize;
        Fam_Ops_Info opsInfo = {
            WRITE,      src, dest,     elementSize, elementIndex[i],
            upperBound, key, itemSize, NULL};
        asyncQHandler->initiate_operation(opsInfo);
        famCtx->inc_num_tx_ops();
    }

    // Release Fam_Context read lock
    famCtx->release_lock();

    return;
}

void Fam_Ops_SHM::check_progress(Fam_Region_Descriptor *descriptor) {

       return;
}

void Fam_Ops_SHM::quiet_context(Fam_Context *famCtx) {

    // Take Fam_Context write lock
    famCtx->aquire_WRLock();

    asyncQHandler->quiet(famCtx);

    // Release Fam_Context write lock
    famCtx->release_lock();

    return;
}

void Fam_Ops_SHM::quiet(Fam_Region_Descriptor *descriptor) {

    if (famContextModel == FAM_CONTEXT_DEFAULT) {
        quiet_context(get_defaultCtx());
        return;
    } else if (famContextModel == FAM_CONTEXT_REGION) {
        // ctx mutex lock
        (void)pthread_mutex_lock(&ctxLock);
        try {
            if (descriptor) {
                Fam_Context *ctx = (Fam_Context *)descriptor->get_context();
                if (ctx) {
                    quiet_context(ctx);
                } else {
                    Fam_Global_Descriptor global =
                        descriptor->get_global_descriptor();
                    uint64_t regionId = global.regionId;
                    auto ctxObj = contexts->find(regionId);
                    if (ctxObj != contexts->end()) {
                        descriptor->set_context(ctxObj->second);
                        quiet_context(ctxObj->second);
                    }
                }
            } else {
                for (auto fam_ctx : *contexts)
                    quiet_context(fam_ctx.second);
            }
        } catch (...) {
            // ctx mutex unlock
            (void)pthread_mutex_unlock(&ctxLock);
            throw;
        }
        // ctx mutex unlock
        (void)pthread_mutex_unlock(&ctxLock);
    }
}

uint64_t Fam_Ops_SHM::progress_context(Fam_Context *famCtx) {
    uint64_t pending = 0;
    famCtx->aquire_RDLock();
    pending = asyncQHandler->progress(famCtx);
    famCtx->release_lock();
    return pending;
}

uint64_t Fam_Ops_SHM::progress() { return progress_context(get_defaultCtx()); }

void Fam_Ops_SHM::abort(int status) FAM_OPS_UNIMPLEMENTED(void__);

void *Fam_Ops_SHM::copy(Fam_Descriptor *src, uint64_t srcOffset,
                        Fam_Descriptor *dest, uint64_t destOffset,
                        uint64_t nbytes) {
    void *baseSrc = src->get_base_address();
    void *baseDest = dest->get_base_address();
    Fam_Region_Item_Info srcInfo = famAllocator->check_permission_get_info(src);
    Fam_Region_Item_Info destInfo =
        famAllocator->check_permission_get_info(dest);

    if ((srcOffset > srcInfo.size) || ((srcOffset + nbytes) > srcInfo.size)) {
        THROW_ERRNO_MSG(Fam_Allocator_Exception, FAM_ERR_OUTOFRANGE,
                        "Source offset or size is beyond dataitem boundary");
    }

    if ((destOffset > destInfo.size) ||
        ((destOffset + nbytes) > destInfo.size)) {
        THROW_ERRNO_MSG(
            Fam_Allocator_Exception, FAM_ERR_OUTOFRANGE,
            "Destination offset or size is beyond dataitem boundary");
    }

    Fam_Copy_Tag *tag = new Fam_Copy_Tag();
    tag->copyDone.store(false, boost::memory_order_seq_cst);
    tag->memoryService = NULL;

    Fam_Ops_Info opsInfo = {COPY, baseSrc, baseDest, nbytes, 0, 0, 0, 0, tag};
    asyncQHandler->initiate_operation(opsInfo);

    return (void *)tag;
}

void Fam_Ops_SHM::wait_for_copy(void *waitObj) {
    asyncQHandler->wait_for_copy(waitObj);
}

void *Fam_Ops_SHM::backup(Fam_Descriptor *descriptor, const char *BackupName) {
    return famAllocator->backup(descriptor, BackupName);
}

void *Fam_Ops_SHM::restore(const char *BackupName, Fam_Descriptor *dest) {

    return famAllocator->restore(dest, BackupName);
}

void Fam_Ops_SHM::wait_for_backup(void *waitObj) {
    return famAllocator->wait_for_backup(waitObj);
}
void Fam_Ops_SHM::wait_for_restore(void *waitObj) {
    return famAllocator->wait_for_restore(waitObj);
}

void Fam_Ops_SHM::fence(Fam_Region_Descriptor *descriptor)
    FAM_OPS_UNIMPLEMENTED(void__);

/*
 * Atomic group, libfam_atomic needs the region to be registerd.
 * Registration is done by NVMM heap open. Since heap will be open,
 * registration is taken care.
 *
 * libfam_atomic does not have implementation of uint varities,
 * May need to add support in libfam_atomic
 *
 * User should be pass a valid descriptor which is already mapped,
 * Or else it can cause crashes.
 *
 */
void Fam_Ops_SHM::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                             int32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    fam_atomic_32_write((int32_t *)((char *)base + offset), value);
}

void Fam_Ops_SHM::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                             int64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    fam_atomic_64_write((int64_t *)((char *)base + offset), value);
}

void Fam_Ops_SHM::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                             int128_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int128_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    int128store oldValueStore;
    oldValueStore.i128 = value;
    fam_atomic_128_write((int64_t *)((char *)base + offset), oldValueStore.i64);
}

void Fam_Ops_SHM::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                             uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    fam_atomic_32_write((int32_t *)((char *)base + offset), value);
}

void Fam_Ops_SHM::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                             uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    fam_atomic_64_write((int64_t *)((char *)base + offset), value);
}

void Fam_Ops_SHM::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                             float value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(float)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    int32_t *buff = reinterpret_cast<int32_t *>(&value);
    fam_atomic_32_write((int32_t *)((char *)base + offset), *buff);
}

void Fam_Ops_SHM::atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                             double value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(double)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    int64_t *buff = reinterpret_cast<int64_t *>(&value);
    fam_atomic_64_write((int64_t *)((char *)base + offset), *buff);
}

void Fam_Ops_SHM::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                             int32_t value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    fam_atomic_32_fetch_add((int32_t *)((char *)base + offset), value);
}

void Fam_Ops_SHM::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                             int64_t value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    fam_atomic_64_fetch_add((int64_t *)((char *)base + offset), value);
}

void Fam_Ops_SHM::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                             uint32_t value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    fam_atomic_32_fetch_add((int32_t *)((char *)base + offset), value);
}

void Fam_Ops_SHM::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                             uint64_t value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    fam_atomic_64_fetch_add((int64_t *)((char *)base + offset), value);
}

void Fam_Ops_SHM::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                             float value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(float)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_SUM][FLOAT](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                             double value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(double)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_SUM][DOUBLE](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                  int32_t value) {
    atomic_add(descriptor, offset, -value);
}
void Fam_Ops_SHM::atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                  int64_t value) {
    atomic_add(descriptor, offset, -value);
}
void Fam_Ops_SHM::atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                  uint32_t value) {
    atomic_add(descriptor, offset, -value);
}
void Fam_Ops_SHM::atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                  uint64_t value) {
    atomic_add(descriptor, offset, -value);
}
void Fam_Ops_SHM::atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                  float value) {
    atomic_add(descriptor, offset, -value);
}
void Fam_Ops_SHM::atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                  double value) {
    atomic_add(descriptor, offset, -value);
}

void Fam_Ops_SHM::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                             int32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][INT32](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                             int64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][INT64](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                             uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][UINT32](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                             uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][UINT64](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                             float value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(float)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][FLOAT](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                             double value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(double)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][DOUBLE](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                             int32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][INT32](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                             int64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][INT64](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                             uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][UINT32](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                             uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][UINT64](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                             float value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(float)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][FLOAT](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                             double value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(double)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][DOUBLE](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_and(Fam_Descriptor *descriptor, uint64_t offset,
                             uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_BAND][UINT32](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_and(Fam_Descriptor *descriptor, uint64_t offset,
                             uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_BAND][UINT64](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_or(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_BOR][UINT32](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_or(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_BOR][UINT64](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_xor(Fam_Descriptor *descriptor, uint64_t offset,
                             uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_BXOR][UINT32](
        (void *)((char *)base + offset), (void *)&value, result);
}

void Fam_Ops_SHM::atomic_xor(Fam_Descriptor *descriptor, uint64_t offset,
                             uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_BXOR][UINT64](
        (void *)((char *)base + offset), (void *)&value, result);
}

int32_t Fam_Ops_SHM::compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                                  int32_t oldValue, int32_t newValue) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    return fam_atomic_32_compare_store((int32_t *)((char *)base + offset),
                                       oldValue, newValue);
}

int64_t Fam_Ops_SHM::compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                                  int64_t oldValue, int64_t newValue) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }

    return fam_atomic_64_compare_store((int64_t *)((char *)base + offset),
                                       oldValue, newValue);
}

int128_t Fam_Ops_SHM::compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                                   int128_t oldValue, int128_t newValue) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int128_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    int128store oldValueStore;
    int128store newValueStore;
    int128store ResultValueStore;
    oldValueStore.i128 = oldValue;
    newValueStore.i128 = newValue;
    fam_atomic_128_compare_store((int64_t *)((char *)base + offset),
                                 oldValueStore.i64, newValueStore.i64,
                                 ResultValueStore.i64);
    return ResultValueStore.i128;
}

uint32_t Fam_Ops_SHM::compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t oldValue, uint32_t newValue) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }

    return fam_atomic_32_compare_store((int32_t *)((char *)base + offset),
                                       oldValue, newValue);
}

uint64_t Fam_Ops_SHM::compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t oldValue, uint64_t newValue) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }

    return fam_atomic_64_compare_store((int64_t *)((char *)base + offset),
                                       oldValue, newValue);
}

int32_t Fam_Ops_SHM::swap(Fam_Descriptor *descriptor, uint64_t offset,
                          int32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    return fam_atomic_32_swap((int32_t *)((char *)base + offset), value);
}

int64_t Fam_Ops_SHM::swap(Fam_Descriptor *descriptor, uint64_t offset,
                          int64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    return fam_atomic_64_swap((int64_t *)((char *)base + offset), value);
}

uint32_t Fam_Ops_SHM::swap(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    return fam_atomic_32_swap((int32_t *)((char *)base + offset), value);
}

uint64_t Fam_Ops_SHM::swap(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    return fam_atomic_64_swap((int64_t *)((char *)base + offset), value);
}

float Fam_Ops_SHM::swap(Fam_Descriptor *descriptor, uint64_t offset,
                        float value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(float)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    int32_t *buff = reinterpret_cast<int32_t *>(&value);
    int32_t result =
        fam_atomic_32_swap((int32_t *)((char *)base + offset), *buff);
    float *floatResult = reinterpret_cast<float *>(&result);
    return *floatResult;
}

double Fam_Ops_SHM::swap(Fam_Descriptor *descriptor, uint64_t offset,
                         double value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(double)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    int64_t *buff = reinterpret_cast<int64_t *>(&value);
    int64_t result =
        fam_atomic_64_swap((int64_t *)((char *)base + offset), *buff);
    double *doubleResult = reinterpret_cast<double *>(&result);
    return *doubleResult;
}

int32_t Fam_Ops_SHM::atomic_fetch_int32(Fam_Descriptor *descriptor,
                                        uint64_t offset) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_READ_KEY_SHM) != FAM_READ_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    return fam_atomic_32_read((int32_t *)((char *)base + offset));
}

int64_t Fam_Ops_SHM::atomic_fetch_int64(Fam_Descriptor *descriptor,
                                        uint64_t offset) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_READ_KEY_SHM) != FAM_READ_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    return fam_atomic_64_read((int64_t *)((char *)base + offset));
}

int128_t Fam_Ops_SHM::atomic_fetch_int128(Fam_Descriptor *descriptor,
                                          uint64_t offset) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int128_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_READ_KEY_SHM) != FAM_READ_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    int128store ResultValueStore;

    fam_atomic_128_read((int64_t *)((char *)base + offset),
                        ResultValueStore.i64);
    return ResultValueStore.i128;
}

uint32_t Fam_Ops_SHM::atomic_fetch_uint32(Fam_Descriptor *descriptor,
                                          uint64_t offset) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_READ_KEY_SHM) != FAM_READ_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    return fam_atomic_32_read((int32_t *)((char *)base + offset));
}

uint64_t Fam_Ops_SHM::atomic_fetch_uint64(Fam_Descriptor *descriptor,
                                          uint64_t offset) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_READ_KEY_SHM) != FAM_READ_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }

    return fam_atomic_64_read((int64_t *)((char *)base + offset));
}

float Fam_Ops_SHM::atomic_fetch_float(Fam_Descriptor *descriptor,
                                      uint64_t offset) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(float)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_READ_KEY_SHM) != FAM_READ_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    int32_t buff = fam_atomic_32_read((int32_t *)((char *)base + offset));
    float *result = reinterpret_cast<float *>(&buff);
    return *result;
}

double Fam_Ops_SHM::atomic_fetch_double(Fam_Descriptor *descriptor,
                                        uint64_t offset) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(double)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_READ_KEY_SHM) != FAM_READ_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to write into dataitem");
    }
    int64_t buff = fam_atomic_64_read((int64_t *)((char *)base + offset));
    double *result = reinterpret_cast<double *>(&buff);
    return *result;
}

int32_t Fam_Ops_SHM::atomic_fetch_add(Fam_Descriptor *descriptor,
                                      uint64_t offset, int32_t value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    return fam_atomic_32_fetch_add((int32_t *)((char *)base + offset), value);
}

int64_t Fam_Ops_SHM::atomic_fetch_add(Fam_Descriptor *descriptor,
                                      uint64_t offset, int64_t value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    return fam_atomic_64_fetch_add((int64_t *)((char *)base + offset), value);
}

uint32_t Fam_Ops_SHM::atomic_fetch_add(Fam_Descriptor *descriptor,
                                       uint64_t offset, uint32_t value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    return fam_atomic_32_fetch_add((int32_t *)((char *)base + offset), value);
}

uint64_t Fam_Ops_SHM::atomic_fetch_add(Fam_Descriptor *descriptor,
                                       uint64_t offset, uint64_t value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    return fam_atomic_64_fetch_add((int64_t *)((char *)base + offset), value);
}

float Fam_Ops_SHM::atomic_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                                    float value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(float)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_SUM][FLOAT](
        (void *)((char *)base + offset), (void *)&value, result);
    float *oldValue = (float *)result;
    return *oldValue;
}

double Fam_Ops_SHM::atomic_fetch_add(Fam_Descriptor *descriptor,
                                     uint64_t offset, double value) {

    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(double)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_SUM][DOUBLE](
        (void *)((char *)base + offset), (void *)&value, result);
    double *oldValue = (double *)result;
    return *oldValue;
}

int32_t Fam_Ops_SHM::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                           uint64_t offset, int32_t value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

int64_t Fam_Ops_SHM::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                           uint64_t offset, int64_t value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

uint32_t Fam_Ops_SHM::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                            uint64_t offset, uint32_t value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

uint64_t Fam_Ops_SHM::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                            uint64_t offset, uint64_t value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

float Fam_Ops_SHM::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                         uint64_t offset, float value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

double Fam_Ops_SHM::atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                          uint64_t offset, double value) {
    return atomic_fetch_add(descriptor, offset, -value);
}

int32_t Fam_Ops_SHM::atomic_fetch_min(Fam_Descriptor *descriptor,
                                      uint64_t offset, int32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][INT32](
        (void *)((char *)base + offset), (void *)&value, result);
    int32_t *oldValue = (int32_t *)result;
    return *oldValue;
}

int64_t Fam_Ops_SHM::atomic_fetch_min(Fam_Descriptor *descriptor,
                                      uint64_t offset, int64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][INT64](
        (void *)((char *)base + offset), (void *)&value, result);
    int64_t *oldValue = (int64_t *)result;
    return *oldValue;
}

uint32_t Fam_Ops_SHM::atomic_fetch_min(Fam_Descriptor *descriptor,
                                       uint64_t offset, uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][UINT32](
        (void *)((char *)base + offset), (void *)&value, result);
    uint32_t *oldValue = (uint32_t *)result;
    return *oldValue;
}

uint64_t Fam_Ops_SHM::atomic_fetch_min(Fam_Descriptor *descriptor,
                                       uint64_t offset, uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][UINT64](
        (void *)((char *)base + offset), (void *)&value, result);
    uint64_t *oldValue = (uint64_t *)result;
    return *oldValue;
}

float Fam_Ops_SHM::atomic_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                                    float value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(float)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][FLOAT](
        (void *)((char *)base + offset), (void *)&value, result);
    float *oldValue = (float *)result;
    return *oldValue;
}

double Fam_Ops_SHM::atomic_fetch_min(Fam_Descriptor *descriptor,
                                     uint64_t offset, double value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(double)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MIN][DOUBLE](
        (void *)((char *)base + offset), (void *)&value, result);
    double *oldValue = (double *)result;
    return *oldValue;
}

int32_t Fam_Ops_SHM::atomic_fetch_max(Fam_Descriptor *descriptor,
                                      uint64_t offset, int32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][INT32](
        (void *)((char *)base + offset), (void *)&value, result);
    int32_t *oldValue = (int32_t *)result;
    return *oldValue;
}

int64_t Fam_Ops_SHM::atomic_fetch_max(Fam_Descriptor *descriptor,
                                      uint64_t offset, int64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(int64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][INT64](
        (void *)((char *)base + offset), (void *)&value, result);
    int64_t *oldValue = (int64_t *)result;
    return *oldValue;
}

uint32_t Fam_Ops_SHM::atomic_fetch_max(Fam_Descriptor *descriptor,
                                       uint64_t offset, uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][UINT32](
        (void *)((char *)base + offset), (void *)&value, result);
    uint32_t *oldValue = (uint32_t *)result;
    return *oldValue;
}

uint64_t Fam_Ops_SHM::atomic_fetch_max(Fam_Descriptor *descriptor,
                                       uint64_t offset, uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][UINT64](
        (void *)((char *)base + offset), (void *)&value, result);
    uint64_t *oldValue = (uint64_t *)result;
    return *oldValue;
}

float Fam_Ops_SHM::atomic_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                                    float value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(float)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][FLOAT](
        (void *)((char *)base + offset), (void *)&value, result);
    float *oldValue = (float *)result;
    return *oldValue;
}

double Fam_Ops_SHM::atomic_fetch_max(Fam_Descriptor *descriptor,
                                     uint64_t offset, double value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(double)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_MAX][DOUBLE](
        (void *)((char *)base + offset), (void *)&value, result);
    double *oldValue = (double *)result;
    return *oldValue;
}

uint32_t Fam_Ops_SHM::atomic_fetch_and(Fam_Descriptor *descriptor,
                                       uint64_t offset, uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }

    void *result;
    fam_atomic_readwrite_handlers[FAM_BAND][UINT32](
        (void *)((char *)base + offset), (void *)&value, result);
    uint32_t *oldValue = (uint32_t *)result;
    return *oldValue;
}

uint64_t Fam_Ops_SHM::atomic_fetch_and(Fam_Descriptor *descriptor,
                                       uint64_t offset, uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_BAND][UINT64](
        (void *)((char *)base + offset), (void *)&value, result);
    uint64_t *oldValue = (uint64_t *)result;
    return *oldValue;
}

uint32_t Fam_Ops_SHM::atomic_fetch_or(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_BOR][UINT32](
        (void *)((char *)base + offset), (void *)&value, result);
    uint32_t *oldValue = (uint32_t *)result;
    return *oldValue;
}

uint64_t Fam_Ops_SHM::atomic_fetch_or(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_BOR][UINT64](
        (void *)((char *)base + offset), (void *)&value, result);
    uint64_t *oldValue = (uint64_t *)result;
    return *oldValue;
}

uint32_t Fam_Ops_SHM::atomic_fetch_xor(Fam_Descriptor *descriptor,
                                       uint64_t offset, uint32_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint32_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_BXOR][UINT32](
        (void *)((char *)base + offset), (void *)&value, result);
    uint32_t *oldValue = (uint32_t *)result;
    return *oldValue;
}

uint64_t Fam_Ops_SHM::atomic_fetch_xor(Fam_Descriptor *descriptor,
                                       uint64_t offset, uint64_t value) {
    void *base = descriptor->get_base_address();
    uint64_t size = descriptor->get_size();
    uint64_t key = descriptor->get_key();

    if ((offset > size) || ((offset + sizeof(uint64_t)) > size)) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_OUTOFRANGE,
                        "offset or data size is out of bound");
    }

    if ((key & FAM_RW_KEY_SHM) != FAM_RW_KEY_SHM) {
        THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_NOPERM,
                        "not permitted to either read or write, "
                        "need both read and write permission");
    }
    void *result;
    fam_atomic_readwrite_handlers[FAM_BXOR][UINT64](
        (void *)((char *)base + offset), (void *)&value, result);
    uint64_t *oldValue = (uint64_t *)result;
    return *oldValue;
}

void Fam_Ops_SHM::context_open(uint64_t contextId) {
    return;
}

void Fam_Ops_SHM::context_close(uint64_t contextId) {
    return;
}

uint64_t Fam_Ops_SHM::get_context_id() {
    return (uint64_t)0;
}

} // end namespace openfam
