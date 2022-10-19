#!/usr/bin/python3
# openfam_adm.py
# Copyright (c) 2022 Hewlett Packard Enterprise Development, LP. All rights reserved.
# Redistribution and use in source and binary forms, with or without modification, are permitted provided
# that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
#    in the documentation and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
#    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
#    BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
#    SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
#    OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# See https://spdx.org/licenses/BSD-3-Clause
#
#
import os
import sys
import argparse
import ruamel.yaml
import time
import math
import re
import subprocess
from pathlib import Path
from tabulate import tabulate

# Create the parser


class ArgumentParserCustom(argparse.ArgumentParser):

    def convert_arg_line_to_args(self, arg_line):

        if (re.match(r'^[\s]*#', arg_line) or   # Look for any number of whitespace characters up to a `#` character
                re.match(r'^[\s]*$', arg_line)):  # Look for lines containing nothing or just whitespace
            return []
        elif '#' in arg_line and arg_line.split('#')[0] != "":
            # Strip the leading and trailing spaces
            new_arg_line = arg_line.split('#')[0].strip()
            return [new_arg_line]
        else:
            return [arg_line]


my_parser = ArgumentParserCustom(
    fromfile_prefix_chars="@", description="OpenFAM utility tool")

my_parser.add_argument(
    "--create_config_files",
    default=False,
    action="store_true",
    help="Generates all configuration files"
)

my_parser.add_argument(
    "--start_service",
    default=False,
    action="store_true",
    help="Start all OpenFAM services"
)

my_parser.add_argument(
    "--stop_service",
    default=False,
    action="store_true",
    help="Stop all OpenFAM services"
)

my_parser.add_argument(
    "--clean",
    default=False,
    action="store_true",
    help="Clean all FAMs and Metadata"
)

my_parser.add_argument(
    "--install_path",
    type=str,
    action="store",
    help="Path to directory where OpenFAM is installed",
    metavar=''
)

my_parser.add_argument(
    "--config_file_path",
    type=str,
    action="store",
    help="Path to create configuration files",
    metavar=''
)

my_parser.add_argument(
    "--launcher",
    action="store",
    type=str,
    help="Launcher to be used to launch processes(slurm/ssh). When set to \"ssh\", services are launched using ssh on remote nodes and tests are launched using mpi and when set to \"slurm\" both services and tests are launched using slurm. Note : If this option is not set, by default all sevices and programs run locally.",
    choices=["ssh", "slurm"],
    metavar=''
)

my_parser.add_argument(
    "--global_launcher_options",
    action="store",
    type=str,
    help="Launcher command options which can be used commonly across all services. Specify all options within quotes(\") eg : --global_launcher_options=\"-N\" \"1\" \"--mpi=pmix\"",
    metavar=''
)

my_parser.add_argument(
    "--cisinterface",
    action="store",
    type=str,
    help="CIS interface type(rpc/direct)",
    choices=["rpc", "direct"],
    metavar=''
)

my_parser.add_argument(
    "--memserverinterface",
    action="store",
    type=str,
    help="Memory Service interface type(rpc/direct)",
    choices=["rpc", "direct"],
    metavar=''
)

my_parser.add_argument(
    "--metaserverinterface",
    action="store",
    type=str,
    help="Metadata Service interface type(rpc/direct)",
    choices=["rpc", "direct"],
    metavar=''
)

my_parser.add_argument(
    "--cisserver",
    action="store",
    type=str,
    help="CIS parameters, eg : ,0:{rpc_interface:127.0.0.1,rpc_port:8795,launcher_options: \"-n\" \"1\"}. Note : Specify all launcher options within quotes(\") eg : launcher_options:\"-N\" \"1\" \"--mpi=pmix\"",
    metavar=" {comma seperated attributes}"
)

my_parser.add_argument(
    "--memservers",
    action="append",
    metavar=" id:{comma seperated attributes}",
    type=str,
    help="Add a new memory server, eg : 0:{memory_type:volatile,fam_path:/dev/shm/vol/,rpc_interface:127.0.0.1,rpc_port:8795,libfabric_port:7500,if_device:eth0,launcher_options: '-n' '1'}. Note : Specify all launcher options within quotes(\") eg : launcher_options: \"-N\" \"1\" \"--mpi=pmix\"",
)

my_parser.add_argument(
    "--metaservers",
    action="append",
    metavar=" id:{comma seperated attributes}",
    type=str,
    help="Add a metadata server, eg : 0:{rpc_interface:127.0.0.1,rpc_port:8795,launcher_options: '-n' '1'}. Note : Specify all launcher options within quotes(\") eg : launcher_options: \"-N\" \"1\" \"--mpi=pmix\""
)

my_parser.add_argument(
    "--test_args",
    action="store",
    type=str,
    help="Test configuration parameters, eg : {num_pes:1,launcher_options: '-n' '1'}. Note : Specify all launcher options within quotes(\") eg : launcher_options: \"-N\" \"1\" \"--mpi=pmix\"",
    metavar=' {comma seperated attributes}'
)

my_parser.add_argument(
    "--model",
    action="store",
    default="memory_server",
    type=str,
    help="OpenFAM model(memory_server/shared_memory)",
    choices=["memory_server", "shared_memory"],
    metavar=''
)

