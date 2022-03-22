#!/bin/bash

test_status=0

test_failed() {
  echo $1
  test_status=1
  if [[ $2 -eq 1 ]]
  then
    exit 2
  fi
}

terminate_services() {
  pkill -9 memory_server
  pkill -9 metadata_server
  pkill -9 cis_server
}

# Start volatile memory servers
python3 ../../../scripts/run_test.py @multi_mem_volatile.txt
# Call create_region to create volatile region with redundancy level as none
./test_fam_create_region "testReg" 1073741824 0 1 0 || test_failed "Create Region should not Fail: $?"
# Call create_region to create persistent region with redundancy level as none
! ./test_fam_create_region "testReg" 1073741824 1 1 0 || test_failed "Create Region Expected to Fail: $?"
# Call create_region to create volatile region with redundancy level as default
./test_fam_create_region "testReg" 1073741824 0 1 1 || test_failed "Create Region should not Fail: $?"
# Call create_region to create persistent region with redundancy level as default
! ./test_fam_create_region "testReg" 1073741824 1 1 1 || test_failed "Create Region Expected to Fail: $?"
terminate_services

# Start persistent memory servers
python3 ../../../scripts/run_test.py @multi_mem_persistent.txt
# Call create_region to create volatile region
! ./test_fam_create_region "testReg" 1073741824 0 1 0 || test_failed "Create Region Expected to Fail: $?"
# Call create_region to create persistent region
./test_fam_create_region "testReg" 1073741824 1 1 0 || test_failed "Create Region should not Fail: $?"
terminate_services

exit $test_status
