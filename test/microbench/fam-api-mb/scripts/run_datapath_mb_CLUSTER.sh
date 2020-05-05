 #
 # run_datapath_mb_CLUSTER.sh
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
#!/bin/bash -x

# $1 - number of task
# $2 - number of nodes
# $3 - data item size
# $4 - log directory.
# $5 - ip address

base_dir=$4
log_dir=${base_dir}/mb_logs/datapath
mkdir -p ${log_dir}

hosts=hpc-cn[02-10]


srun -N 1 -n 1 --nodelist=hpc-cn01 rm -rf  /dev/shm/$USER/
srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2  ./build-rpc/src/memoryserver -m $5 -p "verbs;ofi_rxm" $5 > ${log_dir}/${1}PE_${2}PER_MEMSERVER_${3}_PBUD.log 2>&1  &
sleep 20
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamPutGet.BlockingFamPutNewDesc $5 >  ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_PBUD.log
pkill srun;pkill srun;

srun -N 1 -n 1 --nodelist=hpc-cn01 rm -rf  /dev/shm/$USER/
srun -N 1 -n 1 --nodelist=hpc-cn01  --mpi=pmix_v2  ./build-rpc/src/memoryserver -m $5 -p "verbs;ofi_rxm"  &
sleep 20

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamPutGet.BlockingFamPut $5 >  ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_PB.log
wait

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamPutGet.BlockingFamGet $5 >  ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_GB.log
wait

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamPutGet.NonBlockingFamGet $5 > ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_GNB.log
wait

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamPutGet.NonBlockingFamPut $5 > ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_PNB.log
wait

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamScatter.BlockingScatterIndex $5 > ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_SIB.log
wait

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamScatter.BlockingGatherIndex $5 > ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_GIB.log
wait

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamScatter.NonBlockingScatterIndex $5 > ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_SINB.log
wait

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamScatter.NonBlockingGatherIndex $5 > ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_GINB.log
wait

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamScatter.BlockingScatterIndexSize $5 > ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_SISB.log
wait

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamScatter.BlockingGatherIndexSize $5 > ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_GISB.log
wait

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamScatter.NonBlockingScatterIndexSize $5 > ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_SISNB.log
wait

time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamScatter.NonBlockingGatherIndexSize $5 > ${log_dir}/${1}PE_${2}PER_VMNODE_${3}_GISNB.log
wait




