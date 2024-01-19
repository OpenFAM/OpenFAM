/*
 * memory_server_main.cpp
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

#include "memory_service/fam_memory_service_direct.h"
#include "memory_service/fam_memory_service_server.h"
#include <iostream>

#ifdef USE_THALLIUM
#include "memory_service/fam_memory_service_thallium_server.h"
#include <thallium.hpp>
#include <thread>
namespace tl = thallium;
#endif

using namespace std;
using namespace metadata;

#ifdef OPENFAM_VERSION
#define MEMORYSERVER_VERSION OPENFAM_VERSION
#else
#define MEMORYSERVER_VERSION "0.0.0"
#endif

#ifdef COVERAGE
extern "C" void __gcov_flush();
void signal_handler(int signum) {
    cout << "Shutting down memory server!! signal #" << signum << endl;
    __gcov_flush();
    exit(1);
}
#endif

Fam_Memory_Service_Server *memoryService;
Fam_Memory_Service_Direct *direct_memoryService;

#ifdef USE_THALLIUM
Fam_Memory_Service_Thallium_Server *memoryThalliumService;
void thallium_server() {
    tl::engine myEngine(direct_memoryService->get_rpc_protocol_type(),
                        THALLIUM_SERVER_MODE, false, -1);
    memoryThalliumService =
        new Fam_Memory_Service_Thallium_Server(direct_memoryService, myEngine);
    memoryThalliumService->run();
}
#endif

int main(int argc, char *argv[]) {
    uint64_t memserver_id = 0;
    // char *name = strdup("127.0.0.1");
    char *fam_path = NULL;
    bool initFlag = false;
    ostringstream message;
    int startNvmmStatus = 0;

    for (int i = 1; i < argc; i++) {
        if ((std::string(argv[i]) == "-v") ||
            (std::string(argv[i]) == "--version")) {
            cout << "Memory Server version : " << MEMORYSERVER_VERSION << "\n";
            exit(0);
        } else if ((std::string(argv[i]) == "-h") ||
                   (std::string(argv[i]) == "--help")) {
            cout
                << "Usage : \n"
                << "\t./memory_server <options> \n"
                << "\n"
                << "Options : \n"
                << "\t-f/--fam_path       : Location of FAM (default value "
                   "is /dev/shm/<username>)\n"
                << "\n"
                << "\t-m/--memserver_id   : MemoryServer Id (default value is "
                   "0)\n"
                << "\n"
                << "\t-v/--version        : Display metadata server version  \n"
                << "\n"
                << "\t-i/--init                   : Initialize the root shelf "
                   "\n"
                << "\n"
                << endl;
            exit(0);
        } else if ((std::string(argv[i]) == "-i") ||
                   (std::string(argv[i]) == "--init")) {
            initFlag = true;
        } else if ((std::string(argv[i]) == "-f") ||
                   (std::string(argv[i]) == "--fam_path")) {
            fam_path = strdup(argv[++i]);
        } else if ((std::string(argv[i]) == "-m") ||
                   (std::string(argv[i]) == "--memserver_id")) {
            memserver_id = atoi(argv[++i]);
        }
    }

    if(initFlag) {
        std::string userName = login_username();
        if (fam_path == NULL || (strcmp(fam_path, "") == 0)) {
            startNvmmStatus = StartNVMM();
        } else {
            startNvmmStatus = StartNVMM(fam_path, userName);
        }
        if (startNvmmStatus != 0) {
            message << "Starting of memory server failed";
            THROW_ERRNO_MSG(Memory_Service_Exception,
                            MEMORY_SERVER_START_FAILED, message.str().c_str());
        }

        exit(0);
    }

#ifdef COVERAGE
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif

    try {
        direct_memoryService =
            new Fam_Memory_Service_Direct(memserver_id, false);
        memoryService = new Fam_Memory_Service_Server(direct_memoryService);
#ifdef USE_THALLIUM
        std::thread thread_1;
        // check rpc_framework_type and start server
        if (strcmp(direct_memoryService->get_rpc_framework_type().c_str(),
                   FAM_OPTIONS_THALLIUM_STR) == 0) {
            // starting thallium server on a new thread
            thread_1 = std::thread(&thallium_server);
        }
#endif
        // starting only grpc server
        memoryService->run();
    } catch (Fam_Exception &e) {
        if (memoryService) {
            delete memoryService;
        }
#ifdef USE_THALLIUM
        else if (memoryThalliumService) {
            delete memoryThalliumService;
        }
#endif
        cout << "Error code: " << e.fam_error() << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
    }

    if (memoryService) {
        delete memoryService;
        memoryService = NULL;
    }
#ifdef USE_THALLIUM
    else if (memoryThalliumService) {
        delete memoryThalliumService;
        memoryThalliumService = NULL;
    }
#endif

    return 0;
}
