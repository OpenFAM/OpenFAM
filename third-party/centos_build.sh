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
#
CURRENT_DIR=`pwd`
BUILD_DIR="build"
LIB_DIR="$BUILD_DIR/lib"
INCLUDE_DIR="$BUILD_DIR/include"
BIN_DIR="$BUILD_DIR/bin"
OBJ_DIR="$BUILD_DIR/obj"
PMIX_OBJ_DIR="$CURRENT_DIR/$BUILD_DIR"
ABS_BUILD_DIR="$CURRENT_DIR/$BUILD_DIR"


#creating build directory if not present
if [ ! -d "$BUILD_DIR" ]; then
  mkdir $BUILD_DIR
fi
if [ ! -d "$INCLUDE_DIR" ]; then
  mkdir $INCLUDE_DIR
fi
if [ ! -d "$LIB_DIR" ]; then
  mkdir $LIB_DIR
fi

if [ ! -d "$BIN_DIR" ]; then
  mkdir $BIN_DIR
fi
if [ ! -d "$OBJ_DIR" ]; then
  mkdir $OBJ_DIR
fi

#Download all dependency source files
./download.sh

echo "Installing required RPMs"

sudo yum install --assumeyes gcc gcc-c++ kernel-devel make cmake autoconf-archive glibc python python-devel automake libtool doxygen
sudo yum install --assumeyes libevent cmake xsltproc xmlto autoconf-archive 
if [[ $? > 0 ]]
then
        exit 1
fi

# OpenMPI rpms require libevent-devel rpms. If this package is not available as part of RHEL repos, we fetch it from centos repo
OS=`grep -m1 "ID=" /etc/os-release | sed 's/"//g' | sed 's/ID=//g' `
arch=`lscpu | grep Architecture | awk '{print $2}'`
libevent_ver=`yum list libevent | tail -n 1 | awk '{print $2}'`
os_ver=`grep VERSION_ID /etc/os-release |  awk -F '=' '{print $2}' | sed 's/"//g'`
os_major_version=`echo $os_ver | awk -F '.' '{print $1}'`
sudo yum list libevent-devel
exit_status=$?
case $OS in

"rhel")
	sudo yum install --assumeyes  http://mirror.centos.org/centos/${os_major_version}/os/${arch}/Packages/libevent-devel-${libevent_ver}.${arch}.rpm
	;;
"centos")
	sudo yum install --assumeyes  libevent-devel
	;;
esac

echo $BUILD_DIR

echo "Installing boost-1.63.0"
if [[ ! -e boost_1_63_0 ]]; then
	wget https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.tar.gz
	tar -zxvf boost_1_63_0.tar.gz
fi

cd boost_1_63_0/
./bootstrap.sh
sudo ./b2 install --prefix=$ABS_BUILD_DIR
if [[ $? > 0 ]]
then
        exit 1
fi

export BOOST_ROOT=$ABS_BUILD_DIR

echo "Finished installing required RPMS and boost library"

cd ..

#Install grpc
cd grpc
#Update the third party modules
git submodule update --init

#calling make - For parallel compilation, make -j is selected. For systems with limited DRAM, avoid using -j option for all make commands
make -j
if [[ $? > 0 ]]
then
        echo "Make of grpc failed.. exit..."
        exit 1
fi

#installing gRPC
make install prefix=$ABS_BUILD_DIR
if [[ $? > 0 ]]
then
        echo "Make install of grpc failed.. exit..."
        exit 1
fi

#installing protocol buffer
cd ./third_party/protobuf

if [ -e Makefile ]; then
       sudo make install prefix=$ABS_BUILD_DIR
        if [[ $? > 0 ]]
      then
             echo "Make install of protobuf failed.. exit..."
             exit 1
        fi
fi

#Creating soft links for gRPC libraries
cd ../../../$LIB_DIR

ln -s libgrpc++.so.1.17.0-dev libgrpc++.so.1

ln -s libgrpc++_reflection.so.1.17.0-dev libgrpc++_reflection.so.1

cd $CURRENT_DIR

cd pmix-3.0.2
./autogen.pl --force
PMIX_OBJ_DIR="$CURRENT_DIR/$BUILD_DIR"
./configure --prefix=$PMIX_OBJ_DIR --with-platform=optimized
make all install
cd $CURRENT_DIR


cd openmpi-4.0.1
OPENMPI_OBJ_DIR="$CURRENT_DIR/$BUILD_DIR"
./configure --prefix="$CURRENT_DIR/$BUILD_DIR" --with-pmix="$CURRENT_DIR/$BUILD_DIR" --with-pmi-libdir="$CURRENT_DIR/$BUILD_DIR" --with-libevent=external
make -j
make install
if [[ $? > 0 ]]
then
        echo "Make all install of openmpi failed.. exit..."
        exit 1
fi
cd $CURRENT_DIR

#Build and Install libfabric
INSTALLDIR="$CURRENT_DIR/build"

#Libfabric installation
cd libfabric/

./autogen.sh 
./configure; make
if [[ $? > 0 ]]
then
        echo "Make of libfabric failed.. exit..."
        exit 1
fi

make install prefix=$INSTALLDIR
cd $CURRENT_DIR


#Install nvmm
if [[ ! -e nvmm ]]; then
# Install nvmm 
	tar -zxvf nvmm.tar.gz
fi

cp ./nvmm_build_centos.sh nvmm/
cd ./nvmm
./nvmm_build_redhat.sh

if [[ $? > 0 ]]
then
        echo "NVMM build failed exiting..."
        exit 1
fi
cp build/src/libnvmm.so ../$LIB_DIR/.
cp -r include/nvmm ../$INCLUDE_DIR/.

cd $CURRENT_DIR


#build and install radixtree in third-party/build
#radixtree is required to build fammetadata library.
tar -zxvf radixtree.tar.gz
# Currently radixtree build doesn't provide compiler option -  "-std=c++11" by default.But without this radixtree build fails.
# Following line can be omitted if that option is added in the radixtree github.

sed -i '22i\set(CMAKE_CXX_FLAGS "-std=c++11")\' radixtree/CMakeLists.txt
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



