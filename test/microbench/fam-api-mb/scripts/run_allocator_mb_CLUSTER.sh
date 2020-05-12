#!/bin/bash
 #
 # run_datapath_mb_MEMSERVER.sh
 # Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All rights
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

#$1 = no of tasks
#$2 = no of nodes
#$3 = iteration
#$4 = base dir
#$5 = option to select model(NVMM or RPC)

base_dir=$4
hosts=hpc-cn[02-17]

if [[ $5 == "RPC" ]]
then
        cmd="srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ${base_dir}/build/build-rpc/test/microbench/fam-api-mb/fam_microbenchmark_allocator $3"
        log_dir="${base_dir}/mb_logs/rpc/allocator_allocate_single_region"
        mkdir -p $log_dir
else
        cmd="--allow-run-as-root --bind-to socket -n $1 ${base_dir}/build/build-shm/test/microbench/fam-api-mb/fam_microbenchmark_allocator $3"
        log_dir="${base_dir}/mb_logs/shm/allocator"
        mkdir -p $log_dir
fi

sh -x ./rm.sh ;srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_CDR.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamCreateDestroyRegion.FamCreateDestroyRegionMultiple1 >$log_dir/${1}PE_CLIENT_CDR.log
pkill srun;pkill srun
sleep 20


sh -x ./rm.sh ;srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_CR.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamCreateDestroyRegion.FamCreateRegionMultiple2 >$log_dir/${1}PE_CLIENT_CR.log
pkill srun;pkill srun
sleep 20
wait

srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_LR.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamLookup.FamLookupRegion >$log_dir/${1}PE_CLIENT_LR.log
pkill srun;pkill srun
sleep 20
wait

srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_DR.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamCreateDestroyRegion.FamDestroyRegionMultiple2 >$log_dir/${1}PE_CLIENT_DR.log
pkill srun;pkill srun
sleep 20
wait

sh -x ./rm.sh ;srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_ADI.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamAllocateDellocate.FamAllocateDellocateMultiple1 >$log_dir/${1}PE_CLIENT_ADI.log
pkill srun;pkill srun
sleep 20


sh -x ./rm.sh ;srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_${4}_MEMSERVER_AI.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamAllocateDeallocate.FamAllocateMultiple2 >$log_dir/${1}PE_${4}_CLIENT_AI.log
pkill srun;pkill srun
sleep 20
wait

srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_LD.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamLookup.FamLookupDataItem >$log_dir/${1}PE_CLIENT_LD.log
pkill srun;pkill srun
sleep 20
wait

srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_DI.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamAllocateDeallocate.FamDeallocateMultiple2 >$log_dir/${1}PE_CLIENT_DI.log
pkill srun;pkill srun
sleep 20
wait

sh -x ./rm.sh ;srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_ASR.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamAllocateDeallocate.FamAllocateSingleRegion >$log_dir/${1}PE_CLIENT_ASR.log
pkill srun;pkill srun
sleep 20

srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_LDSR.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamLookup.FamLookupDataItemSingleRegion >$log_dir/${1}PE_CLIENT_LDSR.log
pkill srun;pkill srun
sleep 20
wait

srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_DISR.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamAllocateDeallocate.FamDeallocateSingleRegion >$log_dir/${1}PE_CLIENT_DISR.log
pkill srun;pkill srun
sleep 20
wait

sh -x ./rm.sh ;srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_RCP.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamChangePermissions.RegionChangePermission >$log_dir/${1}PE_CLIENT_RCP.log
pkill srun;pkill srun
sleep 20
wait

sh -x ./rm.sh ;srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_DCP.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamChangePermissions.DataItemChangePermission >$log_dir/${1}PE_CLIENT_DCP.log
pkill srun;pkill srun
sleep 20
wait

sh -x ./rm.sh ;srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_CP.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamCopy.FamCopyAndWait >$log_dir/${1}PE_CLIENT_CP.log
pkill srun;pkill srun
sleep 20
wait

sh -x ./rm.sh ;srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2 ${base_dir}/build/build-rpc/src/memoryserver -m 10.1.0.101 -p "verbs;ofi_rxm" > $log_dir/${1}PE_MEMSERVER_RSZ.log 2>&1 &
sleep 20
$cmd --gtest_filter=FamAllocator.FamResizeRegion >$log_dir/${1}PE_CLIENT_RSZ.log
pkill srun;pkill srun
sleep 20
wait
