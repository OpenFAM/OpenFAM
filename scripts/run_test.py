import os
import sys
import argparse
import ruamel.yaml
import time
import math
from pathlib import Path

# Create the parser
my_parser = argparse.ArgumentParser(
    fromfile_prefix_chars="@", description="Generate the config files"
)

# Add the arguments
my_parser.add_argument(
    "rootpath", action="store", type=str, help="path to OpenFAM root dir"
)

my_parser.add_argument(
    "outpath",
    action="store",
    type=str,
    help="path where generated output config files to be stored",
)


my_parser.add_argument("buildpath", action="store",
                       type=str, help="OpenFAM build path")

my_parser.add_argument("-n", action="store", type=int, help="Number of PEs")

my_parser.add_argument(
    "--pehosts", action="store", type=str, help="List of nodes where PEs has to run"
)

my_parser.add_argument(
    "--launcher",
    action="store",
    type=str,
    help="Launcher to be used to launch processes(mpi/slurm)",
    choices=["mpi", "slurm"],
)

my_parser.add_argument(
    "--model",
    action="store",
    type=str,
    help="OpenFAM model(memory_server/shared_memory)",
    choices=["memory_server", "shared_memory"],
)

my_parser.add_argument(
    "--thrdmodel",
    action="store",
    type=str,
    help="Thread model(single/multiple)",
    choices=["single", "multiple"],
)

my_parser.add_argument(
    "--provider",
    action="store",
    type=str,
    help="Libfabric provider to be used for datapath operations",
    choices=["sockets", "psm2", "verbs;ofi_rxm"],
)

my_parser.add_argument(
    "--runtime",
    action="store",
    type=str,
    help="Runtime process management interface(PMI) to be used",
    choices=["PMIX", "PMI2"],
)

my_parser.add_argument(
    "--cisinterface",
    action="store",
    type=str,
    help="CIS interface type(rpc/direct)",
    choices=["rpc", "direct"],
)

my_parser.add_argument(
    "--cisrpcaddr",
    action="store",
    type=str,
    help="RPC address and port to be used for CIS",
)

my_parser.add_argument(
    "--memserverinterface",
    action="store",
    type=str,
    help="Memory Service interface type(rpc/direct)",
    choices=["rpc", "direct"],
)

my_parser.add_argument(
    "--metaserverinterface",
    action="store",
    type=str,
    help="Metadata Service interface type(rpc/direct)",
    choices=["rpc", "direct"],
)

my_parser.add_argument(
    "--memserverlist",
    action="store",
    type=str,
    help="Memory server address list(comma seperated) eg : 0:0.0.0.0:1234,1:1.1.1.1:5678",
)

my_parser.add_argument(
    "--memservers",
    action="store",
    type=str,
    help="Memory server attributes list(comma seperated) eg : 0:{memory_type:volatile,fam_path:/dev/shm/vol/,rpc_interface:127.0.0.1,rpc_port:8795,libfabric_port:7500,if_device:eth0};1:{memory_type:persistent,fam_path:/dev/shm/per/,rpc_interface:127.0.0.1,rpc_port:8796,libfabric_port:7501,if_device:eth1}",
)

my_parser.add_argument(
    "--metaserverlist",
    action="store",
    type=str,
    help="Metadata Server list(comma seperated) eg : 0:0.0.0.0:1234,1:1.1.1.1:5678",
)

my_parser.add_argument(
    "--kvstype",
    action="store",
    type=str,
    help="Type of key value store to be used for metadata management",
)

my_parser.add_argument(
    "--libfabricport",
    action="store",
    type=str,
    help="Libafabric port for datapath operation",
)

my_parser.add_argument(
    "--metapath", action="store", type=str, help="path to store metadata"
)

my_parser.add_argument(
    "--fampath", action="store", type=str, help="path where data is stored"
)

my_parser.add_argument(
    "--fam_backup_path", action="store", type=str, help="path where data backup is stored(on shared filesystem)"
)


my_parser.add_argument(
    "--atlthreads", action="store", type=str, help="Atomic library threads count"
)

my_parser.add_argument("--atlqsize", action="store",
                       type=str, help="ATL queue size")

my_parser.add_argument(
    "--atldatasize", action="store", type=str, help="TL data size per thread(MiB)"
)

my_parser.add_argument(
    "--disableregionspanning",
    default=False,
    action="store_true",
    help="Disable region spanning",
)

my_parser.add_argument(
    "--regionspanningsize", action="store", type=str, help="Region spanning size"
)

my_parser.add_argument(
    "--runtests",
    default=False,
    action="store_true",
    help="Run regression and unit tests"
)

my_parser.add_argument(
    "--mpi_type",
    default="pmix_v2",
    type=str,
    action="store",
    help="Slurm mpi type"
)

