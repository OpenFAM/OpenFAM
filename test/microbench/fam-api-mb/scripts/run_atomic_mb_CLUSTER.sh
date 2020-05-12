#!/bin/bash
 #
 # run_atomic_mb_CLUSTER.sh
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


# $1 - number of task
# $2 - number of nodes
# $3 - data item size
# $4 - base directory.
# $5 - iteration
base_dir=$4

log_dir="${base_dir}/mb_logs/atomic"

mkdir -p ${log_dir}
HOSTS=hpc-cn[02-10]


time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.FetchInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_FETCH.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.FetchAddInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_FADD.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.FetchSubInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_FSUB.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.FetchAndInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_FAND.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.FetchOrInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_FOR.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.FetchXorInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_FXOR.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.FetchMinInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_FMIN.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.FetchMaxInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_FMAX.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.FetchCmpswapInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_FCSWAP.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.FetchSwapInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_FSWAP.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.SetInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_SETN.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.NonFetchAddInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_ADDN.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamArithmaticAtomicmicrobench.NonFetchSubInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_SUBN.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamLogicalAtomics.NonFetchAndUInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_ANDN.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamLogicalAtomics.NonFetchOrUInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_ORN.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamLogicalAtomics.NonFetchXorUInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_XORN.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamMinMaxAtomicMicrobench.NonFetchMinInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_MINN.log
wait
time srun -N $2 -n $1 --nodelist=$hosts --mpi=pmix_v2 ./fam_micro_benchmark.sh $3 FamMinMaxAtomicMicrobench.NonFetchMaxInt64 $5  "atomic" >$log_dir/${1}PE_MEM_SERVER_${3}_MAXN.log
wait

