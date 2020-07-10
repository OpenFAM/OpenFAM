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

#include "../src/common/yaml_config_info.h"
#include <iostream>
#include <map>
#include <string.h>
#include <vector>
using namespace std;
int main(int argc, char **argv) {
    yaml_config_info info;
    if ((argc <= 1) || (argv[1] == NULL)) {
        cout << "./fam_config_info <config-file-path>" << endl;
        exit(1);
    }
    char *filename = argv[1];
    info.config_init(filename);
    try {
        cout << " allocator value: " << info.get_key_value("allocator")
             << " cis ip: " << info.get_key_value("cis_ip");
        cout << " provider value: " << info.get_key_value("provider") << endl;
        int data_type = info.get_value_type("other_ip");
        ;
        if (data_type == Sequence) {
            vector<string> other_ips;
            other_ips = info.get_value_list("other_ip");
            for (unsigned int i = 0; i < other_ips.size(); ++i)
                cout << "otherip value " << other_ips[i] << std::endl;
        }
        cout << " runtime value: " << info.get_key_value("runtime") << endl;

    } catch (Fam_InvalidOption_Exception &e) {
        cout << "Exception found when accessing config file :" << filename
             << e.fam_error_msg() << endl;
    }
}
