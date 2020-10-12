/*
 * fam_memory_registration_libfabric.h
 * Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All
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

#ifndef FAM_MEMORY_REGISTRATION_LIBFABRIC_H
#define FAM_MEMORY_REGISTRATION_LIBFABRIC_H

#include <iostream>
#include <map>
#include <thread>
#include <unistd.h>

#include "memory_service/fam_memory_registration.h"

#include "common/fam_internal.h"
#include "common/fam_internal_exception.h"
#include "common/fam_libfabric.h"
#include "common/fam_ops_libfabric.h"
#include "common/fam_options.h"

#include <boost/atomic.hpp>

using namespace std;

namespace openfam {
class Fam_Ops_Libfabric;
struct Fam_Region_Map_t;

class Fam_Memory_Registration_Libfabric : public Fam_Memory_Registration {
  public:
    Fam_Memory_Registration_Libfabric(const char *name, const char *service,
                                  const char *provider);

    ~Fam_Memory_Registration_Libfabric();

    void reset_profile();

    void dump_profile();

    uint64_t generate_access_key(uint64_t regionId, uint64_t dataitemId,
                                 bool permission);

    void deregister_memory(uint64_t regionId, uint64_t offset);

    void deregister_region_memory(uint64_t regionId);

    uint64_t register_memory(uint64_t regionId, uint64_t offset, void *base,
                             uint64_t size, bool rwFlag);

    void register_fence_memory();

    void deregister_fence_memory();

    size_t get_addr_size();

    void *get_addr();

    bool is_base_require();

    Fam_Ops_Libfabric *get_famOps() { return famOps; }

    std::map<uint64_t, fi_addr_t> *get_fiMemsrvMap() { return fiMemsrvMap; }

  protected:
    Fam_Ops_Libfabric *famOps;
    int libfabricProgressMode;
    std::thread progressThread;
    boost::atomic<bool> haltProgress;
    void progress_thread();
	bool isBaseRequire;
    std::map<uint64_t, Fam_Region_Map_t *> *fiMrs;
    fid_mr *fenceMr;
    std::map<uint64_t, fi_addr_t> *fiMemsrvMap;
};

} // namespace openfam
#endif
