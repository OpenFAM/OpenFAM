/*
 * fam_async_qhandler.h
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

#ifndef FAM_ASYNC_QHANDLER_H
#define FAM_ASYNC_QHANDLER_H

#include <nvmm/fam.h>

#include <boost/atomic.hpp>

#include "common/fam_context.h"
#include "common/fam_internal.h"
#include "fam/fam.h"
#include "fam/fam_exception.h"
#include "memory_service/fam_memory_service.h"

using namespace std;

namespace openfam {

typedef enum {
    WRITE = 0,
    READ,
    COPY,
    BACKUP,
    RESTORE,
    DELETE_BACKUP
} Fam_Ops_Type;

typedef struct {
    boost::atomic<bool> copyDone;
    Fam_Memory_Service *memoryService;
    uint64_t srcRegionId;
    uint64_t destRegionId;
    uint64_t srcOffset;
    uint64_t destOffset;
    uint64_t size;
    uint64_t srcKey;
    uint64_t srcCopyStart;
    uint64_t srcBaseAddr;
    char *srcAddr;
    uint32_t srcAddrLen;
    uint64_t srcMemserverId;
    uint64_t destMemserverId;
} Fam_Copy_Tag;

typedef struct {
    boost::atomic<bool> backupDone;
    Fam_Memory_Service *memoryService;
    uint64_t srcRegionId;
    uint64_t srcOffset;
    uint64_t size;
    uint64_t srcMemserverId;
    uint32_t uid;
    uint32_t gid;
    mode_t mode;
    string dataitemName;
    string BackupName;
} Fam_Backup_Tag;

typedef struct {
    boost::atomic<bool> restoreDone;
    Fam_Memory_Service *memoryService;
    uint64_t size;
    string BackupName;
    uint64_t destMemserverId;
    uint64_t destOffset;
    uint64_t destRegionId;
} Fam_Restore_Tag;

typedef struct {
    boost::atomic<bool> delbackupDone;
    Fam_Memory_Service *memoryService;
    uint64_t srcRegionId;
    uint64_t srcOffset;
    uint64_t size;
    string dataitemName;
    string BackupName;
    uint64_t srcMemserverId;
    uid_t uid;
    gid_t gid;
    mode_t mode;
} Fam_Delete_Backup_Tag;

typedef struct {
    Fam_Ops_Type opsType;
    void *src;
    void *dest;
    uint64_t nbytes;
    uint64_t offset;
    uint64_t upperBound;
    uint64_t key;
    uint64_t itemSize;
    void *tag;
} Fam_Ops_Info;

class Fam_Async_Err {
  public:
    Fam_Async_Err() : errorCode(FAM_NO_ERROR) {}
    void set_error_code(enum Fam_Error code) { errorCode = code; }
    void set_error_msg(const char *msg) { errorMsg = msg; }
    Fam_Error get_error_code() { return errorCode; }
    char const *get_error_msg() { return errorMsg.c_str(); }

  private:
    enum Fam_Error errorCode;
    string errorMsg;
};

class Fam_Async_QHandler {
  public:
    Fam_Async_QHandler(uint64_t numConsumer);
    ~Fam_Async_QHandler();

    void nonblocking_ops_handler();

    void initiate_operation(Fam_Ops_Info opsInfo);
    void quiet(Fam_Context *famCtx);
    void write_quiet(uint64_t ctr);
    void read_quiet(uint64_t ctr);
    uint64_t progress(Fam_Context *famCtx);
    uint64_t write_progress(Fam_Context *famCtx);
    uint64_t read_progress(Fam_Context *famCtx);
    void wait_for_copy(void *waitObj);
    void wait_for_backup(void *waitObj);
    void wait_for_restore(void *waitObj);
    void wait_for_delete_backup(void *waitObj);
    void decode_and_execute(Fam_Ops_Info opsInfo);
    void write_handler(void *src, void *dest, uint64_t nbytes, uint64_t offset,
                       uint64_t upperBound, uint64_t key, uint64_t itemSize);
    void read_handler(void *src, void *dest, uint64_t nbytes, uint64_t offset,
                      uint64_t upperBound, uint64_t key, uint64_t itemSize);
    void copy_handler(void *src, void *dest, uint64_t nbytes,
                      Fam_Copy_Tag *tag);
    void backup_handler(void *src, void *dest, uint64_t nbytes,
                        Fam_Backup_Tag *tag);
    void restore_handler(void *src, void *dest, uint64_t nbytes,
                         Fam_Restore_Tag *tag);
    void delete_backup_handler(void *src, void *dest, uint64_t nbytes,
                               Fam_Delete_Backup_Tag *tag);

  private:
    class FamAsyncQHandlerImpl_;
    FamAsyncQHandlerImpl_ *fAsyncQHandler_;
};

} // namespace openfam
#endif
