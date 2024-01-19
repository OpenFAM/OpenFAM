# OpenFAM
Recent technology advances in high-density, byte-addressable non-volatile memory (NVM) and low-latency interconnects have enabled building large-scale systems with a large disaggregated fabric-attached memory (FAM) pool shared across heterogeneous and decentralized compute nodes. In this model, compute nodes are decoupled from FAM, which allows separate evolution and scaling of processing and memory. Thus, the compute-to-memory ratio can be tailored to the specific needs of the workload. Compute nodes fail independently of FAM, and these separate fault domains provide a partial failure model that avoids a complete system shutdown in the event of a component failure. When a compute node fails, updates propagated to FAM remain visible to other compute nodes.

OpenFAM is is an API designed for clusters that contain disaggregated memory. The primary purpose of the reference implementation at this site is to enable experimentation with the OpenFAM API, with the broader goal of gathering feedback from the community on how the API should evolve. The reference implementation, thus, is designed to run on standard commercially available servers. More documentation about the API itself is available [here](https://ba-ramya.github.io/OpenFAM.github.io)

## Assumptions
* We assume that the fabric attached memory is modelled using memory servers implemented using standard hardware that expose their memory as FAM to other servers. Unlike target architectures, a memory server is an active entity on the FAM side, and participates in managing FAM-resident data. We also assume that RDMA transports are available to efficiently access the memory exposed by the memory servers. To the extent possible, we are using standard RDMA and communication libraries that are being developed in the community in our implementation.
* While we are interested in making the API functionally available, it is not our immediate goal to optimize performance or scale of the reference implementation. Optimizing performance at scale necessarily has significant dependencies on the hardware and software available at the OS/middleware level, which we are initially trying to avoid. However, we do plan to benchmark the reference implementation and understand where performance bottlenecks exist, so that they can be addressed as the implementation evolves to a true FAM architecture.
* We are initially not assuming fault tolerance in the implementation of the OpenFAM library.


## Supported OS
* SUSE Linux Enterprise Server 15 SP4  
* Ubuntu 22.04 LTS  
* RHEL 8.x  

## Supported Compiler
* gcc 11.2.0

## Building(Ubuntu 22.04 LTS)

1. Download the source code by cloning the repository. If you wish to contribute changes back to OpenFAM, follow the [contribution guidelines](/CONTRIBUTING.md).

```
$ git clone https://github.com/OpenFAM/OpenFAM.git
```

```
$ cd OpenFAM
```

3. Build third-party dependencies as described in the [README](/third-party) under the third-party directory.

4. Build and Test.

   a. Create and change into a build directory.

    ```
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

    To enable Thallium RPC framework, use the below command:

    ```
    $ cmake .. -DENABLE_THALLIUM=1

    Note: [Mochi-thallium](https://mochi.readthedocs.io/en/latest/installing.html) package should be installed to use Thallium. 
    ```

   c. make and install OpenFAM under the current build directory.

    ```
    $ make -j
    $ make install
    ```

   d. After completing the build, user can use **openfam_adm** tool for generating configuration files, start/stop OpenFAM services and running tests.
      The detailed description on this tool is available [here](/tools/README.md).

	The following set of commands illustrate the openfam_adm usage:

	Set $PATH to openfam_adm
	```
	$ PATH=./bin/:$PATH
	```

	```
	$ openfam_adm @arg_file.txt --config_file_path=/path/to/generate/config/files --install_path=/path/to/openfam/install/dir
	```

	In arg_file.txt, arguments has to be mentioned one per line, for example:

	arg_file.txt
	```
	--launcher=slurm
	--model=memory_server
	--cisinterface=rpc
	--memserverinterface=rpc
	--metaserverinterface=rpc
	--rpc_framework=grpc
	--cisserver={rpc_interface:127.0.0.1,rpc_port:8787}
	--memservers=0:{memory_type:volatile,fam_path:/dev/shm/vol,rpc_interface:127.0.0.1,rpc_port:8793,libfabric_port:7500,if_device:eth0}
	--metaservers=0:{rpc_interface:127.0.0.1,rpc_port:8788}
	```

	```
	$ export OPENFAM_ROOT=/path/to/generated/config/files

	$ openfam_adm --start_service
	```

	Run tests (Note : "--runtests" runs regression tests and unit tests. This option works only when user is in build directory)
	```
	$ openfam_adm --runtests
	```

      Note : For testing default is configuration file options, to enable predefined fam options during testing set SET_FAM_OPTION flag in cmake build as, cmake .. -DSET_FAM_OPTION=ON


5. (Optional) If you only wish to test code coverage.

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


6. Build and Test with the script.
   This script is alternative option for build and test OpenFAM with predeined set of configurations.

	```
 	$ ./build_and_test.sh
 	```

Note: cmake command should be re-run if fam\_rpc.proto file is modified to generate updated fam\_rpc.grpc.pb.cc and fam_rpc.pb.cc files
