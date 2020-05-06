#!/bin/bash
 #
 # fam_micro_benchmark.sh
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

CURRENTDIR=`pwd`

#base_dir=$CURRENTDIR/microbm

#mkdir -p ${base_dir}/mb_logs/data_path
#!/bin/bash

CURRENTDIR=`pwd`

TESTFILE=fam_microbenchmark_datapath
if [[ $4 == "atomic" ]]
then
TESTFILE=fam_microbenchmark_atomic
fi

TESTDATASIZE=$1
TESTAPIFILTER=$2
TESTITERATION=$3

#export PATH="$PATH:$CURRENTDIR/../third-party/build/bin/"
#export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$CURRENTDIR/../third-party/build/lib"
#module load pmix/2.2.2
#module load gcc/6.2.0 automake/1.15 autoconf/2.69

cd $CURRENTDIR/build-rpc/test/microbench/fam-api-mb/
ulimit -c unlimited

#./$TESTFILE $1 --gtest_filter=FamPutGet.BlockingFamPutGet >> ${base_dir}/mb_logs/data_path/PE_PER_VMNODE_${1}_PGB.log
#./$TESTFILE $1 --gtest_filter=FamPutGet.BlockingFamPutGet > ${base_dir}/mb_logs/data_path/PE_${SLURMD_NODENAME}_${PMIX_RANK}_${1}_PBG.logS
./$TESTFILE $TESTDATASIZE $TESTITERATION --gtest_filter=$TESTAPIFILTER

exit_status=$?
cd $CURRENTDIR
#make reg-test
exit $exit_status

