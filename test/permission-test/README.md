# Prerequisites for running permission tests
  ## This tests are for checking the permission requirements for different functionalities like allocating data
  ## item from a region, changing data permissions, destroying region etc by a different user. So test requires
  ## four users on the system where we are running. 
## Running permission tests
------------------------
# 1> Execute command - "make" from OpenFAM/build/build-rpc directory. It creates essential executables to run the test.
     ## cd OpenFAM/build/build-rpc
     ## make
# 2> Run the setup script present in OpenFAM/build/build-rpc folder. This script sets the environment variables
#    to access the required binaries and also runs the PRTE (Reference Server).
# 3> Add the following users and modify their group membership by running create_users.sh script.
#    1. user1 : user1(Primary Group)
#    2. user2 :	user1(Primary Group), user2(secondary group)	
#    3. user3 : user3(primary Group), user1(secondary group)
#    4. user4 : user4(Primary Group)
# 4> Run permission test as superuser.
     ## make permission-test