my_parser.add_argument(
    "--partition",
    type=str,
    action="store",
    help="Slurm partition"
)

my_parser.add_argument(
    "--sleep",
    default=5,
    type=int,
    action="store",
    help="Number of seconds to sleep"
)

my_parser.add_argument(
    "--yamlonly",
    default=False,
    action="store_true",
    help="Generate yaml files then exit"
)

args = my_parser.parse_args()

# Open all config file templates
with open(args.rootpath + "/config/fam_pe_config.yaml") as pe_config_infile:
    pe_config_doc = ruamel.yaml.load(
        pe_config_infile, ruamel.yaml.RoundTripLoader)
with open(
    args.rootpath + "/config/fam_client_interface_config.yaml"
) as cis_config_infile:
    cis_config_doc = ruamel.yaml.load(
        cis_config_infile, ruamel.yaml.RoundTripLoader)
with open(
    args.rootpath + "/config/fam_memoryserver_config.yaml"
) as memservice_config_infile:
    memservice_config_doc = ruamel.yaml.load(
        memservice_config_infile, ruamel.yaml.RoundTripLoader
    )
with open(
    args.rootpath + "/config/fam_metadata_config.yaml"
) as metaservice_config_infile:
    metaservice_config_doc = ruamel.yaml.load(
        metaservice_config_infile, ruamel.yaml.RoundTripLoader
    )
# Assign values to config options if corresponding argument is set
sleepsecs = args.sleep
mpitype = args.mpi_type
if args.partition is not None:
    slurm_partition = " -p " + args.partition
else:
    slurm_partition = ""
if args.n is not None:
    npe = args.n
else:
    npe = 1
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
if args.cisrpcaddr is not None:
    pe_config_doc["fam_client_interface_service_address"] = args.cisrpcaddr
    cis_config_doc["rpc_interface"] = args.cisrpcaddr
if args.memserverinterface is not None:
    cis_config_doc["memsrv_interface_type"] = args.memserverinterface
if args.metaserverinterface is not None:
    cis_config_doc["metadata_interface_type"] = args.metaserverinterface
if args.metaserverlist is not None:
    cis_config_doc["metadata_list"] = args.metaserverlist.split(",")
if args.kvstype is not None:
    metaservice_config_doc["metadata_manager"] = args.kvstype
if args.metapath is not None:
    metaservice_config_doc["metadata_path"] = args.metapath
if args.fam_backup_path is not None:
    memservice_config_doc["fam_backup_path"] = args.fam_backup_path
    print(args.fam_backup_path)
else:
    memservice_config_doc["fam_backup_path"] = args.outpath + "/backup/"
if args.atlthreads is not None:
    memservice_config_doc["ATL_threads"] = args.atlthreads
if args.atlqsize is not None:
    memservice_config_doc["ATL_queue_size"] = args.atlqsize
if args.atldatasize is not None:
    memservice_config_doc["ATL_data_size"] = args.atldatasize
memserver_map = {}
memserver_id_list = []
if args.memservers is not None:
    memservers_list=args.memservers.split(";")
    for m in memservers_list:
        m_id=int(m.split('{')[0].split(':')[0])
        m_value=m.split('{')[1].split('}')[0]
        value_list=m_value.split(',')
        value_map={}
        for value in value_list:
            v=value.split(':')
            if v[1].isnumeric():
               v[1]=int(v[1])
            if v[0]=='rpc_port':
               rpc_interface=value_map['rpc_interface']
               value_map['rpc_interface']=rpc_interface+':'+str(v[1])
               memserver_id_list.append(str(m_id)+':'+rpc_interface+':'+str(v[1]))
            else:
               value_map[v[0]]=v[1]
        memserver_map[m_id]=value_map

    memservice_config_doc["Memservers"]=memserver_map
    cis_config_doc["memsrv_list"]=memserver_id_list

if args.disableregionspanning:
    print("region spanning disabled")
    memservice_config_doc["enable_region_spanning"] = "false"
else:
    memservice_config_doc["enable_region_spanning"] = "true"
if args.regionspanningsize is not None:
    memservice_config_doc["region_span_size_per_memoryserver"] = (
        args.regionspanningsize
    )
cmd = "mkdir -p " + args.outpath + "/config"
os.system(cmd)

# set environment variable for test command and options
if args.launcher is None or args.launcher == "mpi":
    os.environ["OPENFAM_TEST_COMMAND"] = args.rootpath + \
        "/third-party/build/bin/mpirun"
    if args.pehosts is not None:
        if len(args.pehosts.split(",")) < npe:
            pe_per_host = npe
        else:
            pe_per_host = math.ceil(len(args.pehosts.split(",")) / npe)
        print(pe_per_host)
        host_list = ""
        for host in args.pehosts.split(","):
            host_list = host_list + host + ":" + str(pe_per_host) + ","
        print(host_list)
        os.environ["OPENFAM_TEST_OPT"] = (
            "-n "
            + str(npe)
            + " -host "
            + host_list
            + " -x OPENFAM_ROOT="
            + args.outpath
        )
    else:
        os.environ["OPENFAM_TEST_OPT"] = "-n " + str(npe)
