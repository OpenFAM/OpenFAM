# run_test.py
This script generates config files for given configuration parameters and starts services accordingly and run both reggression and unit tests.

## Description
```
usage: run_test.py [-h] [-n N] [--pehosts PEHOSTS] [--launcher {mpi,slurm}]
                   [--model {memory_server,shared_memory}]
                   [--thrdmodel {single,multiple}] [--provider PROVIDER]
                   [--runtime {PMIX,PMI2}] [--cisinterface {rpc,direct}]
                   [--cisrpcaddr CISRPCADDR]
                   [--memserverinterface {rpc,direct}]
                   [--metaserverinterface {rpc,direct}]
                   [--memserverlist MEMSERVERLIST]
                   [--metaserverlist METASERVERLIST] [--kvstype KVSTYPE]
                   [--libfabricport LIBFABRICPORT] [--metapath METAPATH]
                   [--fampath FAMPATH] [--atlthreads ATLTHREADS]
                   [--atlqsize ATLQSIZE] [--atldatasize ATLDATASIZE]
                   [--disableregionspanning]
                   [--regionspanningsize REGIONSPANNINGSIZE] [--runtests]
		   [--fam_backup_path backup_path
                   rootpath outpath buildpath
```
positional arguments:

| Arguments |  Description   |
| :---  | :---  | 
|  rootpath              |	path to OpenFAM root dir |
|  outpath               |	path where generated output config files to be stored |
|  buildpath             |	OpenFAM build path |
	
optional arguments:	

| Options  	| Description  	|
| :---	| :---	|
|    -h, --help            				 	|  show this help message and exit 	|
|    -n N                  				 	|  Number of PEs(default value is 1) 	|
|    --pehosts PEHOSTS     				 	|  List of nodes where PEs has to run 	|
|    --launcher {mpi,slurm}				 	|  Launcher to be used to launch preocesses(mpi/slurm) 	|
|    --model {memory_server,shared_memory}	|  OpenFAM model(memory_server/shared_memory) 	|
|    --thrdmodel {single,multiple}			|  Thread model(single/multiple) 	|
|    --provider PROVIDER   				 	|  Libafbric provider to be used for datapath operations 	|
|    --runtime {PMIX,PMI2}					|  Runtime process management interface(PMI) to be used 	|
|    --cisinterface {rpc,direct}			|  CIS interface type(rpc/direct) 	|
|    --cisrpcaddr CISRPCADDR				|  RPC address and port to be used for CIS 	|
|    --memserverinterface {rpc,direct}		|  Memory Service interface type(rpc/direct) 	|
|    --metaserverinterface {rpc,direct}	 	|  Metadata Service interface type(rpc/direct) 	|
|    --memserverlist MEMSERVERLIST			|  Memory server address list(comma seperated) eg : 0:0.0.0.0:1234,1:1.1.1.1:5678 	|
|    --metaserverlist METASERVERLIST		|  Metadata Server list(comma seperated) eg : 0:0.0.0.0:1234,1:1.1.1.1:5678 	|
|    --kvstype KVSTYPE    					|  Type of key value store to be used for metadata management 	|
|    --libfabricport LIBFABRICPORT			|  Libafabric port for datapath operation 	|
|    --metapath METAPATH   				 	|  path to store metadata 	|
|    --fampath FAMPATH     				 	|  path where data is stored 	|
|    --atlthreads ATLTHREADS				|  Atomic library threads count 	|
|    --atlqsize ATLQSIZE   				 	|  ATL queue size 	|
|    --atldatasize ATLDATASIZE				|  ATL data size per thread(MiB) |
|    --disableregionspanning                |  Disable region spanning  |
|    --regionspanningsize REGIONSPANNINGSIZE|  Region spanning size  |
|    --runtests                             |  Run regression and unit tests  |
|    --fam_backup_path backup_path          |  Absolute Path where data item backups are placed  |

  Note : arguments can also be passed via argument file, for example

  ```
  $python3 run_test.py @arg_file
  ```

  In arg_file arguments has to be mentioned one per line, for example: 
  
  arg_file
  ```
  --launcher=mpi
  -n=1
  $OpenFAM/config
  $OpenFAM/config_out
  $OpenFAM/build
  ```

