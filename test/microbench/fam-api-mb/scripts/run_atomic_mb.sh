 #!/bin/bash
 #
 # run_atomic_mb_MEMSERVER.sh
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

base_dir=$3

cmd="${base_dir}/build/test/microbench/fam-api-mb/fam_microbenchmark_atomic"
if [[ $4 == "memory_server" ]]
then
    log_dir="${base_dir}/mb_logs/memory_server/atomic"
    mkdir -p $log_dir
else
    log_dir="${base_dir}/mb_logs/shared_memory/atomic"
    mkdir -p $log_dir
fi

arg_file=$5
num_memserv=$(($(cat ${arg_file} | grep "memserverlist" | cut -d'=' -f2 | grep -o "," | wc -l) + 1))

launcher="${base_dir}/third-party/build/bin/mpirun -n $1"
#uncomment the following line incase of slurm is used as launcher
#and change the options accordingly
#launcher="srun -N $1 -n $1 --nodelist=127.0.0.1 --mpi=pmix_v2"

$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchInt64 >$log_dir/${1}PE_${2}_CLIENT_FETCH_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchAddInt64 >$log_dir/${1}PE_${2}_CLIENT_FADD_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchSubInt64 >$log_dir/${1}PE_${2}_CLIENT_FSUB_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchAndInt64 >$log_dir/${1}PE_${2}_CLIENT_FAND_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchOrInt64 >$log_dir/${1}PE_${2}_CLIENT_FOR_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchXorInt64 >$log_dir/${1}PE_${2}_CLIENT_FXOR_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchMinInt64 >$log_dir/${1}PE_${2}_CLIENT_FMIN_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchMaxInt64 >$log_dir/${1}PE_${2}_CLIENT_FMAX_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchCmpswapInt64 >$log_dir/${1}PE_${2}_CLIENT_FCSWAP_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchSwapInt64 >$log_dir/${1}PE_${2}_CLIENT_FSWAP_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.SetInt64 >$log_dir/${1}PE_${2}_CLIENT_SETN_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.NonFetchAddInt64 >$log_dir/${1}PE_${2}_CLIENT_ADDN_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamArithmaticAtomicmicrobench.NonFetchSubInt64 >$log_dir/${1}PE_${2}_CLIENT_SUBN_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamLogicalAtomics.NonFetchAndUInt64 >$log_dir/${1}PE_${2}_CLIENT_ANDN_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamLogicalAtomics.NonFetchOrUInt64 >$log_dir/${1}PE_${2}_CLIENT_ORN_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamLogicalAtomics.NonFetchXorUInt64 >$log_dir/${1}PE_${2}_CLIENT_XORN_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamMinMaxAtomicMicrobench.NonFetchMinInt64 >$log_dir/${1}PE_${2}_CLIENT_MINN_${num_memserv}.log
wait
$launcher $cmd --gtest_filter=FamMinMaxAtomicMicrobench.NonFetchMaxInt64 >$log_dir/${1}PE_${2}_CLIENT_MAXN_${num_memserv}.log
wait