else:
    if args.pehosts is None:
        print("ERROR: --pehosts option is required when slurm is used as launcher")
        sys.exit(1)
    os.environ["OPENFAM_TEST_COMMAND"] = "srun"
    os.environ["OPENFAM_TEST_OPT"] = (
        "-N "
        + str(len(args.pehosts.split(",")))
        + " -n "
        + str(npe)
        + slurm_partition
        + " --nodelist="
        + args.pehosts
        + " --mpi=" + mpitype
    )
# Check if user has provided any service interface as "rpc" when openfam model is shared_memory
if pe_config_doc["openfam_model"] == "shared_memory":
    if "rpc" in [
        str(pe_config_doc["client_interface_type"]),
        str(cis_config_doc["memsrv_interface_type"]),
        str(cis_config_doc["metadata_interface_type"]),
    ]:
        print(
            '\033[1;31;40mERROR: In shared memory model interface type of all services has to be direct, one or more service interfcae is mentioned as "rpc" \033[0;37;40m'
        )
        sys.exit(1)
else:
    if "rpc" not in [
        str(pe_config_doc["client_interface_type"]),
        str(cis_config_doc["memsrv_interface_type"]),
        str(cis_config_doc["metadata_interface_type"]),
    ]:
        print(
            '\033[1;31;40mERROR: In memory server model interface type of all services can not to be "direct", change model to "shared memory" \033[0;37;40m'
        )
        sys.exit(1)
# Write to output config files
with open(args.outpath + "/config/fam_pe_config.yaml", "w") as pe_config_outfile:
    ruamel.yaml.dump(
        pe_config_doc, pe_config_outfile, Dumper=ruamel.yaml.RoundTripDumper
    )
with open(
    args.outpath + "/config/fam_client_interface_config.yaml", "w"
) as cis_config_outfile:
    ruamel.yaml.dump(
        cis_config_doc, cis_config_outfile, Dumper=ruamel.yaml.RoundTripDumper
    )
with open(
    args.outpath + "/config/fam_memoryserver_config.yaml", "w"
) as memservice_config_outfile:
    ruamel.yaml.dump(
        memservice_config_doc,
        memservice_config_outfile,
        Dumper=ruamel.yaml.RoundTripDumper,
    )
with open(
    args.outpath + "/config/fam_metadata_config.yaml", "w"
) as metaservice_config_outfile:
    ruamel.yaml.dump(
        metaservice_config_doc,
        metaservice_config_outfile,
        Dumper=ruamel.yaml.RoundTripDumper,
    )

if args.yamlonly:
   sys.exit()

# Set OPENFAM_ROOT environment variable
os.environ["OPENFAM_ROOT"] = args.outpath
env_cmd = (
    "export OPENFAM_ROOT="
    + args.outpath
    + ";"
    + "export PATH="
    + args.rootpath
    + "/third-party/build/bin/:$PATH;export LD_LIBRARY_PATH="
    + args.rootpath
    + "/third-party/build/lib:$LD_LIBRARY_PATH;"
)
os.system(env_cmd)

ssh_cmd = "ssh " + os.environ["USER"] + "@"
srun_cmd = "srun -N 1 " + slurm_partition + " --nodelist="

# start all memory services
if (
    pe_config_doc["openfam_model"] == "memory_server"
    and cis_config_doc["memsrv_interface_type"] == "rpc"
):
    # Iterate over list of memory servers
    i = 0
    for server in cis_config_doc["memsrv_list"]:
        memory_server_id = server.split(":")[0]
        if args.launcher == "mpi":
            cmd = (
                ssh_cmd
                + memory_server_addr
                + " \"sh -c '"
                + env_cmd
                + "nohup "
                + args.buildpath
                + "/src/memory_server -m "
                + memory_server_id
                + "> /dev/null 2>&1 &'\""
            )
        elif args.launcher == "slurm":
            cmd = (
                srun_cmd
                + memory_server_addr
                + " --mpi=" + mpitype
                + args.buildpath
                + "/src/memory_server -m "
                + memory_server_id
                + " &"
            )
        else:
            cmd = (
                env_cmd
                + args.buildpath
                + "/src/memory_server -m "
                + memory_server_id
                + " &"
            )
            i = i + 1
        os.system(cmd)
