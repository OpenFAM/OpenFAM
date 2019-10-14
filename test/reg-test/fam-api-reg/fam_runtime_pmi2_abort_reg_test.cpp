/*
 * fam_runtime_pmi2_abort_reg_test.cpp
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

#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

#ifdef COVERAGE
#include <signal.h>
#endif

#include <fam/fam.h>
#include <fam/fam_exception.h>

#include "common/fam_test_config.h"

#ifdef COVERAGE
extern "C" void __gcov_flush();
void signal_handler(int signum) {
    cout << "fam_abort called!! signal #" << signum << endl;
    __gcov_flush();
}
#endif

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;

// Test case#1 call fam_abort with PMI2 options.
TEST(FamPMI2, PMI2Abort) { EXPECT_NO_THROW(my_fam->fam_abort(0)); }
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    my_fam = new fam();

#ifdef COVERAGE
    signal(SIGCONT, signal_handler);
    signal(SIGABRT, signal_handler);
#endif

    init_fam_options(&fam_opts);
    fam_opts.runtime = strdup("PMI2");

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    int ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    delete my_fam;
    return ret;
}
