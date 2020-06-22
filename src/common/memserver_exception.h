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

enum Memserver_Error {
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

class Memserver_Exception : public Fam_Exception {
  public:
    Memserver_Exception();
    Memserver_Exception(enum Memserver_Error serverErr, const char *msg);
    enum Fam_Error convert_to_famerror(enum Memserver_Error serverErr);
};
} // namespace openfam
#endif
