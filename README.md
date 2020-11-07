# OpenFAM
Recent technology advances in high-density, byte-addressable non-volatile memory (NVM) and low-latency interconnects have enabled building large-scale systems with a large disaggregated fabric-attached memory (FAM) pool shared across heterogeneous and decentralized compute nodes. In this model, compute nodes are decoupled from FAM, which allows separate evolution and scaling of processing and memory. Thus, the compute-to-memory ratio can be tailored to the specific needs of the workload. Compute nodes fail independently of FAM, and these separate fault domains provide a partial failure model that avoids a complete system shutdown in the event of a component failure. When a compute node fails, updates propagated to FAM remain visible to other compute nodes.

OpenFAM is is an API designed for clusters that contain disaggregated memory. The primary purpose of the reference implementation at this site is to enable experimentation with the OpenFAM API, with the broader goal of gathering feedback from the community on how the API should evolve. The reference implementation, thus, is designed to run on standard commercially available servers. More documentation about the API itself is available [here](https://openfam.github.io/)

## Assumptions
* We assume that the fabric attached memory is modelled using memory servers implemented using standard hardware that expose their memory as FAM to other servers. Unlike target architectures, a memory server is an active entity on the FAM side, and participates in managing FAM-resident data. We also assume that RDMA transports are available to efficiently access the memory exposed by the memory servers. To the extent possible, we are using standard RDMA and communication libraries that are being developed in the community in our implementation.
* While we are interested in making the API functionally available, it is not our immediate goal to optimize performance or scale of the reference implementation. Optimizing performance at scale necessarily has significant dependencies on the hardware and software available at the OS/middleware level, which we are initially trying to avoid. However, we do plan to benchmark the reference implementation and understand where performance bottlenecks exist, so that they can be addressed as the implementation evolves to a true FAM architecture.
* We are initially not assuming fault tolerance in the implementation of the OpenFAM library.


## Building(Ubuntu 18.04 LTS)

1. Download the source code by cloning the repository. If you wish to contribute changes back to OpenFAM, follow the [contribution guidelines](https://github.com/OpenFAM/OpenFAM/blob/master/CONTRIBUTING.md).

2. Change into the source directory (assuming the code is at directory ```$OpenFAM```):

```
$ cd $OpenFAM
```

3. Build third-party dependencies as described in the [README](https://github.com/OpenFAM/OpenFAM/tree/master/third-party) under the third-party directory.


4. Build and Test with the script.
   (Note that this script will build OpenFAM under build directory and run unit tests and regression tests.
   script takes one argument which specify launcher for tests, argument can take 2 values mpi or slurm)
   For mpi :
 ```
 $ ./build_and_run.sh mpi
 ```
   For slurm :
 ```
 $ ./build_and_run.sh slurm
 ```

5. (Optional) Build and Test manually.

   a. Create and change into a build directory under the source directory ```$OpenFAM```.

    ```
    $ cd $OpenFAM
    $ mkdir build
    $ cd build
    ```

   b. Run cmake.

    ```
    $ cmake ..
    ```

    The default build type is Release. To switch between Release, Debug and Coverage, use one of the below command:

    ```
    $ cmake .. -DCMAKE_BUILD_TYPE=Release
    $ cmake .. -DCMAKE_BUILD_TYPE=Debug
    $ cmake .. -DCMAKE_BUILD_TYPE=Coverage
    ```

   c. make and install OpenFAM under the current build directory.

    ```
    $ make -j
    $ make install
    ```

   d. set OPENFAM_ROOT variable to point to config file location
      (sample config files are located in config directory, User can also configure OpenFAM using openFAM options
           to enable openFAM options during testing set USE_FAM_OPTION flag in cmake build as, cmake .. -DUSE_FAM_OPTION=ON)

        ```
        export OPENFAM_ROOT="$OpenFAM"
        ```

   e. Start all services on localhost(127.0.0.1) as a background process on the current terminal.
      (In sample configurtion Memory Service and  Metadata Service are using direct interface and CIS has RPC interface,
           edit the configuratuion files to run with desired configuration and start services accordingly)

    ```
    $ source setup.sh --cisserver=127.0.0.1
    ```
    (see setup.sh script description below)

   f. Run the unit tests and regresssion tests.

    ```
    $ make unit-test
    $ make reg-test
    ```

   g. Stop all services running locally on the current terminal.

    ```
    $ pkill memory_server
        $ pkill metadata_server
        $ pkill cis_server
    ```


6. (Optional) If you only wish to test code coverage.

   a. Perform steps 5-a to 5-d using build type as Coverage in step 5-a

    ```
    $ cmake .. -DCMAKE_BUILD_TYPE=Coverage
    ```

   b. Clean previous coverage information

    ```
    $ make cov-clean
    ```

   c. Run unit-test and reg-test

    ```
    $ make cov-run-tests
    ```

    If required, run other tests to improve coverage results before generating report.

   d. Generate code coverage reports

    ```
    $ make cov-report
    ```
    A summary report is displayed on the terminal, while the details coverage results in html format is placed under build/test/coverage directory.
    To collect additional coverage information of memoryserver, repeat this step after stopping memoryserver instance(5-f).

## setup script
The script [setup.sh](https://github.com/OpenFAM/OpenFAM/blob/master/test/setup.sh) is used to start all services locally and set the environment variable which are necessary to run tests manually

### Description
```
Usage :

        source setup.sh <options>

OPTIONS :
  -h/--help       : help

  -n              : Number of PEs

  --memserver     : IP address of memory server
                    Note : This option is necessary to start the memory server

  --metaserver    : IP address of metadata server
                    Note : This option is necessary to start the metadata server

  --cisserver     : IP address of CIS server
                    Note : This option is necessary to start the CIS server

  --memrpcport    : RPC port for memory service

  --metarpcport   : RPC port for metadata server

  --cisrpcport    : RPC port for CIS server

  --libfabricport : Libfabric port

  --provider      : Libfabric provider

  --fam_path      : Location of FAM

  --init          : Initializes NVMM (use this option for shared memory model)

(Note : Do no use source command for -h/--help and --init options eg : ./setup.sh -h or ./setup.sh --init)
```

Note: cmake command should be re-run if fam\_rpc.proto file is modified to generate updated fam\_rpc.grpc.pb.cc and fam_rpc.pb.cc files
