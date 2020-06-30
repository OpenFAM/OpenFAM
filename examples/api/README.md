## Example programs for all OpenFAM APIs.

The example programs can be build and run using the following commands:

$> g++ api_fam_add.cpp -o fam_add -I<path_to_OpenFAM>/include -L<path_to_OpenFAM_build>/build-rpc/src -lopenfam

To run the program, set the environment variable LD_LIBRARY_PATH to openfam library and thrid-party dependent libraries.

$> export LD_LIBRARY_PATH=<path_to_OpenFAM_build>/build-rpc/src:<path-to-OpenFAM>/third-party/build/lib
$> ./fam_add


The example program does not set any fam option configuration, like memory servers, libfabric provider, runtime options, etc. It use the default values set by the OpenFAM library, like localhost(127.0.0.1) as memory server, "sockets" as libfabric provider, "PMI" as runtime, etc. Please set appropriate fam_options to the desired value and use the example APIs.

The example programs gets build along with the default OpenFAM build process.