my_parser.add_argument(
    "--thrdmodel",
    action="store",
    type=str,
    help="Thread model(single/multiple)",
    choices=["single", "multiple"],
    metavar=''
)

my_parser.add_argument(
    "--provider",
    action="store",
    type=str,
    help="Libfabric provider to be used for datapath operations",
    choices=["sockets", "psm2", "verbs;ofi_rxm"],
    metavar=''
)

my_parser.add_argument(
    "--runtime",
    action="store",
    type=str,
    help="Runtime process management interface(PMI) to be used",
    choices=["PMIX", "PMI2", "NONE"],
    metavar=''
)

my_parser.add_argument(
    "--kvstype",
    action="store",
    type=str,
    help="Type of key value store to be used for metadata management",
    metavar=''
)

my_parser.add_argument(
    "--metapath", action="store", type=str, help="path to store metadata", metavar=''
)

my_parser.add_argument(
    "--fampath", action="store", type=str, help="path where data is stored", metavar=''
)

my_parser.add_argument(
    "--fam_backup_path", action="store", type=str, help="path where data backup is stored(on shared filesystem)", metavar=''
)


my_parser.add_argument(
    "--atlthreads", action="store", type=str, help="Atomic library threads count", metavar=''
)

my_parser.add_argument("--atlqsize", action="store",
                       type=str, help="ATL queue size", metavar='')

my_parser.add_argument(
    "--atldatasize", action="store", type=str, help="ATL data size per thread(MiB)", metavar=''
)

my_parser.add_argument(
    "--disableregionspanning",
    default=False,
    action="store_true",
    help="Disable region spanning",
)

my_parser.add_argument(
    "--regionspanningsize", action="store", type=str, help="Region spanning size", metavar=''
)

my_parser.add_argument(
    "--interleaveblocksize", action="store", type=str, help="Dataitem interleave block size", metavar=''
)

my_parser.add_argument(
    "--runtests",
    default=False,
    action="store_true",
    help="Run regression and unit tests",
)

my_parser.add_argument(
    "--sleep",
    default=5,
    type=int,
    action="store",
    help="Number of seconds to sleep",
    metavar=''
)

args = my_parser.parse_args()

# set error count and warning to 0
error_count = 0
warning_count = 0
if not len(sys.argv) > 1:
    my_parser.print_help()
    sys.exit(0)

# Get the path to OpenFAM build location


def get_install_path():
    if args.install_path is not None:
        openfam_install_path = args.install_path
    elif "OPENFAM_INSTALL_DIR" in os.environ:
        if os.environ["OPENFAM_INSTALL_DIR"] != "":
            openfam_install_path = os.environ["OPENFAM_INSTALL_DIR"]
    elif 'openfam_admin_tool_config_doc' in globals() and openfam_admin_tool_config_doc["install_path"] != "":
        openfam_install_path = openfam_admin_tool_config_doc["install_path"]
    else:
        global error_count
        error_count = error_count + 1
        print(
            '\033[1;31;40mERROR['+str(error_count)+']: Could not locate OpenFAM build location. Please provide a path to OpenFAM build location with "--install_path" option or by setting environment variable "OPENFAM_INSTALL_DIR".\033[0;37;40m'
        )
        sys.exit(1)
    return openfam_install_path


