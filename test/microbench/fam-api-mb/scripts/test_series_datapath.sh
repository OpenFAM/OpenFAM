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



if [ $# -lt 3 ]
then
echo "Error: Base dir or allocator type not specified."
echo "usage: ./test_series_datapath.sh <base_dir> <Model, memory_server/shared_memory> <arg_file>"
exit 1
fi

root_dir=$1
model=$2
arg_file=$3

python3 ${root_dir}/scripts/run_test.py @${arg_file}

#Running tests with different sizes of data
for i in 1 2 4 8 16 32 64 
do
./run_datapath_mb.sh $i 256 100  ${root_dir} ${model} ${arg_file}
wait
./run_datapath_mb.sh $i 512 100  ${root_dir} ${model} ${arg_file}
wait
./run_datapath_mb.sh $i 1024 100  ${root_dir} ${model} ${arg_file}
wait
./run_datapath_mb.sh $i 4096 100  ${root_dir} ${model} ${arg_file}
wait
./run_datapath_mb.sh $i 65536 100  ${root_dir} ${model} ${arg_file}
wait
./run_datapath_mb.sh $i 131072 100  ${root_dir} ${model} ${arg_file}
wait
./run_datapath_mb.sh $i 524288 100  ${root_dir} ${model} ${arg_file}
wait
./run_datapath_mb.sh $i 1048576 100  ${root_dir} ${model} ${arg_file}
wait
./run_datapath_mb.sh $i 4194304 100  ${root_dir} ${model} ${arg_file}
done
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER/; rm -rf /dev/shm/mem*
#Use the following commands to cleanup and kill services in case of cluster environment which uses slurm as workload manager
#srun -N 1 --nodelist=<your node-list> rm -rf /dev/shm/`whoami`; srun -N 1 --nodelist=<your node-list> rm -rf /dev/shm/mem*
#scancel --quiet -n metadata_server > /dev/null 2>&1; scancel --quiet -n memory_server > /dev/null 2>&1; scancel --quiet -n cis_server > /dev/null 2>&1
sleep 20

