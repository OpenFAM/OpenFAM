/*
 * metadata_server_main.cpp
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

#include "common/fam_options.h"
#include "metadata_service/fam_metadata_service_direct.h"
#include "metadata_service/fam_metadata_service_server.h"
#include <iostream>
#include <thread>
using namespace std;
using namespace metadata;

#ifdef USE_THALLIUM
#include "metadata_service/fam_metadata_service_thallium_server.h"
#include <thallium.hpp>
namespace tl = thallium;
#endif

#ifdef OPENFAM_VERSION
#define METADATASERVER_VERSION OPENFAM_VERSION
#else
#define METADATASERVER_VERSION "0.0.0"
#endif

#ifdef COVERAGE
extern "C" void __gcov_flush();
void signal_handler(int signum) {
    cout << "Shutting down metadata server!! signal #" << signum << endl;
    __gcov_flush();
    exit(1);
}
#endif

Fam_Metadata_Service_Server *metadataService;
Fam_Metadata_Service_Direct *direct_metadataService;

#ifdef USE_THALLIUM
Fam_Metadata_Service_Thallium_Server *metadataThalliumService;
void thallium_server() {
    tl::engine myEngine(direct_metadataService->get_rpc_protocol_type(),
                        THALLIUM_SERVER_MODE, false, -1);
    metadataThalliumService = new Fam_Metadata_Service_Thallium_Server(
        direct_metadataService, myEngine);
    metadataThalliumService->run();
}
#endif

int main(int argc, char *argv[]) {
    uint64_t rpcPort = 8788;
    char *name = strdup("127.0.0.1");

    for (int i = 1; i < argc; i++) {
        if ((std::string(argv[i]) == "-v") ||
            (std::string(argv[i]) == "--version")) {
            cout << "Metadata Server version : " << METADATASERVER_VERSION
                 << "\n";
            exit(0);
        } else if ((std::string(argv[i]) == "-h") ||
                   (std::string(argv[i]) == "--help")) {
            cout
                << "Usage : \n"
                << "\t./metadataserver <options> \n"
                << "\n"
                << "Options : \n"
                << "\t-a/--address   : Address of the metadata server "
                   "(default value is localhost) \n"
                << "\n"
                << "\t-r/--rpcport        : RPC port (default value is 8787)\n"
                << "\n"
                << "\t-v/--version        : Display metadata server version  \n"
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

    try {
        direct_metadataService = new Fam_Metadata_Service_Direct(false);
        metadataService = new Fam_Metadata_Service_Server(
            rpcPort, name, direct_metadataService);
#ifdef USE_THALLIUM
        std::thread thread_1;
        if (strcmp(direct_metadataService->get_rpc_framework_type().c_str(),
                   FAM_OPTIONS_THALLIUM_STR) == 0) {
            // start thallium server on a new thread
            thread_1 = std::thread(&thallium_server);
        }
#endif
        // start grpc server
        metadataService->run();
    } catch (Fam_Exception &e) {
        if (metadataService) {
            delete metadataService;
        }
#ifdef USE_THALLIUM
        else if (metadataThalliumService) {
            delete metadataThalliumService;
        }
#endif
        cout << "Error code: " << e.fam_error() << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
    }

    if (metadataService) {
        delete metadataService;
        metadataService = NULL;
    }
#ifdef USE_THALLIUM
    else if (metadataThalliumService) {
        delete metadataThalliumService;
        metadataThalliumService = NULL;
    }
#endif
    return 0;
}
