#!/bin/bash
 #
 #test_series_datapath.sh
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



if [ $# -lt 2 ]
then
echo "Error: Base dir or allocator type not specified."
echo "usage: ./test_series_datapath.sh <base_dir> <Allocator, RPC/NVMM>"
exit 1
fi

#Running tests on memoryserver
#Note : For running this test on cluster environment                           \
run_datapath_mb_MEMSERVER.sh to run_datapath_mb_CLUSTER.sh
for i in 1 2 4 8 16 32 64 
do
./run_datapath_mb_MEMSERVER.sh $i 1 256 100  $1 $2
wait
./run_datapath_mb_MEMSERVER.sh $i 1 512 100  $1 $2
wait
./run_datapath_mb_MEMSERVER.sh $i 1 1024 100  $1 $2
wait
./run_datapath_mb_MEMSERVER.sh $i 1 4096 100  $1 $2
wait
./run_datapath_mb_MEMSERVER.sh $i 1 65536 100  $1 $2
wait
./run_datapath_mb_MEMSERVER.sh $i 1 131072 100  $1 $2
wait
./run_datapath_mb_MEMSERVER.sh $i 1 524288 100  $1 $2
wait
./run_datapath_mb_MEMSERVER.sh $i 1 1048576 100  $1 $2
wait
./run_datapath_mb_MEMSERVER.sh $i 1 4194304 100  $1 $2
done
