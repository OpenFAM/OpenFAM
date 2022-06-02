#!/bin/bash
#
# build.sh
# Copyright (c) 2019-2022 Hewlett Packard Enterprise Development, LP. All rights
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
print_help() {
    tput bold
    echo "SYNOPSIS :"
    tput reset
    echo ""
    echo "./build.sh <options>"
	echo ""
    tput bold
    echo "OPTIONS :"
    tput reset
	echo ""
	echo "--no-package-install	: Do not install any packages"
	echo ""
	echo ""
	echo "--no-parallel-make	: Do not use make -j (preferred option in low memory systems)"
	echo ""
    exit
}


while :; do
    case $1 in
        -h|-\?|--help)
            print_help
            ;;
		--no-package-install)
			no_package_install=true
			;;
		--no-parallel-make)
			no_parallel_make=true
			;;
		*)
            break
	esac

	shift
done

BUILD_DIR="build"
LIB_DIR="$BUILD_DIR/lib"
BIN_DIR="$BUILD_DIR/bin"
OBJ_DIR="$BUILD_DIR/obj"
INCLUDE_DIR="$BUILD_DIR/include"
CURRENT_DIR=`pwd`
PMIX_OBJ_DIR="$CURRENT_DIR/$BUILD_DIR"
ABS_BUILD_DIR="$CURRENT_DIR/$BUILD_DIR"
#For parallel compilation, make -j is selected. For systems with limited DRAM, 
#avoid using -j option for all make commands
#TODO: set no_parallel_make by looking at system configuration
if [ "$no_parallel_make" == "true" ]
then
    echo "Not using parallel make"
    MAKE_CMD="make"
else
    echo "Using make -j for builds"
    MAKE_CMD="make -j"
fi
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
function get_os_release_version(){
    cat /etc/issue | tr "\\n" " "
}

ubuntu18_package_list="build-essential cmake autoconf libtool pkg-config libpthread-stubs0-dev libevent-2.1-6 libevent-dev flex hwloc libhwloc-dev libhwloc-plugins libyaml-cpp-dev libpmem-dev libpmem1 libboost-all-dev python python3 python-pip python3-pip"

ubuntu20_package_list="build-essential cmake autoconf libtool pkg-config libyaml-cpp-dev python3 python3-pip libevent-dev flex hwloc libhwloc-dev libhwloc-plugins libpmem-dev libpmem1 libboost-all-dev python3 python3-pip"

rhel8_package_list="gcc gcc-c++ python3 python3-pip cmake make kernel-devel libevent libevent-devel glibc automake epel-release boost-devel-1.66.0 libpmem libpmem-devel yaml-cpp yaml-cpp-devel flex"

sles15_sp3_package_list="gcc gcc-c++ make cmake python3 python3-pip autoconf yaml-cpp-devel libtool libevent libevent-devel flex libpmem1 libpmem-devel libboost_system1_66_0-devel libboost_thread1_66_0-devel libboost_filesystem1_66_0-devel libboost_headers1_66_0-devel libboost_context1_66_0-devel libboost_program_options1_66_0-devel libboost_log1_66_0-devel"

#Install dependency packages
OS=`grep -m1 "^ID=" /etc/os-release | sed 's/"//g' | sed 's/ID=//g' `