if args.create_config_files:
    openfam_install_path = get_install_path()
    if args.config_file_path is None:
        error_count = error_count + 1
        print(
            '\033[1;31;40mERROR[' +
            str(error_count) +
            ']: Missing "--config_file_path" option. Please provide a path to create configuration files.\033[0;37;40m'
        )
        sys.exit(1)

    # Open all config file templates
    with open(openfam_install_path + "/config/fam_pe_config.yaml") as pe_config_infile:
        pe_config_doc = ruamel.yaml.load(
            pe_config_infile, ruamel.yaml.RoundTripLoader)
    with open(
        openfam_install_path + "/config/fam_client_interface_config.yaml"
    ) as cis_config_infile:
        cis_config_doc = ruamel.yaml.load(
            cis_config_infile, ruamel.yaml.RoundTripLoader)
    with open(
        openfam_install_path + "/config/fam_memoryserver_config.yaml"
    ) as memservice_config_infile:
        memservice_config_doc = ruamel.yaml.load(
            memservice_config_infile, ruamel.yaml.RoundTripLoader
        )
    with open(
        openfam_install_path + "/config/fam_metadata_config.yaml"
    ) as metaservice_config_infile:
        metaservice_config_doc = ruamel.yaml.load(
            metaservice_config_infile, ruamel.yaml.RoundTripLoader
        )
    with open(
        openfam_install_path + "/config/openfam_admin_tool.yaml"
    ) as openfam_admin_tool_config_infile:
        openfam_admin_tool_config_doc = ruamel.yaml.load(
            openfam_admin_tool_config_infile, ruamel.yaml.RoundTripLoader
        )

    if args.model is not None:
        pe_config_doc["openfam_model"] = args.model
    if args.thrdmodel is not None:
        pe_config_doc["FamThreadModel"] = args.thrdmodel
    if args.provider is not None:
        pe_config_doc["provider"] = args.provider
        memservice_config_doc["provider"] = args.provider
    if args.runtime is not None:
        pe_config_doc["runtime"] = args.runtime
    if args.cisinterface is not None:
        pe_config_doc["client_interface_type"] = args.cisinterface
    if args.memserverinterface is not None:
        cis_config_doc["memsrv_interface_type"] = args.memserverinterface
    if args.metaserverinterface is not None:
        cis_config_doc["metadata_interface_type"] = args.metaserverinterface
    if args.kvstype is not None:
        metaservice_config_doc["metadata_manager"] = args.kvstype
    if args.metapath is not None:
        metaservice_config_doc["metadata_path"] = args.metapath
    if args.fam_backup_path is not None:
        memservice_config_doc["fam_backup_path"] = args.fam_backup_path
    else:
        memservice_config_doc["fam_backup_path"] = args.config_file_path + \
            "/" + os.environ["USER"] + "/backup/"
    if args.atlthreads is not None:
        memservice_config_doc["ATL_threads"] = args.atlthreads
    if args.atlqsize is not None:
        memservice_config_doc["ATL_queue_size"] = args.atlqsize
    if args.atldatasize is not None:
        memservice_config_doc["ATL_data_size"] = args.atldatasize
    if args.launcher is not None:
        openfam_admin_tool_config_doc["launcher"] = str(args.launcher)
    if args.global_launcher_options is not None:
        openfam_admin_tool_config_doc["launcher_options"]["common"] = args.global_launcher_options
    if args.disableregionspanning:
        print("region spanning disabled")
        metaservice_config_doc["enable_region_spanning"] = bool(0)
    else:
        metaservice_config_doc["enable_region_spanning"] = bool(1)
    if args.regionspanningsize is not None:
        metaservice_config_doc["region_span_size_per_memoryserver"] = int(
            args.regionspanningsize
        )

    if args.interleaveblocksize is not None:
        metaservice_config_doc["dataitem_interleave_size"] = int(
            args.interleaveblocksize)

    if args.install_path is not None:
        openfam_admin_tool_config_doc["install_path"] = args.install_path
    if args.sleep is not None:
        openfam_admin_tool_config_doc["sleep_time"] = args.sleep
# Parsing Memory server arguments
    '''
    In config file the memory server details are stored as given below (as map of map):

    Memservers:
     0:
       memory_type: volatile
       fam_path: /dev/shm/vol/
       rpc_interface: 127.0.0.1:8789
       libfabric_port: 7500
       if_device: eth0

     1:
       memory_type: persistent
       fam_path: /dev/shm/per/
       rpc_interface: 127.0.0.1:8788
       libfabric_port: 7501
       if_device: eth1

    The input to this script is passed as :

    --memservers=0:{memory_type:volatile,rpc_interface:127.0.0.1,rpc_port:8795,libfabric_port:7500,if_device:eth0}
    --memservers=1:{memory_type:persistent,fam_path:/dev/shm/per/,rpc_interface:127.0.0.1,rpc_port:8796,if_device:eth1}

    The below code converts this input to the format given in config file

    '''
    memserver_map = {}
    memserver_id_list = []
    memserver_launcher_opt_map = {}
    default_memory_type = 'volatile'
    memserver_valid_options = ['memory_type', 'fam_path', 'rpc_interface',
                               'rpc_port', 'libfabric_port', 'if_device', 'launcher_options']
    if args.memservers is not None:
        memservers_list = args.memservers
        for m in memservers_list:
            m_id = int(m.split('{')[0].split(':')[0])
            m_value = m.split('{')[1].split('}')[0]
            value_list = m_value.split(',')
            value_map = {}
            for value in value_list:
                v = value.split(':')
                if v[1].isnumeric():
                    v[1] = int(v[1])
                if v[0] == 'rpc_port':
                    rpc_interface = value_map['rpc_interface']
                    value_map['rpc_interface'] = rpc_interface+':'+str(v[1])
                    memserver_id_list.append(
                        str(m_id)+':'+rpc_interface+':'+str(v[1]))
                elif v[0] == 'launcher_options':
                    memserver_launcher_opt_map[m_id] = v[1]
                elif v[0] in memserver_valid_options:
                    value_map[v[0]] = v[1]
                else:
                    error_count = error_count + 1
                    print(
                        '\033[1;31;40mERROR['+str(error_count)+']: "' + v[0] +
                        '" is invalid option for --memservers argument\033[0;37;40m'
                    )
                    sys.exit(1)
            if 'memory_type' not in value_map:
                value_map['memory_type'] = default_memory_type
            memserver_map[m_id] = value_map

        memservice_config_doc["Memservers"] = memserver_map
        cis_config_doc["memsrv_list"] = memserver_id_list
        openfam_admin_tool_config_doc["launcher_options"]["memserver"] = memserver_launcher_opt_map

