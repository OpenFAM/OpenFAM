/*
 * fam/fam_exception.h
 * Copyright (c) 2019 - 2020 Hewlett Packard Enterprise Development, LP. All
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
#ifndef FAM_EXCEPTION_H
#define FAM_EXCEPTION_H

#include <exception>
#include <string>

using namespace std;

namespace openfam {
enum Fam_Error {
    FAM_NO_ERROR = 0,
    FAM_ERR_UNKNOWN = 1,
    FAM_ERR_NOPERM,
    FAM_ERR_TIMEOUT,
    FAM_ERR_INVALID,
    FAM_ERR_LIBFABRIC,
    FAM_ERR_SHM,
    FAM_ERR_NOTFOUND,
    FAM_ERR_ALREADYEXIST,
    FAM_ERR_ALLOCATOR,
    FAM_ERR_RPC,
    FAM_ERR_PMI,
    FAM_ERR_OUTOFRANGE,
    FAM_ERR_NULLPTR,
    FAM_ERR_UNIMPL,
    FAM_ERR_RESOURCE,
    FAM_ERR_INVALIDOP,
    FAM_ERR_RPC_CLIENT_NOTFOUND,
    FAM_ERR_MEMSERV_LIST_EMPTY,
    FAM_ERR_ATOMIC_QUEUE_FULL,
    FAM_ERR_ATOMIC_QUEUE_INSERT
};

class Fam_Exception : public std::exception {
  public:
    Fam_Exception();

    Fam_Exception(const char *msg);

    Fam_Exception(enum Fam_Error fErr, const char *msg);

    Fam_Exception(const Fam_Exception &other);

    virtual char const *fam_error_msg();

    virtual int fam_error();

    virtual char const *what();

  protected:
    std::string famErrMsg;
    enum Fam_Error famErr;
};

} // namespace openfam
#endif
