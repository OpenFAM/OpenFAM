#include "fam/fam_exception.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

#ifndef CONFIG_INFO_H
#define CONFIG_INFO_H

class config_info {

  public:
    virtual void config_init(std::string file_name) = 0;
    virtual std::string get_key_value(std::string key) = 0;
    virtual std::vector<std::string> get_value_list(std::string key) = 0;
    virtual int get_value_type(std::string key) = 0;
    virtual ~config_info() {}
};
#endif
