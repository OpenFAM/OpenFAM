#!/bin/bash
CURRENTDIR=`pwd`
CIS_RPC_BUILD_DIR=$CURRENTDIR/build/build-cis-rpc
SHM_BUILD_DIR=$CURRENTDIR/build/build-shm
ALL_RPC_BUILD_DIR=$CURRENTDIR/build/build-all-rpc

#creating build directory if not present
if [ ! -d "$CIS_RPC_BUILD_DIR" ]; then
  echo "Creating $CIS_RPC_BUILD_DIR"
  mkdir -p $CIS_RPC_BUILD_DIR
fi

#Build and run test with RPC allocator
cd $CIS_RPC_BUILD_DIR
cmake ../..; make -j; make install
if [[ $? > 0 ]]
then
        echo "OpenFAM build with CIS RPC version failed.. exit..."
        exit 1
fi

source setup.sh --cisserver=127.0.0.1
export OPENFAM_ROOT="$CIS_RPC_BUILD_DIR/test/config_files/config-cis-rpc-meta-direct-mem-rpc/"
echo $OPENFAM_ROOT
make unit-test
if [[ $? > 0 ]]
then
        echo "One or more unit-test failed while running with CIS RPC version.. exit..."
        exit 1
fi
make reg-test
if [[ $? > 0 ]]
then
        echo "One or more reg-test failed while running with CIS RPC version.. exit..."
        exit 1
fi

pkill cis_server

if [ "$1" == "all" ]; then 
if [ ! -d "$SHM_BUILD_DIR" ]; then
  echo "Creating $SHM_BUILD_DIR"
  mkdir -p $SHM_BUILD_DIR
fi

#Build and run test with shared memory model
cd $SHM_BUILD_DIR
 cmake ../.. -DARG_OPENFAM_MODEL=shared_memory; make -j; make install
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

if [ ! -d "$ALL_RPC_BUILD_DIR" ]; then
	echo "Creating $ALL_RPC_BUILD_DIR"
	mkdir -p $ALL_RPC_BUILD_DIR
fi

#Build and run test with both CIS and metadata services RPC version
cd $ALL_RPC_BUILD_DIR
cmake ../.. -DENABLE_METADATA_SERVICE_RPC=ON -DENABLE_MEMORY_SERVICE_RPC=ON; make -j; make install
if [[ $? > 0 ]]
then
        echo "OpenFAM build with both CIS and Memory Service RPC version failed.. exit..."
        exit 1
fi

source setup.sh --cisserver=127.0.0.1 --metaserver=127.0.0.1 --memserver=127.0.0.1
export OPENFAM_ROOT="$ALL_RPC_BUILD_DIR/test/config_files/config-cis-rpc-meta-rpc-mem-rpc/"
make unit-test
if [[ $? > 0 ]]
then
        echo "One or more unit-test failed while running all the Services with RPC version.. exit..."
        exit 1
fi
make reg-test
if [[ $? > 0 ]]
then
        echo "One or more reg-test failed while running all the Services with RPC version.. exit..."
        exit 1
fi

pkill cis_server
pkill memory_server
pkill metadata_server
fi
cd $CURRENTDIR