# Parsing Metadata server arguments
    metaserver_id_list = []
    metaserver_launcher_opt_map = {}
    if args.metaservers is not None:
        metaservers_list = args.metaservers
        for m in metaservers_list:
            m_id = int(m.split('{')[0].split(':')[0])
            m_value = m.split('{')[1].split('}')[0]
            value_list = m_value.split(',')
            for value in value_list:
                v = value.split(':')
                if v[1].isnumeric():
                    v[1] = int(v[1])
                if v[0] == 'rpc_interface':
                    rpc_interface = v[1]
                elif v[0] == 'rpc_port':
                    rpc_port = v[1]
                elif v[0] == 'launcher_options':
                    metaserver_launcher_opt_map[m_id] = v[1]
                else:
                    error_count = error_count + 1
                    print(
                        '\033[1;31;40mERROR['+str(error_count)+']: "' + v[0] +
                        '" is invalid option for --metaservers argument\033[0;37;40m'
                    )
                    sys.exit(1)
            metaserver_id_list.append(
                str(m_id) + ':' + rpc_interface + ':' + str(rpc_port))

        cis_config_doc["metadata_list"] = metaserver_id_list
        openfam_admin_tool_config_doc["launcher_options"]["metaserver"] = metaserver_launcher_opt_map

# Parsing CIS arguments
    if args.cisserver is not None:
        c_value = args.cisserver.split('{')[1].split('}')[0]
        value_list = c_value.split(',')
        for value in value_list:
            v = value.split(':')
            if v[1].isnumeric():
                v[1] = int(v[1])
            if v[0] == 'rpc_interface':
                rpc_interface = v[1]
            elif v[0] == 'rpc_port':
                rpc_port = v[1]
            elif v[0] == 'launcher_options':
                openfam_admin_tool_config_doc["launcher_options"]["cis"] = v[1]
            else:
                error_count = error_count + 1
                print(
                    '\033[1;31;40mERROR['+str(error_count)+']: "' + v[0] +
                    '" is invalid option for --cisserver argument \033[0;37;40m'
                )
                sys.exit(1)
    pe_config_doc["fam_client_interface_service_address"] = rpc_interface + \
        ':' + str(rpc_port)
    cis_config_doc["rpc_interface"] = rpc_interface + ':' + str(rpc_port)

# Parsing test arguments
    if args.test_args is not None:
        pe_hosts = []
        t_value = args.test_args.split('{')[1].split('}')[0]
        value_list = t_value.split(',')
        for value in value_list:
            v = value.split(':')
            if v[1].isnumeric():
                v[1] = int(v[1])
            if v[0] == 'num_pes':
                openfam_admin_tool_config_doc["num_pes"] = v[1]
            elif v[0] == 'launcher_options':
                openfam_admin_tool_config_doc["launcher_options"]["compute_pes"] = v[1]
            else:
                error_count = error_count + 1
                print(
                    '\033[1;31;40mERROR['+str(error_count)+']: "' + v[0] +
                    '" is invalid option for --test_args argument \033[0;37;40m'
                )
                sys.exit(1)

    # Check if user has provided any service interface as "rpc" when openfam model is shared_memory
    # or as "direct" if openfam model is memory_server
    if pe_config_doc["openfam_model"] == "shared_memory":
        if "rpc" in [
            str(pe_config_doc["client_interface_type"]),
            str(cis_config_doc["memsrv_interface_type"]),
            str(cis_config_doc["metadata_interface_type"]),
        ]:

            error_count = error_count + 1
            print(
                '\033[1;31;40mERROR['+str(
                    error_count)+']: In shared memory model interface type of all services has to be direct, one or more service interfcae is mentioned as "rpc" \033[0;37;40m'
            )
            sys.exit(1)
    else:
        if "rpc" not in [
            str(pe_config_doc["client_interface_type"]),
            str(cis_config_doc["memsrv_interface_type"]),
            str(cis_config_doc["metadata_interface_type"]),
        ]:
            error_count = error_count + 1
            print(
                '\033[1;31;40mERROR['+str(
                    error_count)+']: In memory server model interface type of all services can not to be "direct", change model to "shared memory" \033[0;37;40m'
            )
            sys.exit(1)

    # Create backup directory
    cmd = "mkdir -p " + memservice_config_doc["fam_backup_path"]
    os.system(cmd)
    # Write to output config files

    cmd = "mkdir -p " + args.config_file_path + "/config"
    os.system(cmd)
    with open(args.config_file_path + "/config/fam_pe_config.yaml", "w") as pe_config_outfile:
        ruamel.yaml.dump(
            pe_config_doc, pe_config_outfile, Dumper=ruamel.yaml.RoundTripDumper
        )
    with open(
        args.config_file_path + "/config/fam_client_interface_config.yaml", "w"
    ) as cis_config_outfile:
        ruamel.yaml.dump(
            cis_config_doc, cis_config_outfile, Dumper=ruamel.yaml.RoundTripDumper
        )
    with open(
        args.config_file_path + "/config/fam_memoryserver_config.yaml", "w"
    ) as memservice_config_outfile:
        ruamel.yaml.dump(
            memservice_config_doc,
            memservice_config_outfile,
            Dumper=ruamel.yaml.RoundTripDumper,
        )
    with open(
        args.config_file_path + "/config/fam_metadata_config.yaml", "w"
    ) as metaservice_config_outfile:
        ruamel.yaml.dump(
            metaservice_config_doc,
            metaservice_config_outfile,
            Dumper=ruamel.yaml.RoundTripDumper,
        )
    with open(
        args.config_file_path + "/config/openfam_admin_tool.yaml", "w"
    ) as openfam_admin_tool_config_outfile:
        ruamel.yaml.dump(
            openfam_admin_tool_config_doc,
            openfam_admin_tool_config_outfile,
            Dumper=ruamel.yaml.RoundTripDumper,
        )

    if not args.start_service:
        print(
            '\nConfiguration files are created under ' + args.config_file_path + '/config\n' + '\n' +
            'OpenFAM services can be started using one of the methods mentioned below:\n' +
            '   1] Run, \"export OPENFAM_ROOT=' + args.config_file_path + '\"\n' +
            '      and then execute "' + sys.argv[0] + ' --start_service"\n' +
            '   2] Run, "' +
            sys.argv[0] + ' --start_service --config_file_path=' +
            args.config_file_path + '"\n' +
            '\nNote : It is recommended to define OPENFAM_ROOT as it will be used while running OpenFAM applications.\n'
        )

