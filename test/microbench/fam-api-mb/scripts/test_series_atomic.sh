#!/bin/bash
#
#test_series_atomic.sh
#Copyright(c)2019 Hewlett Packard Enterprise Development, LP.All rights
#reserved.Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice,
#this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#this list of conditions and the following disclaimer in the documentation
#and / or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its contributors
#may be used to endorse or promote products derived from this software without
#specific prior written permission.
#
#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
#IS " AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
#ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#POSSIBILITY OF SUCH DAMAGE.
#
#See https: // spdx.org/licenses/BSD-3-Clause
#
#


if [ $# -lt 3 ]
then
echo "Error: Base dir or allocator type not specified."
echo "usage: ./test_series_atomic.sh <base_dir> <model, memory_server/shared_memory> <arg_file>"
exit 1
fi

root_dir=$1
model=$2
arg_file=$3

python3 $1/scripts/run_test.py @${arg_file}
sleep 20
wait
#Running tests for different PEs
for i in 1 2 4 8 16 32 64
do
./run_atomic_mb.sh $i 8 ${root_dir} ${model} ${arg_file}
wait
done
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER/
rm -rf /dev/shm/mem*
sleep 20
