#!/bin/bash

for i in $( echo user1 user2 user3 user4 ); do
    useradd $i
    if [[ $? == 0 ]]; then
        echo "user $i added successfully!"
        echo $i:$i"123" | chpasswd
        echo "Password for user $i changed successfully"
    fi
done

usermod -G user1,user2 user2
usermod -g user1 user2

usermod -G user1,user3 user3
usermod -g user3 user3
