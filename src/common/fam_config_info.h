/*
 * fam_config_info.h
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
#ifndef FAM_CONFIG_INFO_H
#define FAM_CONFIG_INFO_H

#include <yaml-cpp/node/type.h>
#include <yaml-cpp/yaml.h>

#include "common/fam_internal_exception.h"
#include <iostream>
#include <map>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#define FILE_MAX_LEN 255

enum { KEY_UNDEFINED = 0, KEY_NULL, KEY_SCALAR, KEY_SEQUENCE, KEY_MAP };

typedef std::map<std::string, std::string> configFileParams;

class config_info {

  public:
    virtual std::string get_key_value(std::string key) = 0;
    virtual std::vector<std::string> get_value_list(std::string key) = 0;
    virtual int get_value_type(std::string key) = 0;
    virtual ~config_info() {}
};

class yaml_config_info : public config_info {
  public:
    yaml_config_info(std::string file_name);
    std::string get_key_value(std::string key);
    std::vector<std::string> get_value_list(std::string key);
    int get_value_type(std::string key);
    ~yaml_config_info();

  private:
    YAML::Node config;
};

/*
 * find_config_file - Look for configuration file in path specified by
 * OPENFAM_ROOT environment variable or in /opt/OpenFAM.
 * On Success, return config file with absolute path. Else returns empty string.
 */
inline std::string find_config_file(char *config_file) {
    struct stat buffer;
    std::string config_filename;
    char *config_file_path = (char *)malloc(FILE_MAX_LEN);

    // Look for config file in OPENFAM_ROOT
    char *openfam_root = getenv("OPENFAM_ROOT");

    if (openfam_root != NULL) {
        sprintf(config_file_path, "%s/%s/%s", openfam_root, "config",
                config_file);
    }
    if (stat(config_file_path, &buffer) == 0) {
        config_filename = config_file_path;
        free(config_file_path);
        return config_filename;
    }

    // Look for config file in /opt
    sprintf(config_file_path, "%s/%s/%s", "/opt/OpenFAM", "config",
            config_file);
    if (stat(config_file_path, &buffer) == 0) {
        config_filename = config_file_path;
        free(config_file_path);
        return config_filename;
    }
    free(config_file_path);
    config_filename.clear();
    return config_filename;
}

#endif
