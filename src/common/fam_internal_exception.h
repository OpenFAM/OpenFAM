/*
 * fam/fam_internal_exception.h
 * Copyright (c) 2019-2022 Hewlett Packard Enterprise Development, LP. All
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
#ifndef MEMSERVER_EXCEPTION_H
#define MEMSERVER_EXCEPTION_H

#include <exception>
#include <string>

#include "fam/fam_exception.h"

#define MIN_ERR_VAL -50

using namespace std;
namespace openfam {

/*
 * These derived exception classes are currently being used
 * internally by OpenFAM. All the OpenFAM api return only
 * Fam_Exception objects.
 *
 */

#define THROW_ERR_MSG(exception, message_str)                                  \
    do {                                                                       \
        std::ostringstream errMsgStr;                                          \
        errMsgStr << __func__ << ":" << __LINE__ << ":" << message_str;        \
        throw exception(errMsgStr.str().c_str());                              \
    } while (0)

#define THROW_ERRNO_MSG(exception, error_no, message_str)                      \
    do {                                                                       \
        std::ostringstream errMsgStr;                                          \
        errMsgStr << __func__ << ":" << __LINE__ << ":" << message_str;        \
        throw exception(error_no, errMsgStr.str().c_str());                    \
    } while (0)

enum Internal_Error {
    REGION_EXIST = MIN_ERR_VAL,
    DATAITEM_EXIST,
    DESTROY_REGION_NOT_PERMITTED,
    DATAITEM_ALLOC_NOT_PERMITTED,
    DATAITEM_DEALLOC_NOT_PERMITTED,
    REGION_NOT_CREATED,
    DATAITEM_NOT_CREATED,
    REGION_NOT_INSERTED,
    DATAITEM_NOT_INSERTED,
    REGION_NOT_FOUND,
    DATAITEM_NOT_FOUND,
    REGION_NOT_REMOVED,
    DATAITEM_NOT_REMOVED,
    HEAP_NOT_FOUND,
    HEAP_NOT_CREATED,
    HEAP_NOT_DESTROYED,
    HEAP_NOT_OPENED,
    HEAP_NOT_CLOSED,
    HEAP_ALLOCATE_FAILED,
    HEAPMAP_INSERT_FAILED,
    HEAPMAP_REMOVE_FAILED,
    HEAPMAP_HEAP_NOT_FOUND,
    REGION_PERM_MODIFY_NOT_PERMITTED,
    ITEM_PERM_MODIFY_NOT_PERMITTED,
    NO_PERMISSION,
    NO_FREE_POOLID,
    NULL_POINTER_ACCESS,
    OUT_OF_RANGE,
    UNIMPLEMENTED,
    NO_LOCAL_POINTER,
    OPS_INIT_FAILED,
    FENCE_REG_FAILED,
    FENCE_DEREG_FAILED,
    HEAP_MERGE_FAILED,
    REGION_NAME_TOO_LONG,
    DATAITEM_NAME_TOO_LONG,
    REGION_RESIZE_NOT_PERMITTED,
    RESIZE_FAILED,
    ITEM_REGISTRATION_FAILED,
    ITEM_DEREGISTRATION_FAILED,
    METADATA_ERROR,
    ATL_QUEUE_FULL,
    ATL_QUEUE_INSERT_ERROR,
    ATL_NOT_ENABLED,
    LIBFABRIC_ERROR,
    BACKUP_PATH_NOT_EXIST,
    BACKUP_FILE_EXIST,
    BACKUP_FILE_NOT_FOUND,
    BACKUP_SIZE_TOO_LARGE,
    BACKUP_DATA_INVALID,
    BACKUP_METADATA_INVALID,
    REQUESTED_MEMORY_TYPE_NOT_AVAILABLE,
    MEMORY_SERVER_START_FAILED,
    METADATA_SERVER_START_FAILED,
    ADDRVEC_POPULATION_FAILED,
    MEMSERVER_LIST_CREATE_FAILED,
    DATAITEM_KEY_NOT_AVAILABLE,
    INTERLV_SIZE_NOT_PWR_TWO,
    REGION_NO_SPACE
};

