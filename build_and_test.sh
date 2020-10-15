#!/bin/bash
CURRENTDIR=`pwd`
MEMSRV_BUILD_DIR=$CURRENTDIR/build/build-mem-server
SHM_BUILD_DIR=$CURRENTDIR/build/build-shm

#creating build directory if not present
if [ ! -d "$MEMSRV_BUILD_DIR" ]; then
  echo "Creating $MEMSRV_BUILD_DIR"
  mkdir -p $MEMSRV_BUILD_DIR
fi

#Build and run test with MemoryServer allocator
cd $MEMSRV_BUILD_DIR
cmake ../..; make ; make install
if [[ $? > 0 ]]
then
        echo "OpenFAM build with memoryserver version failed.. exit..."
        exit 1
fi

echo "==========================================================="
echo "Test OpenFAM with cis-rpc-meta-direct-mem-rpc configuration"
echo "==========================================================="
export OPENFAM_ROOT="$MEMSRV_BUILD_DIR/test/config_files/config-cis-rpc-meta-direct-mem-rpc/"
source setup.sh --cisserver=127.0.0.1  --memserver=127.0.0.1
echo $OPENFAM_ROOT
make unit-test
if [[ $? > 0 ]]
then
        echo "One or more unit-test failed while running with cis-rpc-meta-direct-mem-rpc configuration. exit..."
        exit 1
fi
make reg-test
if [[ $? > 0 ]]
then
        echo "One or more reg-test failed while running with cis-rpc-meta-direct-mem-rpc configuration. exit..."
        exit 1
fi

pkill cis_server
pkill memory_server

echo "========================================================"
echo "Test OpenFAM with cis-rpc-meta-rpc-mem-rpc configuration"
echo "========================================================"
export OPENFAM_ROOT="$MEMSRV_BUILD_DIR/test/config_files/config-cis-rpc-meta-rpc-mem-rpc/"
source setup.sh --cisserver=127.0.0.1 --metaserver=127.0.0.1 --memserver=127.0.0.1
make unit-test
if [[ $? > 0 ]]
then
        echo "One or more unit-test failed while running with cis-rpc-meta-rpc-mem-rpc configuration... exit..."
        exit 1
fi
make reg-test
if [[ $? > 0 ]]
then
        echo "One or more reg-test failed while running with cis-rpc-meta-rpc-mem-rpc configuration.. exit..."
        exit 1
fi

pkill cis_server
pkill memory_server
pkill metadata_server

echo "=============================================================="
echo "Test OpenFAM with cis-rpc-meta-direct-mem-direct configuration"
echo "=============================================================="
export OPENFAM_ROOT="$MEMSRV_BUILD_DIR/test/config_files/config-cis-rpc-meta-direct-mem-direct/"
source setup.sh --cisserver=127.0.0.1
make unit-test
if [[ $? > 0 ]]
then
        echo "One or more unit-test failed while running with cis-rpc-meta-direct-mem-direct configuration... exit..."
        exit 1
fi
make reg-test
if [[ $? > 0 ]]
then
        echo "One or more reg-test failed while running with cis-rpc-meta-direct-mem-direct configuration.. exit..."
        exit 1
fi

pkill cis_server

echo "==========================================================="
echo "Test OpenFAM with cis-direct-meta-rpc-mem-rpc configuration"
echo "==========================================================="
export OPENFAM_ROOT="$MEMSRV_BUILD_DIR/test/config_files/config-cis-direct-meta-rpc-mem-rpc/"
source setup.sh --metaserver=127.0.0.1 --memserver=127.0.0.1
make unit-test
if [[ $? > 0 ]]
then
        echo "One or more unit-test failed while running with cis-direct-meta-rpc-mem-rpc configuration... exit..."
        exit 1
fi
make reg-test
if [[ $? > 0 ]]
then
        echo "One or more reg-test failed while running with cis-direct-meta-rpc-mem-rpc configuration.. exit..."
        exit 1
fi

pkill memory_server
pkill metadata_server

if [ "$1" == "all" ]; then

if [ ! -d "$SHM_BUILD_DIR" ]; then
  echo "Creating $SHM_BUILD_DIR"
  mkdir -p $SHM_BUILD_DIR
fi

#Build and run test with shared memory model
cd $SHM_BUILD_DIR
 cmake ../.. -DARG_OPENFAM_MODEL=shared_memory; make ; make install
if [[ $? > 0 ]]
then
        echo "OpenFAM build with shared memory model  failed.. exit..."
        exit 1
fi

export OPENFAM_ROOT="$SHM_BUILD_DIR/test/config_files/config-cis-direct-meta-direct-mem-direct/"
echo $OPENFAM_ROOT
make unit-test
if [[ $? > 0 ]]
then
        echo "One or more unit-test failed while running with shared memory model.. exit..."
        exit 1
fi

make reg-test
if [[ $? > 0 ]]
then
        echo "One or more reg-test failed while running with shared memory model.. exit..."
        exit 1
fi

fi
cd $CURRENTDIR