# Open all config files from user specified location
if args.config_file_path is not None:
    os.environ["OPENFAM_ROOT"] = args.config_file_path
    openfam_config_path = args.config_file_path
else:
    if "OPENFAM_ROOT" in os.environ and os.environ["OPENFAM_ROOT"] == "":
        error_count = error_count + 1
        print(
            '\033[1;31;40mERROR[' +
            str(error_count) +
            ']: Configuration file path not found. Please set environment variable "OPENFAM_ROOT" to point to right onfiguration files\033[0;37;40m'
        )
        sys.exit(1)
    elif "OPENFAM_ROOT" not in os.environ:
        error_count = error_count + 1
        print(
            '\033[1;31;40mERROR[' +
            str(error_count) +
            ']: Could not find configuration files. Please define configuration file path, either set "OPENFAM_ROOT" or pass --config_file_path option\033[0;37;40m'
        )
        sys.exit(1)
    else:
        openfam_config_path = os.environ["OPENFAM_ROOT"]
with open(openfam_config_path + "/config/fam_pe_config.yaml") as pe_config_infile:
    pe_config_doc = ruamel.yaml.load(
        pe_config_infile, ruamel.yaml.RoundTripLoader)
with open(
    openfam_config_path + "/config/fam_client_interface_config.yaml"
) as cis_config_infile:
    cis_config_doc = ruamel.yaml.load(
        cis_config_infile, ruamel.yaml.RoundTripLoader)
with open(
    openfam_config_path + "/config/fam_memoryserver_config.yaml"
) as memservice_config_infile:
    memservice_config_doc = ruamel.yaml.load(
        memservice_config_infile, ruamel.yaml.RoundTripLoader
    )
with open(
    openfam_config_path + "/config/fam_metadata_config.yaml"
) as metaservice_config_infile:
    metaservice_config_doc = ruamel.yaml.load(
        metaservice_config_infile, ruamel.yaml.RoundTripLoader
    )
with open(
    openfam_config_path + "/config/openfam_admin_tool.yaml"
) as openfam_admin_tool_config_infile:
    openfam_admin_tool_config_doc = ruamel.yaml.load(
        openfam_admin_tool_config_infile, ruamel.yaml.RoundTripLoader
    )

# Assign values to config options if corresponding argument is set
if args.sleep is not None:
    sleepsecs = args.sleep
else:
    sleepsecs = openfam_admin_tool_config_doc["sleep_time"]

ssh_cmd = "ssh " + os.environ["USER"] + "@"

