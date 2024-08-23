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

#include "fam/fam_exception.h"
#include "common/fam_internal.h"
namespace openfam {

Fam_Exception::Fam_Exception() : Fam_Exception("Unknown error") {}

Fam_Exception::Fam_Exception(const char *msg) {
    famErrMsg = msg;
    famErr = FAM_ERR_UNKNOWN;
}

Fam_Exception::Fam_Exception(enum Fam_Error fErr, const char *msg) {
    famErrMsg = msg;
    famErr = fErr;
}

Fam_Exception::Fam_Exception(const Fam_Exception &other) {
    famErrMsg = other.famErrMsg;
    famErr = other.famErr;
}

Fam_Exception &Fam_Exception::operator=(const Fam_Exception &other) {
    famErrMsg = other.famErrMsg;
    famErr = other.famErr;
    return *this;
}

char const *Fam_Exception::fam_error_msg() const noexcept {
    return famErrMsg.c_str();
}

char const *Fam_Exception::what() const noexcept {
    return famErrMsg.c_str();
}

int Fam_Exception::fam_error() const noexcept {
    return famErr;
}

} // namespace openfam
