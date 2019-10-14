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

using namespace std;
namespace openfam {

enum Memserver_Error {
    ALLOC_NO_ERROR = 0,
    REGION_EXIST = -1,
    DATAITEM_EXIST = -2,
    DESTROY_REGION_NOT_PERMITTED = -3,
    DATAITEM_ALLOC_NOT_PERMITTED = -4,
    DATAITEM_DEALLOC_NOT_PERMITTED = -5,
    REGION_NOT_INSERTED = -6,
    DATAITEM_NOT_INSERTED = -7,
    REGION_NOT_FOUND = -8,
    DATAITEM_NOT_FOUND = -9,
    REGION_NOT_REMOVED = -10,
    DATAITEM_NOT_REMOVED = -11,
    HEAP_NOT_FOUND = -12,
    HEAP_NOT_CREATED = -13,
    HEAP_NOT_DESTROYED = -14,
    HEAP_NOT_OPENED = -15,
    HEAP_NOT_CLOSED = -16,
    HEAP_ALLOCATE_FAILED = -17,
    HEAP_NO_LOCALPTR = -18,
    RBT_HEAP_NOT_INSERTED = -19,
    RBT_HEAP_NOT_REMOVED = -20,
    RBT_HEAP_NOT_FOUND = -21,
    REGION_PERM_MODIFY_NOT_PERMITTED = -22,
    ITEM_PERM_MODIFY_NOT_PERMITTED = -23,
    NO_PERMISSION = -24,
    NO_FREE_POOLID = -25,
    NULL_POINTER_ACCESS = -26,
    OUT_OF_RANGE = -27,
    UNIMPLEMENTED = -28,
    NO_LOCAL_POINTER = -29,
    OPS_INIT_FAILED = -30,
    FENCE_REG_FAILED = -31,
    HEAP_MERGE_FAILED = -32,
    REGION_NAME_TOO_LONG = -33,
    DATAITEM_NAME_TOO_LONG = -34,
    REGION_RESIZE_NOT_PERMITTED = -35,
    REGION_NOT_MODIFIED = -36,
    RESIZE_FAILED = -37
};

class Memserver_Exception : public Fam_Exception {
  public:
    Memserver_Exception();
    Memserver_Exception(enum Memserver_Error serverErr, const char *msg);
    enum Fam_Error convert_to_famerror(enum Memserver_Error serverErr);
};
} // namespace openfam
#endif