if args.start_service:
    openfam_install_path = get_install_path()
    service_info = []
    # unset http_proxy and https_proxy if they are set
    os.environ.pop('http_proxy', None)
    os.environ.pop('https_proxy', None)
    os.environ.pop('HTTP_PROXY', None)
    os.environ.pop('HTTPS_PROXY', None)
    # start all memory services
    if (
        pe_config_doc["openfam_model"] == "memory_server"
        and cis_config_doc["memsrv_interface_type"] == "rpc"
    ):
        # Iterate over list of memory servers
        i = 0
        for server in cis_config_doc["memsrv_list"]:
            memory_server_id = server.split(":")[0]
            memory_server_addr = server.split(":")[1]
            key_string="openfam:mem:"+server
            if openfam_admin_tool_config_doc["launcher"] == "ssh":
                cmd = (
                    ssh_cmd
                    + memory_server_addr
                    + " \"sh -c '"
                    + "nohup "
                    + openfam_install_path
                    + "/bin/memory_server -m "
                    + memory_server_id
                    + "> /dev/null 2>&1 &'\""
                )
            elif openfam_admin_tool_config_doc["launcher"] == "slurm":
                command_options = openfam_admin_tool_config_doc["launcher_options"]["common"]
                if len(openfam_admin_tool_config_doc["launcher_options"]["memserver"]):
                    command_options = command_options + " " + \
                        openfam_admin_tool_config_doc["launcher_options"]["memserver"][int(
                            memory_server_id)]
                cmd = (
                    "srun "
                    + "--nodelist=" + memory_server_addr + " "
                    + "--comment=" + key_string + " "
                    + command_options + " "
                    + openfam_install_path
                    + "/bin/memory_server -m "
                    + memory_server_id
                    + " &"
                )
            else:
                cmd = (
                    openfam_install_path
                    + "/bin/memory_server -m "
                    + memory_server_id
                    + " &"
                )
            i = i + 1
            os.system(cmd)
            service_info.append(
                ['memory service', memory_server_id, memory_server_addr, server.split(":")[2]])

    # start all metadata services
    if (
        pe_config_doc["openfam_model"] == "memory_server"
        and cis_config_doc["metadata_interface_type"] == "rpc"
    ):
        # Iterate over list of metadata servers
        i = 0
        for server in cis_config_doc["metadata_list"]:
            metadata_server_id = server.split(":")[0]
            metadata_server_addr = server.split(":")[1]
            metadata_server_rpc_port = server.split(":")[2]
            key_string="openfam:meta:"+server
            if openfam_admin_tool_config_doc["launcher"] == "ssh":
                cmd = (
                    ssh_cmd
                    + metadata_server_addr
                    + " \"sh -c '"
                    + "nohup "
                    + openfam_install_path
                    + "/bin/metadata_server -a "
                    + metadata_server_addr
                    + " -r "
                    + str(metadata_server_rpc_port)
                    + "> /dev/null 2>&1 &'\""
                )
            elif openfam_admin_tool_config_doc["launcher"] == "slurm":
                command_options = openfam_admin_tool_config_doc["launcher_options"]["common"]
                if len(openfam_admin_tool_config_doc["launcher_options"]["metaserver"]):
                    command_options = command_options + " " + \
                        openfam_admin_tool_config_doc["launcher_options"]["metaserver"][int(
                            metadata_server_id)]
                cmd = (
                    "srun "
                    + "--nodelist=" + metadata_server_addr + " "
                    + "--comment=" + key_string + " "
                    + command_options + " "
                    + openfam_install_path
                    + "/bin/metadata_server -a "
                    + metadata_server_addr
                    + " -r "
                    + str(metadata_server_rpc_port)
                    + " &"
                )
            else:
                cmd = (
                    openfam_install_path
                    + "/bin/metadata_server -a "
                    + metadata_server_addr
                    + " -r "
                    + str(metadata_server_rpc_port)
                    + " &"
                )
            i = i + 1
            os.system(cmd)
            service_info.append(['metadata service', metadata_server_id,
                                 metadata_server_addr, metadata_server_rpc_port])
    time.sleep(sleepsecs)

    # Start CIS
    if (
        pe_config_doc["openfam_model"] == "memory_server"
        and pe_config_doc["client_interface_type"] == "rpc"
    ):
        cis_addr = cis_config_doc["rpc_interface"].split(":")[0]
        cis_rpc_port = cis_config_doc["rpc_interface"].split(":")[1]
        key_string="openfam:cis:"+cis_config_doc["rpc_interface"]
        if openfam_admin_tool_config_doc["launcher"] == "ssh":
            cmd = (
                ssh_cmd
                + cis_addr
                + " \"sh -c '"
                + "nohup "
                + openfam_install_path
                + "/bin/cis_server -a "
                + cis_addr
                + " -r "
                + str(cis_rpc_port)
                + "> /dev/null 2>&1 &'\""
            )
        elif openfam_admin_tool_config_doc["launcher"] == "slurm":
            cmd = (
                "srun "
                + "--nodelist=" + cis_addr + " "
                + "--comment=" + key_string + " "
                + openfam_admin_tool_config_doc["launcher_options"]["common"] + " "
                + openfam_admin_tool_config_doc["launcher_options"]["cis"] + " "
                + openfam_install_path
                + "/bin/cis_server -a "
                + cis_addr
                + " -r "
                + str(cis_rpc_port)
                + " &"
            )
        else:
            cmd = (
                openfam_install_path
                + "/bin/cis_server -a "
                + cis_addr
                + " -r "
                + str(cis_rpc_port)
                + " &"
            )
        os.system(cmd)
        service_info.append(['CIS', 0, cis_addr, cis_rpc_port])
    time.sleep(sleepsecs)
    print("----------------------------")
    print("Details of OpenFAM Services:")
    print("----------------------------")
    print (tabulate(service_info, headers=[
           "Service", "Id", "Host", "RPC Port", "Status"]))


