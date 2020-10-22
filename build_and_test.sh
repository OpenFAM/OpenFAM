#!/bin/bash
CURRENTDIR=`pwd`
MEMSRV_BUILD_DIR=$CURRENTDIR/build/build-mem-server
SHM_BUILD_DIR=$CURRENTDIR/build/build-shm
SCRIPT_DIR=$CURRENTDIR/scripts
CONFIG_IN_DIR=$CURRENTDIR/config
MAKE_CMD="make -j"
LAUNCHER=$1
#creating build directory if not present
if [ ! -d "$MEMSRV_BUILD_DIR" ]; then
  echo "Creating $MEMSRV_BUILD_DIR"
  mkdir -p $MEMSRV_BUILD_DIR
fi

#Build and run test with MemoryServer allocator
cd $MEMSRV_BUILD_DIR
cmake ../..; $MAKE_CMD ; make install
if [[ $? > 0 ]]
then
        echo "OpenFAM build with memoryserver version failed.. exit..."
        exit 1
fi

echo "==========================================================="
echo "Test OpenFAM with cis-rpc-meta-direct-mem-rpc configuration"
echo "==========================================================="
CONFIG_OUT_DIR=$MEMSRV_BUILD_DIR/test/config_files/config-cis-rpc-meta-direct-mem-rpc
python3 $SCRIPT_DIR/run_test.py --launcher $LAUNCHER -n 1 --cisinterface rpc --cisrpcaddr 127.0.0.1:8787 --memserverinterface rpc --metaserverinterface direct --memserverlist 0:127.0.0.1:8788 $CONFIG_IN_DIR $CONFIG_OUT_DIR $MEMSRV_BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-direct-mem-rpc coniguration failed. exit..."
        exit 1
fi

echo "========================================================"
echo "Test OpenFAM with cis-rpc-meta-rpc-mem-rpc configuration"
echo "========================================================"
CONFIG_OUT_DIR=$MEMSRV_BUILD_DIR/test/config_files/config-cis-rpc-meta-rpc-mem-rpc
python3 $SCRIPT_DIR/run_test.py --launcher $LAUNCHER -n 1 --cisinterface rpc --cisrpcaddr 127.0.0.1:8787 --memserverinterface rpc --metaserverinterface rpc --memserverlist 0:127.0.0.1:8788 --metaserverlist 0:127.0.0.1:8789 $CONFIG_IN_DIR $CONFIG_OUT_DIR $MEMSRV_BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-rpc-mem-rpc coniguration failed. exit..."
        exit 1
fi

echo "=============================================================="
echo "Test OpenFAM with cis-rpc-meta-direct-mem-direct configuration"
echo "=============================================================="
CONFIG_OUT_DIR=$MEMSRV_BUILD_DIR/test/config_files/config-cis-rpc-meta-rpc-mem-rpc
python3 $SCRIPT_DIR/run_test.py --launcher $LAUNCHER -n 1 --cisinterface rpc --cisrpcaddr 127.0.0.1:8787 --memserverinterface direct --metaserverinterface direct $CONFIG_IN_DIR $CONFIG_OUT_DIR $MEMSRV_BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-direct-mem-direct coniguration failed. exit..."
        exit 1
fi

echo "==========================================================="
echo "Test OpenFAM with cis-direct-meta-rpc-mem-rpc configuration"
echo "==========================================================="
CONFIG_OUT_DIR=$MEMSRV_BUILD_DIR/test/config_files/config-cis-rpc-meta-rpc-mem-rpc
python3 $SCRIPT_DIR/run_test.py --launcher $LAUNCHER -n 1 --cisinterface direct --memserverinterface rpc --metaserverinterface rpc --memserverlist 0:127.0.0.1:8788 --metaserverlist 0:127.0.0.1:8789 $CONFIG_IN_DIR $CONFIG_OUT_DIR $MEMSRV_BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-direct-meta-rpc-mem-rpc coniguration failed. exit..."
        exit 1
fi

if [ "$2" == "all" ]; then

if [ ! -d "$SHM_BUILD_DIR" ]; then
  echo "Creating $SHM_BUILD_DIR"
  mkdir -p $SHM_BUILD_DIR
fi

#Build and run test with shared memory model
cd $SHM_BUILD_DIR
 cmake ../.. -DARG_OPENFAM_MODEL=shared_memory; $MAKE_CMD ; make install
if [[ $? > 0 ]]
then
        echo "OpenFAM build with shared memory model  failed.. exit..."
        exit 1
fi

echo "==========================================================="
echo "Test OpenFAM with shared memory configuration"
echo "==========================================================="
CONFIG_OUT_DIR=$MEMSRV_BUILD_DIR/test/config_files/config-shared-memory
python3 $SCRIPT_DIR/run_test.py --launcher $LAUNCHER -n 1 --model shared_memory --cisinterface direct --memserverinterface direct --metaserverinterface direct $CONFIG_IN_DIR $CONFIG_OUT_DIR $MEMSRV_BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with shared memory coniguration failed. exit..."
        exit 1
fi
fi
cd $CURRENTDIR
