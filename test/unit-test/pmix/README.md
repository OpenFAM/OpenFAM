# Steps to run pmix tests
# --------------------------
# 1> Do a cmake from OpenFAM/build directory
# 2> Execute command - "make" from the same folder. It creates essential executables to run the test.
# 3> Execute command : "make install"
# 4> Run the setup script present in OpenFAM/build folder. This script sets the environment variables
#    to access the pmix binaries and also runs the PRTE (Reference Server).
# 5> make test - to run the PMIX tests
#
