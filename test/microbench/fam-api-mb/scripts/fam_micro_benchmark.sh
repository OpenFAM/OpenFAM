#!/bin/bash

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

