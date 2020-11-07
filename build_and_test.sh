#!/bin/bash
CURRENTDIR=`pwd`
BUILD_DIR=$CURRENTDIR/build/
SCRIPT_DIR=$CURRENTDIR/scripts
MAKE_CMD="make -j"
LAUNCHER=$1
#creating build directory if not present
if [ ! -d "$BUILD_DIR" ]; then
  echo "Creating $BUILD_DIR"
  mkdir -p $BUILD_DIR
fi

#Build and run test with MemoryServer allocator
cd $BUILD_DIR
cmake ..; $MAKE_CMD ; make install
if [[ $? > 0 ]]
then
        echo "OpenFAM build with memoryserver version failed.. exit..."
        exit 1
fi

echo "==========================================================="
echo "Test OpenFAM with cis-rpc-meta-direct-mem-rpc configuration"
echo "==========================================================="
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-cis-rpc-meta-direct-mem-rpc
python3 $SCRIPT_DIR/run_test.py --launcher $LAUNCHER -n 1 --cisinterface rpc --cisrpcaddr 127.0.0.1:8787 --memserverinterface rpc --metaserverinterface direct --memserverlist 0:127.0.0.1:8788 $CURRENTDIR $CONFIG_OUT_DIR $BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-direct-mem-rpc coniguration failed. exit..."
        exit 1
fi

echo "========================================================"
echo "Test OpenFAM with cis-rpc-meta-rpc-mem-rpc configuration"
echo "========================================================"
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-cis-rpc-meta-rpc-mem-rpc
python3 $SCRIPT_DIR/run_test.py --launcher $LAUNCHER -n 1 --cisinterface rpc --cisrpcaddr 127.0.0.1:8787 --memserverinterface rpc --metaserverinterface rpc --memserverlist 0:127.0.0.1:8788 --metaserverlist 0:127.0.0.1:8789 $CURRENTDIR $CONFIG_OUT_DIR $BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-rpc-mem-rpc coniguration failed. exit..."
        exit 1
fi

echo "=============================================================="
echo "Test OpenFAM with cis-rpc-meta-direct-mem-direct configuration"
echo "=============================================================="
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-cis-rpc-meta-rpc-mem-rpc
python3 $SCRIPT_DIR/run_test.py --launcher $LAUNCHER -n 1 --cisinterface rpc --cisrpcaddr 127.0.0.1:8787 --memserverinterface direct --metaserverinterface direct $CURRENTDIR $CONFIG_OUT_DIR $BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-direct-mem-direct coniguration failed. exit..."
        exit 1
fi

echo "==========================================================="
echo "Test OpenFAM with cis-direct-meta-rpc-mem-rpc configuration"
echo "==========================================================="
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-cis-rpc-meta-rpc-mem-rpc
python3 $SCRIPT_DIR/run_test.py --launcher $LAUNCHER -n 1 --cisinterface direct --memserverinterface rpc --metaserverinterface rpc --memserverlist 0:127.0.0.1:8788 --metaserverlist 0:127.0.0.1:8789 $CURRENTDIR $CONFIG_OUT_DIR $BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-direct-meta-rpc-mem-rpc coniguration failed. exit..."
        exit 1
fi

echo "==========================================================="
echo "Test OpenFAM with shared memory configuration"
echo "==========================================================="
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-shared-memory
python3 $SCRIPT_DIR/run_test.py --launcher $LAUNCHER -n 1 --model shared_memory --cisinterface direct --memserverinterface direct --metaserverinterface direct $CURRENTDIR $CONFIG_OUT_DIR $BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with shared memory coniguration failed. exit..."
        exit 1
fi
cd $CURRENTDIR
