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
        if (strstr(filename, "fam_pe_config")) {
            cout << "Opened fam_pe_config"
                 << " allocator value: " << info.get_key_value("allocator")
                 << " cis ip: " << info.get_key_value("cis_ip");
            cout << " provider value: " << info.get_key_value("provider")
                 << endl;
            cout << " runtime value: " << info.get_key_value("runtime")
                 << std::endl;
        }
    } catch (Fam_InvalidOption_Exception &e) {
        cout << "Exception found when accessing config file :" << filename
             << e.fam_error_msg() << endl;
    }

    try {
        if (strstr(filename, "fam_cis_config")) {
            cout << "Opened fam_cis_config" << endl;
            vector<string> memsrv_ips;
            int data_type = info.get_value_type("memsrv_ip");
            if (data_type == Sequence) {
                memsrv_ips = info.get_value_list("memsrv_ip");
                for (unsigned int i = 0; i < memsrv_ips.size(); ++i)
                    cout << "memsrvip value " << memsrv_ips[i] << std::endl;
            }
        }
    } catch (Fam_InvalidOption_Exception &e) {
        cout << "Exception found when accessing config file :" << filename
             << " " << e.fam_error_msg() << endl;
    }

    try {
        if (strstr(filename, "fam_memoryserver_config")) {
            cout << "Opened fam_memoryserver_config" << endl;
            vector<string> metadata_ips;
            int data_type = info.get_value_type("metadata_ips");
            if (data_type == Sequence) {
                metadata_ips = info.get_value_list("metadata_ips");
                for (unsigned int i = 0; i < metadata_ips.size(); ++i)
                    cout << "metadata ip value " << metadata_ips[i]
                         << std::endl;
            }
        }
    } catch (Fam_InvalidOption_Exception &e) {
        cout << "Exception found when accessing config file :" << filename
             << e.fam_error_msg() << endl;
    }
    try {
        if (strstr(filename, "fam_metadata_config")) {
            cout << "Opened fam_metadata_config" << endl;
            vector<string> memsrv_ips;
            int data_type = info.get_value_type("memsrv_ip");
            if (data_type == Sequence) {
                memsrv_ips = info.get_value_list("memsrv_ip");
                for (unsigned int i = 0; i < memsrv_ips.size(); ++i)
                    cout << "memsrvip value " << memsrv_ips[i] << std::endl;
            }
        }
    } catch (Fam_InvalidOption_Exception &e) {
        cout << "Exception found when accessing config file :" << filename
             << e.fam_error_msg() << endl;
    }
}
