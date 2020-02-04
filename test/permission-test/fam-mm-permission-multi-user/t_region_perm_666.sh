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

echo "Tests APIs with 666 permission"

#Perm		uid	gid	operuid	opergid	CR	DR	ADi	DDi	CRP	CDiP	RL	DL	Put	Get
#"RW RW RW"	1001	1001	1001	1001	-	✔	✔	✔	✔	✔	✔	✔	✔	✔
#				1002	1001	-	✔ 	✔ 	NA	✔ 	NA	✔ 	NA	NA	NA
#				1003	1001	-	✔ 	✔ 	NA	✔ 	NA	✔ 	NA	NA	NA
#				1004	1004	-	✔ 	✔ 	NA	✔ 	NA	✔ 	NA	NA	NA



# As user1:user1 Create a region with permission 666 of size 100MB
su -c "./fam_create_region myRegion  666 100" user1 || test_failed "Create Region failed: $?" 1

# As user1 : Allocate DataItem
su -c "./fam_allocate_data myRegion myData 666 512" user1 || test_failed "Allocate DataItem user1 failed: $?"

# As user1 : Lookup Region
su -c "./fam_lookup_region myRegion" user1 2>&1 1>/dev/null || test_failed "Lookup Region user1 failed: $?"

# As user1 : Lookup DataItem
su -c "./fam_lookup_data myRegion myData" user1 2>&1 1>/dev/null || test_failed "Lookup DataItem user1 failed: $?"

# As user1 : Deallocate DataItem
su -c "./fam_deallocate_data myRegion myData" user1 || test_failed "Deallocate DataItem user1 failed: $?"

# As user2:user1(PrimaryGroup) perform lookup region on "myRegion"
su -c "./fam_lookup_region myRegion" user2 2>&1 1>/dev/null 2>/dev/null || test_failed "Lookup Region user2 failed: $?"

# As user2 : Allocate DataItem
su -c "./fam_allocate_data myRegion myData 666 512" user2 2>&1 1>/dev/null 2>/dev/null || test_failed "Allocate DataItem user2 failed: $?"

# As user2 : Change Region permission
su -c "./fam_changepermission_region myRegion 777" user2 2>&1 1>/dev/null 2>/dev/null || test_failed "Change Region permission user2 failed: $?"
#<TO DO> if succeeds we should change it back to old permission in all cases

# As user3:user1 as secondary group perform lookup region on "myRegion"
su -c "./fam_lookup_region myRegion" user3 2>&1 1>/dev/null 2>/dev/null || test_failed "Lookup Region user3 failed: $?"

# As user3 : Allocate DataItem
su -c "./fam_allocate_data myRegion myData 666 512" user3 2>&1 1>/dev/null 2>/dev/null || test_failed "Allocate DataItem user3 failed: $?"

# As user3 : Change Region permission
su -c "./fam_changepermission_region myRegion 777" user3 2>&1 1>/dev/null 2>/dev/null || test_failed "Change Region permission user3 failed: $?"
#<TO DO> if succeeds we should change it back to old permission

# As user4:user4 perform lookup region on "myRegion"
su -c "./fam_lookup_region myRegion" user4 2>&1 1>/dev/null 2>/dev/null || test_failed "Lookup Region user4 failed: $?"

# As user4 : Allocate DataItem
su -c "./fam_allocate_data myRegion myData 666 512" user4 2>&1 1>/dev/null 2>/dev/null || test_failed "Allocate DataItem user4 failed: $?"

# As user4 : Change Region permission
su -c "./fam_changepermission_region myRegion 777" user4 2>&1 1>/dev/null 2>/dev/null || test_failed "Change Region permission user4 failed: $?"
#<TO DO> if succeeds we should change it back to old permission

# As user4 Delete Region
su -c "./fam_destroy_region  myRegion" user4 2>&1 1>/dev/null 2>/dev/null || test_failed "Destroy Region user4 failed: $?"

# As user1:user1 Create a region with permission 666 of size 100MB
su -c "./fam_create_region myRegion  666 100" user1 || test_failed "Create Region failed: $?" 1

# As user2 Delete Region
su -c "./fam_destroy_region  myRegion" user2 2>&1 1>/dev/null 2>/dev/null || test_failed "Destroy Region user2 failed: $?"

# As user1:user1 Create a region with permission 666 of size 100MB
su -c "./fam_create_region myRegion  666 100" user1 || test_failed "Create Region failed: $?" 1

# As user3 Delete Region
su -c "./fam_destroy_region  myRegion" user3 2>&1 1>/dev/null 2>/dev/null || test_failed "Destroy Region user3 failed: $?"

# As user1:user1 Create a region with permission 666 of size 100MB
su -c "./fam_create_region myRegion  666 100" user1 || test_failed "Create Region failed: $?" 1

# Cleanup the region and exit with saved return status
# As user1:user1 delete region created at the begining
su -c "./fam_destroy_region  myRegion" user1 || test_failed "Destroy Region user1 failed: $?"

exit $test_status

