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

echo "Tests DataItem APIs with 600 permission"

#Perm		uid	gid	operuid	opergid	CR	DR	ADi	DDi	Backup	Restore	DeleteBackup
#"RW----"	1001	1001	1001	1001	-	NA	-	✔	✔	✔	✔
#				1002	1001	-	NA	-	✖	✖	✖	✖
#				1003	1001	-	NA	-	✖	✖	✖	✖
#				1004	1004	-	NA	-	✖	✖	✖	✖


# As user1:user1 Create a region with permission 777 of size 100MB
su -c "./fam_create_region myRegion 100 777" user1 || test_failed "Create Region failed: $?" 1

# As user1 : Allocate DataItem
su -c "./fam_allocate_data myRegion myData 50 600" user1 || test_failed "Allocate DataItem user1 failed: $?"
# As user2 : Backup DataItem
su -c "./fam_backup_data myRegion myData myBackup" user1 2>&1 1>/dev/null || test_failed "Backup DataItem user1 failed: $?"

# As user2 : Restore to DataItem
su -c "./fam_restore_backup_data myRegion myData myBackup" user1 2>&1 1>/dev/null || test_failed "Restore DataItem user1 failed: $?"
# As user2 : Delete backup
su -c "./fam_delete_backup_data myBackup" user1 2>&1 1>/dev/null || test_failed "Delete myBackup user1 failed: $?"

# As user2 : Backup DataItem
!su -c "./fam_backup_data myRegion myData myBackup" user2 2>&1 1>/dev/null || test_failed "Backup DataItem user2 expected to fail: $?"

# As user2 : Restore to DataItem
#!su -c "./fam_restore_backup_data myRegion myData myBackup" user2 2>&1 1>/dev/null || test_failed "Restore DataItem user2 expected to fail: $?"
# As user2 : Delete backup
#! su -c "./fam_delete_backup_data myBackup" user2 2>&1 1>/dev/null || test_failed "Delete myBackup user2 expected to fail: $?"

# As user3 : Backup DataItem
#! su -c "./fam_backup_data myRegion myData myBackup" user3 2>&1 1>/dev/null || test_failed "Backup DataItem user3 expected to fail: $?"

# As user3 : Restore to DataItem
#! su -c "./fam_restore_backup_data myRegion myData myBackup" user3 2>&1 1>/dev/null || test_failed "Restore DataItem user3 expected to fail: $?"
# As user3 : Delete backup
#! su -c "./fam_delete_backup_data myBackup" user3 2>&1 1>/dev/null || test_failed "Delete myBackup user3 expected to fail: $?"

# As user4 : Backup DataItem
#! su -c "./fam_backup_data myRegion myData myBackup" user4  2>&1 1>/dev/null  || test_failed "Backup DataItem user4 expected to fail: $?"

# As user4 : Restore to DataItem
#! su -c "./fam_restore_backup_data myRegion myBackup" user4  2>&1 1>/dev/null  || test_failed "Restore DataItem user4 expected to fail: $?"

# As user4 : Delete backup
#! su -c "./fam_delete_backup_data myBackup" user4 || test_failed "Delete myBackup user4 expected to fail: $?"


# As user1 : Deallocate DataItem
su -c "./fam_deallocate_data myRegion myData" user1 || test_failed "Deallocate DataItem user1 failed: $?"

# Cleanup the region and exit with saved return status
# As user1:user1 delete region created at the begining
su -c "./fam_destroy_region  myRegion" user1 || test_failed "Destroy Region user1 failed: $?"

exit $test_status

