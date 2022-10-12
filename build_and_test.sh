#!/bin/bash
CURRENTDIR=`pwd`
BUILD_DIR=$CURRENTDIR/build/
TOOL_DIR=$CURRENTDIR/tools
MAKE_CMD="make -j"
THIRDPARTY_BUILD=$CURRENTDIR/third-party/build/

#creating build directory if not present
if [ ! -d "$BUILD_DIR" ]; then
  echo "Creating $BUILD_DIR"
  mkdir -p $BUILD_DIR
fi

#set library path
export LD_LIBRARY_PATH=$THIRDPARTY_BUILD/lib/:$THIRDPARTY_BUILD/lib64/:$LD_LIBRARY_PATH

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
export OPENFAM_ROOT=$CONFIG_OUT_DIR
$BUILD_DIR/bin/openfam_adm @$TOOL_DIR/common-config-arg.txt --install_path $CURRENTDIR --model memory_server --cisinterface rpc --memserverinterface rpc --metaserverinterface direct --create_config_files --config_file_path $CONFIG_OUT_DIR --start_service --runtests
if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-direct-mem-rpc configuration failed. exit..."
        exit 1
fi

sleep 5

$BUILD_DIR/bin/openfam_adm --stop_service --clean 

sleep 5

echo "========================================================"
echo "Test OpenFAM with cis-rpc-meta-rpc-mem-rpc configuration"
echo "========================================================"
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-cis-rpc-meta-rpc-mem-rpc
export OPENFAM_ROOT=$CONFIG_OUT_DIR
$BUILD_DIR/bin/openfam_adm @$TOOL_DIR/common-config-arg.txt --install_path $CURRENTDIR --model memory_server --cisinterface rpc --memserverinterface rpc --metaserverinterface rpc --create_config_files --config_file_path $CONFIG_OUT_DIR --start_service --runtests

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-rpc-mem-rpc configuration failed. exit..."
        exit 1
fi

sleep 5

$BUILD_DIR/bin/openfam_adm --stop_service --clean 

sleep 5

echo "=============================================================="
echo "Test OpenFAM with cis-rpc-meta-direct-mem-direct configuration"
echo "=============================================================="
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-cis-rpc-meta-direct-mem-direct
export OPENFAM_ROOT=$CONFIG_OUT_DIR
$BUILD_DIR/bin/openfam_adm @$TOOL_DIR/common-config-arg.txt --install_path $CURRENTDIR --model memory_server --cisinterface rpc --memserverinterface direct --metaserverinterface direct --create_config_files --config_file_path $CONFIG_OUT_DIR --start_service --runtests

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-direct-mem-direct configuration failed. exit..."
        exit 1
fi

sleep 5

$BUILD_DIR/bin/openfam_adm --stop_service --clean 

sleep 5

echo "==========================================================="
echo "Test OpenFAM with cis-direct-meta-rpc-mem-rpc configuration"
echo "==========================================================="
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-cis-direct-meta-rpc-mem-rpc
export OPENFAM_ROOT=$CONFIG_OUT_DIR
$BUILD_DIR/bin/openfam_adm @$TOOL_DIR/common-config-arg.txt --install_path $CURRENTDIR --model memory_server --cisinterface direct --memserverinterface rpc --metaserverinterface rpc --create_config_files --config_file_path $CONFIG_OUT_DIR --start_service --runtests

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-direct-meta-rpc-mem-rpc configuration failed. exit..."
        exit 1
fi

sleep 5 

$BUILD_DIR/bin/openfam_adm --stop_service --clean 

sleep 5

echo "==========================================================="
echo "Test OpenFAM with shared memory configuration"
echo "==========================================================="
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-shared-memory
export OPENFAM_ROOT=$CONFIG_OUT_DIR
$BUILD_DIR/bin/openfam_adm @$TOOL_DIR/common-config-arg.txt --install_path $CURRENTDIR --model shared_memory --cisinterface direct --memserverinterface direct --metaserverinterface direct --create_config_files --config_file_path $CONFIG_OUT_DIR --start_service --runtests

if [[ $? > 0 ]]
then
        echo "OpenFAM test with shared memory configuration failed. exit..."
        exit 1
fi

sleep 5 

$BUILD_DIR/bin/openfam_adm --stop_service --clean 

sleep 5

echo "==========================================================="
echo "Test OpenFAM with multiple memory servers in all rpc configuration"
echo "==========================================================="
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-multi-mem
export OPENFAM_ROOT=$CONFIG_OUT_DIR
$BUILD_DIR/bin/openfam_adm @$TOOL_DIR/multi-mem-config-arg.txt --install_path $CURRENTDIR --create_config_files --config_file_path $CONFIG_OUT_DIR --start_service --runtests

if [[ $? > 0 ]]
then
        echo "OpenFAM test with multiple memory servers in all rpc configuration failed. exit..."
        exit 1
fi

$BUILD_DIR/bin/openfam_adm --stop_service --clean 

cd $CURRENTDIR
