/*
 * fam_config_info.cpp
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
#include "common/fam_config_info.h"
#include <fam/fam_exception.h>
#include <iostream>
#include <map>
#include <string.h>
#include <vector>
using namespace std;
using namespace openfam;
int main(int argc, char **argv) {
    if ((argc <= 1) || (argv[1] == NULL)) {
        cout << "./fam_config_info <config-file-path>" << endl;
        exit(1);
    }
    char *filename = argv[1];
    config_info *info = NULL;
    try {
        info = new yaml_config_info(filename);

        std::string allocator = info->get_key_value("allocator");
        if (allocator.compare("grpc") == 0) {
            cout << " allocator value: " << allocator << endl;
        } else {
            cout << "Unexpected allocator value: " << allocator << endl;
            delete info;
            exit(1);
        }
        std::string cis_ip = info->get_key_value("cis_ip");
        if (cis_ip.compare("127.0.0.1") == 0) {
            cout << " cis_ip value: " << cis_ip << endl;
        } else {
            cout << "Unexpected cis_ip value: " << cis_ip << endl;
            delete info;
            exit(1);
        }
        std::string provider = info->get_key_value("provider");
        if (provider.compare("sockets") == 0) {
            cout << " provider value: " << provider << endl;
        } else {
            cout << "Unexpected provider value: " << provider << endl;
            delete info;
            exit(1);
        }
        vector<string> ips;
        ips.push_back("0:<ip_address_1>");
        ips.push_back("1:<ip_address_2>");
        ips.push_back("3:<ip_address_3>");
        ips.push_back("2:<ip_address_4>");

        int data_type = info->get_value_type("other_ip");
        ;
        if (data_type == KEY_SEQUENCE) {
            vector<string> other_ips;
            other_ips = info->get_value_list("other_ip");
            for (unsigned int i = 0; i < other_ips.size(); ++i)
                if (other_ips[i].compare(ips[i]) == 0) {
                    cout << "otherip value " << other_ips[i] << std::endl;
                } else {
                    cout << "Unexpected otherip value: " << other_ips[i]
                         << endl;
                    delete info;
                    exit(1);
                }
        }
        delete info;
    } catch (Fam_Exception &e) {
        cout << "Exception found when accessing config file :" << filename
             << e.fam_error_msg() << endl;
        delete info;
        exit(1);
    }
}