inline enum Fam_Error convert_to_famerror(enum Internal_Error serverErr) {
    switch (serverErr) {
    case REGION_EXIST:
    case DATAITEM_EXIST:
    case BACKUP_FILE_EXIST:
        return FAM_ERR_ALREADYEXIST;

    case REGION_NOT_CREATED:
    case DATAITEM_NOT_CREATED:
    case REQUESTED_MEMORY_TYPE_NOT_AVAILABLE:
        return FAM_ERR_NOT_CREATED;
    case REGION_NOT_FOUND:
    case DATAITEM_NOT_FOUND:
    case BACKUP_FILE_NOT_FOUND:
    case BACKUP_PATH_NOT_EXIST:
        return FAM_ERR_NOTFOUND;

    case DESTROY_REGION_NOT_PERMITTED:
    case DATAITEM_ALLOC_NOT_PERMITTED:
    case DATAITEM_DEALLOC_NOT_PERMITTED:
    case REGION_PERM_MODIFY_NOT_PERMITTED:
    case ITEM_PERM_MODIFY_NOT_PERMITTED:
    case REGION_RESIZE_NOT_PERMITTED:
    case NO_PERMISSION:
        return FAM_ERR_NOPERM;

    case OUT_OF_RANGE:
    case BACKUP_DATA_INVALID:
    case BACKUP_METADATA_INVALID:
        return FAM_ERR_OUTOFRANGE;

    case REGION_NO_SPACE:
        return FAM_ERR_NO_SPACE;

    case NULL_POINTER_ACCESS:
        return FAM_ERR_NULLPTR;

    case UNIMPLEMENTED:
        return FAM_ERR_UNIMPL;

    case ATL_QUEUE_FULL:
        return FAM_ERR_ATL_QUEUE_FULL;

    case ATL_QUEUE_INSERT_ERROR:
        return FAM_ERR_ATL_QUEUE_INSERT;

    case ATL_NOT_ENABLED:
        return FAM_ERR_ATL_NOT_ENABLED;

    case LIBFABRIC_ERROR:
    case ADDRVEC_POPULATION_FAILED:
        return FAM_ERR_LIBFABRIC;

    case INTERLV_SIZE_NOT_PWR_TWO:
        return FAM_ERR_NOT_PWR_TWO;

    case REGION_NOT_INSERTED:
    case DATAITEM_NOT_INSERTED:
    case REGION_NOT_REMOVED:
    case DATAITEM_NOT_REMOVED:
    case METADATA_ERROR:
    case METADATA_SERVER_START_FAILED:
    case MEMSERVER_LIST_CREATE_FAILED:
        return FAM_ERR_METADATA;
    case REGION_NAME_TOO_LONG:
    case DATAITEM_NAME_TOO_LONG:
        return FAM_ERR_NAME_TOO_LONG;
    case HEAP_NOT_FOUND:
    case HEAP_NOT_CREATED:
    case HEAP_NOT_DESTROYED:
    case HEAP_NOT_OPENED:
    case HEAP_NOT_CLOSED:
    case HEAP_ALLOCATE_FAILED:
    case HEAP_MERGE_FAILED:
    case RESIZE_FAILED:
    case HEAPMAP_INSERT_FAILED:
    case HEAPMAP_REMOVE_FAILED:
    case HEAPMAP_HEAP_NOT_FOUND:
    case NO_FREE_POOLID:
    case NO_LOCAL_POINTER:
    case OPS_INIT_FAILED:
    case FENCE_REG_FAILED:
    case FENCE_DEREG_FAILED:
    case ITEM_REGISTRATION_FAILED:
    case ITEM_DEREGISTRATION_FAILED:
    case BACKUP_SIZE_TOO_LARGE:
    case MEMORY_SERVER_START_FAILED:
        return FAM_ERR_MEMORY;
    default:
        return FAM_ERR_RESOURCE;
    }
}

class Memory_Service_Exception : public Fam_Exception {
  public:
    Memory_Service_Exception();
    Memory_Service_Exception(enum Internal_Error serverErr, const char *msg);
    Memory_Service_Exception(enum Fam_Error serverErr, const char *msg);
};

class CIS_Exception : public Fam_Exception {
  public:
    CIS_Exception();
    CIS_Exception(enum Internal_Error serverErr, const char *msg);
    CIS_Exception(enum Fam_Error serverErr, const char *msg);
};

class Metadata_Service_Exception : public Fam_Exception {
  public:
    Metadata_Service_Exception();
    Metadata_Service_Exception(enum Internal_Error serverErr, const char *msg);
    Metadata_Service_Exception(enum Fam_Error serverErr, const char *msg);
};

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
#endif
