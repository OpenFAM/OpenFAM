## Multiple Memory Node Test

1. Follow the build instruction to build OpenFAM

2. Follow the steps below to run mutiple memory server tests

    * Update TEST_MEMORY_SERVER in CMakeLists.txt to comma seperated list of memoryserver in
    the format <memory server id>:<memory server ip address>

    * Then follow the build steps (a) - (c)

    * start the memoryserver on all memory nodes using openfam_adm

    * Run the multi memory server test as follow

    ```
    $ make multi-memnode-test
