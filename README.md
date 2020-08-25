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
   Note that this script will build OpenFAM under build/build-rpc directory and run unit tests and regression tests.

 ```
 $ ./build_and_run.sh
 ```
   By default this script will run only CIS with RPC interface. To run the following combination
   a. All services with RPC interface
   b. Shared memory model

   use the following command

 ```
 $ ./build_and_run.sh all
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

   d. Start the memoryserver on localhost(127.0.0.1) as a background process on the current terminal.

    ```
    $ ./src/memoryserver -m 127.0.0.1 &
    ```

   e. Run the unit tests and regresssion tests.

    ```
    $ make unit-test
    $ make reg-test
    ```

   f. Stop the memoryserver running locally on the current terminal.

    ```
    $ pkill memoryserver
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


Note: cmake command should be re-run if fam\_rpc.proto file is modified to generate updated fam\_rpc.grpc.pb.cc and fam_rpc.pb.cc files