if args.runtests:
    openfam_install_path = get_install_path()
    npe = openfam_admin_tool_config_doc["num_pes"]
    # set environment variable for test command a
    npe = openfam_admin_tool_config_doc["num_pes"]
    if openfam_admin_tool_config_doc["launcher"] == "" or openfam_admin_tool_config_doc["launcher"] == "ssh":
        if pe_config_doc["runtime"] == "NONE":
            os.environ["OPENFAM_TEST_COMMAND"] = ""
            os.environ["OPENFAM_TEST_OPT"] = ""
        else:
            os.environ["OPENFAM_TEST_COMMAND"] = openfam_install_path + \
                "/../third-party/build/bin/mpirun"
            os.environ["OPENFAM_TEST_OPT"] = (
                "-n "
                + str(npe) + " "
                + openfam_admin_tool_config_doc["launcher_options"]["compute_pes"]
                + " "
                + openfam_admin_tool_config_doc["launcher_options"]["common"]
                + " -x OPENFAM_ROOT="
                + openfam_config_path
            )
    else:
        os.environ["OPENFAM_TEST_COMMAND"] = "srun"
        os.environ["OPENFAM_TEST_OPT"] = (
            openfam_admin_tool_config_doc["launcher_options"]["compute_pes"]
            + " "
            + openfam_admin_tool_config_doc["launcher_options"]["common"]
            + " -n "
            + str(npe)
        )

    # Run regression and unit tests
    cmd = "cd " + openfam_install_path + "; " + " make reg-test"
    result = os.system(cmd)
    if (result >> 8) != 0:
        error_count = error_count + 1
        print('\033[1;31;40mERROR['+str(error_count) +
              ']: Regression test failed \033[0;37;40m')
        sys.exit(1)
    cmd = "cd " + openfam_install_path + "; " + " make unit-test"
    result = os.system(cmd)
    if (result >> 8) != 0:
        error_count = error_count + 1
        print('\033[1;31;40mERROR['+str(error_count) +
              ']: Unit test failed \033[0;37;40m')
        sys.exit(1)

# Terminate all services
if args.stop_service:
    if (
        pe_config_doc["openfam_model"] == "memory_server"
        and cis_config_doc["memsrv_interface_type"] == "rpc"
    ):
        if openfam_admin_tool_config_doc["launcher"] == "ssh":
            for server in cis_config_doc["memsrv_list"]:
                memory_server_addr = server.split(":")[1]
                memory_server_rpc_port = server.split(":")[2]
                cmd = ssh_cmd + memory_server_addr + " \"sh -c 'pkill -9 memory_server'\""
                os.system(cmd)
        elif openfam_admin_tool_config_doc["launcher"] == "slurm":
            user = os.environ["USER"]
            for server in cis_config_doc["memsrv_list"]:
                memory_server_id = server.split(":")[0]
                memory_server_addr = server.split(":")[1]
                key_string="openfam:mem:"+server
                cmd="squeue --me -O JobId,Command,Comment | grep " + key_string + " | cut -d' ' -f1"
                out=subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).stdout;
                job_id=out.read().decode()
                cmd="scancel " + "--user=" + user + " --nodelist=" + memory_server_addr + " --quiet " + str(job_id) +" > /dev/null 2>&1"
                os.system(cmd)
        else:
            cmd = "pkill -9 memory_server"
            os.system(cmd)
    if (
        pe_config_doc["openfam_model"] == "memory_server"
        and cis_config_doc["metadata_interface_type"] == "rpc"
    ):
        if openfam_admin_tool_config_doc["launcher"] == "ssh":
            for server in cis_config_doc["metadata_list"]:
                metadata_server_addr = server.split(":")[1]
                metadata_server_rpc_port = server.split(":")[2]
                cmd = (
                    ssh_cmd
                    + metadata_server_addr
                    + " \"sh -c 'pkill -9 metadata_server'\""
                )
                os.system(cmd)
        elif openfam_admin_tool_config_doc["launcher"] == "slurm":
            user = os.environ["USER"]
            for server in cis_config_doc["metadata_list"]:
                metadata_server_addr = server.split(":")[1]
                key_string="openfam:meta:"+server
                cmd="squeue --me -O JobId,Command,Comment | grep " + key_string + " | cut -d' ' -f1"
                out=subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).stdout;
                job_id=out.read().decode()
                cmd="scancel " + "--user=" + user + " --nodelist=" + metadata_server_addr + " --quiet " + str(job_id) +" > /dev/null 2>&1"
                os.system(cmd)
        else:
            cmd = "pkill -9 metadata_server"
            os.system(cmd)
    if (
        pe_config_doc["openfam_model"] == "memory_server"
        and pe_config_doc["client_interface_type"] == "rpc"
    ):
        if openfam_admin_tool_config_doc["launcher"] == "ssh":
            cis_addr = cis_config_doc["rpc_interface"].split(":")[0]
            cmd = ssh_cmd + cis_addr + " \"sh -c 'pkill -9 cis_server'\""
        elif openfam_admin_tool_config_doc["launcher"] == "slurm":
            user = os.environ["USER"]
            cis_addr = cis_config_doc["rpc_interface"].split(":")[0]
            key_string="openfam:cis:"+cis_config_doc["rpc_interface"]
            cmd="squeue --me -O JobId,Command,Comment | grep " + key_string + " | cut -d' ' -f1"
            out=subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).stdout;
            job_id=out.read().decode()
            cmd="scancel " + "--user=" + user + " --nodelist=" + cis_addr + " --quiet " + str(job_id) +" > /dev/null 2>&1"
            os.system(cmd)
        else:
            cmd = "pkill -9 cis_server"
        os.system(cmd)
    time.sleep(sleepsecs)  # sufficient time to terminate the services
    print("All OpenFAM services are terminated")

