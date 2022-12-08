/*
 * Copyright (c) 2022 Hewlett Packard Enterprise Development, LP. All
 * rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or
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

#include <iostream>
#include <thread>
#include <atomic>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "fam/fam.h"
#include "fam/fam_exception.h"
#include <fam/fam_c.h>
#define INIT_SLEEP_TIME 100

using namespace std;
using namespace openfam;

using int128_t = openfam::int128_t;

typedef openfam::Fam_Region_Descriptor Region;
typedef openfam::Fam_Descriptor Fd;
typedef openfam::Fam_Exception Fam_Exception;
typedef openfam::Fam_Stat Fam_Stat;
typedef openfam::Fam_Options Fam_Options;
typedef openfam::fam_context Fcont;
typedef openfam::Fam_Backup_Options Fam_Backup_Options;

enum States {
    FAILURE = -1,
    SUCCESS,
    UNINITIALIZED,
    INITIALIZING
};

static std::atomic<States> fam_initialized{UNINITIALIZED};
static thread_local int error_no = 0;
static thread_local string error_msg = "";

#define CAPTURE_EXCEPTION(E) do {         \
    error_no = E.fam_error();             \
    error_msg = E.fam_error_msg();        \
} while(0);

int c_fam_error() {
    return error_no;
}

const char* c_fam_error_msg() {
    return error_msg.c_str();
}

c_fam* c_fam_create() {
    fam* fam_inst = new fam();
    return (c_fam*)fam_inst;
}

int c_fam_initialize(c_fam* fam_obj, const char* groupname, c_fam_options* fam_opts) {
    States old_val = UNINITIALIZED;
    States d_val = INITIALIZING;

    if (!std::atomic_compare_exchange_weak(&fam_initialized, &old_val, d_val)) {
        while(fam_initialized.load() == INITIALIZING) {
            //wait for fam initialization to complete
            usleep(INIT_SLEEP_TIME);
        }
        return fam_initialized.load();
    } else {
        //Create the OpenFAM object & initialize OpenFAM
        fam* fam_inst = (fam*) fam_obj;

        try {
            fam_inst->fam_initialize(groupname, (Fam_Options*)fam_opts);
        } catch (Fam_Exception& e) {
            CAPTURE_EXCEPTION(e);
            fam_initialized.store(FAILURE, std::memory_order_relaxed);
            return (int)FAILURE;
        }
        fam_initialized.store(SUCCESS, std::memory_order_relaxed);
        return (int)SUCCESS;
    }
}

c_fam_region_desc* c_fam_create_region(c_fam* fam_obj, const char* fam_region,
                                       size_t size, mode_t perm, c_fam_Region_Attributes* reg_att) {
    Region* region_desc = nullptr;
    fam* fam_inst = (fam*) fam_obj;
    try {
        region_desc = fam_inst->fam_create_region(fam_region, size, perm,
                                                  (Fam_Region_Attributes*)reg_att);
    } catch (openfam::Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return (c_fam_region_desc*)region_desc;
    }
    return (c_fam_region_desc*)region_desc;
}

c_fam_region_desc* c_fam_lookup_region(c_fam* fam_obj, const char* name) {
    Region* region_desc = NULL;
    fam* fam_inst = (fam*) fam_obj;
    try {
        region_desc = fam_inst->fam_lookup_region(name);  
    } catch (openfam::Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return (c_fam_region_desc*)region_desc;
    }
    return (c_fam_region_desc*)region_desc;
}

int c_fam_destroy_region(c_fam* fam_obj, c_fam_region_desc* region) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_destroy_region((Region*)region);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_finalize(c_fam* fam_obj, const char* groupname) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_finalize(groupname);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

void c_fam_delete(c_fam* fam_obj) {
    fam* fam_inst = (fam*) fam_obj;
    delete fam_inst;
}

c_fam_desc* c_fam_allocate(c_fam* fam_obj, size_t size, mode_t mode, void* region) {
    Fd* fd = nullptr;
    fam* fam_inst = (fam*) fam_obj;

    try {
        fd = fam_inst->fam_allocate(size, mode, (Region*)region);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return (c_fam_desc*)fd;
    }
    return (c_fam_desc*)fd;
}

c_fam_desc* c_fam_allocate_named(c_fam* fam_obj, const char* name, size_t size,
                                                     mode_t mode, void* region) {
    Fd* fd = nullptr;
    fam* fam_inst = (fam*) fam_obj;

    try {
        fd = fam_inst->fam_allocate(name, size, mode, (Region*)region);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return (c_fam_desc*)fd;
    }
    return (c_fam_desc*)fd;
}

void* c_fam_copy(c_fam* fam_obj, c_fam_desc* srcDesc, uint64_t srcOffset,
               c_fam_desc* destDesc, uint64_t destOffset, size_t size) {
    void* waitObj = nullptr;
    fam* fam_inst = (fam*) fam_obj;
    try {
        waitObj = fam_inst->fam_copy((Fd*)srcDesc, srcOffset,
                                   (Fd*)destDesc, destOffset, size);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return waitObj;
    }
    return waitObj;
}

int c_fam_copy_wait(c_fam* fam_obj, void* waitObj) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_copy_wait(waitObj);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

void* c_fam_backup(c_fam* fam_obj, c_fam_desc* srcDesc, const char* backupName, 
            c_fam_backup_options* backupOptions) {
    void* waitObj = nullptr;
    fam* fam_inst = (fam*) fam_obj;
    try {
        waitObj = fam_inst->fam_backup((Fd*)srcDesc, backupName, (Fam_Backup_Options*)backupOptions);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return waitObj;
    }
    return waitObj;
}

int c_fam_backup_wait(c_fam* fam_obj, void* waitObj) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_backup_wait(waitObj);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

void* c_fam_restore(c_fam* fam_obj, const char* backupName, c_fam_desc* destDesc) {
    void* waitObj = nullptr;
    fam* fam_inst = (fam*) fam_obj;
    try {
        waitObj = fam_inst->fam_restore(backupName, (Fd*)destDesc);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return waitObj;
    }
    return waitObj;
}

void* c_fam_restore_region(c_fam* fam_obj, const char* backupName, c_fam_region_desc* destRegDesc, 
        const char* dataitemName, mode_t perm, c_fam_desc** destDesc) {
    void* waitObj = nullptr;
    fam* fam_inst =(fam*) fam_obj;
    try {
        waitObj = fam_inst->fam_restore(backupName, (Region*)destRegDesc, dataitemName, perm, (Fd**)destDesc);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return waitObj;
    }
    return waitObj;
}

int c_fam_restore_wait(c_fam* fam_obj, void* waitObj) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_restore_wait(waitObj);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

void* c_fam_delete_backup(c_fam* fam_obj, const char* backupName) {
    void* waitObj = nullptr;
    fam* fam_inst = (fam*) fam_obj;
    try {
        waitObj = fam_inst->fam_delete_backup(backupName);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return waitObj;
    }
    return waitObj; 
}

int c_fam_delete_backup_wait(c_fam* fam_obj, void* waitObj) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_delete_backup_wait(waitObj);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_deallocate(c_fam* fam_obj, c_fam_desc* desc) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_deallocate((Fd*)desc);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

c_fam_desc* c_fam_lookup(c_fam* fam_obj, const char* name, const char* regionname) {
    Fd* fd = nullptr;
    fam* fam_inst = (fam*) fam_obj;
    try {
        fd = fam_inst->fam_lookup(name, regionname);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return (c_fam_desc*)fd;
    }
    return (c_fam_desc*)fd;  
}

int c_fam_put_nonblocking(c_fam* fam_obj, void* addr, c_fam_desc* fd,
                                        uint64_t offset, size_t size) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_put_nonblocking(addr, (Fd*)fd, offset, size);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_get_nonblocking(c_fam* fam_obj, void* addr, c_fam_desc* fd,
                                        uint64_t offset, size_t size) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_get_nonblocking(addr, (Fd*)fd, offset, size);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_put_blocking(c_fam* fam_obj, void* addr, c_fam_desc* fd,
                                     uint64_t offset, size_t size) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_put_blocking(addr, (Fd*)fd, offset, size);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_get_blocking(c_fam* fam_obj, void* addr, c_fam_desc* fd,
                                     uint64_t offset, size_t size) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_get_blocking(addr, (Fd*)fd, offset, size);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_gather_blocking_stride(c_fam* fam_obj, void* addr, c_fam_desc* fd,
        uint64_t nElements, uint64_t firstElement, uint64_t stride, uint64_t elementSize) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_gather_blocking(addr, (Fd*)fd, nElements, firstElement,
                stride, elementSize);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_gather_nonblocking_stride(c_fam* fam_obj, void* addr, c_fam_desc* fd,
        uint64_t nElements, uint64_t firstElement, uint64_t stride, uint64_t elementSize) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_gather_nonblocking(addr, (Fd*)fd, nElements, firstElement,
                stride, elementSize);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_gather_blocking_index(c_fam* fam_obj, void* addr, c_fam_desc* fd,
        uint64_t nElements, uint64_t* elementIndex, uint64_t elementSize) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_gather_blocking(addr, (Fd*)fd, nElements, elementIndex,
                elementSize);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_gather_nonblocking_index(c_fam* fam_obj, void* addr, c_fam_desc* fd,
        uint64_t nElements, uint64_t* elementIndex, uint64_t elementSize) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_gather_nonblocking(addr, (Fd*)fd, nElements, elementIndex,
                elementSize);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_scatter_blocking_stride(c_fam* fam_obj, void* addr, c_fam_desc* fd,
        uint64_t nElements, uint64_t firstElement, uint64_t stride, uint64_t elementSize) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_scatter_blocking(addr, (Fd*)fd, nElements, firstElement,
                stride, elementSize);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_scatter_nonblocking_stride(c_fam* fam_obj, void* addr, c_fam_desc* fd,
        uint64_t nElements, uint64_t firstElement, uint64_t stride, uint64_t elementSize) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_scatter_nonblocking(addr, (Fd*)fd, nElements, firstElement,
                stride, elementSize);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_scatter_blocking_index(c_fam* fam_obj, void* addr, c_fam_desc* fd,
        uint64_t nElements, uint64_t* elementIndex, uint64_t elementSize) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_scatter_blocking(addr, (Fd*)fd, nElements, elementIndex,
                elementSize);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_scatter_nonblocking_index(c_fam* fam_obj, void* addr, c_fam_desc* fd,
        uint64_t nElements, uint64_t* elementIndex, uint64_t elementSize) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_scatter_nonblocking(addr, (Fd*)fd, nElements, elementIndex,
                elementSize);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_quiet(c_fam* fam_obj) {
    fam* fam_inst = (fam*) fam_obj;
    try{
        fam_inst->fam_quiet();
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fence(c_fam* fam_obj) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_fence();
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_abort(c_fam* fam_obj, int status) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_abort(status);
    } catch(Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

const char** c_fam_list_options(c_fam* fam_obj) {
    const char** options = nullptr;
    fam* fam_inst = (fam*)fam_obj;
    try {
        options = fam_inst->fam_list_options();
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return options;
    }
    return options;
}

const void* c_fam_get_option(c_fam* fam_obj, const char* optionName) {
    const void *option = nullptr;
    fam* fam_inst = (fam*)fam_obj;
    try {
        option = fam_inst->fam_get_option((char*)optionName);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return option;
    }
    return option;
}

int c_fam_stat_data(c_fam* fam_obj, c_fam_desc* fd, c_fam_stat* stat) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_stat((Fd*)fd, (Fam_Stat*) stat);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_stat_region(c_fam* fam_obj, c_fam_region_desc* region_desc, c_fam_stat* stat) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_stat((Region*)region_desc, (Fam_Stat*)stat);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_resize_region(c_fam* fam_obj, c_fam_region_desc* region_desc, uint64_t size) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_resize_region((Region*)region_desc, size);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_change_permissions_data(c_fam* fam_obj, c_fam_desc* fd, mode_t perm) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_change_permissions((Fd*)fd, perm);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_change_permissions_region(c_fam* fam_obj, c_fam_region_desc* region_desc, mode_t perm) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_change_permissions((Region*)region_desc, perm);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

void* c_fam_map(c_fam* fam_obj, c_fam_desc* fd) {
    void* localptr = nullptr;
    fam* fam_inst = (fam*)fam_obj;
    try {
        localptr = fam_inst->fam_map((Fd*)fd);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return localptr;
    }
    return localptr;
}

int c_fam_unmap(c_fam* fam_obj, void* localptr, c_fam_desc* fd) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_unmap(localptr, (Fd*)fd);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

c_fam_context* c_fam_context_open(c_fam* fam_obj) {
    Fcont* fcont = nullptr;
    fam* fam_inst = (fam*) fam_obj;

    try {
        fcont = fam_inst->fam_context_open();
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return (c_fam_context*)fcont;
    }
    return (c_fam_context*)fcont;
}

int c_fam_context_close(c_fam* fam_obj, c_fam_context* fcont) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_context_close((Fcont*) fcont);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_progress(c_fam* fam_obj, uint64_t* value) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        *value = fam_inst->fam_progress();
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

//Atomic API support
int c_fam_set_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_set((Fd*)fd, offset, value);
    } catch(Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_set_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_set((Fd*)fd, offset, value);
    } catch(Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_set_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_set((Fd*)fd, offset, value);
    } catch(Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_set_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_set((Fd*)fd, offset, value);
    } catch(Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_set_float(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, float value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_set((Fd*)fd, offset, value);
    } catch(Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_set_double(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, double value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_set((Fd*)fd, offset, value);
    } catch(Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_set_int128(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int128_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_set((Fd*)fd, offset, value);
    } catch(Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t *value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *value = fam_inst->fam_fetch_int32((Fd*)fd, offset);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t *value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *value = fam_inst->fam_fetch_int64((Fd*)fd, offset);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t *value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *value = fam_inst->fam_fetch_uint32((Fd*)fd, offset);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t *value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *value = fam_inst->fam_fetch_uint64((Fd*)fd, offset);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_float(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, float *value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *value = fam_inst->fam_fetch_float((Fd*)fd, offset);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_double(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, double *value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *value = fam_inst->fam_fetch_double((Fd*)fd, offset);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_int128(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int128_t *value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *value = fam_inst->fam_fetch_int128((Fd*)fd, offset);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_add_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_add_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_add_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_add_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_add_float(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, float value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_add_double(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, double value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_subtract_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_subtract_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_subtract_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_subtract_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_subtract_float(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, float value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_subtract_double(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, double value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_min_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_min_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_min_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_min_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_min_float(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, float value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_min_double(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, double value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_max_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_max_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_max_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_max_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_max_float(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, float value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_max_double(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, double value) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        fam_inst->fam_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_and_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_and((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_and_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_and((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_or_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_or((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_or_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_or((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_xor_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_xor((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_xor_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value) {
    fam* fam_inst = (fam*) fam_obj;
    try {
        fam_inst->fam_xor((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_swap_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t value, int32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_swap((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_swap_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t value, int64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_swap((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_swap_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value, uint32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_swap((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_swap_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value, uint64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_swap((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_swap_float(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, float value, float* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_swap((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_swap_double(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, double value, double* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_swap((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_compare_swap_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t oldval,
        int32_t newval, int32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_compare_swap((Fd*)fd, offset, oldval, newval);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_compare_swap_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t oldval,
        int64_t newval, int64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_compare_swap((Fd*)fd, offset, oldval, newval);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_compare_swap_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t oldval,
        uint32_t newval, uint32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_compare_swap((Fd*)fd, offset, oldval, newval);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_compare_swap_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t oldval,
        uint64_t newval, uint64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_compare_swap((Fd*)fd, offset, oldval, newval);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_compare_swap_int128(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int128_t oldval,
        int128_t newval, int128_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_compare_swap((Fd*)fd, offset, (int128_t)oldval, (int128_t)newval);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_add_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t value, 
        int32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_add_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t value, 
        int64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_add_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value, 
        uint32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_add_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value, 
        uint64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_add_float(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, float value, 
        float* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_add_double(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, double value, 
        double* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_add((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_subtract_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t value, 
        int32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_subtract_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t value, 
        int64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_subtract_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value, 
        uint32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_subtract_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value, 
        uint64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_subtract_float(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, float value, 
        float* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_subtract_double(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, double value, 
        double* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_subtract((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_min_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t value, 
        int32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_min_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t value, 
        int64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_min_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value, 
        uint32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_min_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value, 
        uint64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_min_float(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, float value, 
        float* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_min_double(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, double value, 
        double* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_min((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_max_int32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int32_t value, 
        int32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_max_int64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, int64_t value, 
        int64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_max_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value, 
        uint32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_max_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value, 
        uint64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_max_float(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, float value, 
        float* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_max_double(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, double value, 
        double* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_max((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_and_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value,
        uint32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_and((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_and_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value,
        uint64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_and((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_or_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value,
        uint32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_or((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_or_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value,
        uint64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_or((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_xor_uint32(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint32_t value,
        uint32_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_xor((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

int c_fam_fetch_xor_uint64(c_fam* fam_obj, c_fam_desc* fd, uint64_t offset, uint64_t value,
        uint64_t* retval) {
    fam* fam_inst = (fam*)fam_obj;
    try {
        *retval = fam_inst->fam_fetch_xor((Fd*)fd, offset, value);
    } catch (Fam_Exception &e) {
        CAPTURE_EXCEPTION(e);
        return -1;
    }
    return 0;
}

