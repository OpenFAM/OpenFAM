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

echo "Tests DataItem APIs with 666 permission"

#Perm		uid	gid	operuid	opergid	CR	DR	ADi	DDi	CRP	CDiP	RL	DL	Put	Get
#"RW RW RW"	1001	1001	1001	1001	-	NA	-	✔	NA	✔	NA	✔	✔ 	✔ 
#				1002	1001	-	NA	-	✔ 	NA	✔ 	NA	✔ 	✔ 	✔ 
#				1003	1001	-	NA	-	✔ 	NA	✔ 	NA	✔ 	✔ 	✔ 
#				1004	1004	-	NA	-	✔ 	NA	✔ 	NA	✔ 	✔ 	✔ 



# As user1:user1 Create a region with permission 777 of size 100MB
su -c "./fam_create_region myRegion  777 100" user1 || test_failed "Create Region failed: $?" 1

# As user1 : Allocate DataItem
su -c "./fam_allocate_data myRegion myData 666 512" user1 || test_failed "Allocate DataItem user1 failed: $?"

# As user1 : Lookup DataItem
su -c "./fam_lookup_data myRegion myData" user1 2>&1 1>/dev/null || test_failed "Lookup DataItem user1 failed: $?"

# As user1 : Read DataItem
su -c "./fam_get_data myRegion myData 0 13" user1 2>&1 1>/dev/null || test_failed "Read DataItem user1 failed: $?"

# As user1 : Write DataItem
su -c "./fam_put_data myRegion myData 0 13 welcome" user1 2>&1 1>/dev/null || test_failed "Write DataItem user1 failed: $?"

# As user2 : Lookup DataItem
su -c "./fam_lookup_data myRegion myData" user2 2>&1 1>/dev/null || test_failed "Lookup DataItem user2 failed: $?"

# As user2 : Read DataItem
su -c "./fam_get_data myRegion myData 0 13" user2 2>&1 1>/dev/null || test_failed "Read DataItem user2 failed: $?"

# As user2 : Write DataItem
su -c "./fam_put_data myRegion myData 0 13 welcome" user2 2>&1 1>/dev/null || test_failed "Write DataItem user2 failed: $?"

# As user2 : Change DataItem Permission
su -c "./fam_changepermission_data myRegion myData 777" user2 2>&1 1>/dev/null || test_failed "Change DataItem Permission user2 failed: $?"
#<TO DO> Change it back to old permission

# As user3 : Lookup DataItem
su -c "./fam_lookup_data myRegion myData" user3 2>&1 1>/dev/null || test_failed "Lookup DataItem user3 failed: $?"

# As user3 : Read DataItem
su -c "./fam_get_data myRegion myData 0 13" user3 2>&1 1>/dev/null || test_failed "Read DataItem user3 failed: $?"

# As user3 : Write DataItem
su -c "./fam_put_data myRegion myData 0 13 welcome" user3 2>&1 1>/dev/null || test_failed "Write DataItem user3 failed: $?"

# As user3 : Change DataItem Permission
su -c "./fam_changepermission_data myRegion myData 777" user3 2>&1 1>/dev/null || test_failed "Change DataItem Permission user3 failed: $?"
#<TO DO> Change it back to old permission

# As user4 : Lookup DataItem
su -c "./fam_lookup_data myRegion myData" user4 2>&1 1>/dev/null || test_failed "Lookup DataItem user4 failed: $?"

# As user4 : Read DataItem
su -c "./fam_get_data myRegion myData 0 13" user4 2>&1 1>/dev/null || test_failed "Read DataItem user4 failed: $?"

# As user4 : Write DataItem
su -c "./fam_put_data myRegion myData 0 13 welcome" user4 2>&1 1>/dev/null || test_failed "Write DataItem user4 failed: $?"

# As user4 : Change DataItem Permission
su -c "./fam_changepermission_data myRegion myData 777" user4 2>&1 1>/dev/null || test_failed "Change DataItem Permission user4 failed: $?"
#<TO DO> Change it back to old permission

# As user4 : Deallocate DataItem
su -c "./fam_deallocate_data myRegion myData" user4 || test_failed "Deallocate DataItem user4 failed: $?"

# As user1 : Allocate DataItem
su -c "./fam_allocate_data myRegion myData 666 512" user1 || test_failed "Allocate DataItem user1 failed: $?"

# As user3 : Deallocate DataItem
su -c "./fam_deallocate_data myRegion myData" user3 || test_failed "Deallocate DataItem user3 failed: $?"

# As user1 : Allocte DataItem
su -c "./fam_allocate_data myRegion myData 666 512" user1 || test_failed "Allocate DataItem user1 failed: $?"

# As user2 : Deallocate DataItem
su -c "./fam_deallocate_data myRegion myData" user2 || test_failed "Deallocate DataItem user2 failed: $?"

# As user1 : Allocte DataItem
su -c "./fam_allocate_data myRegion myData 666 512" user1 || test_failed "Allocate DataItem user1 failed: $?"

# As user1 : Deallocate DataItem
su -c "./fam_deallocate_data myRegion myData" user1 || test_failed "Deallocate DataItem user1 failed: $?"

# Cleanup the region and exit with saved return status
# As user1:user1 delete region created at the begining
su -c "./fam_destroy_region  myRegion" user1 || test_failed "Destroy Region user1 failed: $?"

exit $test_status

