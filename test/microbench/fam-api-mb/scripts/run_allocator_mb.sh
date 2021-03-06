 #!/bin/bash
 #
 # run_allocator_mb_MEMSERVER.sh
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
cmd="${base_dir}/build/test/microbench/fam-api-mb/fam_microbenchmark_allocator $2 $5 $6"
if [[ $4 == "memory_server" ]]
then
	log_dir="${base_dir}/mb_logs/memory_server/allocator"
	mkdir -p $log_dir
else
	log_dir="${base_dir}/mb_logs/shared_memory/allocator"
	mkdir -p $log_dir
fi

arg_file=$7
tmp="$(cut -d'.' -f1 <<< $arg_file)"
num_memserv="$(cut -d'_' -f2 <<< $tmp)"

launcher="${base_dir}/third-party/build/bin/mpirun -n $1"
#uncomment the following line incase of slurm is used as launcher
#and change the options accordingly
#launcher=srun -N $1 -n $1 --nodelist=127.0.0.1 --mpi=pmix_v2 
python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamCreateDestroyRegion.FamCreateDestroyRegionMultiple1 >$log_dir/${1}PE_${5}_CLIENT_CDR_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER//
rm -rf /dev/shm/mem*
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamCreateDestroyRegion.FamCreateRegionMultiple2 >$log_dir/${1}PE_${5}_CLIENT_CR_${num_memserv}.log
wait

$launcher $cmd --gtest_filter=FamLookup.FamLookupRegion >$log_dir/${1}PE_${5}_CLIENT_LR_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamCreateDestroyRegion.FamDestroyRegionMultiple2 >$log_dir/${1}PE_${5}_CLIENT_DR_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER//
rm -rf /dev/shm/mem*
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamAllocateDellocate.FamAllocateDellocateMultiple1 >$log_dir/${1}PE_${6}_CLIENT_ADI_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER//
rm -rf /dev/shm/mem*
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamAllocateDeallocate.FamAllocateSingleRegion >$log_dir/${1}PE_${6}_CLIENT_ASR_${num_memserv}.log
wait

$launcher $cmd --gtest_filter=FamLookup.FamLookupDataItemSingleRegion >$log_dir/${1}PE_${6}_CLIENT_LDSR_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamAllocateDeallocate.FamDeallocateSingleRegion >$log_dir/${1}PE_${6}_CLIENT_DISR_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER/
rm -rf /dev/shm/mem*
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamAllocateDeallocate.FamAllocateMultiple2 >$log_dir/${1}PE_${6}_CLIENT_AI_${num_memserv}.log
wait

$launcher $cmd --gtest_filter=FamLookup.FamLookupDataItem >$log_dir/${1}PE_${6}_CLIENT_LD_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamAllocateDeallocate.FamDeallocateMultiple2 >$log_dir/${1}PE_${6}_CLIENT_DI_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER/
rm -rf /dev/shm/mem*
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamChangePermissions.RegionChangePermission >$log_dir/${1}PE_${5}_CLIENT_RCP_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER/
rm -rf /dev/shm/mem*
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamChangePermissions.DataItemChangePermission >$log_dir/${1}PE_${6}_CLIENT_DCP_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER/
rm -rf /dev/shm/mem*
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamCopy.FamCopyAndWait >$log_dir/${1}PE_${5}_CLIENT_CP_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER/
rm -rf /dev/shm/mem*
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher $cmd --gtest_filter=FamAllocator.FamResizeRegion >$log_dir/${1}PE_${5}_CLIENT_RSZ_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER/
rm -rf /dev/shm/mem*
sleep 20
wait

python3 ${base_dir}/scripts/run_test.py @$arg_file
sleep 20
$launcher ${base_dir}/build/test/microbench/fam-api-mb/fam_microbenchmark_datapath 256 100  --gtest_filter=FamPutGet.BlockingFamPutNewDesc >$log_dir/${1}PE_${6}_CLIENT_PBUD_${num_memserv}.log
pkill memory_server; pkill metadata_server; pkill cis_server
rm -rf /dev/shm/$USER/
rm -rf /dev/shm/mem*
sleep 20
wait
