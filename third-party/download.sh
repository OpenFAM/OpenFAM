#!/bin/bash
#
# download.sh
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

CURRENT_DIR=`pwd`

#GRPC v1.17.0
echo "Downloading GRPC source"
git clone https://github.com/grpc/grpc.git
cd grpc
git fetch --all --tags --prune
git checkout tags/v1.17.0 -b openfam

#LIBFABRIC v1.7.2 
cd $CURRENT_DIR
echo "Downloading LIBFABRIC source"
git clone https://github.com/ofiwg/libfabric.git
cd libfabric
git fetch --all --tags --prune
git checkout tags/v1.7.2 -b openfam

#PMIX v3.0.2
cd $CURRENT_DIR
echo "Downloading PMIX source"
git clone https://github.com/openpmix/openpmix.git pmix-3.0.2
cd pmix-3.0.2 
git fetch --all --tags --prune
git checkout tags/v3.0.2 -b openfam

#OPENMPI v4.0.1
cd $CURRENT_DIR
echo "Downloading OPENMPI source"
wget https://download.open-mpi.org/release/open-mpi/v4.0/openmpi-4.0.1.tar.gz
tar -zxvf openmpi-4.0.1.tar.gz >/dev/null

#GOOGLETEST 
cd $CURRENT_DIR
echo "Downloading GOOGLETEST source"
git clone https://github.com/google/googletest.git googletest_master
cp -fr googletest_master/googletest .
rm -fr googletest_master

#NVMM
cd $CURRENT_DIR
echo "Downloading NVMM source"
git clone https://github.com/HewlettPackard/gull.git nvmm

cd $CURRENT_DIR
echo "Done..............."
