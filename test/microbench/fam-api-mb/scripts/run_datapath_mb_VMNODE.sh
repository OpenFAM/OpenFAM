 #
 # run_datapath_mb_VMNODE.sh
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

base_dir=$4

HOSTS="-host mdc-01-ch4-cust1-vm1:$2,mdc-01-ch4-cust1-vm2:$2,mdc-01-ch4-cust1-vm3:$2,mdc-01-ch4-cust1-vm4:$2,mdc-01-ch4-cust1-vm5:$2,mdc-01-ch4-cust1-vm6:$2,mdc-01-ch4-cust1-vm7:$2,mdc-01-ch4-cust1-vm8:$2,mdc-01-ch4-cust1-vm9:$2,mdc-01-ch4-cust1-vm10:$2,mdc-01-ch4-cust1-vm11:$2,mdc-01-ch4-cust1-vm12:$2" 

if [[ $5 == "RPC" ]]
then
    cmd="--allow-run-as-root -n $1 $HOSTS ${base_dir}/build/build-rpc/test/microbench/fam-api-mb/fam_microbenchmark_datapath $3"
    log_dir="${base_dir}/mb_logs/rpc/data_path"
    mkdir -p $log_dir
else
    cmd="--allow-run-as-root -n $1 $HOSTS ${base_dir}/build/build-shm/test/microbench/fam-api-mb/fam_microbenchmark_datapath $3"
    log_dir="${base_dir}/mb_logs/shm/data_path"
    mkdir -p $log_dir
fi


${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamPutGet.BlockingFamPutGet >$log_dir/${1}PE_${2}PER_VMNODE_${3}_PGB.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamPutGet.NonBlockingFamGet >$log_dir/${1}PE_${2}PER_VMNODE_${3}_GNB.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamPutGet.NonBlockingFamPut >$log_dir/${1}PE_${2}PER_VMNODE_${3}_PNB.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamScatter.BlockingScatterGatherIndex >$log_dir/${1}PE_${2}PER_VMNODE_${3}_SGIB.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamScatter.NonBlockingScatterIndex >$log_dir/${1}PE_${2}PER_VMNODE_${3}_SINB.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamScatter.NonBlockingGatherIndex >$log_dir/${1}PE_${2}PER_VMNODE_${3}_GINB.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamScatter.BlockingScatterGatherIndexSize >$log_dir/${1}PE_${2}PER_VMNODE_${3}_SGISB.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamScatter.NonBlockingScatterIndexSize >$log_dir/${1}PE_${2}PER_VMNODE_${3}_SISNB.log
wait
${base_dir}/third-party/build/bin/mpirun $cmd --gtest_filter=FamScatter.NonBlockingGatherIndexSize >$log_dir/${1}PE_${2}PER_VMNODE_${3}_GISNB.log
wait
