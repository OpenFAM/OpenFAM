#!/bin/bash
CURRENTDIR=`pwd`
RPC_BUILD_DIR=$CURRENTDIR/build/build-rpc

#creating build directory if not present
if [ ! -d "$RPC_BUILD_DIR" ]; then
  echo "Creating $RPC_BUILD_DIR"
  mkdir -p $RPC_BUILD_DIR
fi

#Build and run test with RPC allocator
cd $RPC_BUILD_DIR
cmake ../..
make -j
make install
source setup.sh 127.0.0.1
make unit-test
make reg-test

pkill memoryserver
