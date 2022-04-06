/*
 * memory_server_main.cpp
 * Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All rights
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

#ifdef COVERAGE
#include <signal.h>
#endif

#include "memory_service/fam_memory_service_server.h"
#include <iostream>
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

int main(int argc, char *argv[]) {
    uint64_t memserver_id = 0;
    // char *name = strdup("127.0.0.1");
    char *fam_path = NULL;
	bool initFlag = false;

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
                << "\t-i/--init			  : Initialize the root shelf "
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
            StartNVMM();
        } else
            StartNVMM(fam_path, userName);
        exit(0);
    }

#ifdef COVERAGE
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif

    memoryService = NULL;
    try {
        memoryService = new Fam_Memory_Service_Server(memserver_id);
        memoryService->run();
    } catch (Fam_Exception &e) {
        if (memoryService) {
            delete memoryService;
        }
        cout << "Error code: " << e.fam_error() << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
    }

    if (memoryService) {
        delete memoryService;
        memoryService = NULL;
    }

    return 0;
}
