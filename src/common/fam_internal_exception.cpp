/*
 * fam_internal_exception.cpp
 * Copyright (c) 2019-2020 Hewlett Packard Enterprise Development, LP. All
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

#include "common/fam_internal_exception.h"

namespace openfam {

Memory_Service_Exception::Memory_Service_Exception(
    enum Internal_Error serverErr, const char *msg) {
    famErrMsg = msg;
    famErr = convert_to_famerror(serverErr);
}

Memory_Service_Exception::Memory_Service_Exception(enum Fam_Error serverErr,
                                                   const char *msg) {
    famErrMsg = msg;
    famErr = serverErr;
}

CIS_Exception::CIS_Exception(enum Internal_Error serverErr, const char *msg) {
    famErrMsg = msg;
    famErr = convert_to_famerror(serverErr);
}

CIS_Exception::CIS_Exception(enum Fam_Error serverErr, const char *msg) {
    famErrMsg = msg;
    famErr = serverErr;
}

Metadata_Service_Exception::Metadata_Service_Exception(
    enum Internal_Error serverErr, const char *msg) {
    famErrMsg = msg;
    famErr = convert_to_famerror(serverErr);
}

Metadata_Service_Exception::Metadata_Service_Exception(enum Fam_Error serverErr,
                                                       const char *msg) {
    famErrMsg = msg;
    famErr = serverErr;
}

Fam_Permission_Exception::Fam_Permission_Exception(const char *msg) {
    famErrMsg = msg;
    famErr = FAM_ERR_NOPERM;
}

Fam_Permission_Exception::Fam_Permission_Exception()
    : Fam_Permission_Exception("No Permission") {}

Fam_InvalidOption_Exception::Fam_InvalidOption_Exception(const char *msg) {
    famErrMsg = msg;
    famErr = FAM_ERR_INVALID;
}

Fam_InvalidOption_Exception::Fam_InvalidOption_Exception()
    : Fam_InvalidOption_Exception("Invalid Option") {}

Fam_Timeout_Exception::Fam_Timeout_Exception(const char *msg) {
    famErrMsg = msg;
    famErr = FAM_ERR_TIMEOUT;
}

Fam_Timeout_Exception::Fam_Timeout_Exception()
    : Fam_Timeout_Exception("Timeout failure") {}

Fam_Datapath_Exception::Fam_Datapath_Exception(enum Fam_Error fErr,
                                               const char *msg) {
    famErrMsg = msg;
    famErr = fErr;
}

Fam_Datapath_Exception::Fam_Datapath_Exception(const char *msg) {
    famErrMsg = msg;
    famErr = FAM_ERR_LIBFABRIC;
}

Fam_Datapath_Exception::Fam_Datapath_Exception()
    : Fam_Datapath_Exception("Datapath failure") {}

Fam_Allocator_Exception::Fam_Allocator_Exception(enum Fam_Error fErr,
                                                 const char *msg) {
    famErrMsg = msg;
    famErr = fErr;
}

Fam_Allocator_Exception::Fam_Allocator_Exception()
    : Fam_Allocator_Exception(FAM_ERR_ALLOCATOR, "Allocator failure") {}

Fam_Pmi_Exception::Fam_Pmi_Exception(const char *msg) {
    famErrMsg = msg;
    famErr = FAM_ERR_PMI;
}

Fam_Pmi_Exception::Fam_Pmi_Exception() : Fam_Pmi_Exception("PMI failure") {}

Fam_Unimplemented_Exception::Fam_Unimplemented_Exception(const char *msg) {
    famErrMsg = msg;
    famErr = FAM_ERR_UNIMPL;
}

Fam_Unimplemented_Exception::Fam_Unimplemented_Exception()
    : Fam_Unimplemented_Exception("Function Not implemented") {}

} // namespace openfam
