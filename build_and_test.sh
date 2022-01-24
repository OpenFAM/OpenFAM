#!/bin/bash
CURRENTDIR=`pwd`
BUILD_DIR=$CURRENTDIR/build/
SCRIPT_DIR=$CURRENTDIR/scripts
MAKE_CMD="make -j"


print_help() {
    tput bold
    echo "SYNOPSIS :"
    tput reset
    echo ""
    echo "./build_and_test.sh <options>"
	echo ""
    tput bold
    echo "OPTIONS :"
    tput reset
	echo ""
	echo "--launcher		: Launcher to be used to launch services and tests"
	echo ""
	echo "--run-multi-mem	: Enable region spanning and multiple memory server tests"
	echo ""
    exit
}


while :; do
    case $1 in
        -h|-\?|--help)
            print_help
            ;;
		--launcher=?*)
			LAUNCHER="--launcher ${1#*=}"
			;;
		--launcher=)
			echo 'ERROR: "--launcher" requires a non-empty option argument.'
			;;
		--run-multi-mem)
			run_multi_mem_test=true
			;;
		*)
            break	
	esac
	
	shift
done
		
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
python3 $SCRIPT_DIR/run_test.py --runtests $LAUNCHER -n 1 --cisinterface rpc --cisrpcaddr 127.0.0.1:8787 --memserverinterface rpc --metaserverinterface direct --memservers '0:{memory_type:volatile,fam_path:/dev/shm/vol/,rpc_interface:127.0.0.1,rpc_port:8795,libfabric_port:7500,if_device:eth0}' $CURRENTDIR $CONFIG_OUT_DIR $BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-direct-mem-rpc configuration failed. exit..."
        exit 1
fi

sleep 5

echo "========================================================"
echo "Test OpenFAM with cis-rpc-meta-rpc-mem-rpc configuration"
echo "========================================================"
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-cis-rpc-meta-rpc-mem-rpc
python3 $SCRIPT_DIR/run_test.py --runtests $LAUNCHER -n 1 --cisinterface rpc --cisrpcaddr 127.0.0.1:8787 --memserverinterface rpc --metaserverinterface rpc --memservers '0:{memory_type:volatile,fam_path:/dev/shm/vol/,rpc_interface:127.0.0.1,rpc_port:8795,libfabric_port:7500,if_device:eth0}' --metaserverlist 0:127.0.0.1:8788 $CURRENTDIR $CONFIG_OUT_DIR $BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-rpc-mem-rpc configuration failed. exit..."
        exit 1
fi
sleep 5

echo "=============================================================="
echo "Test OpenFAM with cis-rpc-meta-direct-mem-direct configuration"
echo "=============================================================="
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-cis-rpc-meta-direct-mem-direct
python3 $SCRIPT_DIR/run_test.py --runtests $LAUNCHER -n 1 --cisinterface rpc --cisrpcaddr 127.0.0.1:8787 --memserverinterface direct --metaserverinterface direct $CURRENTDIR $CONFIG_OUT_DIR $BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-rpc-meta-direct-mem-direct configuration failed. exit..."
        exit 1
fi

sleep 5
echo "==========================================================="
echo "Test OpenFAM with cis-direct-meta-rpc-mem-rpc configuration"
echo "==========================================================="
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-cis-direct-meta-rpc-mem-rpc
python3 $SCRIPT_DIR/run_test.py --runtests $LAUNCHER -n 1 --cisinterface direct --memserverinterface rpc --metaserverinterface rpc --memservers '0:{memory_type:volatile,fam_path:/dev/shm/vol/,rpc_interface:127.0.0.1,rpc_port:8795,libfabric_port:7500,if_device:eth0}' --metaserverlist 0:127.0.0.1:8788 $CURRENTDIR $CONFIG_OUT_DIR $BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with cis-direct-meta-rpc-mem-rpc configuration failed. exit..."
        exit 1
fi

sleep 5 
echo "==========================================================="
echo "Test OpenFAM with shared memory configuration"
echo "==========================================================="
CONFIG_OUT_DIR=$BUILD_DIR/test/config_files/config-shared-memory
python3 $SCRIPT_DIR/run_test.py --runtests $LAUNCHER -n 1 --model shared_memory --cisinterface direct --memserverinterface direct --metaserverinterface direct $CURRENTDIR $CONFIG_OUT_DIR $BUILD_DIR

if [[ $? > 0 ]]
then
        echo "OpenFAM test with shared memory configuration failed. exit..."
        exit 1
fi
if [ "$run_multi_mem_test" == "true" ]
then
sleep 5 

echo "==========================================================="
echo "Test OpenFAM with multiple memory servers in all rpc configuration"
echo "==========================================================="
python3 $SCRIPT_DIR/run_test.py --runtests $LAUNCHER @$SCRIPT_DIR/multi-mem-test-arg.txt

if [[ $? > 0 ]]
then
        echo "OpenFAM test with multiple memory servers in all rpc configuration failed. exit..."
        exit 1
fi
fi
cd $CURRENTDIR
