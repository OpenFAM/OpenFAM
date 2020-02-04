# Prerequisites for running permission tests
  ## This tests are for checking the permission requirements for different functionalities like allocating data
  ## item from a region, changing data permissions, destroying region etc by a different user. So test requires
  ## two users on the system where we are running. The test assumes there is a user named "ex1" exists other than   
  ## the superuser root.

## Running permission tests
------------------------
# 1> Execute command - "make" from OpenFAM/build/build-rpc directory. It creates essential executables to run the test.
     ## cd OpenFAM/build/build-rpc
     ## make
# 2> Run the setup script present in OpenFAM/build/build-rpc folder. This script sets the environment variables
#    to access the required binaries and also runs the PRTE (Reference Server).
# 3> Add the following users and modify their group membership as given below:
#    1. user1 : user1(Primary Group)
#    2. user2 :	user2(Primary Group), user1(secondary group)	
#    3. user3 : user1(primary Group), user3(secondary group)
#    4. user4 : user4(Primary Group)
     ## sudo useradd user1
     ## sudo useradd user2
     ## sudo usermod -G user1 user2
     ## sudo useradd user3
     ## sudo usermod -g user1 user3
     ## sudo usermod -G user3 user3
     ## sudo useradd user4
# 4> Run permission test
     ## make permission-test
