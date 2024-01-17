/*
 * cis_server_main.cpp
 * Copyright (c) 2020,2023 Hewlett Packard Enterprise Development, LP. All
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

#ifdef COVERAGE
#include <signal.h>
#endif

#include "cis/fam_cis_async_handler.h"
#include "cis/fam_cis_direct.h"
#include "cis/fam_cis_server.h"
#include "common/fam_config_info.h"
#include <iostream>
#include <string>

#ifdef USE_THALLIUM
#include "cis/fam_cis_thallium_server.h"
#include <common/fam_thallium_engine_helper.h>
#include <thallium.hpp>
#include <thread>
namespace tl = thallium;
#endif
using namespace std;
using namespace openfam;

#ifdef OPENFAM_VERSION
#define CIS_SERVER_VERSION OPENFAM_VERSION
#else
#define CIS_SERVER_VERSION "0.0.0"
#endif

#ifdef COVERAGE
extern "C" void __gcov_flush();
void signal_handler(int signum) {
    cout << "Shutting down CIS server!! signal #" << signum << endl;
    __gcov_flush();
    exit(1);
}
#endif

#ifdef MEMSERVER_PROFILE
void profile_dump_handler(int signum) {
    cout << "Dumping CIS server profile data....!! #" << signum << endl;
    cisServer->dump_profile();
}
#endif

Fam_CIS_Direct *direct_CIS;
Fam_CIS_Async_Handler *cisServer;

#ifdef USE_THALLIUM
Fam_CIS_Thallium_Server *ThalliumCIS;
void thallium_server() {
    Thallium_Engine *thal_engine_gen =
        Thallium_Engine::get_instance(direct_CIS->get_rpc_protocol_type());
    tl::engine engine = thal_engine_gen->get_engine();
    ThalliumCIS = new Fam_CIS_Thallium_Server(direct_CIS, engine);
    ThalliumCIS->run();
}
#endif

int main(int argc, char *argv[]) {
    uint64_t rpcPort = 8787;
    char *name = strdup("127.0.0.1");

    for (int i = 1; i < argc; i++) {
        if ((std::string(argv[i]) == "-v") ||
            (std::string(argv[i]) == "--version")) {
            cout << "CIS Server version : " << CIS_SERVER_VERSION << "\n";
            exit(0);
        } else if ((std::string(argv[i]) == "-h") ||
                   (std::string(argv[i]) == "--help")) {
            cout << "Usage : \n"
                 << "\t./cis_server <options> \n"
                 << "\n"
                 << "Options : \n"
                 << "\t-a/--address   : Address of the CIS server "
                    "(default value is localhost) \n"
                 << "\n"
                 << "\t-r/--rpcport        : RPC port (default value is 8787)\n"
                 << "\n"
                 << "\t-v/--version        : Display CIS server version  \n"
                 << "\n"
                 << endl;
            exit(0);
        } else if ((std::string(argv[i]) == "-a") ||
                   (std::string(argv[i]) == "--address")) {
            name = strdup(argv[++i]);
        } else if ((std::string(argv[i]) == "-r") ||
                   (std::string(argv[i]) == "--rpcport")) {
            rpcPort = atoi(argv[++i]);
        }
    }

#ifdef COVERAGE
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif

#ifdef MEMSERVER_PROFILE
    signal(SIGINT, profile_dump_handler);
#endif
    try {
        direct_CIS = new Fam_CIS_Direct(NULL);
        cisServer = new Fam_CIS_Async_Handler(rpcPort, name, direct_CIS);
#ifdef USE_THALLIUM
        // check rpc and start thallium server
        ThalliumCIS = NULL;
        std::thread thread_1;
        if (strcmp(direct_CIS->get_rpc_framework_type().c_str(),
                   FAM_OPTIONS_THALLIUM_STR) == 0) {
            thread_1 = std::thread(&thallium_server);
        }
#endif
        // start grpc server
        cisServer->run();
    } catch (Fam_Exception &e) {
        if (cisServer) {
            delete cisServer;
        }
#ifdef USE_THALLIUM
        else if (ThalliumCIS) {
            delete ThalliumCIS;
        }
#endif
        cout << "Error code: " << e.fam_error() << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
    }

    if (cisServer) {
        delete cisServer;
    }
#ifdef USE_THALLIUM
    else if (ThalliumCIS) {
        delete ThalliumCIS;
    }
#endif
    return 0;
}
