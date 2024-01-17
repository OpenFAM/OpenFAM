/*
 * fam_thallium_engine_helper.h
 * Copyright (c) 2023 Hewlett Packard Enterprise Development, LP. All rights
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
#ifndef FAM_THALLIUM_ENGINE_HELPER_H
#define FAM_THALLIUM_ENGINE_HELPER_H

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thread>
#include <unistd.h>

using namespace std;
namespace tl = thallium;

namespace openfam {

class Thallium_Engine {
  private:
    tl::engine myEngine;
    static Thallium_Engine *instancePtr;

    Thallium_Engine(string thallium_protocol);

  public:
    // Disable copy constructor
    Thallium_Engine(const Thallium_Engine &obj) = delete;

    static Thallium_Engine *get_instance(string thallium_protocol);

    tl::engine get_engine();
};

} // namespace openfam
#endif
