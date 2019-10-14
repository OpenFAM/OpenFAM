# RUNNING MICRO BENCHMARK TESTS

## Run tests for datapath APIs

 $ cd scripts

 $ ./test_series_datapath.sh base_dir
 ('base_dir' is path till OpenFAM root dir, eg. /home/OpenFAM)

 (Note: Log files will be created under {base_dir}/mb_logs/data_path)

## Run tests for atomic APIs

 $ cd scripts

 $ ./test_series_atomic.sh base_dir

 (Note: Log files will be created under {base_dir}/mb_logs/atomic)
## Creating CSV file from test logs

 $ cd scripts 

 $ ./extract_microbench.py log_dir csv_file

 ('log_dir' is a path to directory where log files are stored and 'csv_file' is the name of the CSV file to be created)

