#!/bin/bash
#
# download.sh
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

CURRENT_DIR=`pwd`

#GRPC v1.39.0
echo "Downloading GRPC source"
git clone https://github.com/grpc/grpc.git
cd grpc
git fetch --all --tags --prune
git checkout tags/v1.39.0 -b openfam

if [ "$no_libfabric" == "true" ]
then
    echo "Please provide LIBFABRIC_PATH while building openfam"
else
	#LIBFABRIC v1.14.0
	cd $CURRENT_DIR
	echo "Downloading LIBFABRIC source"
	git clone https://github.com/ofiwg/libfabric.git
	cd libfabric
	git fetch --all --tags --prune
	git checkout tags/v1.14.0 -b openfam
fi

if [ "$no_pmix" == "true" ]
then
    echo "Please provide PMIX_PATH while building openfam"
else
	#PMIX v3.0.2
	cd $CURRENT_DIR
	echo "Downloading PMIX source"
	git clone https://github.com/openpmix/openpmix.git pmix-3.1.2
	cd pmix-3.1.2
	git fetch --all --tags --prune
	git checkout tags/v3.1.2 -b openfam
fi

if [ "$no_openmpi" == "true" ]
then
    echo "System openmpi library will be used"
else
	#OPENMPI v4.0.1
	cd $CURRENT_DIR
	echo "Downloading OPENMPI source"
	wget https://download.open-mpi.org/release/open-mpi/v4.0/openmpi-4.0.1.tar.gz
	tar -zxvf openmpi-4.0.1.tar.gz >/dev/null
fi

#GOOGLETEST
cd $CURRENT_DIR
echo "Downloading GOOGLETEST source"
git clone --branch v1.10.x https://github.com/google/googletest.git googletest_v1_10_x
cp -fr googletest_v1_10_x/googletest .
rm -fr googletest_v1_10_x

#NVMM
#TODO: Replace with right NVMM branch before release
cd $CURRENT_DIR
echo "Downloading NVMM source"
git clone https://github.com/HewlettPackard/gull.git nvmm
#git clone -b devel https://github.com/HewlettPackard/gull.git nvmm
cd nvmm
git fetch --all --tags --prune
git checkout tags/v0.2 -b openfam

#Radixtree
cd $CURRENT_DIR
echo "Downloading radixtree source"
git clone https://github.com/HewlettPackard/meadowlark.git radixtree
cd radixtree
git fetch --all --tags --prune
git checkout tags/v0.1 -b openfam

cd $CURRENT_DIR
echo "Done..............."

