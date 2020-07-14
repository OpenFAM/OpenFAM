#include "fam_config_info.h"
#include <yaml-cpp/node/type.h>
#include <yaml-cpp/yaml.h>

enum { KEY_UNDEFINED = 0, KEY_NULL, KEY_SCALAR, KEY_SEQUENCE, KEY_MAP };

using namespace openfam;
class yaml_config_info : public config_info {
  public:
    yaml_config_info(){};
    yaml_config_info(std::string file_name) {
        try {
            config = YAML::LoadFile(file_name);
        } catch (std::exception &e) {
            std::ostringstream message;
            message << "Fam Config: Invalid Configuration file - " << file_name;
            THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
        }
    }
    std::string get_key_value(std::string key);
    std::vector<std::string> get_value_list(std::string key);
    int get_value_type(std::string key);
    ~yaml_config_info(){};

  private:
    YAML::Node config;
};

std::string yaml_config_info::get_key_value(std::string key) {
    std::string value;
    if (config[key]) {
        value = (config[key].as<std::string>());
    } else {
        std::ostringstream message;
        message << "Fam Config: Key does not exist - " << key;
        THROW_ERR_MSG(Fam_InvalidOption_Exception, message.str().c_str());
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
        message << "Fam Config: Key does not exist - " << key;
        throw Fam_InvalidOption_Exception(message.str().c_str());
    }

    return valueList;
}

int yaml_config_info::get_value_type(std::string key) {
    int type = 0;
    switch ((int)(config[key].Type())) {
    case YAML::NodeType::Undefined:
        type = KEY_UNDEFINED;
        break;
    case YAML::NodeType::Null:
        type = KEY_NULL;
        break;
    case YAML::NodeType::Scalar:
        type = KEY_SCALAR;
        break;
    case YAML::NodeType::Sequence:
        type = KEY_SEQUENCE;
        break;
    case YAML::NodeType::Map:
        type = KEY_MAP;
        break;
    }
    return type;
}
