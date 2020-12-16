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
#This script should work on ubunutu 18.04 and centos 7.x. (We have tested on centos 7.8). 
#
BUILD_DIR="build"
LIB_DIR="$BUILD_DIR/lib"
BIN_DIR="$BUILD_DIR/bin"
OBJ_DIR="$BUILD_DIR/obj"
INCLUDE_DIR="$BUILD_DIR/include"
CURRENT_DIR=`pwd`
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

#Function for checking ubuntu release version
function get_ubuntu_release(){
    cat /etc/issue | tr "\\n" " "
}

#Install dependency packages
OS=`grep -m1 "ID=" /etc/os-release | sed 's/"//g' | sed 's/ID=//g' `

case $OS in
        "ubuntu")

		sudo apt-get install --assume-yes build-essential autoconf libtool pkg-config
		if [[ $? > 0 ]]
		then
			echo "apt-get install failed, exiting..."
			exit 1
		fi
		case $(get_ubuntu_release) in
		    *16.04*)
			sudo apt-get install --assume-yes libpthread-stubs0-dev libevent-dev flex hwloc libhwloc-dev libhwloc-plugins libyaml-cpp-dev python python3 python-pip python3-pip
			;;
		    *)
			sudo apt-get install --assume-yes libpthread-stubs0-dev libevent-2.1-6 libevent-dev flex hwloc libhwloc-dev libhwloc-plugins libyaml-cpp-dev python python3 python-pip python3-pip
		esac
		if [[ $? > 0 ]]
		then
			echo "apt-get install failed, exiting..."
			exit 1
		fi

		;;
	"rhel" | "centos")
		sudo yum install --assumeyes gcc gcc-c++ kernel-devel make cmake autoconf-archive glibc python python-devel python3 automake libtool doxygen epel-release python-pip python3-pip
		sudo yum install --assumeyes libevent cmake xmlto libevent-devel yaml-cpp yaml-cpp-devel
		if [[ $? > 0 ]]
		then
			exit 1
		fi

		echo "Installing boost-1.63.0"
		if [[ ! -e boost_1_63_0 ]]; then
		    wget https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.tar.gz
		    tar -zxvf boost_1_63_0.tar.gz
		fi

		cd boost_1_63_0/
		./bootstrap.sh
		./b2 install --prefix=$ABS_BUILD_DIR
		if [[ $? > 0 ]]
		then
			exit 1
		fi
		export BOOST_ROOT=$ABS_BUILD_DIR
		;;

esac		

pip3 install ruamel.yaml

echo "Finished installing required RPMS"
cd $CURRENT_DIR
pwd
#Install grpc
cd grpc

#Update the third party modules
git submodule update --init

#For parallel compilation, make -j is selected. For systems with limited DRAM, avoid using -j option for all make commands
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
if [[ ! -f "$ABS_BUILD_DIR/bin/protoc" ]]; then
    ./autogen.sh
    ./configure --prefix=$ABS_BUILD_DIR --disable-shared
    make -j
    make install prefix=$ABS_BUILD_DIR
    if [[ $? > 0 ]]
    then
        echo "Make install of protobuf failed.. exit..."
        exit 1
    fi
else
    echo "$ABS_BUILD_DIR/bin/protoc is already present"	
fi

cd $CURRENT_DIR

#PMIX and PRRTE Build

cd pmix-3.0.2
./autogen.pl --force
PMIX_OBJ_DIR="$CURRENT_DIR/$BUILD_DIR"
./configure --prefix=$PMIX_OBJ_DIR --with-platform=optimized
make all install
cd $CURRENT_DIR


cd openmpi-4.0.1
OPENMPI_OBJ_DIR="$CURRENT_DIR/$BUILD_DIR"
case $OS in
        "ubuntu")

            ./configure --prefix="$CURRENT_DIR/$BUILD_DIR" --with-pmix="$CURRENT_DIR/$BUILD_DIR" --with-pmi-libdir="$CURRENT_DIR/$BUILD_DIR"
            ;;
        "rhel" | "centos")
            ./configure --prefix="$CURRENT_DIR/$BUILD_DIR" --with-pmix="$CURRENT_DIR/$BUILD_DIR" --with-pmi-libdir="$CURRENT_DIR/$BUILD_DIR" --with-libevent=external          
	    ;;
esac

make -j
make install
if [[ $? > 0 ]]
then
        echo "Make all install of openmpi failed.. exit..."
        exit 1
fi
cd $CURRENT_DIR
export CMAKE_PREFIX_PATH=$ABS_BUILD_DIR/include:$ABS_BUILD_DIR/lib
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

cd radixtree; mkdir build; cd build
cmake .. -DFAME=OFF; make

if [[ $? > 0 ]]
then
        echo "radixtree build failed exiting..."
        exit 1
fi

cp  ./src/libradixtree.so  ../../$LIB_DIR/.
cp -r ../include/radixtree ../../$INCLUDE_DIR/
cp ../src/*.h  ../../$INCLUDE_DIR/radixtree

cd $CURRENT_DIR

