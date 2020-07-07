/*
 * fam/fam_exception.h
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
#ifndef MEMSERVER_EXCEPTION_H
#define MEMSERVER_EXCEPTION_H

#include <exception>
#include <string>

#include "fam/fam_exception.h"

#define MIN_ERR_VAL -50

using namespace std;
namespace openfam {

enum Internal_Error {
    ALLOC_NO_ERROR = 0,
    REGION_EXIST = MIN_ERR_VAL,
    DATAITEM_EXIST,
    DESTROY_REGION_NOT_PERMITTED,
    DATAITEM_ALLOC_NOT_PERMITTED,
    DATAITEM_DEALLOC_NOT_PERMITTED,
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
    HEAP_NO_LOCALPTR,
    RBT_HEAP_NOT_INSERTED,
    RBT_HEAP_NOT_REMOVED,
    RBT_HEAP_NOT_FOUND,
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
    REGION_NOT_MODIFIED,
    RESIZE_FAILED,
    ITEM_REGISTRATION_FAILED,
    ITEM_DEREGISTRATION_FAILED
};

inline enum Fam_Error convert_to_famerror(enum Internal_Error serverErr) {
    switch (serverErr) {
    case REGION_EXIST:
    case DATAITEM_EXIST:
        return FAM_ERR_ALREADYEXIST;

    case REGION_NOT_FOUND:
    case DATAITEM_NOT_FOUND:
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
        return FAM_ERR_OUTOFRANGE;

    case NULL_POINTER_ACCESS:
        return FAM_ERR_NULLPTR;

    case UNIMPLEMENTED:
        return FAM_ERR_UNIMPL;

    case ALLOC_NO_ERROR:
    case REGION_NOT_INSERTED:
    case DATAITEM_NOT_INSERTED:
    case REGION_NOT_REMOVED:
    case DATAITEM_NOT_REMOVED:
    case REGION_NOT_MODIFIED:
    case HEAP_NOT_FOUND:
    case HEAP_NOT_CREATED:
    case HEAP_NOT_DESTROYED:
    case HEAP_NOT_OPENED:
    case HEAP_NOT_CLOSED:
    case HEAP_ALLOCATE_FAILED:
    case HEAP_MERGE_FAILED:
    case HEAP_NO_LOCALPTR:
    case RESIZE_FAILED:
    case RBT_HEAP_NOT_INSERTED:
    case RBT_HEAP_NOT_REMOVED:
    case RBT_HEAP_NOT_FOUND:
    case NO_FREE_POOLID:
    case NO_LOCAL_POINTER:
    case OPS_INIT_FAILED:
    case FENCE_REG_FAILED:
    case FENCE_DEREG_FAILED:
    case REGION_NAME_TOO_LONG:
    case DATAITEM_NAME_TOO_LONG:
    case ITEM_REGISTRATION_FAILED:
    case ITEM_DEREGISTRATION_FAILED:
    default:
        return FAM_ERR_RESOURCE;
    }
}

class Memserver_Exception : public Fam_Exception {
  public:
    Memserver_Exception();
    Memserver_Exception(enum Internal_Error serverErr, const char *msg);
};

class CIS_Exception : public Fam_Exception {
  public:
    CIS_Exception();
    CIS_Exception(enum Internal_Error serverErr, const char *msg);
    CIS_Exception(enum Fam_Error serverErr, const char *msg);
};
} // namespace openfam
#endif
