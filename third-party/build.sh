#!/bin/bash
#
# build.sh
# Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All rights
# reserved. Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its contributors
# may be used to endorse or promote products derived from this software without
# specific prior written permission.
#
#    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# See https://spdx.org/licenses/BSD-3-Clause
#

#
#This script assumes you have NOPASSWD access to sudo command
#If it doesnt work, encourage you to copy paste the command and workaround,
#Or you can edit sudoers file to give NOPASSWD access.
#Note : This works only on ubuntu.
#
BUILD_DIR="build"
LIB_DIR="$BUILD_DIR/lib"
INCLUDE_DIR="$BUILD_DIR/include"
CURRENT_DIR=`pwd`
PMIX_OBJ_DIR="$CURRENT_DIR/$BUILD_DIR"

#Download all dependency source files
./download.sh

#Function for checking ubuntu release version
function get_ubuntu_release(){
    cat /etc/issue | tr "\\n" " "
}

#Install dependency packages
sudo apt-get install --assume-yes build-essential autoconf libtool pkg-config
if [[ $? > 0 ]]
then
        echo "apt-get install failed, exiting..."
        exit 1
fi

#creating build directory if not present
if [ ! -d "$BUILD_DIR" ]; then
  echo "Creating $BUILD_DIR"
  mkdir $BUILD_DIR
fi

cd grpc

#Update the third party modules
git submodule update --init

#calling make
make  -j
if [[ $? > 0 ]]
then
        echo "Make of grpc failed.. exit..."
        exit 1
fi

#installing gRPC
make install prefix=../$BUILD_DIR
if [[ $? > 0 ]]
then
        echo "Make install of grpc failed.. exit..."
        exit 1
fi

#installing protocol buffer
cd ./third_party/protobuf

if [ -e Makefile ]; then
        sudo make install
        if [[ $? > 0 ]]
        then
             echo "Make install of protobuf failed.. exit..."
             exit 1
        fi
fi

cd $CURRENT_DIR
#PMIX and PRRTE Build
case $(get_ubuntu_release) in
    *16.04*)
        sudo apt-get install --assume-yes libpthread-stubs0-dev libevent-dev flex hwloc libhwloc-dev libhwloc-plugins
        wget https://github.com/libevent/libevent/archive/release-2.1.6-beta.tar.gz
        tar zxvf release-2.1.6-beta.tar.gz
        cd libevent-release-2.1.6-beta/
        ./autogen.sh; ./configure; make; sudo make install
        cd $CURRENT_DIR
        ;;
    *)
        sudo apt-get install --assume-yes libpthread-stubs0-dev libevent-2.1-6 libevent-dev flex hwloc libhwloc-dev libhwloc-plugins
esac

if [[ $? > 0 ]]
then
        echo "apt-get install failed, exiting..."
        exit 1
fi

cd pmix-3.0.2
./autogen.pl --force
PMIX_OBJ_DIR="$CURRENT_DIR/$BUILD_DIR"
./configure --prefix=$PMIX_OBJ_DIR --with-platform=optimized
make all install
cd $CURRENT_DIR


cd openmpi-4.0.1
OPENMPI_OBJ_DIR="$CURRENT_DIR/$BUILD_DIR"
./configure --prefix="$CURRENT_DIR/$BUILD_DIR" --with-pmix="$CURRENT_DIR/$BUILD_DIR" --with-pmi-libdir="$CURRENT_DIR/$BUILD_DIR"
make -j
make install
if [[ $? > 0 ]]
then
        echo "Make all install of openmpi failed.. exit..."
        exit 1
fi
cd $CURRENT_DIR

#Install nvmm
cd ./nvmm
../nvmm_build.sh

if [[ $? > 0 ]]
then
        echo "NVMM build failed exiting..."
        exit 1
fi
cp build/src/libnvmm.so ../$LIB_DIR/.
cp -r include/nvmm ../$INCLUDE_DIR/.

cd $CURRENT_DIR

#Build and Install libfabric
INSTALLDIR="$CURRENT_DIR/build"


cd libfabric/

./autogen.sh ;
./configure; make -j
if [[ $? > 0 ]]
then
        echo "Make of libfabric failed.. exit..."
        exit 1
fi

make install prefix=$INSTALLDIR

cd $CURRENT_DIR

#build and install radixtree in third-party/build
#radixtree is required to build fammetadata library.
tar -zxvf radixtree.tar.gz

cd radixtree; mkdir build; cd build
cmake ..; make

if [[ $? > 0 ]]
then
        echo "radixtree build failed exiting..."
        exit 1
fi
cp  libfamradixtree.so  ../../$LIB_DIR/.
mkdir ../../$INCLUDE_DIR/radixtree
cp ../*.h  ../../$INCLUDE_DIR/radixtree

cd $CURRENT_DIR
