#!/bin/bash
 #
 # run_datapath_mb_MEMSERVER.sh
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
base_dir=$4

cmd="${base_dir}/build/test/microbench/fam-api-mb/fam_microbenchmark_datapath $2 $3"
if [[ $5 == "memory_server" ]]
then
	log_dir="${base_dir}/mb_logs/memory_server/data_path"
	mkdir -p $log_dir
else
	log_dir="${base_dir}/mb_logs/shared_memory/data_path"
	mkdir -p $log_dir
fi

arg_file=$6

num_memserv=$(($(cat ${arg_file} | grep "memserverlist" | cut -d'=' -f2 | grep -o "," | wc -l) + 1))

launcher="${base_dir}/third-party/build/bin/mpirun -n $1"
#uncomment the following line incase of slurm is used as launcher
#and change the options accordingly
#launcher="srun -N $1 -n $1 --nodelist=127.0.0.1 --mpi=pmix_v2"

$launcher $cmd --gtest_filter=FamPutGet.BlockingFamPut >$log_dir/${1}PE_${2}_CLIENT_PB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamPutGet.BlockingFamGet >$log_dir/${1}PE_${2}_CLIENT_GB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamPutGet.NonBlockingFamGet >$log_dir/${1}PE_${2}_CLIENT_GNB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamPutGet.NonBlockingFamPut >$log_dir/${1}PE_${2}_CLIENT_PNB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamScatter.BlockingScatterIndex >$log_dir/${1}PE_${2}_CLIENT_SIB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamScatter.BlockingGatherIndex >$log_dir/${1}PE_${2}_CLIENT_GIB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamScatter.NonBlockingScatterIndex >$log_dir/${1}PE_${2}_CLIENT_SINB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamScatter.NonBlockingGatherIndex >$log_dir/${1}PE_${2}_CLIENT_GINB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamScatter.BlockingScatterIndexSize >$log_dir/${1}PE_${2}_CLIENT_SISB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamScatter.BlockingGatherIndexSize >$log_dir/${1}PE_${2}_CLIENT_GISB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamScatter.NonBlockingScatterIndexSize >$log_dir/${1}PE_${2}_CLIENT_SISNB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamScatter.NonBlockingGatherIndexSize >$log_dir/${1}PE_${2}_CLIENT_GISNB_${num_memserv}.log
wait



