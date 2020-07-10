#include "fam_config_info.h"
#include <yaml-cpp/node/type.h>
#include <yaml-cpp/yaml.h>

enum { Undefined = 0, Null, Scalar, Sequence, Map };

using namespace openfam;
class yaml_config_info : public config_info {
  public:
    yaml_config_info() {}
    void config_init(std::string file_name);
    std::string get_key_value(std::string key);
    std::vector<std::string> get_value_list(std::string key);
    int get_value_type(std::string key);
    ~yaml_config_info() {}

  private:
    YAML::Node config;
};

void yaml_config_info::config_init(std::string file_name) {
    config = YAML::LoadFile(file_name);
}
std::string yaml_config_info::get_key_value(std::string key) {
    std::string value;
    if (config[key]) {
        value = (config[key].as<std::string>());
    } else {
        std::ostringstream message;
        message << "Fam Invalid Key Fam_Config: Key doesn't exist - " << key;
        throw Fam_InvalidOption_Exception(message.str().c_str());
    }
    return value;
}

std::vector<std::string> yaml_config_info::get_value_list(std::string key) {
    std::vector<std::string> valueList;
    if (config[key]) {
        if (config[key].IsSequence()) {
            for (auto element : config[key]) {
                valueList.push_back(element.as<std::string>());
            }
        } else {
            valueList.push_back(config[key].as<std::string>());
	}
    } else {
        std::ostringstream message;
        message << "Fam Invalid Key Fam_Config: Key doesn't exist - " << key;
        throw Fam_InvalidOption_Exception(message.str().c_str());
    }

    return valueList;
}

int yaml_config_info::get_value_type(std::string key) {
    return (int)(config[key].Type());
}
