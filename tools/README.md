# Openfam Admin Tool
**openfam_adm** is an utility tool which helps to perform the basic tasks useful to an OpenFAM user such as creating configuration files, starting OpenFAM services, terminating OpenFAM services, running tests and cleaning up FAM and metadata. The detailed description and available options are given below.

## Description
```
usage: openfam_adm.py [-h] [--create_config_files] [--start_service] [--stop_service] [--clean] [--install_path] [--config_file_path] [--launcher]
                      [--global_launcher_options] [--cisinterface] [--memserverinterface] [--metaserverinterface]
                      [--cisserver  {comma seperated attributes}] [--memservers  id:{comma seperated attributes}]
                      [--metaservers  id:{comma seperated attributes}] [--test_args  {comma seperated attributes}] [--model] [--thrdmodel] [--provider]
                      [--runtime] [--kvstype] [--metapath] [--fampath] [--fam_backup_path] [--atlthreads] [--atlqsize] [--atldatasize]
                      [--disableregionspanning] [--regionspanningsize] [--interleaveblocksize] [--runtests] [--sleep] [--delayed_free_threads]
                      [--rpc_framework] [--disable_resource_release]

OpenFAM utility tool

optional arguments:
  -h, --help            show this help message and exit
  --create_config_files
                        Generates all configuration files
  --start_service       Start all OpenFAM services
  --stop_service        Stop all OpenFAM services
  --clean               Clean all FAMs and Metadata
  --install_path        Path to directory where OpenFAM is installed
  --config_file_path    Path to create configuration files
  --launcher            Launcher to be used to launch processes(slurm/ssh). When set to "ssh", services are launched using ssh on remote nodes and tests
                        are launched using mpi and when set to "slurm" both services and tests are launched using slurm. Note : If this option is not set,
                        by default all sevices and programs run locally.
  --global_launcher_options
                        Launcher command options which can be used commonly across all services. Specify all options within quotes(") eg :
                        --global_launcher_options="-N" "1" "--mpi=pmix"
  --cisinterface        CIS interface type(rpc/direct)
  --memserverinterface
                        Memory Service interface type(rpc/direct)
  --metaserverinterface
                        Metadata Service interface type(rpc/direct)
  --cisserver  {comma seperated attributes}
                        CIS parameters, eg : ,0:{rpc_interface:127.0.0.1,rpc_port:8795,launcher_options: "-n" "1"}. Note : Specify all launcher options
                        within quotes(") eg : launcher_options:"-N" "1" "--mpi=pmix"
  --memservers  id:{comma seperated attributes}
                        Add a new memory server, eg : 0:{memory_type:volatile,fam_path:/dev/shm/vol/,rpc_interface:127.0.0.1,rpc_port:8795,libfabric_port:7
                        500,if_device:eth0,launcher_options: '-n' '1'}. Note : Specify all launcher options within quotes(") eg : launcher_options: "-N"
                        "1" "--mpi=pmix"
  --metaservers  id:{comma seperated attributes}
                        Add a metadata server, eg : 0:{rpc_interface:127.0.0.1,rpc_port:8795,launcher_options: '-n' '1'}. Note : Specify all launcher
                        options within quotes(") eg : launcher_options: "-N" "1" "--mpi=pmix"
  --test_args  {comma seperated attributes}
                        Test configuration parameters, eg : {num_pes:1,launcher_options: '-n' '1'}. Note : Specify all launcher options within quotes(") eg
                        : launcher_options: "-N" "1" "--mpi=pmix"
  --model               OpenFAM model(memory_server/shared_memory)
  --thrdmodel           Thread model(single/multiple)
  --provider            Libfabric provider to be used for datapath operations
  --runtime             Runtime process management interface(PMI) to be used
  --kvstype             Type of key value store to be used for metadata management
  --metapath            path to store metadata
  --fampath             path where data is stored
  --fam_backup_path     path where data backup is stored(on shared filesystem)
  --atlthreads          Atomic library threads count
  --atlqsize            ATL queue size
  --atldatasize         ATL data size per thread(MiB)
  --disableregionspanning
                        Disable region spanning
  --regionspanningsize
                        Region spanning size
  --interleaveblocksize
                        Dataitem interleave block size
  --runtests            Run regression and unit tests
  --sleep               Number of seconds to sleep
  --delayed_free_threads
                        Delayed free threads
  --rpc_framework       RPC Framework type(grpc/thallium)
  --disable_resource_release
                        Disable the resource relinquishment in FAM

```
## Generate Configuration Files

  ```
  $openfam_adm @arg_file.txt --config_file_path=/path/to/generate/config/files --install_path=/path/to/openfam/install/dir
  ```

  In arg_file.txt, arguments has to be mentioned one per line, for example:

  arg_file.txt
  ```
  --launcher=slurm
  --model=memory_server
  --cisinterface=rpc
  --memserverinterface=rpc
  --metaserverinterface=rpc
  --cisserver={rpc_interface:127.0.0.1,rpc_port:8787}
  --memservers=0:{memory_type:volatile,fam_path:/dev/shm/vol,rpc_interface:127.0.0.1,rpc_port:8793,libfabric_port:7500,if_device:eth0}
  --metaservers=0:{rpc_interface:127.0.0.1,rpc_port:8788}

  ```

## Start/Stop OpenFAM Services

  ```
  $openfam_adm --start_service --config_file_path=/path/to/config/files
  ```

  Alternatively

  ```
  $export OPENFAM_ROOT=/path/to/generate/config/files

  $openfam_adm --start_service
  ```

  Following commands can be used to stop service

  ```
  $openfam_adm --stop_service --config_file_path=/path/to/config/files
  ```

  Alternatively

  ```
  $export OPENFAM_ROOT=/path/to/config/files

  $openfam_adm --stop_service
  ```

## Cleanup

  FAM and metadata can be cleaned up using following command

  ```
  $openfam_adm --clean --config_file_path=/path/to/config/files
  ```

  Or

  ```
  $export OPENFAM_ROOT=/path/to/generate/config/files
  $openfam_adm --clean
  ```
  Note : While generating the configuration files, if "--install_path" argument is specified, it stores that in openfam_admin_tool.yaml. Subsequent commands will use install path from openfam_admin_tool.yaml. This can be overridden  by OPENFAM_INSTALL_DIR variable. And both, OPENFAM_INSTALL_DIR value and install path from openfam_admin_tool.yaml can be overridden by specifying the option "--install_path" in start, stop or clean-up commands.

