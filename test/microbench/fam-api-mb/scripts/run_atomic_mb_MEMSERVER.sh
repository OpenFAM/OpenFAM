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

#!/bin/bash
unset http_proxy
unset https_proxy

base_dir=$4

if [[ $5 == "RPC" ]]
then
    cmd="--allow-run-as-root --bind-to core -n $1 ${base_dir}/build/build-rpc/test/microbench/fam-api-mb/fam_microbenchmark_atomic"
    log_dir="${base_dir}/mb_logs/rpc/atomic"
    mkdir -p $log_dir
else
    cmd="--allow-run-as-root --bind-to core -n $1 ${base_dir}/build/build-shm/test/microbench/fam-api-mb/fam_microbenchmark_atomic"
    log_dir="${base_dir}/mb_logs/shm/atomic"
    mkdir -p $log_dir
fi

${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_FETCH.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchAddInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_FADD.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchSubInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_FSUB.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchAndInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_FAND.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchOrInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_FOR.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchXorInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_FXOR.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchMinInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_FMIN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchMaxInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_FMAX.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchCmpswapInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_FCSWAP.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchSwapInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_FSWAP.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.SetInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_SETN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.NonFetchAddInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_ADDN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.NonFetchSubInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_SUBN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamLogicalAtomics.NonFetchAndUInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_ANDN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamLogicalAtomics.NonFetchOrUInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_ORN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamLogicalAtomics.NonFetchXorUInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_XORN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamMinMaxAtomicMicrobench.NonFetchMinInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_MINN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamMinMaxAtomicMicrobench.NonFetchMaxInt64 >$log_dir/${1}PE_${2}MEM_SERVER_${3}_MAXN.log
wait
