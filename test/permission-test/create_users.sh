#!/bin/bash

for i in $( cat users.txt ); do
    useradd $i
    echo "user $i added successfully!"
    echo $i:$i"123" | chpasswd
    echo "Password for user $i changed successfully"
done
#user1 = read 1 "users.txt"
#user2 = read 2 "users.txt"
#user3 = read 3 "users.txt"
usermod -g user1 user2
usermod -G user1 user3
