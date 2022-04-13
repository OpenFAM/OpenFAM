#!/bin/bash
#
# nvmm_build.sh
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
# This script assumes you have NOPASSWD access to sudo command
# If it doesnt work, encourage you to copy paste the command and workaround, 
# Or you can edit sudoers file to give NOPASSWD access.
# 
OS=`grep -m1 "ID=" /etc/os-release | sed 's/"//g' | sed 's/ID=//g' `
CWD=`pwd`
case $OS in
        "ubuntu")
                sudo apt-get install --assume-yes build-essential cmake libboost-all-dev
                if [[ $? > 0 ]]
                then
                        echo "apt-get install failed, exiting..."
                        exit 1
                fi
                sudo apt-get install --assume-yes autoconf pkg-config doxygen graphviz
                if [[ $? > 0 ]]
                then
                        echo "apt-get install failed, exiting..."
                        exit 1
                fi
                ;;
        "rhel" | "centos")
                sudo yum install --assumeyes gcc gcc-c++ kernel-devel make cmake autoconf glibc python python-devel automake libtool doxygen
                ;;
esac

mkdir build
cd build
cmake .. -DFAME=OFF
make -j
if [[ $? > 0 ]]
then
        echo "NVMM make failed, exiting..."
        exit 1
fi