case $OS in
        "ubuntu")
		case $(get_os_release_version) in
		     *20.04*)
			if [ "$no_package_install" != "true" ]
                        then
			        sudo apt-get install --assume-yes ${ubuntu20_package_list}
		                if [[ $? > 0 ]]
		                then
			            echo "apt-get install failed, exiting..."
		   	            exit 1
		                fi
			fi
	    		;;		    
    		    *18.04*)
			if [ "$no_package_install" != "true" ]
			then	
			    sudo apt-get install --assume-yes ${ubuntu18_package_list}
		            if [[ $? > 0 ]]
      	                    then
			        echo "apt-get install failed, exiting..."
			        exit 1
		            fi

			fi
			;;
		esac
		;;
	"rhel" | "centos" | "rocky")
		ver=$(rpm -E %{rhel})
		case ${ver} in
			8)
			 if [ "$no_package_install" != "true" ]
			 then
				sudo yum install --assumeyes ${rhel8_package_list}
				if [[ $? > 0 ]]
				then
					echo "yum install failed, exiting..."
					exit 1
				fi
			 fi
		         ;;
		        *)
				echo "Unsupported os version: $OS $ver"
				exit 1
				;;
	                 esac
			;;
	"opensuse-leap" | "sles")
		case $(get_os_release_version) in
                     *15*SP3*)
		         if [ "$no_package_install" != "true" ]
		         then
                            sudo zypper --non-interactive install ${sles15_sp3_package_list}
		            if [[ $? > 0 ]]
		            then
			       echo "zypper install failed, exiting..."
			    exit 1
		         fi 
	                fi
	             ;;
	             *)
			     echo "Unsupported os version, Supported SLES version : 15 SP3"
			     exit 1
			     ;;
	             esac
		     ;;
	*)
		echo "Operating system $OS is not supported"
		exit 1
		;;

esac		
if [ "$no_package_install" != "true" ] 
then
    pip3 install ruamel.yaml
    echo "Finished installing required RPMS"
fi

cd $CURRENT_DIR
pwd
#Install grpc
cd grpc

#Update the third party modules
git submodule update --init

mkdir -p cmake/build
cd cmake/build
cmake ../.. -DBUILD_SHARED_LIBS=ON -DgRPC_INSTALL=ON -DCMAKE_INSTALL_PREFIX=$ABS_BUILD_DIR -DABSL_ENABLE_INSTALL=ON
#cmake ../.. -DgRPC_INSTALL=ON -DCMAKE_INSTALL_PREFIX=$ABS_BUILD_DIR
$MAKE_CMD
if [[ $? > 0 ]]
then
        echo "Make of grpc failed.. exit..."
        exit 1
fi

#installing gRPC
make install 
if [[ $? > 0 ]]
then
        echo "Make install of grpc failed.. exit..."
        exit 1
fi

#installing protocol buffer
cd ../../third_party/protobuf
if [[ ! -f "$ABS_BUILD_DIR/bin/protoc" ]]; then
    ./autogen.sh
    ./configure --prefix=$ABS_BUILD_DIR --disable-shared
    $MAKE_CMD
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
cd pmix-3.1.2
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
        "rhel" | "centos" | "rocky")
            ./configure --prefix="$CURRENT_DIR/$BUILD_DIR" --with-pmix="$CURRENT_DIR/$BUILD_DIR" --with-pmi-libdir="$CURRENT_DIR/$BUILD_DIR" --with-libevent=external      
	    ;;
	"opensuse-leap" | "sles")
	    ./configure --prefix="$CURRENT_DIR/$BUILD_DIR" --with-pmix="$CURRENT_DIR/$BUILD_DIR" --with-pmi-libdir="$CURRENT_DIR/$BUILD_DIR" --with-libevent=external 
	    ;;
esac

$MAKE_CMD
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
mkdir build
cd build
cmake .. -DFAME=OFF
make -j
if [[ $? > 0 ]]
then
        echo "NVMM build failed, exiting..."
        exit 1
fi
cd ..
cp build/src/libnvmm.so ../$LIB_DIR/.
cp -r include/nvmm ../$INCLUDE_DIR/.

cd $CURRENT_DIR

#Build and Install libfabric
INSTALLDIR="$CURRENT_DIR/build"


cd libfabric/

./autogen.sh ;
./configure; $MAKE_CMD
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
cmake .. -DFAME=OFF; make radixtree

if [[ $? > 0 ]]
then
        echo "radixtree build failed exiting..."
        exit 1
fi

cp  ./src/libradixtree.so  ../../$LIB_DIR/.
cp -r ../include/radixtree ../../$INCLUDE_DIR/
cp ../src/*.h  ../../$INCLUDE_DIR/radixtree

cd $CURRENT_DIR