# Clean FAM and Metadata
if args.clean:
    if(pe_config_doc["openfam_model"] == "shared_memory"):
        error_count = error_count + 1
        print('\033[1;31;40mERROR['+str(error_count)+']: Shared memory model does not support --clean option. Please clean FAM and metadata manually\033[0;37;40m')
        sys.exit(1)

    if(cis_config_doc["memsrv_interface_type"] == "direct" and pe_config_doc["client_interface_type"] == "rpc"):
        addr = cis_config_doc["rpc_interface"].split(':')[0]
        path = memservice_config_doc["Memservers"][0]['fam_path']
        if path == "" or path == "/":
            error_count = error_count + 1
            print(
                '\033[1;31;40mERROR['+str(error_count)+']: Please check the fam path, The provided fam path for cleanup does not seems to be correct\033[0;37;40m')
            sys.exit(1)

        if openfam_admin_tool_config_doc["launcher"] == "ssh":
            cmd = ssh_cmd + addr + " \"sh -c 'rm -rf " + path + "'\""
        elif openfam_admin_tool_config_doc["launcher"] == "slurm":
            command_options = openfam_admin_tool_config_doc["launcher_options"]["common"]
            if len(openfam_admin_tool_config_doc["launcher_options"]["cis"]):
                command_options = command_options + " " + \
                    openfam_admin_tool_config_doc["launcher_options"]["cis"]
            cmd = "srun --nodelist=" + addr + " " + command_options + " rm -rf " + path
        else:
            cmd = "rm -rf " + path
        os.system(cmd)

    elif(cis_config_doc["metadata_interface_type"] == "direct" and pe_config_doc["client_interface_type"] == "rpc"):
        addr = cis_config_doc["rpc_interface"].split(':')[0]
        path = metaservice_config_doc["metadata_path"]
        if path == "" or path == "/":
            error_count = error_count + 1
            print(
                '\033[1;31;40mERROR['+str(error_count)+']: Please check the metadata path, The provided metadata path for cleanup does not seems to be correct\033[0;37;40m')
            sys.exit(1)

        if openfam_admin_tool_config_doc["launcher"] == "ssh":
            cmd = ssh_cmd + addr + " \"sh -c 'rm -rf " + path + "'\""
        elif openfam_admin_tool_config_doc["launcher"] == "slurm":
            command_options = openfam_admin_tool_config_doc["launcher_options"]["common"]
            if len(openfam_admin_tool_config_doc["launcher_options"]["cis"]):
                command_options = command_options + " " + \
                    openfam_admin_tool_config_doc["launcher_options"]["cis"]
            cmd = "srun --nodelist=" + addr + " " + command_options + " rm -rf " + path
        else:
            cmd = "rm -rf " + path
        os.system(cmd)

    if(cis_config_doc["memsrv_interface_type"] == "rpc"):
        for server in memservice_config_doc["Memservers"]:
            addr = memservice_config_doc["Memservers"][server]['rpc_interface'].split(':')[
                0]
            path = memservice_config_doc["Memservers"][server]['fam_path']
            if path == "" or path == "/":
                error_count = error_count + 1
                print(
                    '\033[1;31;40mERROR['+str(error_count)+']: Please check the fam path, The provided fam path for cleanup seems to be not correct\033[0;37;40m')
                sys.exit(1)
            if openfam_admin_tool_config_doc["launcher"] == "ssh":
                cmd = ssh_cmd + addr + " \"sh -c 'rm -rf " + path + "'\""
            elif openfam_admin_tool_config_doc["launcher"] == "slurm":
                command_options = openfam_admin_tool_config_doc["launcher_options"]["common"]
                if len(openfam_admin_tool_config_doc["launcher_options"]["memserver"]):
                    command_options = command_options + " " + \
                        openfam_admin_tool_config_doc["launcher_options"]["memserver"][server]
                cmd = "srun --nodelist=" + addr + " " + command_options + " rm -rf " + path
            else:
                cmd = "rm -rf " + path
            os.system(cmd)

    if(cis_config_doc["metadata_interface_type"] == "rpc"):
        for server in cis_config_doc["metadata_list"]:
            metaserver_id = server.split(":")[0]
            metadata_server_addr = server.split(":")[1]
            if metaservice_config_doc["metadata_path"] == "" or metaservice_config_doc["metadata_path"] == "/":
                error_count = error_count + 1
                print('\033[1;31;40mERROR['+str(error_count) +
                      ']: Please check the metadata path, The provided metadata path for cleanup seems to be not correct\033[0;37;40m')
                sys.exit(1)
            if openfam_admin_tool_config_doc["launcher"] == "ssh":
                cmd = ssh_cmd + metadata_server_addr + " \"sh -c 'rm -rf " + \
                    metaservice_config_doc["metadata_path"] + "'\""
            elif openfam_admin_tool_config_doc["launcher"] == "slurm":
                command_options = openfam_admin_tool_config_doc["launcher_options"]["common"]
                if len(openfam_admin_tool_config_doc["launcher_options"]["metaserver"]):
                    command_options = command_options + " " + \
                        openfam_admin_tool_config_doc["launcher_options"]["metaserver"][int(
                            metaserver_id)]
                cmd = "srun --nodelist=" + metadata_server_addr + " " + command_options + \
                    " rm -rf " + metaservice_config_doc["metadata_path"]
            else:
                cmd = "rm -rf " + metaservice_config_doc["metadata_path"]
            os.system(cmd)
    print("All FAM and Metadata are cleared")

