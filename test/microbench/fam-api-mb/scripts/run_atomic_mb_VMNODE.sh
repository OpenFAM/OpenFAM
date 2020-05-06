 #!/bin/bash
 #
 # run_atomic_mb_VMNODE.sh
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

HOSTS="-host mdc-01-ch4-cust1-vm1:$2,mdc-01-ch4-cust1-vm2:$2,mdc-01-ch4-cust1-vm3:$2,mdc-01-ch4-cust1-vm4:$2,mdc-01-ch4-cust1-vm5:$2,mdc-01-ch4-cust1-vm6:$2,mdc-01-ch4-cust1-vm7:$2,mdc-01-ch4-cust1-vm8:$2,mdc-01-ch4-cust1-vm9:$2,mdc-01-ch4-cust1-vm10:$2,mdc-01-ch4-cust1-vm11:$2,mdc-01-ch4-cust1-vm12:$2"

if [[ $5 == "RPC" ]]
then
    cmd="--allow-run-as-root -n $1 $HOSTS ${base_dir}/build/build-rpc/test/microbench/fam-api-mb/fam_microbenchmark_atomic $3"
    log_dir="${base_dir}/mb_logs/rpc/atomic"
    mkdir -p $log_dir
else
    cmd="--allow-run-as-root -n $1 $HOSTS ${base_dir}/build/build-shm/test/microbench/fam-api-mb/fam_microbenchmark_atomic $3"
    log_dir="${base_dir}/mb_logs/shm/atomic"
    mkdir -p $log_dir
fi

${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.FetchALLInt64 >$log_dir/${1}PE_${2}PER_VMNODE_${3}_ALLF.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.SetInt64 >$log_dir/${1}PE_${2}PER_VMNODE_${3}_SETN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.NonFetchAddInt64 >$log_dir/${1}PE_${2}PER_VMNODE_${3}_ADDN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamArithmaticAtomicmicrobench.NonFetchSubInt64 >$log_dir/${1}PE_${2}PER_VMNODE_${3}_SUBN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamLogicalAtomics.NonFetchAndUInt64 >$log_dir/${1}PE_${2}PER_VMNODE_${3}_ANDN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamLogicalAtomics.NonFetchOrUInt64 >$log_dir/${1}PE_${2}PER_VMNODE_${3}_ORN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamLogicalAtomics.NonFetchXorUInt64 >$log_dir/${1}PE_${2}PER_VMNODE_${3}_XORN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamMinMaxAtomicMicrobench.NonFetchMinInt64 >$log_dir/${1}PE_${2}PER_VMNODE_${3}_MINN.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamMinMaxAtomicMicrobench.NonFetchMaxInt64 >$log_dir/${1}PE_${2}PER_VMNODE_${3}_MAXN.log
wait
