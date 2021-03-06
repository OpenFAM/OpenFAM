# RUNNING MICRO BENCHMARK TESTS

## Run tests for datapath APIs

 $ cd scripts

 $ ./test_series_datapath.sh <base_dir> <model> <arg_file> 
 ('<base_dir>' is path till OpenFAM root dir, eg. /home/OpenFAM
  '<model>' is OpenFAM model either memory_server or shared memory
  '<arg_file>' argument file to be used to start services)

 (Note: Log files will be created under {<base_dir>}/mb_logs/data_path)

## Run tests for atomic APIs

 $ cd scripts

 $ ./test_series_atomic.sh <base_dir> <model> <arg_file>

 (Note: Log files will be created under {<base_dir>}/mb_logs/atomic)

## Run tests for allocator APIs

 $ cd scripts

 $ ./test_series_allocator.sh <base_dir> <model> <arg_file>

 (Note: Log files will be created under {<base_dir>}/mb_logs/allocator)

## Creating CSV file from test logs

 $ cd scripts 

 For extracting datapath and atomic profiling results

 $ ./extract_datapath_atomic_microbench.py log_dir csv_file

 For extracting libfabric profiling results

 $ ./extract_libfabric.py log_dir csv_file

 For extracting allocator profiling results

 $ ./extract_allocator_profiling.py log_dir csv_file

 ('log_dir' is a path to directory where log files are stored and 'csv_file' is the name of the CSV file to be created)

## With microbenchmark tests, please use fam_reset_profile, if any warmup calls are being made.
## fam_reset_profile is a method defined in fam.cpp, which is used to ensure the warmup calls are not considered 
## while collecting profiling data. 
## In addition, to be able to use fam_reset_profile, we need to add this method in include/fam/fam.h.