# start all metadata services
if (
    pe_config_doc["openfam_model"] == "memory_server"
    and cis_config_doc["metadata_interface_type"] == "rpc"
):
    # Iterate over list of metadata servers
    for server in cis_config_doc["metadata_list"]:
        metadata_server_addr = server.split(":")[1]
        metadata_server_rpc_port = server.split(":")[2]
        if args.launcher == "mpi":
            cmd = (
                ssh_cmd
                + metadata_server_addr
                + " \"sh -c '"
                + env_cmd
                + "nohup "
                + args.buildpath
                + "/src/metadata_server -a "
                + metadata_server_addr
                + " -r "
                + str(metadata_server_rpc_port)
                + "> /dev/null 2>&1 &'\""
            )
        elif args.launcher == "slurm":
            cmd = (
                srun_cmd
                + metadata_server_addr
                + " --mpi=" + mpitype
                + args.buildpath
                + "/src/metadata_server -a "
                + metadata_server_addr
                + " -r "
                + str(metadata_server_rpc_port)
                + " &"
            )
        else:
            cmd = (
                env_cmd
                + args.buildpath
                + "/src/metadata_server -a "
                + metadata_server_addr
                + " -r "
                + str(metadata_server_rpc_port)
                + " &"
            )
        os.system(cmd)
time.sleep(sleepsecs)

# Start CIS
if (
    pe_config_doc["openfam_model"] == "memory_server"
    and pe_config_doc["client_interface_type"] == "rpc"
):
    cis_addr = cis_config_doc["rpc_interface"].split(":")[0]
    cis_rpc_port = cis_config_doc["rpc_interface"].split(":")[1]

    if args.launcher == "mpi":
        cmd = (
            ssh_cmd
            + cis_addr
            + " \"sh -c '"
            + env_cmd
            + "nohup "
            + args.buildpath
            + "/src/cis_server -a "
            + cis_addr
            + " -r "
            + str(cis_rpc_port)
            + "> /dev/null 2>&1 &'\""
        )
    elif args.launcher == "slurm":
        cmd = (
            srun_cmd
            + cis_addr
            + " --mpi=" + mpitype
            + args.buildpath
            + "/src/cis_server -a "
            + cis_addr
            + " -r "
            + str(cis_rpc_port)
            + " &"
        )
    else:
        cmd = (
            env_cmd
            + args.buildpath
            + "/src/cis_server -a "
            + cis_addr
            + " -r "
            + str(cis_rpc_port)
            + " &"
        )
    os.system(cmd)
time.sleep(sleepsecs)

# Terminate all services


def terminate_services():
    if (
        pe_config_doc["openfam_model"] == "memory_server"
        and cis_config_doc["memsrv_interface_type"] == "rpc"
    ):
        if args.launcher == "mpi":
            for server in cis_config_doc["memsrv_list"]:
                memory_server_addr = server.split(":")[1]
                memory_server_rpc_port = server.split(":")[2]
                cmd = ssh_cmd + memory_server_addr + " \"sh -c 'pkill memory_server'\""
        elif args.launcher == "slurm":
            cmd = "scancel --quiet -n memory_server > /dev/null 2>&1"
        else:
            cmd = "pkill -9 memory_server"
        os.system(cmd)
    if (
        pe_config_doc["openfam_model"] == "memory_server"
        and cis_config_doc["metadata_interface_type"] == "rpc"
    ):
        if args.launcher == "mpi":
            for server in cis_config_doc["metadata_list"]:
                metadata_server_addr = server.split(":")[1]
                metadata_server_rpc_port = server.split(":")[2]
                cmd = (
                    ssh_cmd
                    + metadata_server_addr
                    + " \"sh -c 'pkill metadata_server'\""
                )
        elif args.launcher == "slurm":
            cmd = "scancel --quiet -n metadata_server > /dev/null 2>&1"
        else:
            cmd = "pkill -9 metadata_server"
        os.system(cmd)
    if (
        pe_config_doc["openfam_model"] == "memory_server"
        and pe_config_doc["client_interface_type"] == "rpc"
    ):
        if args.launcher == "mpi":
            cmd = ssh_cmd + cis_addr + " \"sh -c 'pkill cis_server'\""
        elif args.launcher == "slurm":
            cmd = "scancel --quiet -n cis_server > /dev/null 2>&1"
        else:
            cmd = "pkill -9 cis_server"
        os.system(cmd)
    time.sleep(sleepsecs)  # sufficient time to terminate the services

if args.runtests:
    # Run regression and unit tests
    cmd = "cd " + args.buildpath + "; " + env_cmd + " make reg-test"
    result = os.system(cmd)
    if (result >> 8) != 0:
        print("\033[1;31;40mERROR: Regression test failed \033[0;37;40m")
        terminate_services()
        sys.exit(1)
    cmd = "cd " + args.buildpath + "; " + env_cmd + " make unit-test"
    result = os.system(cmd)
    if (result >> 8) != 0:
        print("\033[1;31;40mERROR: Unit test failed \033[0;37;40m")
        terminate_services()
        sys.exit(1)
    terminate_services()

