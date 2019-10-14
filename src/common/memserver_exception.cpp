/*
 * fam_exception.cpp
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

#include "common/memserver_exception.h"

namespace openfam {

Memserver_Exception::Memserver_Exception(enum Memserver_Error serverErr,
                                         const char *msg) {
    famErrMsg = msg;
    famErr = convert_to_famerror(serverErr);
}

enum Fam_Error
Memserver_Exception::convert_to_famerror(enum Memserver_Error serverErr) {
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
    case REGION_NAME_TOO_LONG:
    case DATAITEM_NAME_TOO_LONG:
    default:
        return FAM_ERR_RESOURCE;
    }
}
} // namespace openfam
